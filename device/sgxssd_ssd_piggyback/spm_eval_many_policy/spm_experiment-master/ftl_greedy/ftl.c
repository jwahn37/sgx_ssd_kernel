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
#define VC_FREE 0xCECE
#define VC_BACKUP 0xCFCF
#define VC_LOCK 0xFEFE

#define MISCBLK_VBN 0x1 // vblock #1 <- misc metadata
#define MAPBLKS_PER_BANK (((PAGE_MAP_BYTES / NUM_BANKS) + BYTES_PER_PAGE - 1) / BYTES_PER_PAGE)
#define META_BLKS_PER_BANK (1 + 1 + MAPBLKS_PER_BANK) // include block #0, misc block

// the number of sectors of misc. metadata info.
#define NUM_MISC_META_SECT ((sizeof(misc_metadata) + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR)
#define NUM_VCOUNT_SECT ((VBLKS_PER_BANK * sizeof(UINT16) + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR)

// For SGX-SSD
#define MAX_POLICY 101
#define MAX_CHAIN_NUM 10

//---------------------------------------
// Newly-Defined metadata for backup plan
//---------------------------------------
static struct policy_metadata pvssd_policy[MAX_POLICY];
static const struct policy_metadata blank_policy = {0, 0, 0, 0};
static free_blk_pool f_pool[NUM_BANKS];

extern const struct file_metadata null_data;
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
    UINT32 cur_write_vpn;                         // physical page for new write
    UINT32 cur_miscblk_vpn;                       // current write vpn for logging the misc. metadata
    UINT32 cur_mapblk_vpn[MAPBLKS_PER_BANK];      // current write vpn for logging the page mapping info.
    UINT32 gc_vblock;                             // vblock number for garbage collection
    UINT32 free_blk_cnt;                          // total number of free block count

    // reserved backup block
    UINT32 cur_backup_write_vpn;                  // physical page for new write of backup zone
    UINT32 backup_vblock;           

    // 4 different OOB metadata 
    UINT32 lpn[PAGES_PER_BLK];                    // ***  [SGX-SSD] lpn for back-up             *** //
    UINT32 old_vpn[PAGES_PER_BLK];                // ***  [SGX-SSD] old_vpn for back-up         *** //
    UINT32 written_time[PAGES_PER_BLK];           // ***  [SGX-SSD] written_time for retrieval  *** //
    UINT32 pid[PAGES_PER_BLK];                    // ***  [SGX-SSD] pid for back-up             *** //
    UINT32 fid[PAGES_PER_BLK];                    // ***  [SGX-SSD] fid for back-up             *** //
    // UINT32 offset[PAGES_PER_BLK];                 // ***  [SGX-SSD] file offset for retrieval   *** //

    // 4 different OOB metadata for BACKUP ZONE
    // Not for now but some day...
    // UINT32 BZ_lpn[PAGES_PER_BLK];              // ***  [SGX-SSD] lpn for back-up             *** //
    // UINT32 BZ_old_vpn[PAGES_PER_BLK];          // ***  [SGX-SSD] old_vpn for back-up         *** //
    // UINT32 BZ_written_time[PAGES_PER_BLK];     // ***  [SGX-SSD] written_time for retrieval  *** //
    // UINT32 pid[PAGES_PER_BLK];                 // ***  [SGX-SSD] pid for back-up             *** //
} misc_metadata; // per bank

//----------------------------------
// FTL metadata (maintain in SRAM)
//----------------------------------
static misc_metadata g_misc_meta[NUM_BANKS];
static ftl_statistics g_ftl_statistics[NUM_BANKS];
static UINT32 g_bad_blk_count[NUM_BANKS];

// Jeewon : Backup Zone Management Block
// 지원: backup zone에서 현 bank에 active 되어있는 블록 backup zone 

// SATA read/write buffer pointer id
UINT32 g_ftl_read_buf_id;
UINT32 g_ftl_write_buf_id;

UINT32 backup_block_count = 0;
UINT32 page_write_num = 0;
UINT32 page_copy_num = 0;
UINT32 erase_backup_num = 0;
UINT32 delta_write_offset[NUM_BANKS] = {0, 0, 0, 0, 0, 0, 0, 0};
UINT32 gc_count = 0;
UINT32 policy_num = 0;

// For Delta Compression
UINT32 delta_offset[NUM_BUCKETS] = {0, 0, 0, 0, 0};
struct delta_header delta_header[NUM_BUCKETS];

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
#define is_full_all_blks(bank) (g_misc_meta[bank].free_blk_cnt <= MAX_FREE_BLKS)
// #define is_full_all_blks(bank) (g_misc_meta[bank].free_blk_cnt == 1)
#define inc_full_blk_cnt(bank) (g_misc_meta[bank].free_blk_cnt--)
#define dec_full_blk_cnt(bank) (g_misc_meta[bank].free_blk_cnt++)
#define inc_mapblk_vpn(bank, mapblk_lbn) (g_misc_meta[bank].cur_mapblk_vpn[mapblk_lbn]++)
#define inc_miscblk_vpn(bank) (g_misc_meta[bank].cur_miscblk_vpn++)

// page-level striping technique (I/O parallelism)
#define get_num_bank(lpn) ((lpn) % NUM_BANKS)
#define get_bad_blk_cnt(bank) (g_bad_blk_count[bank])
#define get_cur_write_vpn(bank) (g_misc_meta[bank].cur_write_vpn)
#define set_new_write_vpn(bank, vpn) (g_misc_meta[bank].cur_write_vpn = vpn)
#define get_cur_backup_write_vpn(bank) (g_misc_meta[bank].cur_backup_write_vpn) // seungjin added
#define set_new_backup_write_vpn(bank, vpn) (g_misc_meta[bank].cur_backup_write_vpn = vpn) // seungjin added
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

// Newly Defined Macro by Jeewon
#define set_backup_written_time(bank, page_num) \
    (g_misc_meta[bank].written_time[page_num] = ptimer_record())
#define set_backup_zone_block(bank, vblock) \
    set_bit_dram(BACKUP_ZONE_BMP_ADDR + bank * (VBLKS_PER_BANK / 8 + 1), vblock % VBLKS_PER_BANK);

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
static void set_vcount(UINT32 const bank, UINT32 const vblock, UINT32 const vcount);
static BOOL32 is_bad_block(UINT32 const bank, UINT32 const vblock);
static BOOL32 check_format_mark(void);
static UINT32 get_vcount(UINT32 const bank, UINT32 const vblock);
static UINT32 get_vpn(UINT32 const lpn);
static UINT32 get_vt_vblock(UINT32 const bank);
static UINT32 assign_new_write_vpn(UINT32 const bank);

// Backup Zone Management Functions
static UINT32 get_backup_vpn(UINT32 const lpn);
static void set_backup_vpn(UINT32 const lpn, UINT32 const vpn);
static void modified_garbage_collection(UINT32 const bank);
static void move_to_backup_zone(UINT32 const bank, UINT32 const vpn, UINT32 const lpn, UINT32 const old_vpn, UINT32 const written_time, UINT32 const pid, UINT32 const fid, UINT32 const offset);
static UINT32 assign_new_backup_write_vpn(UINT32 const bank);

// Free Block Pool Management
static void insert_free_blk_pool (int vblock, free_blk_pool* f_pool);
static int pop_free_blk_pool (free_blk_pool* f_pool);
static int is_full (free_blk_pool* f_pool);
static int is_empty (free_blk_pool* f_pool);
static void init_free_blk_pool (free_blk_pool* f_pool);


UINT32 gcnum = 0;


static void sanity_check(void)
{
    UINT32 dram_requirement = RD_BUF_BYTES + WR_BUF_BYTES + COPY_BUF_BYTES + FTL_BUF_BYTES \
                            + HIL_BUF_BYTES + TEMP_BUF_BYTES + BAD_BLK_BMP_BYTES + PAGE_MAP_BYTES \
                            + VCOUNT_BYTES + FLUSH_MAP_BYTES + RECLAIMABILITY_BMP_BYTES \
                            + BACKUP_ZONE_BMP_BYTES + BACKUP_PAGE_MAP_BYTES + BACKUP_DELTA_BUFFER_BYTES \
                            + BACKUP_CONTENT_TEMP_BYTES + BACKUP_POLICY_BMP_BYTES;

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

// Testing FTL protocol APIs
void ftl_test_write(UINT32 const lba, UINT32 const num_sectors)
{
    ASSERT(lba + num_sectors <= NUM_LSECTORS);
    ASSERT(num_sectors > 0);

    ftl_write(lba, num_sectors, null_data);
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
void ftl_write(UINT32 const lba, UINT32 const num_sectors, struct file_metadata f)
{
    UINT32 remain_sects, num_sectors_to_write;
    UINT32 lpn, sect_offset;

    lpn = lba / SECTORS_PER_PAGE;
    sect_offset = lba % SECTORS_PER_PAGE;
    remain_sects = num_sectors;
    
    while (remain_sects != 0)
    {
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
static void write_page(UINT32 const lpn, UINT32 const sect_offset, UINT32 const num_sectors, struct file_metadata f)
{
    CHECK_LPAGE(lpn);
    ASSERT(sect_offset < SECTORS_PER_PAGE);
    ASSERT(num_sectors > 0 && num_sectors <= SECTORS_PER_PAGE);

    UINT32 bank, old_vpn, new_vpn;
    UINT32 vblock, page_num, page_offset, column_cnt;

    bank = get_num_bank(lpn); // page striping
    page_offset = sect_offset;
    column_cnt = num_sectors;

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
        // invalid old page (decrease vcount)
        if (get_vcount(bank, vblock) == VC_FREE)
        {
            set_vcount(bank, vblock, 1);
        }
        else
        {
            set_vcount(bank, vblock, get_vcount(bank, vblock) - 1);
        }
    }

    vblock = new_vpn / PAGES_PER_BLK;
    page_num = new_vpn % PAGES_PER_BLK;
    // ASSERT(get_vcount(bank,vblock) < (PAGES_PER_BLK - 1));

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
    g_misc_meta[bank].old_vpn[page_num] = old_vpn;
    g_misc_meta[bank].written_time[page_num] = ptimer_record();
    g_misc_meta[bank].pid[page_num] = f.pid;
    g_misc_meta[bank].fid[page_num] = f.fid;
    // g_misc_meta[bank].offset[page_num] = f.offset;
    
    // For Debugging
    set_vpn(lpn, new_vpn);

    // Clear Bit DRAM!
    // Set Reclaimablity map to 0!
    clr_bit_dram(RECLAIMABILITY_BMP_ADDR + (new_vpn / 8), new_vpn % 8);

    // update the valid page count for greedy victim selection policy
    if (get_vcount(bank, vblock) == VC_FREE) // if it's empty block
    {
        set_vcount(bank, vblock, 1);
    }
    else // if not increment the number of valid pages
    {
        if (get_vcount(bank,vblock) == 127)
            //uart_printf("write_page.. - bank : %d, vblock : %d, vcount : %d", bank, vblock, get_vcount(bank, vblock));
            set_vcount(bank, vblock, get_vcount(bank, vblock) + 1);
    }
}

// Move chain to backup zone
static void move_to_backup_zone(UINT32 const bank, UINT32 const vpn, UINT32 const lpn, UINT32 const old_vpn, UINT32 const written_time, UINT32 const pid, UINT32 const fid, UINT32 const offset)
{

    UINT32 backup_vpn;
    UINT32 old_vpn_temp, page_num, vblock;
    UINT32 j = 0;
    UINT32 chain_len = 0;

    // temporarily defined delta write buffer offset pointer
    // 20% compression (default as of NDSS_21), so offset range is 0 ~ 4

    //uart_printf("lpn=%d lpn_tmp:%d old_vpn=%d, written time=%d, reservationtime=%d\n",lpn,lpn_temp[page_num],old_vpn,written_time,reservation_time);
    UINT32 lpn_next, old_vpn_next, written_time_next;
    UINT32 pid_next, fid_next, offset_next;

    // UINT32 old_vpn_stack[MAX_CHAIN_NUM];
    UINT32 lpn_stack[MAX_CHAIN_NUM];
    UINT32 written_time_stack[MAX_CHAIN_NUM];
    UINT32 pid_stack[MAX_CHAIN_NUM];
    UINT32 fid_stack[MAX_CHAIN_NUM];
    UINT32 offset_stack[MAX_CHAIN_NUM];
    
    UINT32 i = 0;
    // old_vpn_stack[i] = old_vpn;
    lpn_stack[i] = lpn;
    written_time_stack[i] = written_time;
    pid_stack[i] = pid;
    fid_stack[i] = fid;
    offset_stack[i] = offset;

    // If it has chain
    if (old_vpn != NULL)
    {
        // i = 0 is for the victim page (first page in the 'broken chain')
        old_vpn_temp = old_vpn;
        
        // Get all the backed up chains
        while(1)
        {
            // uart_printf("traversing the chain! %d\n",old_vpn);
            vblock = old_vpn_temp / PAGES_PER_BLK;
            page_num = old_vpn_temp % PAGES_PER_BLK;

            // read OOB area
            nand_page_ptread(bank, vblock, PAGES_PER_BLK - 1, 0, (sizeof(UINT32) * PAGES_PER_BLK * 6), FTL_BUF(bank), RETURN_WHEN_DONE);
            
            // get lpn from oob area
            mem_copy(lpn_next, FTL_BUF(bank) + (sizeof(UINT32) * page_num), sizeof(UINT32));
            mem_copy(old_vpn_next, FTL_BUF(bank) + (sizeof(UINT32) * PAGES_PER_BLK + sizeof(UINT32) * page_num), sizeof(UINT32));
            mem_copy(written_time_next, FTL_BUF(bank) + (sizeof(UINT32) * PAGES_PER_BLK * 2 + sizeof(UINT32) * page_num), sizeof(UINT32));
            mem_copy(pid_next, FTL_BUF(bank) + (sizeof(UINT32) * PAGES_PER_BLK * 3 + sizeof(UINT32) * page_num), sizeof(UINT32));
            mem_copy(fid_next, FTL_BUF(bank) + (sizeof(UINT32) * PAGES_PER_BLK * 4 + sizeof(UINT32) * page_num), sizeof(UINT32));
            mem_copy(offset_next, FTL_BUF(bank) + (sizeof(UINT32) * PAGES_PER_BLK * 5 + sizeof(UINT32) * page_num), sizeof(UINT32));

            // no following page in the chain
            if (lpn_next != lpn)
                break;

            // if tst_bit_dram == 1 -> then reclaimable
            // if tst_bit_dram == 0 -> then not reclaimable (need to check if it's expired or not!)
            if (tst_bit_dram(RECLAIMABILITY_BMP_ADDR + (old_vpn_next / 8), (old_vpn_next % 8)) == 1) // if reclaimable
                break;

            // Originally, we need to compare the time 
            // but we just look if it has any policy for now
            if (pid_next == NULL)
            {
                // set_bit_dram(RECLAIMABILITY_BMP_ADDR + old_vpn_stack[chain_len]/8, old_vpn_stack[chain_len]%8);
                break;
            }

            // need to temporarily push in stack to save data reversely later on
            lpn_stack[++i] = lpn_next;
            // old_vpn_stack[i] = old_vpn_next; // old_vpn does not need anymore 'cause it's already changed
            written_time_stack[i] = written_time_next;
            pid_stack[i] = pid_next;
            fid_stack[i] = fid_next;
            offset_stack[i] = offset;

            // for the next page read
            old_vpn_temp = old_vpn_next;          
        }
    }

    chain_len = i;
    //uart_printf("Including the first page, chain length is %d", chain_len + 1);

    // Get the first vpn of backup zone for chain
    // old_vpn_temp = read_dram_32(BACKUP_PAGE_MAP_ADDR + lpn * sizeof(UINT32));
    while (1)
    {
        /* Time Bucket 판단해라 */
        // UINT8 hash_index = get_bucket();
        UINT8 hash_index = 5 % 5;

        // if (delta_write_offset[bank] == 4) // write compressed buffer into a new page
        if (delta_offset == MAX_PAGES_IN_DELTA - 1)
        {
            // assign new backup write pointer
            backup_vpn = assign_new_backup_write_vpn(bank);
            // set write_vpn for backup_pagemap!
            vblock = backup_vpn / PAGES_PER_BLK;
            page_num = backup_vpn % PAGES_PER_BLK;

            for (j=0; j<MAX_PAGES_IN_DELTA; j++)
                set_backup_vpn(delta_header[i].lpn, backup_vpn);

#if OPTION_COMPRESSION_ON
    // Need to save trash value + header
    // 1. Bucket에 해당하는 buffer에 delta_header 넣고 program!
#else
    // Need to save real data with header..
    // 1. Temp Content buff
#endif
            nand_page_ptprogram(bank, vblock, page_num, 0, SECTORS_PER_PAGE, BACKUP_DELTA_BUFFER_ADDR);
            delta_write_offset[bank] = 0;
        }
        else 
        {
            /* Give Compression Delay Here! */
            

            delta_write_offset[bank]++;
            // set_bit_dram(RECLAIMABILITY_BMP_ADDR + old_vpn_stack[chain_len] / 8, old_vpn_stack[chain_len] % 8);

        }

        // old_vpn = write_vpn;
        if (chain_len == 0)
            break;

        chain_len--;
    }
    
    // uart_print("finish!");

    //lpn과 가장 첫번째 페이지 연결해주는 테이블 갱신
    write_dram_32(BACKUP_PAGE_MAP_ADDR + lpn * sizeof(UINT32), backup_vpn);
}

//////////////////////////////////////////////////////////////////////////////////////////

// get vpn from BACKUP_PAGE_MAP
static UINT32 get_backup_vpn(UINT32 const lpn)
{
    CHECK_LPAGE(lpn);
    return read_dram_32(BACKUP_PAGE_MAP_ADDR + lpn * sizeof(UINT32));
}

// set vpn to BACKUP_PAGE_MAP
static void set_backup_vpn(UINT32 const lpn, UINT32 const vpn)
{
    UINT8 new_flush;
    UINT32 map_entry_per_page = sizeof(UINT32);

    CHECK_LPAGE(lpn);
    ASSERT(vpn >= (META_BLKS_PER_BANK * PAGES_PER_BLK) && vpn < (VBLKS_PER_BANK * PAGES_PER_BLK));

    write_dram_32(BACKUP_PAGE_MAP_ADDR + lpn * sizeof(UINT32), vpn);
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
    ASSERT((vcount < PAGES_PER_BLK) || (vcount == VC_BACKUP) || (vcount == VC_MAX) || (vcount == VC_FREE));

    return vcount;
}

// set valid page count of vblock
static void set_vcount(UINT32 const bank, UINT32 const vblock, UINT32 const vcount)
{
    ASSERT(bank < NUM_BANKS);
    ASSERT((vblock >= META_BLKS_PER_BANK) && (vblock < VBLKS_PER_BANK));
    if(!((vcount < PAGES_PER_BLK) || (vcount == VC_BACKUP) || (vcount == VC_MAX) || (vcount == VC_FREE) || (vcount == VC_LOCK)))
    {
        //uart_printf("gc_count %d - bank : %d, vblock : %d, vcount : %d", gc_count, bank, vblock, vcount);
    }
    ASSERT((vcount < PAGES_PER_BLK) || (vcount == VC_BACKUP) || (vcount == VC_MAX) || (vcount == VC_FREE) || (vcount == VC_LOCK));

    write_dram_16(VCOUNT_ADDR + (((bank * VBLKS_PER_BANK) + vblock) * sizeof(UINT16)), vcount);
}

static UINT32 assign_new_write_vpn(UINT32 const bank)
{
    ASSERT(bank < NUM_BANKS);

    UINT32 write_vpn;
    UINT32 vblock;

    // get write vpn pointer from metadata of each bank
    write_vpn = get_cur_write_vpn(bank);
    vblock = write_vpn / PAGES_PER_BLK;

    // NOTE: if next new write page's offset is
    // the last page offset of vblock (i.e. PAGES_PER_BLK - 1),
    if ((write_vpn % PAGES_PER_BLK) == (PAGES_PER_BLK - 2))
    {
        // uart_printf("filesys metadata block is %d", ++filesys_blk);
        // then, because of the flash controller limitation
        // (prohibit accessing a spare area (i.e. OOB)),
        // thus, we persistenly write a lpn list into last page of vblock.

        // Put 6 OOB metadata into the write buffer to save metadata
        mem_copy(FTL_BUF(bank), g_misc_meta[bank].lpn, sizeof(UINT32) * PAGES_PER_BLK);
        mem_copy(FTL_BUF(bank) + sizeof(UINT32) * PAGES_PER_BLK, g_misc_meta[bank].old_vpn, sizeof(UINT32) * PAGES_PER_BLK);
        mem_copy(FTL_BUF(bank) + sizeof(UINT32) * PAGES_PER_BLK * 2, g_misc_meta[bank].written_time, sizeof(UINT32) * PAGES_PER_BLK);
        mem_copy(FTL_BUF(bank) + sizeof(UINT32) * PAGES_PER_BLK * 3, g_misc_meta[bank].pid, sizeof(UINT32) * PAGES_PER_BLK);
        mem_copy(FTL_BUF(bank) + sizeof(UINT32) * PAGES_PER_BLK * 4, g_misc_meta[bank].fid, sizeof(UINT32) * PAGES_PER_BLK);
        mem_copy(FTL_BUF(bank) + sizeof(UINT32) * PAGES_PER_BLK * 5, g_misc_meta[bank].fid, sizeof(UINT32) * PAGES_PER_BLK);
        // mem_copy(FTL_BUF(bank) + sizeof(UINT32) * PAGES_PER_BLK * 5, g_misc_meta[bank].offset, sizeof(UINT32) * PAGES_PER_BLK);

        // last page for OOB metadata
        // PAGES_PER_BLK - 1 = OOB metadata
        // 6 * 4B * 127 = amount of OOB metadata
        nand_page_ptprogram(bank, vblock, PAGES_PER_BLK - 1, 0,
                            ((6 * sizeof(UINT32) * PAGES_PER_BLK + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR), FTL_BUF(bank));

        // reinitialize OOB metadata
        mem_set_sram(g_misc_meta[bank].lpn, 0x00000000, sizeof(UINT32) * PAGES_PER_BLK);
        mem_set_sram(g_misc_meta[bank].old_vpn, 0x00000000, sizeof(UINT32) * PAGES_PER_BLK);
        mem_set_sram(g_misc_meta[bank].written_time, 0x00000000, sizeof(UINT32) * PAGES_PER_BLK);
        mem_set_sram(g_misc_meta[bank].pid, 0x00000000, sizeof(UINT32) * PAGES_PER_BLK);
        mem_set_sram(g_misc_meta[bank].fid, 0x00000000, sizeof(UINT32) * PAGES_PER_BLK);
        mem_set_sram(g_misc_meta[bank].fid, 0x00000000, sizeof(UINT32) * PAGES_PER_BLK);
        // mem_set_sram(g_misc_meta[bank].offset, 0x00000000, sizeof(UINT32) * PAGES_PER_BLK);
        inc_full_blk_cnt(bank);

        // uart_printf("Flushing OOB metadta is done!\n");
        // uart_printf("(freeblk, copy, write, backup)= %d %d %d %d\n", g_misc_meta[bank].free_blk_cnt, page_copy_num, page_write_num, erase_backup_num);
        
        // assigning new block if necessary,
        // in the mean time, if all blocks are full
        // do garbage collection
        if (is_full_all_blks(bank))
        {
            modified_garbage_collection(bank); // after garbage collection,
                                      // cur_write_vpn is set to new block's next page  
                                      // or page right after copyied valid pages 
            return get_cur_write_vpn(bank); // return an erased block
        }
        
        // possibly, it may be a bug if the vblock is full...
        // 'cause we're doing it incrementally..
        // Need to brute-force search from vblk 0
        // vblock = META_BLKS_PER_BANK;

        // after flushing, if there's enough free block,
        // then find a new free block
        do
        {
            vblock++;
            ASSERT(vblock != VBLKS_PER_BANK);
        } while (get_vcount(bank, vblock) != VC_FREE);
    } // done flushing the full vblock
    
    // if the vblock is newly allocated,
    if (vblock != (write_vpn / PAGES_PER_BLK))
    {
        write_vpn = vblock * PAGES_PER_BLK; // first page of new block
    }
    else 
    {   // if not, return next write_vpn
        write_vpn++;
    }
    // update misc_metadata[bank]
    set_new_write_vpn(bank, write_vpn);

    return write_vpn;
}

// ******* newly defined function for SGX-SSD ******** //
// assign new backup zone write vpn for backed up data //
// Gotta have new flow!
static UINT32 assign_new_backup_write_vpn(UINT32 const bank)
{
    ASSERT(bank < NUM_BANKS);

    UINT32 write_vpn;
    UINT32 vblock;
    UINT32 i;
    // get write vpn pointer from metadata of each bank
    write_vpn = get_cur_backup_write_vpn(bank);
    
    // need to improve!
    // initial condition
    if (write_vpn == INIT_BACKUP_WRITE_PTR) 
    {
        // uart_print("need to search initial free block for backup zone");
        
        // DATA BLOCK # > META BLOCK #
        // uart_printf("META_BLKS_PER_BLK is %d", META_BLKS_PER_BANK);
        vblock = META_BLKS_PER_BANK;
        do
        {
            vblock++;
        } while (get_vcount(bank, vblock) != VC_FREE);
       
    }

    vblock = write_vpn / PAGES_PER_BLK;

    // NOTE: if next current write page's offset is
    // the last page offset of vblock (i.e. PAGES_PER_BLK - 1),
    if ((write_vpn % PAGES_PER_BLK) == (PAGES_PER_BLK - 1))
    {
        // uart_printf("backup data write starts\n");
                    
        // then find a new free block
        vblock = META_BLKS_PER_BANK;
        do
        {
            vblock++;
        } while (get_vcount(bank, vblock) != VC_FREE); 

        /* Free Block Get*/
        // if (pop_free_blk_pool(&f_pool[bank]) < 0)
            // uart_print("error!");



    } // NOTE: if next new write page's offset is
    // the last page offset of vblock (i.e. PAGES_PER_BLK - 1),

    // if vblock is newly allocated,
    if (vblock != (write_vpn / PAGES_PER_BLK))
    {
        set_vcount(bank, vblock, VC_BACKUP);
        //uart_printf("\n\n\nbank %d, vblock %d, is set to VC_BACKUP!!!!\n\n\n", bank, vblock);
        inc_full_blk_cnt(bank); // backup block is always treated as full block
        write_vpn = vblock * PAGES_PER_BLK; // first page of new block
    }
    else 
    {   // if not, return next write_vpn
        write_vpn++;
    }

    // update misc_metadata[bank].cur_backup_write_vpn
    set_new_backup_write_vpn(bank, write_vpn);

    return write_vpn;
}

static BOOL32 is_bad_block (UINT32 const bank, UINT32 const vblk_offset)
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
    {
        // uart_printf("No Free Block Left!!\n");
        // uart_printf("Let's see if it's true!\n");

        // for (j=0; j<8; j++)
            // for (i=META_BLKS_PER_BANK; i < 256; i = i + 2)
            // {
            //     uart_printf("bank: %d VBLOCK %d has %d valid pages, bank: %d VBLOCK %d has %d valid pages", bank, i, get_vcount(bank, i), bank, i+1, get_vcount(bank, i+1));
            // }
    }                                

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
    // uart_printf("meta=%dKB\n", METADATA_BACKUPZONE_BYTES / 1024);

    uart_printf("VBLKS_PER_BANK: %d", VBLKS_PER_BANK);
    uart_printf("LBLKS_PER_BANK: %d", NUM_LPAGES / PAGES_PER_BLK / NUM_BANKS);
    uart_printf("META_BLKS_PER_BANK: %d", META_BLKS_PER_BANK);
    uart_printf("PAGES_PER_BLK : %d", PAGES_PER_BLK);

    ///지원: 확인용//////////
    uart_printf("NUM_LPAGES: %d", NUM_LPAGES);
    uart_printf("NUM_BANKS : %d", NUM_BANKS);
    
    
    uart_printf("SSD SIZE_BLKS : %d", (VBLKS_PER_BANK - MAX_BAD_BLKS_PER_BANKS - 32 - FILESYS_METADATA_BLKS_PER_BANK - META_BLKS_PER_BANK));
    ////////////////////////

    //----------------------------------------
    // initialize DRAM metadata
    //----------------------------------------
    mem_set_dram(PAGE_MAP_ADDR, NULL, PAGE_MAP_BYTES);
    mem_set_dram(VCOUNT_ADDR, 0xCECECECE, VCOUNT_BYTES);
    
    /*
    for(i=0; i<VCOUNT_BYTES; i+=1)
        uart_printf("!!!!!!!=%x\n",read_dram_8(VCOUNT_ADDR+i));
    while(1){

    }
    */
    mem_set_dram(FLUSH_MAP_ADDR, NULL, FLUSH_MAP_BYTES);
    mem_set_dram(RECLAIMABILITY_BMP_ADDR, NULL, RECLAIMABILITY_BMP_BYTES);
    mem_set_dram(BACKUP_ZONE_BMP_ADDR, NULL, BACKUP_ZONE_BMP_BYTES);
    mem_set_dram(BACKUP_PAGE_MAP_ADDR, NULL, BACKUP_PAGE_MAP_BYTES);
    mem_set_dram(BACKUP_POLICY_BMP_ADDR, NULL, BACKUP_POLICY_BMP_BYTES);

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
                vcount_val = VC_FREE;
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
    // uart_print("format complete");
}

static void init_metadata_sram(void)
{
    UINT32 bank;
    UINT32 vblock;
    UINT32 mapblk_lbn;

    //----------------------------------------
    // initialize misc. metadata
    //----------------------------------------
    for (bank = 0; bank < NUM_BANKS; bank++)
    {
        // Initialize FREE BLK POOLs
        init_free_blk_pool (&f_pool[bank]);

        g_misc_meta[bank].free_blk_cnt = VBLKS_PER_BANK - META_BLKS_PER_BANK;
        g_misc_meta[bank].free_blk_cnt -= get_bad_blk_cnt(bank);

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
        // assign free block for gc
        //----------------------------------------
        do
        {
            vblock++;

            // NOTE: free block should not be secleted as a victim @ GC
            write_dram_16(VCOUNT_ADDR + ((bank * VBLKS_PER_BANK) + vblock) * sizeof(UINT16), VC_MAX);
            
            if (is_bad_block(bank, vblock) == TRUE)
                continue;

            /* set free block */
            // set_gc_vblock(bank, vblock);
                     
            /* [MODIFIED_GARBAGE_COLLECTION] */
            /* set free block */
            insert_free_blk_pool (vblock, &f_pool[bank]);           

            ASSERT(vblock < VBLKS_PER_BANK);

        } while (is_full (&f_pool[bank]) == FALSE);
        // } while (is_bad_block(bank, vblock) == TRUE);

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

        // Seungjin Lee
        //----------------------------------------
        // For initial backup_write_vpn
        //----------------------------------------        
        set_new_backup_write_vpn(bank, INIT_BACKUP_WRITE_PTR);
        // uart_printf("bank : %d backup ptr : %x", bank, g_misc_meta[bank].cur_backup_write_vpn);
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

void ftl_policy_update (struct policy_metadata p_info, UINT32 const cmd_type) {
    
    UINT32 pid = p_info.pid;

    switch (cmd_type) {

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
            
            if (policy_num < 0) {
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

void print_policy (struct policy_metadata policy) {
    uart_printf("The Data is : %d, %d, %d, %d", policy.pid, policy.ret_time, policy.backup_cycle, policy.backup_cycle, policy.num_version);
    // uart_printf("ret_time : %d\n", policy.ret_time);
    // uart_printf("backup_cycle : %d ", policy.backup_cycle);
    // uart_printf("num_version : %d\n", policy.num_version);
}

/* [PV-SSD] Modified Garbage_collection for Backup Zone! */
static void modified_garbage_collection(UINT32 const bank)
{
    
    // ASSERT(bank < NUM_BANKS);
    // g_ftl_statistics[bank].gc_cnt++;

    // UINT32 src_page; // iterator
    // UINT32 src_lpn, src_old_vpn, src_written_time; // OOB metadata
    // UINT32 src_pid, src_fid, src_offset;
    // UINT32 vt_vblock;
    // UINT32 free_vpn;
    // UINT32 vcount; // valid page count in victim block
    // UINT32 gc_vblock;

    // UINT8 reclaimability;
    // UINT32 map_vpn, src_vpn;

    // do {
    //     // uart_printf("garbage_collection start! gc_count is %d \n", ++gc_count);
    //     g_ftl_statistics[bank].gc_cnt++;

    //     // get victim block
    //     vt_vblock = get_vt_vblock(bank);
    //     vcount = get_vcount(bank, vt_vblock);

    //     /********** [MODIFIED_GARBAGE_COLLECTION] DO THE LOCK FOR THE BLOCK! ***********/
    //     set_vcount (bank, vt_vblock, VC_LOCK);

    //     /* get a reserved free block gc_vblock */
    //     // gc_vblock = get_gc_vblock(bank); // ->
    //     gc_vblock = pop_free_blk_pool(&f_pool[bank]);
    //     free_vpn = gc_vblock * PAGES_PER_BLK;
        
    //     ASSERT(vt_vblock != gc_vblock);
    //     ASSERT(vt_vblock >= META_BLKS_PER_BANK && vt_vblock < VBLKS_PER_BANK);

    //     // ASSERT(vcount < (PAGES_PER_BLK - 1));
    //     // ASSERT(get_vcount(bank, gc_vblock) == VC_MAX);
    //     ASSERT(!is_bad_block(bank, gc_vblock));

    //     // 1. load OOB metadata from last page offset of victim block (4 * 4B x PAGES_PER_BLK)
    //     // OOB metadata : LPN, Previous PPN, written_time, BID
    //     // * NOTE: After garbage_collection(), we're going to use the block where all valid pages are copied.
    //     // so, we need to get all of those metadata first

    //     nand_page_ptread(bank, vt_vblock, PAGES_PER_BLK - 1, 0,
    //                     ((4 * sizeof(UINT32) * PAGES_PER_BLK + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR), FTL_BUF(bank), RETURN_WHEN_DONE);
    //     mem_copy(g_misc_meta[bank].lpn, FTL_BUF(bank), sizeof(UINT32) * PAGES_PER_BLK);
    //     mem_copy(g_misc_meta[bank].old_vpn, FTL_BUF(bank) + sizeof(UINT32) * PAGES_PER_BLK, sizeof(UINT32) * PAGES_PER_BLK);
    //     mem_copy(g_misc_meta[bank].written_time, FTL_BUF(bank) + sizeof(UINT32) * PAGES_PER_BLK * 2, sizeof(UINT32) * PAGES_PER_BLK);
    //     mem_copy(g_misc_meta[bank].pid, FTL_BUF(bank) + sizeof(UINT32) * PAGES_PER_BLK * 3, sizeof(UINT32) * PAGES_PER_BLK);
    //     mem_copy(g_misc_meta[bank].fid, FTL_BUF(bank) + sizeof(UINT32) * PAGES_PER_BLK * 4, sizeof(UINT32) * PAGES_PER_BLK);
    //     mem_copy(g_misc_meta[bank].offset, FTL_BUF(bank) + sizeof(UINT32) * PAGES_PER_BLK * 5, sizeof(UINT32) * PAGES_PER_BLK);

    //     // 2. copy-back all valid pages to free space, backup page to backup zone
    //     for (src_page = 0; src_page < (PAGES_PER_BLK - 1); src_page++)
    //     {
    //         // get lpn of victim block from a read lpn list
    //         src_lpn = get_lpn(bank, src_page);
    //         src_vpn = (vt_vblock * PAGES_PER_BLK) + src_page;
    //         map_vpn = get_vpn(src_lpn); // From FTL Page Mapping Table
    //         CHECK_VPAGE(map_vpn);

    //         // Get file metadata
    //         src_pid = g_misc_meta[bank].pid[src_page];
    //         src_fid = g_misc_meta[bank].fid[src_page];
    //         src_offset = g_misc_meta[bank].offset[src_page];

    //         // uart_printf("src_lpn=%d src_vpn=%d src_page=%d\n",src_lpn,src_vpn,(vt_vblock * PAGES_PER_BLK) + src_page);
    //         if (map_vpn != src_vpn)
    //         {   
    //             UINT8 is_expired = FALSE;

    //             if (src_pid == NULL) // Invalid page with no following chains to be retained
    //                 continue;                    

    //             else 
    //             {
    //                 /*
    //                     Check Reclaimablity to see if
    //                     1. the given page is already discovred as invalid (from previous GC)
    //                     2. already moved to backup zone!
    //                 */
    //                 if (tst_bit_dram(RECLAIMABILITY_BMP_ADDR + (src_vpn / 8), src_vpn % 8) == 1) // if reclaimable
    //                     continue;

    //                 // Check if it's expired or not
    //                 // [N] Need to have consensus about time in terms of experiment
    //                 /* Expiration Check Here! */

    //                 // move the page and all the follwing chain into backup page
    //                 if (is_expired == FALSE) 
    //                 {
    //                     uart_print ("*****************\nmove chain to backup zone safely!\n*****************");
    //                     move_to_backup_zone (bank, src_vpn, src_lpn, src_old_vpn, src_written_time, src_pid, src_fid, src_offset);
    //                     continue;
    //                 }
    //             }
    //         }
    //         else
    //         {   //valid인 경우
    //             //uart_print("valid pages\n");
    //             ASSERT(get_lpn(bank, src_page) != INVALID);
    //             CHECK_LPAGE(src_lpn);
    //             // if the page is valid,
    //             // then do copy-back op. to free space
    //             nand_page_copyback(bank,
    //                             vt_vblock,
    //                             src_page,
    //                             free_vpn / PAGES_PER_BLK,
    //                             free_vpn % PAGES_PER_BLK);
    //             ASSERT((free_vpn / PAGES_PER_BLK) == gc_vblock);
    //             page_write_num++; page_copy_num++;
                
    //             set_vpn(src_lpn, free_vpn);
                
    //             // free_vpn => cur_data_write_vpn
    //             // Move OOB metadata of valid pages for OOB
    //             g_misc_meta[bank].lpn[free_vpn % PAGES_PER_BLK] = src_lpn;
    //             g_misc_meta[bank].old_vpn[free_vpn % PAGES_PER_BLK] = src_old_vpn;
    //             g_misc_meta[bank].written_time[free_vpn % PAGES_PER_BLK] = src_written_time;
    //             g_misc_meta[bank].pid[free_vpn % PAGES_PER_BLK] = src_pid;
    //             g_misc_meta[bank].fid[free_vpn % PAGES_PER_BLK] = src_fid;
    //             g_misc_meta[bank].offset[free_vpn % PAGES_PER_BLK] = src_offset;

    //             // set_lpn(bank, (free_vpn % PAGES_PER_BLK), src_lpn);
    //             // uart_printf("gc_count: %d - bank is %d free_vpn is %d and vcount is %d", gc_count, bank, free_vpn % PAGES_PER_BLK, vcount);
    //             if (free_vpn % PAGES_PER_BLK != 127)
    //                 free_vpn++;
    //         }
    //     }
    // #if OPTION_ENABLE_ASSERT
    //     if (vcount == 0)
    //     {
    //         ASSERT(free_vpn == (gc_vblock * PAGES_PER_BLK));
    //     }
    // #endif

    //     // 3. erase victim block
    //     nand_block_erase(bank, vt_vblock);
    //     // uart_printf("PAGES_PER_BLK - 2 is %d", PAGES_PER_BLK - 2);
    //     // uart_printf("free vpn %% PAGES_PER_BLK is %d", free_vpn % PAGES_PER_BLK);
    //     // uart_printf("free vpn is %d", free_vpn);

    //     // ASSERT((free_vpn % PAGES_PER_BLK) < (PAGES_PER_BLK - 2));
    //     ASSERT((free_vpn % PAGES_PER_BLK == vcount));

    //     // 4. update metadata
    //     // vt_vblock will be gc_vblock
    //     set_vcount(bank, vt_vblock, VC_MAX);

    //     if (vcount == 0)
    //     {
    //         set_vcount(bank, gc_vblock, VC_FREE);
    //     }
    //     else
    //     {
    //         set_vcount(bank, gc_vblock, vcount);
    //     }

    //     set_new_write_vpn(bank, free_vpn); // set a free page for new write
        
    //     /* [MODIFIED_GARBAGE_COLLECTION] Insert the free block into queue! */ 
    //     // set_gc_vblock(bank, vt_vblock);    // next free block (reserve for GC)
    //     insert_free_blk_pool (vt_vblock, &f_pool[bank]);
    //     dec_full_blk_cnt(bank); // decrease full block count

    // } while (is_full_all_blks(bank));
    
}

void insert_free_blk_pool (int vblock, free_blk_pool* f_pool) {

    if (is_full(f_pool)) {
        uart_print ("Free BLK Pool is full, meaning that something went wrong!\n");
        return; 
    }
    
    f_pool->rear = (f_pool->rear + 1) % MAX_FREE_BLKS; 
    f_pool->array[f_pool->rear] = vblock; 
    f_pool->size++;

    // uart_printf ("Free Blk %d is inserted!", vblock);
}

int pop_free_blk_pool (free_blk_pool* f_pool) {

    if (is_empty(f_pool)) {
        uart_print ("Free BLK Pool is empty, meaning that something went wrong!\n");
        return -1; 
    }

    int ret = f_pool->array[f_pool->front]; 
    f_pool->front = (f_pool->front + 1) % MAX_FREE_BLKS; 
    f_pool->size--;

    return ret; 
}

int is_full (free_blk_pool* f_pool) {
    return (f_pool->size == MAX_FREE_BLKS);
}

int is_empty (free_blk_pool* f_pool) {
    return (f_pool->size == 0);
}

static void init_free_blk_pool (free_blk_pool* f_pool) {
    
    int i;

    f_pool->front = 0;
    f_pool->rear = 0;
    f_pool->size = 0;

    for (i=0; i<MAX_FREE_BLKS; i++)   
        f_pool->array[i] = NULL;
}

/* Further Implementation */

// PV_ftl_recovery