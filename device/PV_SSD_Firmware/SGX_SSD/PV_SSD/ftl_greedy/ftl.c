// Copyright 2011 INDILINX Co., Ltd.
//
// This file is part of Jasmine.
//
// Jasmine is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Jasmine is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Jasmine. See the file COPYING.
// If not, see <http://www.gnu.org/licenses/>.
//
// GreedyFTL source file
//
// Author; Sang-Phil Lim (SKKU VLDB Lab.)
//
// - support POR
//  + fixed metadata area (Misc. block/Map block)
//  + logging entire FTL metadata when each ATA commands(idle/ready/standby) was issued
//

#include "jasmine.h"
//----------------------------------
// macro
//----------------------------------
#define INIT_BACKUP_WRITE_PTR 0xFFFFFFFF
#define VC_MAX 0xCDCD
// #define VC_FREE 0xCECE
#define VC_BACKUP 0xCFCF
#define VC_LOCK 0xFEFE

#define MISCBLK_VBN 0x1 // vblock #1 <- misc metadata
#define MAPBLKS_PER_BANK (((PAGE_MAP_BYTES / NUM_BANKS) + BYTES_PER_PAGE - 1) / BYTES_PER_PAGE)

/////////////////////////////////////////////////////////
// //backup block jw backupzone: 10G. 10G/4M=2560
// /*
// 각 bank에서
// 0 : firmware binary image
// 1 : FTL misc metadata
// 2-31 : page mapping table
// 32 : Free block for GC
// 33-352 : SGXSSD backup zone
// 353-2075 : Valid zone
// */
// #define SGXSSD_BACKUP_SIZE (10*1024) //MB단위
// #define SGXSSD_BACKUUP_BLKS (SGXSSD_BACKUP_SIZE / 4)    //4 : 한 블락은 4MB임. ->   10G를 위해 2560개의 블락이 필요함.
// #define BACKUP_BLKS_PER_BANK (SGXSSD_BACKUP_BLKS/NUM_BANKS) //bank은 8개임 -> 각 bank당 320개의 블록 할당.
//백업존은 META BLOCK에 포함하지 않고 DATA BLOCK처럼 관리할것임.
//#define META_BLKS_PER_BANK (1 + 1 + MAPBLKS_PER_BANK) // include block #0, misc block
//#define META_BLKS_PER_BANK (1 + 1 + MAPBLKS_PER_BANK + SGXSSD_BACKUP_BLKS_PER_BANK) // include block #0, misc block
UINT32 META_BLKS_PER_BANK = (1 + 1 + MAPBLKS_PER_BANK);
/////////////////////////////////////////////////////////

// the number of sectors of misc. metadata info.
#define NUM_MISC_META_SECT ((sizeof(misc_metadata) + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR)
#define NUM_VCOUNT_SECT ((VBLKS_PER_BANK * sizeof(UINT16) + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR)

// For SGX-SSD
#define MAX_POLICY 10

// For Reclaimability Bitmap
#define CLEAR 0
#define SET 1
#define TEST 2

//---------------------------------------
// Newly-Defined metadata for backup plan
//---------------------------------------
UINT32 backuped_page_num[NUM_BANKS] = {0};
UINT32 written_page_num[NUM_BANKS] = {0};


struct policy_metadata pvssd_policy[MAX_POLICY];
static const struct policy_metadata blank_policy = {0, 0, 0, 0};

//UINT32 cur_backup_write_ppn[NUM_BUCKETS];
UINT32 cur_backup_write_cnt[NUM_BUCKETS] = {0}; //jw    각  bucket마다 할당된 페이지의 갯수
//UINT32 erased_backup_cnt[NUM_BUCKETS] = {0};    //jw 각 버켓마다 지워진 블락 갯수
//UINT32 erased_bucket_idx = 0;
UINT8 init_write[NUM_BANKS] = {0};
//----------------------------------
// metadata structure
//----------------------------------

typedef struct _ftl_statistics
{
    UINT32 gc_cnt;
    UINT32 page_wcount; // page write count
} ftl_statistics;

typedef struct _misc_metadata
{
    UINT32 cur_write_vpn;                    // physical page for new write
    UINT32 cur_miscblk_vpn;                  // current write vpn for logging the misc. metadata
    UINT32 cur_mapblk_vpn[MAPBLKS_PER_BANK]; // current write vpn for logging the page mapping info.
    UINT32 gc_vblock;                        // vblock number for garbage collection
    UINT32 free_blk_cnt;                     // total number of free block count
    // jw backup zone block 얘가 backup pool의 역할을 대체함. 각 bank마다 cur_backup block 을 가리킴.
    ///UINT32 cur_backup_vpn;

    // 6 different OOB metadata
    UINT32 lpn[PAGES_PER_BLK];          // ***  [SGX-SSD] lpn for back-up             *** //
    // UINT32 old_vpn[PAGES_PER_BLK];      // ***  [SGX-SSD] old_vpn for back-up         *** //
   // UINT8 written_time[PAGES_PER_BLK]; // ***  [SGX-SSD] written_time for retrieval  *** //
    // UINT32 pid[PAGES_PER_BLK];          // ***  [SGX-SSD] pid for back-up             *** //
    // UINT32 fid[PAGES_PER_BLK];          // ***  [SGX-SSD] fid for back-up             *** //
    // UINT32 offset[PAGES_PER_BLK];       // ***  [SGX-SSD] file offset for retrieval   *** //
} misc_metadata;                        // per bank

//----------------------------------
// FTL metadata (maintain in SRAM)
//----------------------------------
static misc_metadata g_misc_meta[NUM_BANKS];
static ftl_statistics g_ftl_statistics[NUM_BANKS];
static UINT32 g_bad_blk_count[NUM_BANKS];

// SATA read/write buffer pointer id
UINT32 g_ftl_read_buf_id;
UINT32 g_ftl_write_buf_id;

UINT32 backup_block_count = 0;
UINT32 page_write_num = 0;
UINT32 page_copy_num = 0;
UINT32 erase_backup_num = 0;
UINT32 gc_count = 0;
UINT32 policy_num = 0;
UINT32 tmp_num_backup_page_allocated=0;
// For Delta Compression
UINT32 delta_offset[NUM_BUCKETS] = {0};
struct delta_header delta_header[NUM_BUCKETS];

extern UINT32 num_gc;
extern UINT32 num_move_backup;
extern UINT32 num_move_backup1;
extern UINT32 num_move_backup2;
extern UINT32 cur_time;
extern UINT32 num_backup;
extern UINT32 num_backup2;
extern UINT32 num_fin1;
extern UINT32 num_fin2;
extern UINT32 num_expired1;
extern UINT32 num_expired2;



extern UINT32 r_pos_bank;
extern UINT32 r_pos_vblock;
extern UINT32 r_pos_vpn;

//----------------------------------
// NAND layout
//----------------------------------
// block #0: scan list, firmware binary image, etc.
// block #1: FTL misc. metadata
// block #2 ~ #31: page mapping table
// block #32: a free block for gc
// block #33~: user data blocks

//----------------------------------
// macro functions
//----------------------------------
#define FILESYS_METADATA_BLKS_PER_BANK (150 / NUM_BANKS)
#define MAX_BAD_BLKS_PER_BANKS 70
// #define is_full_all_blks(bank) (g_misc_meta[bank].free_blk_cnt <= 1660-85) //easy test   //58GB 2G만 사용
#define is_full_all_blks(bank) (g_misc_meta[bank].free_blk_cnt == 1)
#define inc_full_blk_cnt(bank) (g_misc_meta[bank].free_blk_cnt--)
#define dec_full_blk_cnt(bank) (g_misc_meta[bank].free_blk_cnt++)
#define inc_mapblk_vpn(bank, mapblk_lbn) (g_misc_meta[bank].cur_mapblk_vpn[mapblk_lbn]++)
#define inc_miscblk_vpn(bank) (g_misc_meta[bank].cur_miscblk_vpn++)

// page-level striping technique (I/O parallelism)
#define get_num_bank(lpn) ((lpn) % NUM_BANKS)
#define get_bad_blk_cnt(bank) (g_bad_blk_count[bank])
#define get_cur_write_vpn(bank) (g_misc_meta[bank].cur_write_vpn)
#define set_new_write_vpn(bank, vpn) (g_misc_meta[bank].cur_write_vpn = vpn)

//#define set_new_backup_vpn(bank, vpn) (g_misc_meta[bank].cur_backup_vpn = vpn) //jw
//#define get_cur_backup_vpn(bank) (g_misc_meta[bank].cur_backup_vpn)            //jw

#define get_gc_vblock(bank) (g_misc_meta[bank].gc_vblock)
#define set_gc_vblock(bank, vblock) (g_misc_meta[bank].gc_vblock = vblock)
#define set_lpn(bank, page_num, lpn) (g_misc_meta[bank].lpn[page_num] = lpn)
#define get_lpn(bank, page_num) (g_misc_meta[bank].lpn[page_num])
#define get_miscblk_vpn(bank) (g_misc_meta[bank].cur_miscblk_vpn)
#define set_miscblk_vpn(bank, vpn) (g_misc_meta[bank].cur_miscblk_vpn = vpn)
#define get_mapblk_vpn(bank, mapblk_lbn) (g_misc_meta[bank].cur_mapblk_vpn[mapblk_lbn])
#define set_mapblk_vpn(bank, mapblk_lbn, vpn) (g_misc_meta[bank].cur_mapblk_vpn[mapblk_lbn] = vpn)
#define CHECK_LPAGE(lpn) ASSERT((lpn) < NUM_LPAGES)
#define CHECK_VPAGE(vpn) ASSERT((vpn) < (VBLKS_PER_BANK * PAGES_PER_BLK))

//----------------------------------
// FTL internal function prototype
//----------------------------------

static void format(void);
static void write_format_mark(void);
static void sanity_check(void);
static void load_pmap_table(void);
static void load_misc_metadata(void);
static void init_metadata_sram(void);
static void load_metadata(void);
static void logging_pmap_table(void);
static void logging_misc_metadata(void);
static void write_page(UINT32 const lpn, UINT32 const sect_offset, UINT32 const num_sectors, const struct file_metadata f);
static void set_vpn(UINT32 const lpn, UINT32 const vpn);
// static void garbage_collection(UINT32 const bank);
static void set_vcount(UINT32 const bank, UINT32 const vblock, UINT32 const vcount);
static BOOL32 is_bad_block(UINT32 const bank, UINT32 const vblock);
static BOOL32 check_format_mark(void);
static UINT32 get_vcount(UINT32 const bank, UINT32 const vblock);
static UINT32 get_vpn(UINT32 const lpn);
static UINT32 get_vt_vblock(UINT32 const bank);
static UINT32 assign_new_write_vpn(UINT32 const bank);

// Backup Zone Management Functions
static UINT32 get_backup_ppn(UINT32 const lpn);
static void set_backup_ppn(UINT32 const lpn, UINT32 const bank, UINT32 const vpn);
static void modified_garbage_collection(UINT32 const bank);
static void move_to_backup_zone_jw(UINT32 const bank, UINT32 const vpn, UINT32 const _lpn, UINT32 const _old_vpn, UINT32 const _written_time, UINT32 const _pid, UINT32 const _fid, UINT32 const _offset, UINT32 const prev_WT);
static UINT32 assign_new_backup_write_ppn(UINT32 const hash_index);
static void set_reclaim_bitmap(UINT32 const bank, UINT32 const vpn);
static void clr_reclaim_bitmap(UINT32 const bank, UINT32 const vpn);
static UINT32 test_reclaim_bitmap(UINT32 const bank, UINT32 const vpn);

// Free Block Pool Management
static UINT32 get_bucket (UINT32 time);

static void sanity_check(void)
{
    UINT32 dram_requirement = RD_BUF_BYTES + WR_BUF_BYTES + COPY_BUF_BYTES + FTL_BUF_BYTES + HIL_BUF_BYTES + TEMP_BUF_BYTES + BAD_BLK_BMP_BYTES + PAGE_MAP_BYTES + VCOUNT_BYTES + FLUSH_MAP_BYTES + RECLAIMABILITY_BMP_BYTES + BACKUP_ZONE_BMP_BYTES + BACKUP_PAGE_MAP_BYTES + BACKUP_DELTA_BUFFER_BYTES + OOB_OLD_VPN_BYTES + OOB_WRITTEN_TIME_BYTES + OOB_PID_BYTES + OOB_FID_BYTES + OOB_FILE_OFFSET_BYTES + BACKUP_CONTENT_TEMP_BYTES + BACKUP_POLICY_BMP_BYTES + BACKUP_BLK_BYTES;

    uart_printf("dram requirment: %d, dram size: %d \n", dram_requirement, DRAM_SIZE);
    uart_printf("size of misc_metadata is %d \n", sizeof(misc_metadata));

    // if ((dram_requirement != DRAM_SIZE) ||         // DRAM metadata size check
    if ((dram_requirement > DRAM_SIZE) ||         // DRAM metadata size check
        (sizeof(misc_metadata) > BYTES_PER_PAGE)) // misc metadata size check
    {
        uart_printf("dram requirment: %d\n", dram_requirement);
        uart_print("dram size is not enough!\n");
        led_blink();
        while (1)
            ;
    }
}

static void build_bad_blk_list(void)
{
    UINT32 bank, num_entries, result, vblk_offset;
    scan_list_t *scan_list = (scan_list_t *)TEMP_BUF_ADDR;

    mem_set_dram(BAD_BLK_BMP_ADDR, NULL, BAD_BLK_BMP_BYTES);

    disable_irq();

    flash_clear_irq();

    for (bank = 0; bank < NUM_BANKS; bank++)
    {
        SETREG(FCP_CMD, FC_COL_ROW_READ_OUT);
        SETREG(FCP_BANK, REAL_BANK(bank));
        SETREG(FCP_OPTION, FO_E);
        SETREG(FCP_DMA_ADDR, (UINT32)scan_list);
        SETREG(FCP_DMA_CNT, SCAN_LIST_SIZE);
        SETREG(FCP_COL, 0);
        SETREG(FCP_ROW_L(bank), SCAN_LIST_PAGE_OFFSET);
        SETREG(FCP_ROW_H(bank), SCAN_LIST_PAGE_OFFSET);

        SETREG(FCP_ISSUE, NULL);
        while ((GETREG(WR_STAT) & 0x00000001) != 0)
            ;
        while (BSP_FSM(bank) != BANK_IDLE)
            ;

        num_entries = NULL;
        result = OK;

        if (BSP_INTR(bank) & FIRQ_DATA_CORRUPT)
        {
            result = FAIL;
        }
        else
        {
            UINT32 i;

            num_entries = read_dram_16(&(scan_list->num_entries));

            if (num_entries > SCAN_LIST_ITEMS)
            {
                result = FAIL;
            }
            else
            {
                for (i = 0; i < num_entries; i++)
                {
                    UINT16 entry = read_dram_16(scan_list->list + i);
                    UINT16 pblk_offset = entry & 0x7FFF;

                    if (pblk_offset == 0 || pblk_offset >= PBLKS_PER_BANK)
                    {
#if OPTION_REDUCED_CAPACITY == FALSE
                        result = FAIL;
#endif
                    }
                    else
                    {
                        write_dram_16(scan_list->list + i, pblk_offset);
                    }
                }
            }
        }

        if (result == FAIL)
        {
            num_entries = 0; // We cannot trust this scan list. Perhaps a software bug.
        }
        else
        {
            write_dram_16(&(scan_list->num_entries), 0);
        }

        g_bad_blk_count[bank] = 0;

        for (vblk_offset = 1; vblk_offset < VBLKS_PER_BANK; vblk_offset++)
        {
            BOOL32 bad = FALSE;

#if OPTION_2_PLANE
            {
                UINT32 pblk_offset;

                pblk_offset = vblk_offset * NUM_PLANES;

                // fix bug@jasmine v.1.1.0
                if (mem_search_equ_dram(scan_list, sizeof(UINT16), num_entries + 1, pblk_offset) < num_entries + 1)
                {
                    bad = TRUE;
                }

                pblk_offset = vblk_offset * NUM_PLANES + 1;

                // fix bug@jasmine v.1.1.0
                if (mem_search_equ_dram(scan_list, sizeof(UINT16), num_entries + 1, pblk_offset) < num_entries + 1)
                {
                    bad = TRUE;
                }
            }
#else
            {
                // fix bug@jasmine v.1.1.0
                if (mem_search_equ_dram(scan_list, sizeof(UINT16), num_entries + 1, vblk_offset) < num_entries + 1)
                {
                    bad = TRUE;
                }
            }
#endif

            if (bad)
            {
                g_bad_blk_count[bank]++;
                set_bit_dram(BAD_BLK_BMP_ADDR + bank * (VBLKS_PER_BANK / 8 + 1), vblk_offset);
            }
        }
    }
}

void ftl_open(void)
{
    // debugging example 1 - use breakpoint statement!
    /* *(UINT32*)0xFFFFFFFE = 10; */

    /* UINT32 volatile g_break = 0; */
    /* while (g_break == 0); */

    led(0);
    sanity_check();
    //----------------------------------------
    // read scan lists from NAND flash
    // and build bitmap of bad blocks
    //----------------------------------------
    build_bad_blk_list();

    ///////////////////////////////////////////////////
    //----------------------------------------
    // If necessary, do low-level format
    // format() should be called after loading scan lists, because format() calls is_bad_block().
    //----------------------------------------

    /* 	if (check_format_mark() == FALSE) */
    if (TRUE)
    {
        uart_print("do format");
        format();
        uart_print("end format");
    }
    // load FTL metadata
    else
    {
        load_metadata();
    }
    g_ftl_read_buf_id = 0;
    g_ftl_write_buf_id = 0;

    // This example FTL can handle runtime bad block interrupts and read fail (uncorrectable bit errors) interrupts
    flash_clear_irq();

    SETREG(INTR_MASK, FIRQ_DATA_CORRUPT | FIRQ_BADBLK_L | FIRQ_BADBLK_H);
    SETREG(FCONF_PAUSE, FIRQ_DATA_CORRUPT | FIRQ_BADBLK_L | FIRQ_BADBLK_H);

    enable_irq();
}

void ftl_flush(void)
{
    //key
    // flush_num++;
    // uart_printf("flush num : %d write num : %d, page_num : %d",flush_num,write_num,PAGE_MAP_BYTES/BYTES_PER_PAGE);

    // ptimer_start();
    logging_pmap_table();
    logging_misc_metadata();
    // ptimer_stop_and_uart_print();
}

void ftl_read(UINT32 const lba, UINT32 const num_sectors)
{
    UINT32 remain_sects, num_sectors_to_read;
    UINT32 lpn, sect_offset;
    UINT32 bank, vpn;

    lpn = lba / SECTORS_PER_PAGE;
    sect_offset = lba % SECTORS_PER_PAGE;
    remain_sects = num_sectors;

    while (remain_sects != 0)
    {
        if ((sect_offset + remain_sects) < SECTORS_PER_PAGE)
        {
            num_sectors_to_read = remain_sects;
        }
        else
        {
            num_sectors_to_read = SECTORS_PER_PAGE - sect_offset;
        }
        bank = get_num_bank(lpn); // page striping
        vpn = get_vpn(lpn);
        CHECK_VPAGE(vpn);

        if (vpn != NULL)
        {
            nand_page_ptread_to_host(bank,
                                     vpn / PAGES_PER_BLK,
                                     vpn % PAGES_PER_BLK,
                                     sect_offset,
                                     num_sectors_to_read);
        }
        // The host is requesting to read a logical page that has never been written to.
        else
        {
            UINT32 next_read_buf_id = (g_ftl_read_buf_id + 1) % NUM_RD_BUFFERS;

#if OPTION_FTL_TEST == 0
            while (next_read_buf_id == GETREG(SATA_RBUF_PTR))
                ; // wait if the read buffer is full (slow host)
#endif

            // fix bug @ v.1.0.6
            // Send 0xFF...FF to host when the host request to read the sector that has never been written.
            // In old version, for example, if the host request to read unwritten sector 0 after programming in sector 1, Jasmine would send 0x00...00 to host.
            // However, if the host already wrote to sector 1, Jasmine would send 0xFF...FF to host when host request to read sector 0. (ftl_read() in ftl_xxx/ftl.c)
            mem_set_dram(RD_BUF_PTR(g_ftl_read_buf_id) + sect_offset * BYTES_PER_SECTOR,
                         0xFFFFFFFF, num_sectors_to_read * BYTES_PER_SECTOR);

            flash_finish();

            SETREG(BM_STACK_RDSET, next_read_buf_id); // change bm_read_limit
            SETREG(BM_STACK_RESET, 0x02);             // change bm_read_limit

            g_ftl_read_buf_id = next_read_buf_id;
        }
        sect_offset = 0;
        remain_sects -= num_sectors_to_read;
        lpn++;
    }
}

// void ftl_write(UINT32 const lba, UINT32 const num_sectors, UINT32 const pid)
void ftl_write(UINT32 const lba, UINT32 const num_sectors, struct file_metadata const f)
{
    UINT32 remain_sects, num_sectors_to_write;
    UINT32 lpn, sect_offset;

    lpn = lba / SECTORS_PER_PAGE;
    sect_offset = lba % SECTORS_PER_PAGE;
    remain_sects = num_sectors;


    while (remain_sects != 0)
    {
        // if ((lpn % NUM_BANKS) == 2 && lpn < 1004)
        //     uart_printf("bank : %d, lpn : %d", lpn % NUM_BANKS, lpn);

        if ((sect_offset + remain_sects) < SECTORS_PER_PAGE)
        {
            num_sectors_to_write = remain_sects;
        }
        else
        {
            num_sectors_to_write = SECTORS_PER_PAGE - sect_offset;
        }
        // single page write individually
        write_page(lpn, sect_offset, num_sectors_to_write, f);

        sect_offset = 0;
        remain_sects -= num_sectors_to_write;
        lpn++;
    }
}

// static void write_page(UINT32 const lpn, UINT32 const sect_offset, UINT32 const num_sectors, UINT32 pid)
static void write_page(UINT32 const lpn, UINT32 const sect_offset, UINT32 const num_sectors, struct file_metadata const f)
{
    CHECK_LPAGE(lpn);
    ASSERT(sect_offset < SECTORS_PER_PAGE);
    ASSERT(num_sectors > 0 && num_sectors <= SECTORS_PER_PAGE);

    UINT32 bank, old_vpn, new_vpn;
    UINT32 vblock, page_num, page_offset, column_cnt;

    bank = get_num_bank(lpn); // page striping
    page_offset = sect_offset;
    column_cnt = num_sectors;

    /* 
        Statistics for Verifying
    */
    written_page_num[bank]++;


    new_vpn = assign_new_write_vpn(bank);
    old_vpn = get_vpn(lpn);

    CHECK_VPAGE(old_vpn);
    CHECK_VPAGE(new_vpn);
    ASSERT(old_vpn != new_vpn);

    g_ftl_statistics[bank].page_wcount++;

    // if old data already exist,
    // which means that overwrite happens
    if (old_vpn != NULL)
    {
        vblock = old_vpn / PAGES_PER_BLK;
        page_num = old_vpn % PAGES_PER_BLK;

        //--------------------------------------------------------------------------------------
        // `Partial programming'
        // we could not determine whether the new data is loaded in the SATA write buffer.
        // Thus, read the left/right hole sectors of a valid page and copy into the write buffer.
        // And then, program whole valid data
        //--------------------------------------------------------------------------------------
        if (num_sectors != SECTORS_PER_PAGE)
        {
            // Performance optimization (but, not proved)
            // To reduce flash memory access, valid hole copy into SATA write buffer after reading whole page
            // Thus, in this case, we need just one full page read + one or two mem_copy
            if ((num_sectors <= 8) && (page_offset != 0))
            {
                // one page async read
                nand_page_read(bank,
                               vblock,
                               page_num,
                               FTL_BUF(bank));
                // copy 'left hole sectors' into SATA write buffer
                if (page_offset != 0)
                {
                    mem_copy(WR_BUF_PTR(g_ftl_write_buf_id),
                             FTL_BUF(bank),
                             page_offset * BYTES_PER_SECTOR);
                }
                // copy 'right hole sectors' into SATA write buffer
                if ((page_offset + column_cnt) < SECTORS_PER_PAGE)
                {
                    UINT32 const rhole_base = (page_offset + column_cnt) * BYTES_PER_SECTOR;

                    mem_copy(WR_BUF_PTR(g_ftl_write_buf_id) + rhole_base,
                             FTL_BUF(bank) + rhole_base,
                             BYTES_PER_PAGE - rhole_base);
                }
            }
            // left/right hole async read operation (two partial page read)
            else
            {
                // read `left hole sectors'
                if (page_offset != 0)
                {
                    if (vblock > 30000000)
                        uart_printf("Fuck3");

                    nand_page_ptread(bank,
                                     vblock,
                                     page_num,
                                     0,
                                     page_offset,
                                     WR_BUF_PTR(g_ftl_write_buf_id),
                                     RETURN_ON_ISSUE);
                }
                // read `right hole sectors'
                if ((page_offset + column_cnt) < SECTORS_PER_PAGE)
                {
                    if (vblock > 30000000)
                        uart_printf("Fuck4");
                    nand_page_ptread(bank,
                                     vblock,
                                     page_num,
                                     page_offset + column_cnt,
                                     SECTORS_PER_PAGE - (page_offset + column_cnt),
                                     WR_BUF_PTR(g_ftl_write_buf_id),
                                     RETURN_ON_ISSUE);
                }
            }
        }
        // full page write
        page_offset = 0;
        column_cnt = SECTORS_PER_PAGE;
        // // invalidate old page (decrease vcount)
        // if (get_vcount(bank, vblock) == VC_FREE)
        // {
        //     set_vcount(bank, vblock, 1);
        // }
        // else
        // {
        // uart_printf("bank : %d, vblock : %d, vcount : %d", bank, vblock, get_vcount(bank,vblock) );
        // uart_print("Invalidate Old Page");
        set_vcount(bank, vblock, get_vcount(bank, vblock) - 1);
        // }
    }


    vblock = new_vpn / PAGES_PER_BLK;
    page_num = new_vpn % PAGES_PER_BLK;
    // ASSERT(get_vcount(bank,vblock) < (PAGES_PER_BLK - 1));

    // if (bank == 2 && vblock == 1976)
    //     uart_printf("%dth page address : 0x%x", page_num, OOB_WRITTEN_TIME_ADDR + ((bank * PAGES_PER_BLK + page_num) * sizeof(UINT32)));

    // if (bank == 2 && vblock == 1976){
    //     uart_printf("[ASSIGN!!!]write_vpn: %d bank : %d, vblock : %d, page_num : %d, lpn : %d, old_vpn : %d, prev_WT : %d, pid : %d, fid : %d", new_vpn, bank, vblock, page_num, lpn, old_vpn, cur_cnt, f.pid, f.fid);
    // }


    // write new data (make sure that the new data is ready in the write buffer frame)
    // (c.f FO_B_SATA_W flag in flash.h)
    page_write_num++;
    nand_page_ptprogram_from_host(bank,
                                  vblock,
                                  page_num,
                                  page_offset,
                                  column_cnt);

    // Seungjin Lee
    // OOB metadata write for backup chain
    // if old_vpn doesn't exist, it should be NULL
    g_misc_meta[bank].lpn[page_num] = lpn;
    //g_misc_meta[bank].written_time[page_num] = cur_time;

    // g_misc_meta[bank].old_vpn[page_num] = old_vpn;
    // g_misc_meta[bank].written_time[page_num] = cur_time;
    // g_misc_meta[bank].pid[page_num] = f.pid;
    // g_misc_meta[bank].fid[page_num] = f.fid;
    // g_misc_meta[bank].offset[page_num] = f.offset; [bank] | [page_num]
    // if(OOB_PID_ADDR + (bank * PAGES_PER_BLK + page_num) * sizeof(UINT32) == 1129639828 || OOB_PID_ADDR + (bank * PAGES_PER_BLK + page_num) * sizeof(UINT32) == 1129639832)
    //     uart_printf("fuck!!!! at %d, pid : %d fid : %d offset : %d", OOB_PID_ADDR + (bank * PAGES_PER_BLK + page_num) * sizeof(UINT32), f.pid, f.fid, f.offset);
  

    write_dram_32(OOB_OLD_VPN_ADDR + ((bank * PAGES_PER_BLK + page_num) * sizeof(UINT32)), old_vpn);
    write_dram_32(OOB_WRITTEN_TIME_ADDR + ((bank * PAGES_PER_BLK + page_num) * sizeof(UINT32)), cur_time);
    write_dram_32(OOB_PID_ADDR + ((bank * PAGES_PER_BLK + page_num) * sizeof(UINT32)), f.pid);
    write_dram_32(OOB_FID_ADDR + ((bank * PAGES_PER_BLK + page_num) * sizeof(UINT32)), f.fid);
    write_dram_32(OOB_FILE_OFFSET_ADDR + ((bank * PAGES_PER_BLK + page_num) * sizeof(UINT32)), f.offset);

      if(page_num / PAGES_PER_BLK == 93312 / PAGES_PER_BLK)
    {
        UINT32 ext_page = 93312 % PAGES_PER_BLK;
        UINT32 pr_old_vpn = read_dram_32(OOB_OLD_VPN_ADDR + (bank * PAGES_PER_BLK + ext_page) * sizeof(UINT32));
        UINT32 pr_old_pid = read_dram_32(OOB_PID_ADDR + (bank * PAGES_PER_BLK + ext_page) * sizeof(UINT32));
        uart_printf("problem3 : bank %d vblock %d vpn %d old_vpn %d pid %d", bank, vblock, page_num, pr_old_vpn, pr_old_pid);  
        uart_printf("old_vpn?? %d", old_vpn);
     }

    // if (bank == 2 && vblock == 1976) {
    //     UINT32 lpn2, old_vpn2, WT2, pid2, fid2;
    //     lpn2 = g_misc_meta[bank].lpn[page_num];
    //     old_vpn2 = read_dram_32(OOB_OLD_VPN_ADDR + ((bank * PAGES_PER_BLK + page_num) * sizeof(UINT32)));
    //     WT2 = read_dram_32(OOB_WRITTEN_TIME_ADDR + ((bank * PAGES_PER_BLK + page_num) * sizeof(UINT32)));
    //     pid2 = read_dram_32(OOB_PID_ADDR + ((bank * PAGES_PER_BLK + page_num) * sizeof(UINT32)));
    //     fid2 = read_dram_32(OOB_FID_ADDR + ((bank * PAGES_PER_BLK + page_num) * sizeof(UINT32)));

    //     uart_printf("[ASSIGN2!!!] bank : %d, vblock : %d, page_num : %d, lpn : %d, old_vpn : %d, prev_WT : %d, pid : %d, fid : %d", bank, vblock, page_num, lpn2, old_vpn2, WT2, pid2, fid2);
    // }

    // if((lpn % 8 == 2) && lpn > 600 && lpn < 750)
    //     uart_printf("[WRITE!!!] bank : %d, cur_vpn : %d, lpn : %d, old_vpn : %d, written_time : %d, pid : %d, fid : %d, offset : %d", new_vpn, lpn, old_vpn, cur_time, f.pid, f.fid, f.offset);
    
    // uart_printf("");
    // uart_print("*********************************************");
    // uart_printf("DATA : lpn : %d, old_vpn : %d, WT : %d, pid : %d, fid : %d, offset : %d, vpn : %d", lpn, old_vpn, cur_time, f.pid, f.fid, f.offset, new_vpn);
    // UINT32 a, b, c, d, e;
    // a = read_dram_32(OOB_OLD_VPN_ADDR + ((bank * PAGES_PER_BLK + page_num) * sizeof(UINT32)));
    // b = read_dram_32(OOB_WRITTEN_TIME_ADDR + ((bank * PAGES_PER_BLK + page_num) * sizeof(UINT32)));
    // c = read_dram_32(OOB_PID_ADDR + ((bank * PAGES_PER_BLK + page_num) * sizeof(UINT32)));
    // d = read_dram_32(OOB_FID_ADDR + ((bank * PAGES_PER_BLK + page_num) * sizeof(UINT32)));
    // e = read_dram_32(OOB_FILE_OFFSET_ADDR + ((bank * PAGES_PER_BLK + page_num) * sizeof(UINT32)));
    // uart_printf("-----------------------------------");
    // uart_printf("DRAM : lpn : %d, old_vpn : %d, WT : %d, pid : %d, fid : %d, offset : %d", g_misc_meta[bank].lpn[page_num], a, b, c, d, e);
    // uart_print("*********************************************");
    // uart_printf("");
    
    // For Debugging
    set_vpn(lpn, new_vpn);

    // Clear Bit DRAM!
    // Set Reclaimablity map to 0!
    // clr_bit_dram(RECLAIMABILITY_BMP_ADDR + (new_vpn / 8), new_vpn % 8);
    // if (f.pid == NULL)
    // {
    //     set_reclaim_bitmap(bank, new_vpn);

    //     uart_printf("f.pid is NULL???");
    // }
    // else
        clr_reclaim_bitmap(bank, new_vpn);

    // // // update the valid page count for greedy victim selection policy
    // if (get_vcount(bank, vblock) == VC_FREE) // if it's empty block
    // {
    //     set_vcount(bank, vblock, 1);
    // }
    // else // if not increment the number of valid pages
    // {
        // if (get_vcount(bank,vblock) == 127)
        //uart_printf("write_page.. - bank : %d, vblock : %d, vcount : %d", bank, vblock, get_vcount(bank, vblock));
    set_vcount(bank, vblock, get_vcount(bank, vblock) + 1);
    // }
}
// Move chain to backup zone

/*
jw input: Recent OV 주소: bank, vpn, 
          Recent OV OOB: _lpn, _old_vpn(chain) _written_time, pid, fid, offset
          recent V(valid)의 WT : prev_wt         
*/
typedef struct oobset
{
    UINT32 lpn, old_vpn, written_time, pid, fid, offset, expiration_time, backup_ppn;
    UINT32 bank, vpn;
} OOBSET;

/*
jw input: Recent OV 주소: bank, vpn, 
          Recent OV OOB: _lpn, _old_vpn(chain) _written_time, pid, fid, offset
          recent V(valid)의 WT : prev_wt
          
*/
//per-file chain에서는 체인마다 뱅크가 달라질 것으로 예상됨.
//per-page chain은 같은 lpn이므로 bank가 같다.
//구현이 달라질이유는 없다.
static void move_to_backup_zone_jw(UINT32 const bank, UINT32 const vpn, UINT32 const _lpn, UINT32 const _old_vpn, UINT32 const _written_time, UINT32 const _pid, UINT32 const _fid, UINT32 const _offset, UINT32 const prev_WT)
{
    UINT32 pblock, vblock, page_num;
    UINT32 j = 0;
    UINT32 exit_flag = 0;
    OOBSET cur, prev;
    UINT8 flag_expired=0;
    num_move_backup++;

    if (test_reclaim_bitmap(bank, vpn))
    {
        //uart_printf("reclaimed?");
        return;
    }
    else
    {
        if(prev_WT + pvssd_policy[_pid].ret_time < cur_time){
            set_reclaim_bitmap(bank, vpn);
            num_expired1++;
            flag_expired=1;
     //   if(bank==2)
       //     uart_printf("num_expired1 prev_WT %d, wrtime %d pid %d ret time %d, cur_time %d", prev_WT, _written_time, _pid, pvssd_policy[_pid].ret_time, cur_time);
            
            return;
        }
    }


    num_move_backup1++;
    cur.lpn = _lpn;
    cur.old_vpn = _old_vpn;
    cur.written_time = _written_time;
    cur.pid = _pid;
    cur.fid = _fid;
    cur.offset = _offset;

    cur.bank = cur.lpn % NUM_BANKS;
    cur.vpn = vpn;
     cur.expiration_time = prev_WT + pvssd_policy[cur.pid].ret_time;
    //cur.expiration_time = prev_WT + pvssd_policy[cur.fid].ret_time; // 임시
    if(flag_expired)
        cur.backup_ppn = 0;
    else
        cur.backup_ppn = assign_new_backup_write_ppn(get_bucket(cur.expiration_time)); //back page 할당
    
    do{
     //   if ((cur.lpn % 8 == 2) && cur.lpn > 600 && cur.lpn < 750) 
    //        uart_printf("bank : %d, vpn : %d, lpn : %d, old_vpn : %d, WT : %d, pid : %d, fid : %d, offset : %d", cur.bank, cur.vpn, cur.lpn, cur.old_vpn, cur.written_time, cur.pid, cur.fid, cur.offset); 

        // PRT 체크했을때 1이 나오면 -> 더이상 체인을 탐색해서는 안됨.
        if (test_reclaim_bitmap(bank, cur.old_vpn))
        {
            //uart_print("1");
            num_fin1++;
            // uart_print("reclaimed check");
            cur.old_vpn = NULL;
            exit_flag =1; 
        }
        else
        {
       //     uart_print("need to read prev ppn");
            // 과거 버전을 미리 읽어온다. ** per-file chaining에서는 ppn을 저장해야할듯.  old_vpn -> old_ppn으로!
            vblock = cur.old_vpn / PAGES_PER_BLK;
            page_num = cur.old_vpn % PAGES_PER_BLK;
          
            //nand_page_ptread(bank, vblock, PAGES_PER_BLK - 1, 0, (sizeof(UINT32) * PAGES_PER_BLK * 6), FTL_BUF(bank), RETURN_WHEN_DONE);
            nand_page_ptread(bank, vblock, PAGES_PER_BLK - 1, 0, ((6 * sizeof(UINT32) * PAGES_PER_BLK + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR), FTL_BUF(bank), RETURN_WHEN_DONE);
           // nand_page_ptread(bank, vt_vblock, PAGES_PER_BLK - 1, 0,((6 * sizeof(UINT32) * PAGES_PER_BLK + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR), FTL_BUF(bank), RETURN_WHEN_DONE);
   
            // Invalidation time is previous page's written time
            // invalid_time = written_time;

            // get lpn from oob area
            // mem_copy(&prev.lpn, FTL_BUF(bank) + (sizeof(UINT32) * page_num), sizeof(UINT32));
            // mem_copy(&prev.old_vpn, FTL_BUF(bank) + (sizeof(UINT32) * PAGES_PER_BLK + sizeof(UINT32) * page_num), sizeof(UINT32));
            // mem_copy(&prev.written_time, FTL_BUF(bank) + (sizeof(UINT32) * PAGES_PER_BLK * 2 + sizeof(UINT32) * page_num), sizeof(UINT32));
            // mem_copy(&prev.pid, FTL_BUF(bank) + (sizeof(UINT32) * PAGES_PER_BLK * 3 + sizeof(UINT32) * page_num), sizeof(UINT32));
            // mem_copy(&prev.fid, FTL_BUF(bank) + (sizeof(UINT32) * PAGES_PER_BLK * 4 + sizeof(UINT32) * page_num), sizeof(UINT32));
            // mem_copy(&prev.offset, FTL_BUF(bank) + (sizeof(UINT32) * PAGES_PER_BLK * 5 + sizeof(UINT32) * page_num), sizeof(UINT32));

            prev.lpn = read_dram_32(FTL_BUF(bank) + (sizeof(UINT32) * page_num));
            prev.old_vpn = read_dram_32(FTL_BUF(bank) + (sizeof(UINT32) * PAGES_PER_BLK + sizeof(UINT32) * page_num));
            prev.written_time = read_dram_32(FTL_BUF(bank) + (sizeof(UINT32) * PAGES_PER_BLK * 2 + sizeof(UINT32) * page_num));
            prev.pid = read_dram_32(FTL_BUF(bank) + (sizeof(UINT32) * PAGES_PER_BLK * 3 + sizeof(UINT32) * page_num));
            prev.fid = read_dram_32(FTL_BUF(bank) + (sizeof(UINT32) * PAGES_PER_BLK * 4 + sizeof(UINT32) * page_num));
            prev.offset = read_dram_32(FTL_BUF(bank) + (sizeof(UINT32) * PAGES_PER_BLK * 5 + sizeof(UINT32) * page_num));



            prev.vpn = cur.old_vpn;
            prev.bank = prev.lpn % NUM_BANKS;
            //과거 버전의 만료 시간 : 현재 버전이 쓰인 시점(invalidation) + 보존 기간.
            prev.expiration_time = cur.written_time + pvssd_policy[prev.pid].ret_time;  // 임시        
            // prev.expiration_time = cur.written_time + pvssd_policy[prev.pid].ret_time;          
            //uart_printf("fuck! %d %d %d %d %d", prev.expiration_time, prev.pid, prev.written_time, pvssd_policy[prev.pid].ret_time, cur_time);

        
            // 페이지 할당 (for 과거버전)
            // 현재 버전의 체인이 바뀐다. (과거 버전의 백업 체인이 옮겨질 예정인 곳으로)

            if(prev.written_time > cur.written_time) //적으면 체인이 last임.
            //if (prev.lpn != _lpn || prev.pid == NULL || expired_time < 0)
            { 
                num_fin2++;
                cur.old_vpn = NULL;
                exit_flag =1; 
            }
            else if(prev.expiration_time < cur_time)
            {
                num_expired2++;
               // uart_printf("%dth : num_expired2 %d %d %d %d %d %d %d", num_move_backup, prev.expiration_time, prev.pid, prev.written_time, pvssd_policy[prev.pid].ret_time, cur_time, prev.fid, prev.offset); // 임시
                // uart_printf("%dth : num_expired2 %d %d %d %d %d", num_move_backup, prev.expiration_time, prev.pid, prev.written_time, pvssd_policy[prev.pid].ret_time, cur_time); // 임시

                cur.old_vpn = NULL;
                flag_expired=1;
                goto EXPIRED;
                //exit_flag =1; 
            }
            else
            {
                prev.backup_ppn = assign_new_backup_write_ppn(get_bucket(prev.expiration_time));
                cur.old_vpn = prev.backup_ppn; //네이밍 헷갈리지말기.  // expired_time = invalid_time + pvssd_policy[pid].ret_time;
            }
        }

        // uart_printf("CURRENT!!! -> bank : %d, vpn : %d, lpn : %d, old_vpn : %d, WT : %d, pid : %d, fid : %d, offset : %d", cur.bank, cur.vpn, cur.lpn, cur.old_vpn, cur.written_time, cur.pid, cur.fid, cur.offset);
        // if(cur.old_vpn != NULL) 
        //     uart_printf("PREV!!! -> bank : %d, vpn : %d, lpn : %d, old_vpn : %d, WT : %d, pid : %d, fid : %d, offset : %d", prev.bank, prev.vpn, prev.lpn, prev.old_vpn, prev.written_time, prev.pid, prev.fid, prev.offset); 

        num_move_backup2++;

        // 현재 버전을 백업존으로 복사한다.
        // get an index of the bucket
        UINT32 idx = get_bucket(cur.expiration_time); // Need to have hash function
        // uart_printf("exptime: %d, idx %d", cur.expiration_time, idx);
        /* Give Compression Delay Here! */

        /* Header Update */
        UINT32 num = delta_offset[idx] % MAX_PAGES_IN_DELTA;
        delta_header[idx].lpn[num] = cur.lpn;          //lpn_stack[chain_len];
        delta_header[idx].WT[num] = cur.written_time;  //WT_stack[chain_len];
        delta_header[idx].pid[num] = cur.pid;          //pid_stack[chain_len];
        delta_header[idx].fid[num] = cur.fid;          //fid_stack[chain_len];
        delta_header[idx].offset[num] = cur.offset;    //offset_stack[chain_len];
        delta_header[idx].prev_ppn[num] = cur.old_vpn; //과거버전이 할당될 백업존을 가리킴.

        backuped_page_num[bank]++;
        // uart_printf("HEADER UPDATED!!!: idx : %d, delta_offset : %d, lpn : %d, old_vpn : %d, WT : %d, pid : %d, fid : %d, offset : %d", idx, delta_offset[idx], cur.lpn, cur.old_vpn, cur.written_time, cur.pid, cur.fid, cur.offset); 
        
        // UINT32 z;
        // for (z=0; z<=delta_offset[idx]; z++)
        //     uart_printf("delta_header[%d].lpn[%d] : %d, delta_header[%d].WT[%d] : %d, delta_header[%d].pid[%d] : %d, delta_header[%d].fid[%d] : %d\n", idx, z, delta_header[idx].lpn[z], idx, z, delta_header[idx].WT[z], idx, z, delta_header[idx].pid[z], idx, z, delta_header[idx].fid[z]);

        delta_offset[idx]++;
        //UINT32 vpn_tmp = get_vpn(lpn_stack[chain_len]);
        // set_reclaim_bitmap(bank, vpn_tmp);

        //per-file chain이면 달라짐.

        //delta buffer is full
        if (delta_offset[idx] % MAX_PAGES_IN_DELTA == 0)
        {
            num_backup++;
       //     for (z=0; z<MAX_PAGES_IN_DELTA; z++)
        //        uart_printf("delta_header[%d].lpn[%d] : %d, delta_header[%d].WT[%d] : %d, delta_header[%d].pid[%d] : %d, delta_header[%d].fid[%d] : %d", idx, z, delta_header[idx].lpn[z], idx, z, delta_header[idx].WT[z], idx, z, delta_header[idx].pid[z], idx, z, delta_header[idx].fid[z]);

            //backup_ppn = assign_new_backup_write_ppn(idx); // 수정 요망 //physcial page의 bank는 왜 안뽑나요??? jw pblock = |BANK NO|BLOCK NO|PAGE NO| 구조 맞나요? 그렇게 가정하고 하겟슴.

            
            pblock = cur.backup_ppn / PAGES_PER_BLK;
            vblock = pblock % VBLKS_PER_BANK;
            page_num = cur.backup_ppn % PAGES_PER_BLK;

            //임시방편
            //pblock = read_dram_32(BACKUP_BLK_ADDR + (tmp_num_backup_page_allocated/PAGES_PER_BLK)*sizeof(UINT32));
            pblock = read_dram_32(BACKUP_BLK_ADDR);
            UINT32 bank_ = pblock / VBLKS_PER_BANK;
            tmp_num_backup_page_allocated++; //할당된 페이지갯수
            vblock = pblock % VBLKS_PER_BANK;
            page_num = tmp_num_backup_page_allocated%PAGES_PER_BLK;//cur.backup_ppn % PAGES_PER_BLK;

            // 1. Set Delta Header and Backup Page Table
            for (j = 0; j < MAX_PAGES_IN_DELTA; j++)
            {   
         //       uart_printf("delta_header[%d].lpn[%d] : %d, delta_header[%d].WT[%d] : %d, delta_header[%d].pid[%d] : %d, delta_header[%d].fid[%d] : %d", idx, j, delta_header[idx].lpn[j], idx, j, delta_header[idx].WT[j], idx, j, delta_header[idx].pid[j], idx, j, delta_header[idx].fid[j]);
                    
                delta_header[idx].prev_ppn[j] = get_backup_ppn(delta_header[idx].lpn[j]);
                set_backup_ppn(delta_header[idx].lpn[j], bank, cur.backup_ppn % PAGES_PER_BLK);
            }

            // 2. Bucket에 해당하는 buffer에 delta_header 넣고
            mem_copy(BACKUP_DELTA_BUFFER_ADDR + BYTES_PER_PAGE * idx, &delta_header[idx], sizeof(struct delta_header));

            // 3. Program
            //nand_page_ptprogram(bank, vblock, page_num, 0, SECTORS_PER_PAGE, BACKUP_DELTA_BUFFER_ADDR + (BYTES_PER_PAGE * idx));
            nand_page_ptprogram(bank_, vblock, page_num, 0, SECTORS_PER_PAGE, BACKUP_DELTA_BUFFER_ADDR + (BYTES_PER_PAGE * idx));
            
            // 4. Re-Initialize delta
            mem_set_sram(&delta_header[idx], 0, sizeof(struct delta_header));
           // delta_offset[idx] = 0;
        }
EXPIRED:
        set_reclaim_bitmap(bank, cur.vpn);
        cur = prev;

    } while(!exit_flag); //PRT를 보고 체크해야햠. 내부적으로 체크하니 굳이 여기서 조건문쓸필요 없음.
    
    return;
}

//////////////////////////////////////////////////////////////////////////////////////////
static void set_reclaim_bitmap(UINT32 const bank, UINT32 const vpn)
{
    UINT32 ppn = (bank * PAGES_PER_BANK) + vpn;
    
    // if(bank == 2 && vpn > 52167 && vpn < 52190)
    //     uart_printf("ppn/8 : %d, the byte of (ppn/8) is 0x%x", ppn/8, read_dram_8(RECLAIMABILITY_BMP_ADDR + (ppn / 8)));

    // if(bank == 2 && vpn > 52167 && vpn < 52190)
    //     uart_printf("bank : %d, vpn : %d, page_num : %d is set to be reclaimed", bank, vpn, vpn % 8);

    set_bit_dram(RECLAIMABILITY_BMP_ADDR + (ppn / 8), ppn % 8);
    
    // if(bank == 2 && vpn > 52167 && vpn < 52190)
    //     uart_printf("ppn/8 : %d, the byte of (ppn/8) is 0x%x\n", ppn/8, read_dram_8(RECLAIMABILITY_BMP_ADDR + (ppn / 8)));

    return;
}

static void clr_reclaim_bitmap(UINT32 const bank, UINT32 const vpn)
{
    UINT32 ppn = (bank * PAGES_PER_BANK) + vpn;

    // if(bank == 2 && vpn > 52167 && vpn < 52190)
    //     uart_printf("ppn/8 : %d, the byte of (ppn/8) is 0x%x", ppn/8, read_dram_8(RECLAIMABILITY_BMP_ADDR + (ppn / 8)));

    // if(bank == 2 && vpn > 52167 && vpn < 52190)
    //     uart_printf("bank : %d, vpn : %d, page_num : %d is cleared to be reclaimed-proof", bank, vpn, vpn % 8);

    clr_bit_dram(RECLAIMABILITY_BMP_ADDR + (ppn / 8), ppn % 8);

    // if(bank == 2 && vpn > 52167 && vpn < 52190)
    //     uart_printf("ppn/8 : %d, the byte of (ppn/8) is 0x%x\n", ppn/8, read_dram_8(RECLAIMABILITY_BMP_ADDR + (ppn / 8)));

    return;
}

static UINT32 test_reclaim_bitmap(UINT32 const bank, UINT32 const vpn)
{
    UINT32 ppn = (bank * PAGES_PER_BANK) + vpn;
    return tst_bit_dram(RECLAIMABILITY_BMP_ADDR + (ppn / 8), ppn % 8);
}

// get vpn from BACKUP_PAGE_MAP
static UINT32 get_backup_ppn(UINT32 const lpn)
{
    if((lpn) >= NUM_LPAGES)
        uart_printf("lpn : %d, NUM_LPAGES : %d", lpn, NUM_LPAGES);

    CHECK_LPAGE(lpn);
    return read_dram_32(BACKUP_PAGE_MAP_ADDR + lpn * sizeof(UINT32));
}

// set vpn to BACKUP_PAGE_MAP
static void set_backup_ppn(UINT32 const lpn, UINT32 const bank, UINT32 const vpn)
{
    UINT8 new_flush;
    UINT32 ppn = (bank * PAGES_PER_BANK) + vpn;

    CHECK_LPAGE(lpn);
    // ASSERT(vpn >= (META_BLKS_PER_BANK * PAGES_PER_BLK) && vpn < (VBLKS_PER_BANK * PAGES_PER_BLK));

    write_dram_32(BACKUP_PAGE_MAP_ADDR + lpn * sizeof(UINT32), ppn);
}

// get vpn from PAGE_MAP
static UINT32 get_vpn(UINT32 const lpn)
{
    CHECK_LPAGE(lpn);
    return read_dram_32(PAGE_MAP_ADDR + lpn * sizeof(UINT32));
}


// set vpn to PAGE_MAP
static void set_vpn(UINT32 const lpn, UINT32 const vpn)
{
    UINT8 new_flush;
    UINT32 map_entry_per_page = sizeof(UINT32);

    CHECK_LPAGE(lpn);
    ASSERT(vpn >= (META_BLKS_PER_BANK * PAGES_PER_BLK) && vpn < (VBLKS_PER_BANK * PAGES_PER_BLK));

    new_flush = read_dram_8(FLUSH_MAP_ADDR + (lpn / map_entry_per_page) / 8);
    new_flush = new_flush | (0x80 >> ((lpn / map_entry_per_page) % 8));
    write_dram_8(FLUSH_MAP_ADDR + (lpn / map_entry_per_page) / 8, new_flush);

    write_dram_32(PAGE_MAP_ADDR + lpn * sizeof(UINT32), vpn);
}

// get valid page count of vblock
static UINT32 get_vcount(UINT32 const bank, UINT32 const vblock)
{
    UINT32 vcount;

    ASSERT(bank < NUM_BANKS);
    ASSERT((vblock >= META_BLKS_PER_BANK) && (vblock < VBLKS_PER_BANK));

    vcount = read_dram_16(VCOUNT_ADDR + (((bank * VBLKS_PER_BANK) + vblock) * sizeof(UINT16)));
    ASSERT((vcount < PAGES_PER_BLK) || (vcount == VC_MAX));

    return vcount;
}

// set valid page count of vblock
static void set_vcount(UINT32 const bank, UINT32 const vblock, UINT32 const vcount)
{
    if (!(bank < NUM_BANKS) || !((vblock >= META_BLKS_PER_BANK) && (vblock < VBLKS_PER_BANK)))
        uart_printf("bank : %d, vblock : %d, vcount : %d", bank, vblock, vcount);
    ASSERT(bank < NUM_BANKS);
    ASSERT((vblock >= META_BLKS_PER_BANK) && (vblock < VBLKS_PER_BANK));

    if (!((vcount < PAGES_PER_BLK) || (vcount == VC_MAX)))
    {
        uart_printf("gc_count %d - bank : %d, vblock : %d, vcount : %d", gc_count, bank, vblock, vcount);
    }
    ASSERT((vcount < PAGES_PER_BLK) || (vcount == VC_MAX));

    write_dram_16(VCOUNT_ADDR + (((bank * VBLKS_PER_BANK) + vblock) * sizeof(UINT16)), vcount);
}

static UINT32 assign_new_write_vpn(UINT32 const bank)
{
    ASSERT(bank < NUM_BANKS);

    UINT32 write_vpn, write_vpn_org;
    UINT32 original;
    UINT32 vblock;

    if(!init_write[bank]) {
        uart_printf("first assignment for bank %d", bank);
        init_write[bank] = 1;
        return get_cur_write_vpn(bank);
    }

    // get write vpn pointer from metadata of each bank
    write_vpn = get_cur_write_vpn(bank);
    vblock = write_vpn / PAGES_PER_BLK;

    // NOTE: if next new write page's offset is
    // the last page offset of vblock (i.e. PAGES_PER_BLK - 1),
    if ((write_vpn % PAGES_PER_BLK) == (PAGES_PER_BLK - 2))
    {
        mem_copy(FTL_BUF(bank), g_misc_meta[bank].lpn, sizeof(UINT32) * PAGES_PER_BLK);

        // old_vpn
        mem_copy(FTL_BUF(bank) + sizeof(UINT32) * PAGES_PER_BLK * 1,
                 OOB_OLD_VPN_ADDR + (bank * PAGES_PER_BLK) * sizeof(UINT32),
                 sizeof(UINT32) * PAGES_PER_BLK);
        // written_time
        mem_copy(FTL_BUF(bank) + sizeof(UINT32) * PAGES_PER_BLK * 2,
                 OOB_WRITTEN_TIME_ADDR + (bank * PAGES_PER_BLK) * sizeof(UINT32),
                 sizeof(UINT32) * PAGES_PER_BLK);
        // pid

        // 여기
        UINT32 ii, jj, ppiidd;
        UINT32 ext_page;
        if(write_vpn / PAGES_PER_BLK == 93312 / PAGES_PER_BLK)
        {
            ext_page = 93312 % PAGES_PER_BLK;
            UINT32 pr_old_vpn = read_dram_32(OOB_OLD_VPN_ADDR + (bank * PAGES_PER_BLK + ext_page) * sizeof(UINT32));
            UINT32 pr_old_pid = read_dram_32(OOB_PID_ADDR + (bank * PAGES_PER_BLK + ext_page) * sizeof(UINT32));
            uart_printf("problem2 : bank %d vblock %d vpn %d old_vpn %d pid %d", bank, vblock, write_vpn, pr_old_vpn, pr_old_pid);  
        }

        mem_copy(FTL_BUF(bank) + sizeof(UINT32) * PAGES_PER_BLK * 3,
                 OOB_PID_ADDR + (bank * PAGES_PER_BLK) * sizeof(UINT32),
                 sizeof(UINT32) * PAGES_PER_BLK);
        // fid
        mem_copy(FTL_BUF(bank) + sizeof(UINT32) * PAGES_PER_BLK * 4,
                 OOB_FID_ADDR + (bank * PAGES_PER_BLK) * sizeof(UINT32),
                 sizeof(UINT32) * PAGES_PER_BLK);
        // offset
        mem_copy(FTL_BUF(bank) + sizeof(UINT32) * PAGES_PER_BLK * 5,
                 OOB_FILE_OFFSET_ADDR + (bank * PAGES_PER_BLK) * sizeof(UINT32),
                 sizeof(UINT32) * PAGES_PER_BLK);

  
        // last page for OOB metadata
        // PAGES_PER_BLK - 1 = OOB metadata
        // 6 * 4B * 127 = amount of OOB metadata
        nand_page_ptprogram(bank, vblock, PAGES_PER_BLK - 1, 0,
                            ((6 * sizeof(UINT32) * PAGES_PER_BLK + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR), FTL_BUF(bank));

        // reinitialize OOB metadata
        mem_set_sram(g_misc_meta[bank].lpn, 0x00000000, sizeof(UINT32) * PAGES_PER_BLK);
        mem_set_dram(OOB_OLD_VPN_ADDR + (bank * PAGES_PER_BLK * sizeof(UINT32)) , 0x00000000, PAGES_PER_BLK * sizeof(UINT32));
        mem_set_dram(OOB_WRITTEN_TIME_ADDR + (bank * PAGES_PER_BLK * sizeof(UINT32)), 0x00000000, PAGES_PER_BLK * sizeof(UINT32));
        mem_set_dram(OOB_PID_ADDR + (bank * PAGES_PER_BLK * sizeof(UINT32)), 0x00000000, PAGES_PER_BLK * sizeof(UINT32));
        mem_set_dram(OOB_FID_ADDR + (bank * PAGES_PER_BLK * sizeof(UINT32)), 0x00000000, PAGES_PER_BLK * sizeof(UINT32));
        mem_set_dram(OOB_FILE_OFFSET_ADDR + (bank * PAGES_PER_BLK * sizeof(UINT32)), 0x00000000, PAGES_PER_BLK * sizeof(UINT32));

        inc_full_blk_cnt(bank);

        // uart_printf("Flushing OOB metadta is done!\n");
        // uart_printf("(freeblk, copy, write, backup)= %d %d %d %d\n", g_misc_meta[bank].free_blk_cnt, page_copy_num, page_write_num, erase_backup_num);

        // assigning new block if necessary,
        // in the mean time, if all blocks are full
        // do garbage collection
        if (is_full_all_blks(bank)) //free block의 수가 10GB이하일때 GC유발해야함.
        {
            modified_garbage_collection(bank); // after garbage collection,
                                               // cur_write_vpn is set to new block's next page
                                               // or page right after copyied valid pages
            return get_cur_write_vpn(bank);    // return an erased block
        }

        // next free block
        // vblock = META_BLKS_PER_BANK;
        do
        {
            vblock++;

            if(vblock == VBLKS_PER_BANK)
            {
                uart_printf("!!!!****** bank : %d *******!!!!", bank);
                UINT32 cnt;
                for (cnt = META_BLKS_PER_BANK; cnt < VBLKS_PER_BANK -2; cnt += 3)
                    uart_printf("vblock : %d, vcount : %d | vblock : %d, vcount : %d | vblock : %d, vcount : %d", vblock, get_vcount(bank, vblock), vblock+1, get_vcount(bank, vblock+1), vblock+2, get_vcount(bank,vblock+2));
            }
            ASSERT(vblock != VBLKS_PER_BANK);
        } while (get_vcount(bank, vblock) == VC_MAX);
        // } while (get_vcount(bank, vblock) != 0);
    } // done flushing the full vblock

    // if the vblock is newly allocated,
    if (vblock != (write_vpn / PAGES_PER_BLK))
    {
        write_vpn = vblock * PAGES_PER_BLK; // first page of new block
    }
    else
    { // if not, return next write_vpn
        write_vpn++;
    }
    // update misc_metadata[bank]
    set_new_write_vpn(bank, write_vpn);

    return write_vpn;
}

// ******* newly defined function for SGX-SSD ******** //
// assign new backup zone write vpn for backed up data //

//jw 수정
static UINT32 assign_new_backup_write_ppn(UINT32 const hash_index)
{
    //정적 변수. 함수가 call할때마다 1씩 증가함.
    //현재까지 할당된 백업블록의 갯수를 의미한다.
    //static UINT32 back_blk_cnt = 0;
    //cur_backup_write_cnt[hash_index] = delta_offset[hash_index]/5;
    //if(hash_index==1)  
    //  uart_printf("delta_offset:%d  %d", hash_index, delta_offset[hash_index]);
    UINT32 blk_idx = cur_backup_write_cnt[hash_index] / PAGES_PER_BLK;
    UINT32 page_no = cur_backup_write_cnt[hash_index] % PAGES_PER_BLK;
    UINT32 pblock = read_dram_32(BACKUP_BLK_ADDR + (hash_index * sizeof(UINT32) * BLOCK_PER_BUCKET) + blk_idx * sizeof(UINT32));
    UINT32 ppn = pblock * PAGES_PER_BLK + page_no;
    
    
   
    if (delta_offset[hash_index] != 0 && delta_offset[hash_index] % 5 == 0) //2.
    {    
        cur_backup_write_cnt[hash_index]++;
        num_backup2++;
    }
    
    return ppn;
}

// Bad Block Management
static BOOL32 is_bad_block(UINT32 const bank, UINT32 const vblk_offset)
{
    if (tst_bit_dram(BAD_BLK_BMP_ADDR + bank * (VBLKS_PER_BANK / 8 + 1), vblk_offset) == FALSE)
    {
        return FALSE;
    }
    return TRUE;
}

//-------------------------------------------------------------
// Victim selection policy: Greedy
//
// Select the block which contain minumum valid pages
//-------------------------------------------------------------
static UINT32 get_vt_vblock(UINT32 const bank)
{
    ASSERT(bank < NUM_BANKS);

    UINT32 vblock;
    UINT32 i, j;
    // search the block which has mininum valid pages
    // uart_print("get victim block!\n");
    vblock = mem_search_min_max(VCOUNT_ADDR + (bank * VBLKS_PER_BANK * sizeof(UINT16)),
                                sizeof(UINT16),
                                VBLKS_PER_BANK,
                                MU_CMD_SEARCH_MIN_DRAM);

    if (get_vcount(bank, vblock) == 127)
        uart_printf("No Free Block Left!!\n");

    ASSERT(is_bad_block(bank, vblock) == FALSE);
    ASSERT(vblock >= META_BLKS_PER_BANK && vblock < VBLKS_PER_BANK);
    ASSERT(get_vcount(bank, vblock) < (PAGES_PER_BLK - 1));

    return vblock;
}
static void format(void)
{
    UINT32 bank, vblock, vcount_val;
    int i;
    ASSERT(NUM_MISC_META_SECT > 0);
    ASSERT(NUM_VCOUNT_SECT > 0);

    uart_printf("Total FTL DRAM metadata size: %d KB", DRAM_BYTES_OTHER / 1024);
    uart_printf("VBLKS_PER_BANK: %d", VBLKS_PER_BANK);
    uart_printf("LBLKS_PER_BANK: %d", NUM_LPAGES / PAGES_PER_BLK / NUM_BANKS);
    uart_printf("META_BLKS_PER_BANK: %d", META_BLKS_PER_BANK);
    uart_printf("PAGES_PER_BLK : %d", PAGES_PER_BLK);
    uart_printf("SSD SIZE_BLKS : %d", (VBLKS_PER_BANK - MAX_BAD_BLKS_PER_BANKS - 32 - FILESYS_METADATA_BLKS_PER_BANK - META_BLKS_PER_BANK));

    //----------------------------------------
    // initialize DRAM metadata
    //----------------------------------------
    mem_set_dram(PAGE_MAP_ADDR, NULL, PAGE_MAP_BYTES);
    mem_set_dram(VCOUNT_ADDR, 0xCECECECE, VCOUNT_BYTES);

    /*
    for(i=0; i<VCOUNT_BYTES; i+=1)
        uart_printf("!!!!!!!=%x\n",read_dram_8(VCOUNT_ADDR+i));
    */
    mem_set_dram(FLUSH_MAP_ADDR, NULL, FLUSH_MAP_BYTES);
    // mem_set_dram(RECLAIMABILITY_BMP_ADDR, NULL, RECLAIMABILITY_BMP_BYTES);
    mem_set_dram(RECLAIMABILITY_BMP_ADDR, 0xFFFFFFFF, RECLAIMABILITY_BMP_BYTES);
    mem_set_dram(BACKUP_ZONE_BMP_ADDR, NULL, BACKUP_ZONE_BMP_BYTES);
    mem_set_dram(BACKUP_PAGE_MAP_ADDR, NULL, BACKUP_PAGE_MAP_BYTES);
    mem_set_dram(BACKUP_POLICY_BMP_ADDR, NULL, BACKUP_POLICY_BMP_BYTES);
    // mem_set_dram(OOB_FILE_METADATA_ADDR, NULL, OOB_FILE_METADATA_BYTES);
    mem_set_dram(OOB_OLD_VPN_ADDR, NULL, OOB_OLD_VPN_BYTES);
    mem_set_dram(OOB_WRITTEN_TIME_ADDR, NULL, OOB_WRITTEN_TIME_BYTES);
    mem_set_dram(OOB_PID_ADDR, NULL, OOB_PID_BYTES);
    mem_set_dram(OOB_FID_ADDR, NULL, OOB_FID_BYTES);
    mem_set_dram(OOB_FILE_OFFSET_ADDR, NULL, OOB_FILE_OFFSET_BYTES);
    
    //jw 승진 만든 자료구조 여기서 전부 초기화했는지 확인해보세요.
    mem_set_dram(BACKUP_BLK_ADDR, NULL, BACKUP_BLK_BYTES);

    //----------------------------------------
    // erase all blocks except vblock #0
    //----------------------------------------
    for (vblock = MISCBLK_VBN; vblock < VBLKS_PER_BANK; vblock++)
    {
        for (bank = 0; bank < NUM_BANKS; bank++)
        {
            vcount_val = VC_MAX;
            if (is_bad_block(bank, vblock) == FALSE)
            {
                nand_block_erase(bank, vblock);
                // vcount_val = VC_FREE;
                vcount_val = 0;
            }
            write_dram_16(VCOUNT_ADDR + ((bank * VBLKS_PER_BANK) + vblock) * sizeof(UINT16),
                          vcount_val);
        }
    }
    //----------------------------------------
    // initialize SRAM metadata
    //----------------------------------------
    init_metadata_sram();

    // flush metadata to NAND
    logging_pmap_table();
    logging_misc_metadata();

    write_format_mark();
    led(1);
    uart_print("format complete");
}

static void init_metadata_sram(void)
{
    UINT32 bank;
    UINT32 vblock;
    UINT32 mapblk_lbn;
    UINT32 i;

    UINT32 backupblk_lbn;
    UINT32 blkcnt_per_bank = 0;
    UINT32 pblock;
    UINT32 index;
    //----------------------------------------
    // initialize misc. metadata
    //----------------------------------------

    //초기화 잘됐나 확인 모두 0여야함.
   // uart_printf("cur_backup_write_cnt %d %d %d %d", cur_backup_write_cnt[5], cur_backup_write_cnt[10], cur_backup_write_cnt[20], cur_backup_write_cnt[29]);

    for (bank = 0; bank < NUM_BANKS; bank++)
    {
        g_misc_meta[bank].free_blk_cnt = VBLKS_PER_BANK - META_BLKS_PER_BANK;
        g_misc_meta[bank].free_blk_cnt -= get_bad_blk_cnt(bank); //1660-85
        //g_misc_meta[bank].free_blk_cnt -= 85;   //free_blk_cnt일치시킴. 최악이 85임..
        //g_misc_meta[bank].free_blk_cnt = 64;//2GB는 512블록, 각 64개의 free_blk_cnt
        //g_misc_meta[bank].free_blk_cnt = 64*30/2;//30G //GB는 512블록, 각 64개의 free_blk_cnt
        uart_printf("%d bank free_blk_cnt: %d bad_blk_cnt %d", bank, g_misc_meta[bank].free_blk_cnt, get_bad_blk_cnt(bank) );
        // NOTE: vblock #0, 1 we don't use 'em for user space
        write_dram_16(VCOUNT_ADDR + ((bank * VBLKS_PER_BANK) + 0) * sizeof(UINT16), VC_MAX);
        write_dram_16(VCOUNT_ADDR + ((bank * VBLKS_PER_BANK) + 1) * sizeof(UINT16), VC_MAX);

        //----------------------------------------
        // assign misc. block
        //----------------------------------------
        // assumption: vblock #1 = fixed location.
        // Thus if vblock #1 is a bad block, it should be allocate another block.
        set_miscblk_vpn(bank, MISCBLK_VBN * PAGES_PER_BLK - 1);
        ASSERT(is_bad_block(bank, MISCBLK_VBN) == FALSE);

        vblock = MISCBLK_VBN;

        //----------------------------------------
        // assign map block
        //----------------------------------------
        mapblk_lbn = 0;
        while (mapblk_lbn < MAPBLKS_PER_BANK)
        {
            vblock++;
            ASSERT(vblock < VBLKS_PER_BANK);
            if (is_bad_block(bank, vblock) == FALSE)
            {
                set_mapblk_vpn(bank, mapblk_lbn, vblock * PAGES_PER_BLK);
                write_dram_16(VCOUNT_ADDR + ((bank * VBLKS_PER_BANK) + vblock) * sizeof(UINT16), VC_MAX);
                mapblk_lbn++;
            }
        }
    
        //----------------------------------------
        // assign backup block
        //----------------------------------------
        while(g_misc_meta[bank].free_blk_cnt == 64*30/2)
        {
            g_misc_meta[bank].free_blk_cnt--;
        }
        blkcnt_per_bank = bank; //bank을 적절하게 분배하기 위해.
        backupblk_lbn = 0;
        index = 0; //bucket index
        UINT32 tmp_idx=0;
         
         UINT32 meta_size = g_misc_meta[bank].free_blk_cnt - TOTAL_BLK_PER_BANK;
        META_BLKS_PER_BANK += meta_size;
        while(g_misc_meta[bank].free_blk_cnt > TOTAL_BLK_PER_BANK)
        //while (backupblk_lbn < SGXSSD_BACKUP_BLKS_PER_BANK) //320개
        {
            vblock++;
            ASSERT(vblock < VBLKS_PER_BANK);
            if (is_bad_block(bank, vblock) == FALSE)
            {

                //backup block address 등록.
                pblock = vblock + bank * VBLKS_PER_BANK;
                
                //write_dram_32(BACKUP_BLK_ADDR + (index * sizeof(UINT32) * BLOCK_PER_BUCKET + blkcnt_per_bank * sizeof(UINT32)), pblock);
                //임시방편
                if(tmp_idx==0) 
                {
                    write_dram_32(BACKUP_BLK_ADDR + sizeof(UINT32), pblock);
                    tmp_idx=1;
                }
                //tmp_idx++;
                //tmp_idx = 1;
                write_dram_16(VCOUNT_ADDR + ((bank * VBLKS_PER_BANK) + vblock) * sizeof(UINT16), VC_MAX);

                //각 bucket index상에서 매번 할당할떄마다 다른 bank의 pblock을 가져옴.
                // index++;
                // if (index == NUM_BUCKETS)
                // {
                //     index = 0;
                //     blkcnt_per_bank += NUM_BANKS;
                // }
                // backupblk_lbn++;
                 g_misc_meta[bank].free_blk_cnt--;
            }
        }
        //debugging
        uart_printf("[init_metadata_sram] backupblk_lbn: %d, blk_cnt_per_bank: %d", backupblk_lbn, blkcnt_per_bank);
    
        //----------------------------------------
        // assign free block for gc
        //----------------------------------------
        do
        {
            vblock++;
            write_dram_16(VCOUNT_ADDR + ((bank * VBLKS_PER_BANK) + vblock) * sizeof(UINT16), VC_MAX); //VC_MAX는 회수되어서는 안된다는 표기임.
            set_gc_vblock(bank, vblock);
            ASSERT(vblock < VBLKS_PER_BANK);

        } while (is_bad_block(bank, vblock) == TRUE);
    
        //----------------------------------------
        // assign free vpn for first new write
        //----------------------------------------
        do
        {
            vblock++;
            // 현재 next vblock부터 새로운 데이터를 저장을 시작
            set_new_write_vpn(bank, vblock * PAGES_PER_BLK); 
            //0이면 free 1이면 안에 뭐 있음
            ASSERT(vblock < VBLKS_PER_BANK);
        } while (is_bad_block(bank, vblock) == TRUE);

        uart_printf("bank %d, first vblock : %d write_vpn : %d", bank, vblock, g_misc_meta[bank].cur_write_vpn); 
    }
}


// logging misc + vcount metadata
static void logging_misc_metadata(void)
{
    UINT32 misc_meta_bytes = NUM_MISC_META_SECT * BYTES_PER_SECTOR; // per bank
    UINT32 vcount_addr = VCOUNT_ADDR;
    UINT32 vcount_bytes = NUM_VCOUNT_SECT * BYTES_PER_SECTOR; // per bank
    UINT32 vcount_boundary = VCOUNT_ADDR + VCOUNT_BYTES;      // entire vcount data
    UINT32 bank;

    flash_finish();

    for (bank = 0; bank < NUM_BANKS; bank++)
    {
        inc_miscblk_vpn(bank);

        // note: if misc. meta block is full, just erase old block & write offset #0
        if ((get_miscblk_vpn(bank) / PAGES_PER_BLK) != MISCBLK_VBN)
        {
            nand_block_erase(bank, MISCBLK_VBN);
            set_miscblk_vpn(bank, MISCBLK_VBN * PAGES_PER_BLK); // vpn = 128
        }
        // copy misc. metadata to FTL buffer
        mem_copy(FTL_BUF(bank), &g_misc_meta[bank], misc_meta_bytes);

        // copy vcount metadata to FTL buffer
        if (vcount_addr <= vcount_boundary)
        {
            mem_copy(FTL_BUF(bank) + misc_meta_bytes, vcount_addr, vcount_bytes);
            vcount_addr += vcount_bytes;
        }
    }
    // logging the misc. metadata to nand flash
    for (bank = 0; bank < NUM_BANKS; bank++)
    {
        nand_page_ptprogram(bank,
                            get_miscblk_vpn(bank) / PAGES_PER_BLK,
                            get_miscblk_vpn(bank) % PAGES_PER_BLK,
                            0,
                            NUM_MISC_META_SECT + NUM_VCOUNT_SECT,
                            FTL_BUF(bank));
    }
    flash_finish();
}

static void logging_pmap_table(void)
{
    UINT32 pmap_addr = PAGE_MAP_ADDR;
    UINT32 pmap_bytes = BYTES_PER_PAGE; // per bank
    UINT32 mapblk_vpn;
    UINT32 bank;
    UINT32 pmap_boundary = PAGE_MAP_ADDR + PAGE_MAP_BYTES;
    BOOL32 finished = FALSE;

    // Selective Flush
    // If the table has changed, it should be marked in flush map
    // so that it doesn't have to flush "UNCHANGED" parts of the table
    UINT32 fmap_next = 0;
    UINT32 fmap_addr = FLUSH_MAP_ADDR; // page mapping table flush map
    UINT8 cur_fmap;

    // flush
    // num_flush++;
    for (UINT32 mapblk_lbn = 0; mapblk_lbn < MAPBLKS_PER_BANK; mapblk_lbn++)
    {
        flash_finish();

        // get page map from each bank
        for (bank = 0; bank < NUM_BANKS; bank++)
        {
            if (finished)
            {
                break;
            }
            else if (pmap_addr >= pmap_boundary)
            {
                finished = TRUE;
                break;
            }
            else if (pmap_addr + BYTES_PER_PAGE >= pmap_boundary)
            {
                finished = TRUE;
                pmap_bytes = (pmap_boundary - pmap_addr + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR * BYTES_PER_SECTOR;
            }

            //flush
            if (fmap_next % 8 == 0)
            {
                if (fmap_next != 0) //처음에는 update하면 안된다.
                    fmap_addr += 1;

                cur_fmap = read_dram_8(fmap_addr); // Get a byte of flush BMP from DRAM
                // if(uart_tp == fmap_addr)
                // uart_printf("1. uart_tp : %x, fmap_addr : %x , cur_fmap : %x", uart_tp, fmap_addr, cur_fmap);
            }
            fmap_next += 1;

            // if(uart_tp == fmap_addr)
            // uart_printf("2. uart_tp : %x, fmap_addr : %x , cur_fmap : %x, fmap_next: %d, comp : %d", uart_tp, fmap_addr, cur_fmap, fmap_next%8,cur_fmap >> (8 - fmap_next%8 ));
            if ((cur_fmap >> (8 - fmap_next % 8)) & 0x01 != 0) // if the page has changed
            {

                //uart_printf("3. uart_tp : %x, fmap_addr : %x, fmap_num: %d , cur_fmap : %x, fmap_next : %d, set_vpn: page num : %d", uart_tp, fmap_addr, fmap_addr - FLUSH_MAP_ADDR, cur_fmap, fmap_next%8,  uart_tp_pn);
                //uart_printf("bingo!");

                // Need to flush the page of page map
                inc_mapblk_vpn(bank, mapblk_lbn);

                mapblk_vpn = get_mapblk_vpn(bank, mapblk_lbn);

                // note: if there is no free page, then erase old map block first.
                if ((mapblk_vpn % PAGES_PER_BLK) == 0)
                {
                    // erase full map block
                    nand_block_erase(bank, (mapblk_vpn - 1) / PAGES_PER_BLK);

                    // next vpn of mapblk is offset #0
                    set_mapblk_vpn(bank, mapblk_lbn, ((mapblk_vpn - 1) / PAGES_PER_BLK) * PAGES_PER_BLK);
                    mapblk_vpn = get_mapblk_vpn(bank, mapblk_lbn);
                }

                // copy the page mapping table to FTL buffer
                mem_copy(FTL_BUF(bank), pmap_addr, pmap_bytes);

                // logging update page mapping table into map_block
                nand_page_ptprogram(bank,
                                    mapblk_vpn / PAGES_PER_BLK,
                                    mapblk_vpn % PAGES_PER_BLK,
                                    0,
                                    pmap_bytes / BYTES_PER_SECTOR,
                                    FTL_BUF(bank));
                // num_logging++;
            }
            pmap_addr += pmap_bytes;
        }
        if (finished)
        {
            break;
        }
    }
    mem_set_dram(FLUSH_MAP_ADDR, 0, FLUSH_MAP_BYTES); //key flush
    flash_finish();

    //  uart_printf("num_flush : %d, num_logging : %d",num_flush, num_logging);
}

// load flushed FTL metadta
static void load_metadata(void)
{
    load_misc_metadata();
    load_pmap_table();
}

// misc + VCOUNT
static void load_misc_metadata(void)
{
    UINT32 misc_meta_bytes = NUM_MISC_META_SECT * BYTES_PER_SECTOR;
    UINT32 vcount_bytes = NUM_VCOUNT_SECT * BYTES_PER_SECTOR;
    UINT32 vcount_addr = VCOUNT_ADDR;
    UINT32 vcount_boundary = VCOUNT_ADDR + VCOUNT_BYTES;

    UINT32 load_flag = 0;
    UINT32 bank, page_num;
    UINT32 load_cnt = 0;

    flash_finish();

    disable_irq();
    flash_clear_irq(); // clear any flash interrupt flags that might have been set

    // scan valid metadata in descending order from last page offset
    for (page_num = PAGES_PER_BLK - 1; page_num != ((UINT32)-1); page_num--)
    {
        for (bank = 0; bank < NUM_BANKS; bank++)
        {
            if (load_flag & (0x1 << bank))
            {
                continue;
            }
            // read valid metadata from misc. metadata area
            nand_page_ptread(bank,
                             MISCBLK_VBN,
                             page_num,
                             0,
                             NUM_MISC_META_SECT + NUM_VCOUNT_SECT,
                             FTL_BUF(bank),
                             RETURN_ON_ISSUE);
        }
        flash_finish();

        for (bank = 0; bank < NUM_BANKS; bank++)
        {
            if (!(load_flag & (0x1 << bank)) && !(BSP_INTR(bank) & FIRQ_ALL_FF))
            {
                load_flag = load_flag | (0x1 << bank);
                load_cnt++;
            }
            CLR_BSP_INTR(bank, 0xFF);
        }
    }
    ASSERT(load_cnt == NUM_BANKS);

    for (bank = 0; bank < NUM_BANKS; bank++)
    {
        // misc. metadata
        mem_copy(&g_misc_meta[bank], FTL_BUF(bank), sizeof(misc_metadata));

        // vcount metadata
        if (vcount_addr <= vcount_boundary)
        {
            mem_copy(vcount_addr, FTL_BUF(bank) + misc_meta_bytes, vcount_bytes);
            vcount_addr += vcount_bytes;
        }
    }
    enable_irq();
}

static void load_pmap_table(void)
{
    UINT32 pmap_addr = PAGE_MAP_ADDR;
    UINT32 temp_page_addr;
    UINT32 pmap_bytes = BYTES_PER_PAGE; // per bank
    UINT32 pmap_boundary = PAGE_MAP_ADDR + (NUM_LPAGES * sizeof(UINT32));
    UINT32 mapblk_lbn, bank;
    BOOL32 finished = FALSE;

    flash_finish();

    for (mapblk_lbn = 0; mapblk_lbn < MAPBLKS_PER_BANK; mapblk_lbn++)
    {
        temp_page_addr = pmap_addr; // backup page mapping addr

        for (bank = 0; bank < NUM_BANKS; bank++)
        {
            if (finished)
            {
                break;
            }
            else if (pmap_addr >= pmap_boundary)
            {
                finished = TRUE;
                break;
            }
            else if (pmap_addr + BYTES_PER_PAGE >= pmap_boundary)
            {
                finished = TRUE;
                pmap_bytes = (pmap_boundary - pmap_addr + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR * BYTES_PER_SECTOR;
            }
            // read page mapping table from map_block
            nand_page_ptread(bank,
                             get_mapblk_vpn(bank, mapblk_lbn) / PAGES_PER_BLK,
                             get_mapblk_vpn(bank, mapblk_lbn) % PAGES_PER_BLK,
                             0,
                             pmap_bytes / BYTES_PER_SECTOR,
                             FTL_BUF(bank),
                             RETURN_ON_ISSUE);
            pmap_addr += pmap_bytes;
        }
        flash_finish();

        pmap_bytes = BYTES_PER_PAGE;
        for (bank = 0; bank < NUM_BANKS; bank++)
        {
            if (temp_page_addr >= pmap_boundary)
            {
                break;
            }
            else if (temp_page_addr + BYTES_PER_PAGE >= pmap_boundary)
            {
                pmap_bytes = (pmap_boundary - temp_page_addr + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR * BYTES_PER_SECTOR;
            }
            // copy page mapping table to PMAP_ADDR from FTL buffer
            mem_copy(temp_page_addr, FTL_BUF(bank), pmap_bytes);

            temp_page_addr += pmap_bytes;
        }
        if (finished)
        {
            break;
        }
    }
}
static void write_format_mark(void)
{
    // This function writes a format mark to a page at (bank #0, block #0).

#ifdef __GNUC__
    extern UINT32 size_of_firmware_image;
    UINT32 firmware_image_pages = (((UINT32)(&size_of_firmware_image)) + BYTES_PER_FW_PAGE - 1) / BYTES_PER_FW_PAGE;
#else
    extern UINT32 Image$$ER_CODE$$RO$$Length;
    extern UINT32 Image$$ER_RW$$RW$$Length;
    UINT32 firmware_image_bytes = ((UINT32)&Image$$ER_CODE$$RO$$Length) + ((UINT32)&Image$$ER_RW$$RW$$Length);
    UINT32 firmware_image_pages = (firmware_image_bytes + BYTES_PER_FW_PAGE - 1) / BYTES_PER_FW_PAGE;
#endif

    UINT32 format_mark_page_offset = FW_PAGE_OFFSET + firmware_image_pages;

    mem_set_dram(FTL_BUF_ADDR, 0, BYTES_PER_SECTOR);

    SETREG(FCP_CMD, FC_COL_ROW_IN_PROG);
    SETREG(FCP_BANK, REAL_BANK(0));
    SETREG(FCP_OPTION, FO_E | FO_B_W_DRDY);
    SETREG(FCP_DMA_ADDR, FTL_BUF_ADDR); // DRAM -> flash
    SETREG(FCP_DMA_CNT, BYTES_PER_SECTOR);
    SETREG(FCP_COL, 0);
    SETREG(FCP_ROW_L(0), format_mark_page_offset);
    SETREG(FCP_ROW_H(0), format_mark_page_offset);

    // At this point, we do not have to check Waiting Room status before issuing a command,
    // because we have waited for all the banks to become idle before returning from format().
    SETREG(FCP_ISSUE, NULL);

    // wait for the FC_COL_ROW_IN_PROG command to be accepted by bank #0
    while ((GETREG(WR_STAT) & 0x00000001) != 0)
        ;

    // wait until bank #0 finishes the write operation
    while (BSP_FSM(0) != BANK_IDLE)
        ;
}

static BOOL32 check_format_mark(void)
{
    // This function reads a flash page from (bank #0, block #0) in order to check whether the SSD is formatted or not.

#ifdef __GNUC__
    extern UINT32 size_of_firmware_image;
    UINT32 firmware_image_pages = (((UINT32)(&size_of_firmware_image)) + BYTES_PER_FW_PAGE - 1) / BYTES_PER_FW_PAGE;
#else
    extern UINT32 Image$$ER_CODE$$RO$$Length;
    extern UINT32 Image$$ER_RW$$RW$$Length;
    UINT32 firmware_image_bytes = ((UINT32)&Image$$ER_CODE$$RO$$Length) + ((UINT32)&Image$$ER_RW$$RW$$Length);
    UINT32 firmware_image_pages = (firmware_image_bytes + BYTES_PER_FW_PAGE - 1) / BYTES_PER_FW_PAGE;
#endif

    UINT32 format_mark_page_offset = FW_PAGE_OFFSET + firmware_image_pages;
    UINT32 temp;

    flash_clear_irq(); // clear any flash interrupt flags that might have been set

    SETREG(FCP_CMD, FC_COL_ROW_READ_OUT);
    SETREG(FCP_BANK, REAL_BANK(0));
    SETREG(FCP_OPTION, FO_E);
    SETREG(FCP_DMA_ADDR, FTL_BUF_ADDR); // flash -> DRAM
    SETREG(FCP_DMA_CNT, BYTES_PER_SECTOR);
    SETREG(FCP_COL, 0);
    SETREG(FCP_ROW_L(0), format_mark_page_offset);
    SETREG(FCP_ROW_H(0), format_mark_page_offset);

    // At this point, we do not have to check Waiting Room status before issuing a command,
    // because scan list loading has been completed just before this function is called.
    SETREG(FCP_ISSUE, NULL);

    // wait for the FC_COL_ROW_READ_OUT command to be accepted by bank #0
    while ((GETREG(WR_STAT) & 0x00000001) != 0)
        ;

    // wait until bank #0 finishes the read operation
    while (BSP_FSM(0) != BANK_IDLE)
        ;

    // Now that the read operation is complete, we can check interrupt flags.
    temp = BSP_INTR(0) & FIRQ_ALL_FF;

    // clear interrupt flags
    CLR_BSP_INTR(0, 0xFF);

    if (temp != 0)
    {
        return FALSE; // the page contains all-0xFF (the format mark does not exist.)
    }
    else
    {
        return TRUE; // the page contains something other than 0xFF (it must be the format mark)
    }
}

// BSP interrupt service routine
void ftl_isr(void)
{
    UINT32 bank;
    UINT32 bsp_intr_flag;

    uart_print("BSP interrupt occured...");

    // uart_printf("ppn %d", ppn);
    // interrupt pending clear (ICU)
    SETREG(APB_INT_STS, INTR_FLASH);

    for (bank = 0; bank < NUM_BANKS; bank++)
    {
        while (BSP_FSM(bank) != BANK_IDLE)
            ;
        // get interrupt flag from BSP
        bsp_intr_flag = BSP_INTR(bank);

        if (bsp_intr_flag == 0)
        {
            continue;
        }
        UINT32 fc = GETREG(BSP_CMD(bank));
        // BSP clear
        CLR_BSP_INTR(bank, bsp_intr_flag);

        // interrupt handling
        if (bsp_intr_flag & FIRQ_DATA_CORRUPT)
        {
            uart_printf("BSP interrupt at bank: 0x%x", bank);
            uart_print("FIRQ_DATA_CORRUPT occured...");
        }
        if (bsp_intr_flag & (FIRQ_BADBLK_H | FIRQ_BADBLK_L))
        {
            uart_printf("BSP interrupt at bank: 0x%x", bank);
            if (fc == FC_COL_ROW_IN_PROG || fc == FC_IN_PROG || fc == FC_PROG)
            {
                uart_print("find runtime bad block when block program...");
            }
            else
            {
                uart_printf("find runtime bad block when block erase...vblock #: %d", GETREG(BSP_ROW_H(bank)) / PAGES_PER_BLK);
                ASSERT(fc == FC_ERASE);
            }
        }
    }
}

void ftl_policy_update(struct policy_metadata p_info, UINT32 const cmd_type)
{

    UINT32 pid = p_info.pid;

    switch (cmd_type)
    {

    case PV_CREATE:
        policy_num++;
        pvssd_policy[pid] = p_info;
        // uart_print("Policy Create Success!\n");
        print_policy(pvssd_policy[pid]);
        break;

    case PV_CHANGE:
        pvssd_policy[pid] = p_info;
        // uart_print("Policy Change Success!\n");
        print_policy(pvssd_policy[pid]);
        break;

    case PV_DELETE:
        policy_num--;

        if (policy_num < 0)
        {
            policy_num = 0;
            // uart_print("Policy Management Error!\n");
            return;
        }

        pvssd_policy[pid] = blank_policy;
        // uart_print("Policy Delete Success!\n");
        print_policy(pvssd_policy[pid]);
        break;
    }

    // uart_print("Policy update done!\n");
}

void print_policy(struct policy_metadata policy)
{
    uart_printf("pid : %d! ", policy.pid);
    uart_printf("ret_time : %d\n", policy.ret_time);
    uart_printf("backup_cycle : %d ", policy.backup_cycle);
    uart_printf("num_version : %d\n", policy.num_version);
}

/* [PV-SSD] Modified Garbage_collection for Backup Zone! */
static void modified_garbage_collection(UINT32 const bank)
{
    ASSERT(bank < NUM_BANKS);
    //g_ftl_statistics[bank].gc_cnt++;
    num_gc++;
   // uart_printf("Bank %d : %dth Garbage Collection is triggered", bank, g_ftl_statistics[bank].gc_cnt);

    UINT32 src_page;                               // iterator
    UINT32 src_lpn, src_old_vpn, src_written_time; // OOB metadata
    UINT32 src_pid, src_fid, src_offset;
    UINT32 vt_vblock;
    UINT32 free_vpn;
    UINT32 vcount; // valid page count in victim block
    UINT32 gc_vblock, prev_WT;

    UINT32 cur_vpn, prev_vpn;
    UINT8 reclaimability;
    UINT32 map_vpn, src_vpn;


    // check_and_reclaim_backup_zone();

    // get victim block
    vt_vblock = get_vt_vblock(bank);
    vcount = get_vcount(bank, vt_vblock);

    if(vcount!=0)
    {
        uart_printf("gc vcount %d", vcount);
    }
    /* get a reserved free block gc_vblock */
    gc_vblock = get_gc_vblock(bank);
    // gc_vblock = pop_free_blk_pool(bank, f_pool);
    free_vpn = gc_vblock * PAGES_PER_BLK;

    ASSERT(vt_vblock != gc_vblock);
    ASSERT(vt_vblock >= META_BLKS_PER_BANK && vt_vblock < VBLKS_PER_BANK);
    ASSERT(vcount < (PAGES_PER_BLK - 1));
    ASSERT(get_vcount(bank, gc_vblock) == VC_MAX);
    ASSERT(!is_bad_block(bank, gc_vblock));

    
    // 1. load OOB metadata from last page offset of victim block (4 * 4B x PAGES_PER_BLK)
    // OOB metadata : LPN, Previous PPN, written_time, BID
    // * NOTE: After garbage_collection(), we're going to use the block where all valid pages are copied.
    // so, we need to get all of those metadata first

    if (vt_vblock > 30000000)
        uart_printf("Fuck7");

   
    nand_page_ptread(bank, vt_vblock, PAGES_PER_BLK - 1, 0,
                     ((6 * sizeof(UINT32) * PAGES_PER_BLK + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR), FTL_BUF(bank), RETURN_WHEN_DONE);
    mem_copy(g_misc_meta[bank].lpn, FTL_BUF(bank), sizeof(UINT32) * PAGES_PER_BLK);

     
    // UINT32 k;
    // for (k=0; k<PAGES_PER_BLK; k++)
    //     uart_printf("bank : %d, vt_vblock : %d, lpn[%d] : %d", bank, vt_vblock, k, g_misc_meta[bank].lpn[k]);


    // mem_copy(g_misc_meta[bank].old_vpn, FTL_BUF(bank) + sizeof(UINT32) * PAGES_PER_BLK, sizeof(UINT32) * PAGES_PER_BLK);
    // mem_copy(g_misc_meta[bank].written_time, FTL_BUF(bank) + sizeof(UINT32) * PAGES_PER_BLK * 2, sizeof(UINT32) * PAGES_PER_BLK);
    // old_vpn
    mem_copy(OOB_OLD_VPN_ADDR + (bank * PAGES_PER_BLK) * sizeof(UINT32),
            FTL_BUF(bank) + sizeof(UINT32) * PAGES_PER_BLK * 1,
            sizeof(UINT32) * PAGES_PER_BLK);
    // written_time
    mem_copy(OOB_WRITTEN_TIME_ADDR + (bank * PAGES_PER_BLK) * sizeof(UINT32),
            FTL_BUF(bank) + sizeof(UINT32) * PAGES_PER_BLK * 2,
            sizeof(UINT32) * PAGES_PER_BLK);
    // pid
    mem_copy(OOB_PID_ADDR + (bank * PAGES_PER_BLK) * sizeof(UINT32),
            FTL_BUF(bank) + sizeof(UINT32) * PAGES_PER_BLK * 3,
            sizeof(UINT32) * PAGES_PER_BLK);

    // UINT32 ii, jj;
    // for (ii=0; ii<PAGES_PER_BLK-1; ii++)
    //     if(!read_dram_32(OOB_PID_ADDR + (bank * PAGES_PER_BLK + ii) * sizeof(UINT32))) {
    //         uart_printf("GCGCGCGC!!! bank : %d, vblock : %d, page_num : %d", bank, vt_vblock, ii);
    //         uart_printf("%d's lpn is %d", ii-1, g_misc_meta[bank].lpn[ii-1]);    
    //         uart_printf("%d's lpn is %d", ii, g_misc_meta[bank].lpn[ii]);    
    //         uart_printf("%d's lpn is %d", ii+1, g_misc_meta[bank].lpn[ii+1]);    
    //         uart_printf("%d's old_vpn is %d", ii-1, read_dram_32(OOB_OLD_VPN_ADDR + (bank * PAGES_PER_BLK + ii-1) * sizeof(UINT32)));    
    //         uart_printf("%d's old_vpn is %d", ii, read_dram_32(OOB_OLD_VPN_ADDR + (bank * PAGES_PER_BLK + ii) * sizeof(UINT32)));    
    //         uart_printf("%d's old_vpn is %d", ii+1, read_dram_32(OOB_OLD_VPN_ADDR + (bank * PAGES_PER_BLK + ii+1) * sizeof(UINT32)));    
    //         uart_printf("%d's W T is %d", ii-1, read_dram_32(OOB_WRITTEN_TIME_ADDR + (bank * PAGES_PER_BLK + ii-1) * sizeof(UINT32)));    
    //         uart_printf("%d's W T is %d", ii, read_dram_32(OOB_WRITTEN_TIME_ADDR + (bank * PAGES_PER_BLK + ii) * sizeof(UINT32)));    
    //         uart_printf("%d's W T is %d", ii+1, read_dram_32(OOB_WRITTEN_TIME_ADDR + (bank * PAGES_PER_BLK + ii+1) * sizeof(UINT32)));    
    //         uart_printf("%d's pid is %d", ii-1, read_dram_32(OOB_PID_ADDR + (bank * PAGES_PER_BLK + ii-1) * sizeof(UINT32)));    
    //         uart_printf("%d's pid is %d", ii, read_dram_32(OOB_PID_ADDR + (bank * PAGES_PER_BLK + ii) * sizeof(UINT32)));    
    //         uart_printf("%d's pid is %d", ii+1, read_dram_32(OOB_PID_ADDR + (bank * PAGES_PER_BLK + ii+1) * sizeof(UINT32)));    
    //         uart_printf("%d's fid is %d", ii-1, read_dram_32(OOB_FID_ADDR + (bank * PAGES_PER_BLK + ii-1) * sizeof(UINT32)));    
    //         uart_printf("%d's fid is %d", ii, read_dram_32(OOB_FID_ADDR + (bank * PAGES_PER_BLK + ii) * sizeof(UINT32)));    
    //         uart_printf("%d's fid is %d", ii+1, read_dram_32(OOB_FID_ADDR + (bank * PAGES_PER_BLK + ii+1) * sizeof(UINT32)));    
    //         uart_printf("%d's off is %d", ii-1, read_dram_32(OOB_FILE_OFFSET_ADDR + (bank * PAGES_PER_BLK + ii-1) * sizeof(UINT32)));    
    //         uart_printf("%d's off is %d", ii, read_dram_32(OOB_FILE_OFFSET_ADDR + (bank * PAGES_PER_BLK + ii) * sizeof(UINT32)));    
    //         uart_printf("%d's off is %d", ii+1, read_dram_32(OOB_FILE_OFFSET_ADDR + (bank * PAGES_PER_BLK + ii+1) * sizeof(UINT32)));    
    //     }

    // fid
    mem_copy(OOB_FID_ADDR + (bank * PAGES_PER_BLK) * sizeof(UINT32),
            FTL_BUF(bank) + sizeof(UINT32) * PAGES_PER_BLK * 4,
            sizeof(UINT32) * PAGES_PER_BLK);
    // offset
    mem_copy(OOB_FILE_OFFSET_ADDR + (bank * PAGES_PER_BLK) * sizeof(UINT32),
            FTL_BUF(bank) + sizeof(UINT32) * PAGES_PER_BLK * 5,
            sizeof(UINT32) * PAGES_PER_BLK);
    


    // UINT32 k=0;
    // UINT32 a, b, c, d, e, f;
    // uart_printf("MODIFIED GC!!! bank : %d, vt_vblock : %d", bank, vt_vblock);
    // for (k=0; k<PAGES_PER_BLK-1; k++){
    //     a = g_misc_meta[bank].lpn[k];
    //     b = read_dram_32(OOB_OLD_VPN_ADDR + (((bank * PAGES_PER_BLK) + k) * sizeof(UINT32)));
    //     c = read_dram_32(OOB_WRITTEN_TIME_ADDR + (((bank * PAGES_PER_BLK) + k) * sizeof(UINT32)));
    //     d = read_dram_32(OOB_PID_ADDR + (((bank * PAGES_PER_BLK) + k) * sizeof(UINT32)));
    //     e = read_dram_32(OOB_FID_ADDR + (((bank * PAGES_PER_BLK) + k) * sizeof(UINT32)));
    //     f = read_dram_32(OOB_FILE_OFFSET_ADDR + (((bank * PAGES_PER_BLK) + k) * sizeof(UINT32)));

    //     uart_printf("lpn[%d] : %d, old_vpn[%d] : %d, WT[%d] : %d, pid[%d] : %d, fid[%d] : %d, offset[%d] : %d", k, a, k, b, k, c, k, d, k, e, k, f);
    // }

    // mem_copy(g_misc_meta[bank].pid, FTL_BUF(bank) + sizeof(UINT32) * PAGES_PER_BLK * 3, sizeof(UINT32) * PAGES_PER_BLK);
    // mem_copy(g_misc_meta[bank].fid, FTL_BUF(bank) + sizeof(UINT32) * PAGES_PER_BLK * 4, sizeof(UINT32) * PAGES_PER_BLK);
    // mem_copy(g_misc_meta[bank].offset, FTL_BUF(bank) + sizeof(UINT32) * PAGES_PER_BLK * 5, sizeof(UINT32) * PAGES_PER_BLK);

    // 2. copy-back all valid pages to free space, backup page to backup zone
    for (src_page = 0; src_page < (PAGES_PER_BLK - 1); src_page++)
    {

        //check
        src_vpn = (vt_vblock * PAGES_PER_BLK) + src_page;

        if (test_reclaim_bitmap(bank, src_vpn))
                     continue;
        // get lpn of victim block from a read lpn list
        
        src_lpn = get_lpn(bank, src_page);
        map_vpn = get_vpn(src_lpn); // From FTL Page Mapping Table
        CHECK_VPAGE(map_vpn);

        // Get file metadata
        src_old_vpn = read_dram_32(OOB_OLD_VPN_ADDR + (((bank * PAGES_PER_BLK) + src_page) * sizeof(UINT32)));
        src_written_time = read_dram_32(OOB_WRITTEN_TIME_ADDR + (((bank * PAGES_PER_BLK) + src_page) * sizeof(UINT32)));
        src_pid = read_dram_32(OOB_PID_ADDR + (((bank * PAGES_PER_BLK) + src_page) * sizeof(UINT32)));
        src_fid = read_dram_32(OOB_FID_ADDR + (((bank * PAGES_PER_BLK) + src_page) * sizeof(UINT32)));
        src_offset = read_dram_32(OOB_FILE_OFFSET_ADDR + (((bank * PAGES_PER_BLK) + src_page) * sizeof(UINT32)));

        // src_pid = g_misc_meta[bank].pid[src_page];
        // src_fid = g_misc_meta[bank].fid[src_page];
        // src_offset = g_misc_meta[bank].offset[src_page];
        
        // uart_printf("src_lpn=%d src_vpn=%d src_page=%d\n",src_lpn,src_vpn,(vt_vblock * PAGES_PER_BLK) + src_page);
        // Invalid Page
        if (map_vpn != src_vpn)
        {
            UINT8 is_expired = FALSE;

            if (src_pid == NULL) // Invalid page with no following chains to be retained
                continue;
            else
            {
                /*
                    Check Reclaimablity to see if
                    1. the given page is already discovred as invalid (from previous GC)
                    2. already moved to backup zone!
                */
                if (test_reclaim_bitmap(bank, src_vpn))
                    continue;
                // if (tst_bit_dram(RECLAIMABILITY_BMP_ADDR + (src_vpn / 8), src_vpn % 8) == 1) // if reclaimable
                //     continue;

                // To determine Invalidation Time,
                // We need to pass WT of previous page as a parameter
                // Initial Condition -> the very first valid page
                UINT32 valid_vpn = get_vpn(src_lpn);
                UINT32 valid_bn = valid_vpn / PAGES_PER_BLK;
                UINT32 valid_pn = valid_vpn % PAGES_PER_BLK;

                if(valid_pn == 127) {
                    uart_printf("fffffuuuuucccckkkk!!!!");
                    while(1);
                }
                if(valid_vpn == 0)
                {
                    uart_printf("oh, vpn is NULL!");
                }

                nand_page_ptread(bank,
                                valid_bn,// Block Number
                                PAGES_PER_BLK - 1,                                                                // Last page of the block
                                0,                                                                                // Sector Offset
                                ((6 * sizeof(UINT32) * PAGES_PER_BLK + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR), // Size
                                BACKUP_CONTENT_TEMP_ADDR,                                                                               
                                RETURN_WHEN_DONE);

                prev_vpn = read_dram_32(BACKUP_CONTENT_TEMP_ADDR + (sizeof(UINT32) * PAGES_PER_BLK + sizeof(UINT32) * valid_pn));
                prev_WT  = read_dram_32(BACKUP_CONTENT_TEMP_ADDR + (sizeof(UINT32) * PAGES_PER_BLK * 2 + sizeof(UINT32) * valid_pn));

                // UINT32 ii;
                // if (prev_vpn == 0)
                // {
                //     for (ii=0; ii< PAGES_PER_BLK; ii++)
                //         uart_printf("[GC!!!]bank : %d, vblock : %d, page_num : %d, lpn : %d, old_vpn : %d, prev_WT : %d, pid : %d, fid : %d",
                //                     bank,
                //                     valid_bn,
                //                     ii,
                //                     read_dram_32(BACKUP_CONTENT_TEMP_ADDR + (sizeof(UINT32) * PAGES_PER_BLK * 0 + sizeof(UINT32) * ii)),
                //                     read_dram_32(BACKUP_CONTENT_TEMP_ADDR + (sizeof(UINT32) * PAGES_PER_BLK * 1 + sizeof(UINT32) * ii)),
                //                     read_dram_32(BACKUP_CONTENT_TEMP_ADDR + (sizeof(UINT32) * PAGES_PER_BLK * 2 + sizeof(UINT32) * ii)),
                //                     read_dram_32(BACKUP_CONTENT_TEMP_ADDR + (sizeof(UINT32) * PAGES_PER_BLK * 3 + sizeof(UINT32) * ii)),
                //                     read_dram_32(BACKUP_CONTENT_TEMP_ADDR + (sizeof(UINT32) * PAGES_PER_BLK * 4 + sizeof(UINT32) * ii))
                //                     );
                // }


                // uart_printf("src_lpn : %d, prev_vpn : %d, prev_WT : %d, valid_vpn : %d", src_lpn, prev_vpn, prev_WT, valid_vpn);

                UINT32 bn = prev_vpn / PAGES_PER_BLK;
                UINT32 pn = prev_vpn % PAGES_PER_BLK;

                
                if(pn == 127) {
                    uart_printf("it shouldn't be! fffffuuuuucccckkkk!!!! %d %d %d %u %u", bank, src_lpn, valid_vpn, prev_vpn, prev_WT);
                    //while(1);
                }

                // uart_printf("bn : %d, pn : %d", bn, pn);
                
                UINT32 lpn, old_vpn, written_time, pid, fid, offset; // OOB DATA
                if (bn > 30000000) {
                    uart_printf("valid_vpn : %d, first prev_vpn : %d, src_lpn : %d, bn : %d pn : %d", valid_vpn, prev_vpn, src_lpn, bn, pn);
                    uart_printf("Fuck2");
                }
                
                nand_page_ptread(bank,
                                bn,// Block Number
                                PAGES_PER_BLK - 1,                                                                // Last page of the block
                                0,                                                                                // Sector Offset
                                ((6 * sizeof(UINT32) * PAGES_PER_BLK + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR), // Size
                                FTL_BUF(bank),                                                                               
                                RETURN_WHEN_DONE);

                // get lpn from oob area
                // mem_copy(&lpn, FTL_BUF(bank) + (sizeof(UINT32) * pn), sizeof(UINT32));
                // mem_copy(&old_vpn, FTL_BUF(bank) + (sizeof(UINT32) * PAGES_PER_BLK + sizeof(UINT32) * pn), sizeof(UINT32));
                // mem_copy(&written_time, FTL_BUF(bank) + (sizeof(UINT32) * PAGES_PER_BLK * 2 + sizeof(UINT32) * pn), sizeof(UINT32));
                // mem_copy(&pid, FTL_BUF(bank) + (sizeof(UINT32) * PAGES_PER_BLK * 3 + sizeof(UINT32) * pn), sizeof(UINT32));
                // mem_copy(&fid, FTL_BUF(bank) + (sizeof(UINT32) * PAGES_PER_BLK * 4 + sizeof(UINT32) * pn), sizeof(UINT32));
                // mem_copy(&offset, FTL_BUF(bank) + (sizeof(UINT32) * PAGES_PER_BLK * 5 + sizeof(UINT32) * pn), sizeof(UINT32));
                
                lpn = read_dram_32(FTL_BUF(bank) + (sizeof(UINT32) * pn));
                old_vpn = read_dram_32(FTL_BUF(bank) + (sizeof(UINT32) * PAGES_PER_BLK + sizeof(UINT32) * pn));
                written_time = read_dram_32(FTL_BUF(bank) + (sizeof(UINT32) * PAGES_PER_BLK * 2 + sizeof(UINT32) * pn));
                pid = read_dram_32(FTL_BUF(bank) + (sizeof(UINT32) * PAGES_PER_BLK * 3 + sizeof(UINT32) * pn));
                fid = read_dram_32(FTL_BUF(bank) + (sizeof(UINT32) * PAGES_PER_BLK * 4 + sizeof(UINT32) * pn));
                offset = read_dram_32(FTL_BUF(bank) + (sizeof(UINT32) * PAGES_PER_BLK * 5 + sizeof(UINT32) * pn));

      // uart_printf("MODIFIED_GC!!! -> bank : %d, vpn : %d, lpn : %d, old_vpn : %d, WT : %d, pid : %d, fid : %d, offset : %d", bank, prev_vpn, lpn, old_vpn, written_time, pid, fid, offset); 
                // // move the page and all the follwing chain into backup page
                // uart_print("*****************\nmove chain to backup zone\n*****************");

                /*
                    prev_vpn : First Invalid Page
                    prev_WT : Current Valid Page WT (Invalidation Time of First Valid Page)
                    lpn, old_vpn ... offset : OOB of prev_vpn
                */
                // uart_printf("bank : %d, invalid_vpn : %d, lpn : %d, old_vpn : %d, WT : %d, pid : %d, offset : %d, prev_WT : %d\n", bank, prev_vpn, lpn, old_vpn, written_time, pid, fid, offset, prev_WT);
                if(pid==0)
                {
                    uart_printf("oh pid is 0! %d %d %d %u %u", bank, src_lpn, valid_vpn, prev_vpn, prev_WT);
                }
                move_to_backup_zone_jw(bank, prev_vpn, lpn, old_vpn, written_time, pid, fid, offset, prev_WT);
            }
        }
        else
        { //valid인 경우
            uart_print("valid pages\n");
            ASSERT(get_lpn(bank, src_page) != INVALID);
            CHECK_LPAGE(src_lpn);
            // if the page is valid,
            // then do copy-back op. to free space
            nand_page_copyback(bank,
                               vt_vblock,
                               src_page,
                               free_vpn / PAGES_PER_BLK,
                               free_vpn % PAGES_PER_BLK);
            ASSERT((free_vpn / PAGES_PER_BLK) == gc_vblock);
            page_write_num++;
            page_copy_num++;

            set_vpn(src_lpn, free_vpn);

            // if (src_pid == NULL)
            // {
            //     uart_printf("src_pid is 0????");
            //     set_reclaim_bitmap(bank, free_vpn);
            // }
            // else
                clr_reclaim_bitmap(bank, free_vpn);

            // free_vpn => cur_data_write_vpn
            // Move OOB metadata of valid pages for OOB
            UINT32 page_num = free_vpn % PAGES_PER_BLK;

            g_misc_meta[bank].lpn[page_num] = src_lpn;
            //g_misc_meta[bank].written_time[page_num] = src_written_time;
            // g_misc_meta[bank].old_vpn[page_num] = src_old_vpn;
            // g_misc_meta[bank].written_time[page_num] = src_written_time;
            // g_misc_meta[bank].pid[free_vpn % PAGES_PER_BLK] = src_pid;
            // g_misc_meta[bank].fid[free_vpn % PAGES_PER_BLK] = src_fid;
            // g_misc_meta[bank].offset[free_vpn % PAGES_PER_BLK] = src_offset;

            write_dram_32(OOB_OLD_VPN_ADDR + (bank * PAGES_PER_BLK + page_num) * sizeof(UINT32), src_old_vpn);
            write_dram_32(OOB_WRITTEN_TIME_ADDR + (bank * PAGES_PER_BLK + page_num) * sizeof(UINT32), src_written_time);
            write_dram_32(OOB_PID_ADDR + (bank * PAGES_PER_BLK + page_num) * sizeof(UINT32), src_pid);
            write_dram_32(OOB_FID_ADDR + (bank * PAGES_PER_BLK + page_num) * sizeof(UINT32), src_fid);
            write_dram_32(OOB_FILE_OFFSET_ADDR + (bank * PAGES_PER_BLK + page_num) * sizeof(UINT32), src_offset);
            if(bank==2) uart_printf("gc check prevWT %d", src_written_time);

            // set_lpn(bank, (free_vpn % PAGES_PER_BLK), src_lpn);
            // uart_printf("gc_count: %d - bank is %d free_vpn is %d and vcount is %d", gc_count, bank, free_vpn % PAGES_PER_BLK, vcount);
            if (free_vpn % PAGES_PER_BLK != 127)
                free_vpn++;
                else{
                    uart_printf("free vpn 127?? .....");
                }
        }
    }
#if OPTION_ENABLE_ASSERT
    if (vcount == 0)
    {
        ASSERT(free_vpn == (gc_vblock * PAGES_PER_BLK));
    }
#endif

    // 3. erase victim block
    nand_block_erase(bank, vt_vblock);
    // uart_printf("PAGES_PER_BLK - 2 is %d", PAGES_PER_BLK - 2);
    // uart_printf("free vpn %% PAGES_PER_BLK is %d", free_vpn % PAGES_PER_BLK);
    // uart_printf("free vpn is %d", free_vpn);

    // ASSERT((free_vpn % PAGES_PER_BLK) < (PAGES_PER_BLK - 2));
    ASSERT((free_vpn % PAGES_PER_BLK == vcount));

    // 4. update metadata
    // vt_vblock will be gc_vblock
    set_vcount(bank, vt_vblock, VC_MAX); // insert
    set_vcount(bank, gc_vblock, vcount);

    // if (vcount == 0)
    // {
    //     set_vcount(bank, gc_vblock, VC_FREE);
    // }
    // else
    // {
    //     set_vcount(bank, gc_vblock, vcount);
    // }

    set_new_write_vpn(bank, free_vpn); // set a free page for new write
    set_gc_vblock(bank, vt_vblock);    // next free block (reserve for GC)
    dec_full_blk_cnt(bank); // decrease full block count
}

//time은 expiratoin time;임.

static UINT32 get_bucket(UINT32 time)
{
    //static int inf_cnt=0;
    //expired time이 infinite이면 적절히 분배하자.
    return time % 30;
}   
/* Further Implementation */
UINT32 ftl_recovery (UINT32 const lba, UINT32 const sect_count, UINT32 const fid, UINT32 const r_time)
{
    UINT32 cur_WT,  prev_WT;
    UINT32 cur_vpn, prev_vpn;
    UINT32 cur_fid, prev_fid;
    UINT32 cur_offset, prev_offset;
    UINT32 lpn, bank;
    UINT32 vblock, page_num;

    uart_print("recovery!!!!");
    if (lba % SECTORS_PER_PAGE != 0)
    {
        uart_print("LBA should be aligned with page : lba % 8 = 0");
        return -1;
    }

    // 1. 해당 LBA로 Read를 시작함
    lpn = lba / SECTORS_PER_PAGE;
    bank = lpn % NUM_BANKS;
    
    // 2. Get first valid page(vpn)
    cur_vpn = get_vpn(lpn);
    cur_WT = read_dram_32(OOB_WRITTEN_TIME_ADDR + (bank * PAGES_PER_BLK + cur_vpn % PAGES_PER_BLK) * sizeof(UINT32));
    prev_vpn = read_dram_32(OOB_OLD_VPN_ADDR + (bank * PAGES_PER_BLK + cur_vpn % PAGES_PER_BLK) * sizeof(UINT32));
    cur_fid = read_dram_32(OOB_FID_ADDR + (bank * PAGES_PER_BLK + cur_vpn % PAGES_PER_BLK) * sizeof(UINT32));
    cur_offset = read_dram_32(OOB_FILE_OFFSET_ADDR + (bank * PAGES_PER_BLK + cur_vpn % PAGES_PER_BLK) * sizeof(UINT32));


    // g_misc_meta[bank].written_time[cur_vpn % PAGES_PER_BLK];
    // prev_vpn = g_misc_meta[bank].old_vpn[cur_vpn % PAGES_PER_BLK];

    // 3. search the vpn with the time we look for 
    do
    {
        vblock = prev_vpn / PAGES_PER_BLK;
        page_num = prev_vpn % PAGES_PER_BLK;
        
        // Read OOB
        //승진아 여기 오류있다. 
        // nand_page_ptread (bank,
        //                 vblock,
        //                 PAGES_PER_BLK-1,
        //                 0,
        //                 sizeof(UINT32) * PAGES_PER_BLK * 6,
        //                 FTL_BUF(bank),
        //                 RETURN_WHEN_DONE);
        //오류수정-jw
        nand_page_ptread (bank,
                        vblock,
                        PAGES_PER_BLK-1,
                        0,
                        (sizeof(UINT32) * PAGES_PER_BLK * 6 + BYTES_PER_SECTOR - 1)/BYTES_PER_SECTOR,
                        FTL_BUF(bank),
                        RETURN_WHEN_DONE);

        mem_copy(&prev_WT, FTL_BUF(bank) + (sizeof(UINT32) * PAGES_PER_BLK * 2 + sizeof(UINT32) * page_num), sizeof(UINT32));
        mem_copy(&prev_fid, FTL_BUF(bank) + (sizeof(UINT32) * PAGES_PER_BLK * 4 + sizeof(UINT32) * page_num), sizeof(UINT32));
        mem_copy(&prev_offset, FTL_BUF(bank) + (sizeof(UINT32) * PAGES_PER_BLK * 5 + sizeof(UINT32) * page_num), sizeof(UINT32));

        /* fid 체크? */
        if  ((r_time <= prev_WT && r_time > cur_WT) && prev_fid == fid)
            break;
        
        cur_vpn = prev_vpn;
        cur_WT = prev_WT;
        cur_fid = prev_fid;
        cur_offset = prev_offset;

        mem_copy(&prev_vpn, FTL_BUF(bank) + (sizeof(UINT32) * PAGES_PER_BLK + sizeof(UINT32) * page_num), sizeof(UINT32));

        // Termination Condition
        if (prev_vpn == NULL){
            uart_printf("no page with the lba and fid!");  
            return -1;
        };
    } while(1);

    //확인해볼것!!
    nand_page_ptread_to_host(bank,
                            prev_vpn / PAGES_PER_BLK,
                            prev_vpn % PAGES_PER_BLK,
                            lba % SECTORS_PER_PAGE,
                            sect_count);

    return prev_offset;
}

// UINT32 ftl_recovery_all (UINT32 const lba, UINT32 const fid, UINT32 const r_time)
// {
//     UINT32 bank, vblock, vpn;
//     UINT8 terminated = FALSE;
    
//     bank = r_pos_bank;
//     vblock = r_pos_vblock;
//     vpn = r_pos_vpn;

//     for (; bank < NUM_BANKS; bank++)
//     {
//         for (; vblock < VBLKS_PER_BANK; vblock++)
//         {
//             for (; vpn < PAGES_PER_BLK; vpn++)
//             {
//                 /*
//                     OOB Read 해본 뒤 판단! how?
//                 */

//                 if (vpn == PAGES_PER_BLK-1)
//                 {
//                     vpn = 0;
//                     break;
//                 }
//             }

//             if(terminated)
//                 break;

//             if (vblock == VBLKS_PER_BANK - 1) 
//             {
//                 vblock = META_BLKS_PER_BANK;
//                 break;
//             }
//         }

//         if(terminated)
//             break;
//     }

//     if(!terminated)
//         uart_print("fail to search, something may have gone wrong");
//     else 
//     {
//         r_pos_bank = bank;
//         r_pos_vblock = vblock;
//         r_pos_vpn = vpn;
//     }
// }