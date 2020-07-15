/*
fibmap 참조, 특히 hdparm 코드
*/


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <assert.h>
#include <linux/hdreg.h>
#include <dirent.h>
#include <sys/sysmacros.h>

#include "frecovery.h"

//#define PATH_MAX 30
#define SECTOR_SIZE 512
#define BUF_SIZE 16384*4
#define BUF_ELE_SIZE 12
#define BUF_LBA_SIZE 8
#define NUM_LBA 16384*4
#define RECOVERY_KEY 336

#define SSD_PAGESIZE (32*1024)
#define SECTORS_PER_PAGE (SSD_PAGESIZE / SECTOR_SIZE)

static unsigned long get_start_lba(dev_t dev);
static void find_dev_file(dev_t dev, char *dev_file);
static void print_buf(char *buf, unsigned int buf_size);

//static unsigned char lba_list[BUF_SIZE];

typedef struct{
	unsigned int lba;
	unsigned int size;
}LBA_LIST;

LBA_LIST lba_list[NUM_LBA];
int lba_num = 0;
char dev_file[PATH_MAX];

int frecovery(int fd, unsigned int recovery_time)
{
	unsigned int blkcnt;
	unsigned int blocksize;
	unsigned int sec_per_blk;
	struct stat st;
	unsigned int i, block;
	unsigned int block_bf;
	unsigned long start_lba;	
	unsigned long lba;
	unsigned int size;
	unsigned int lba_list_size;
	//int lba_num;
	//unsigned char lba_list[BUF_SIZE];

	printf("frecovery (%d %d)\n", fd, recovery_time);

	if (ioctl(fd, FIGETBSZ, &blocksize))
	{
		printf("ioctl FIBGETBSZ error\n");
	}
	if (fstat(fd, &st))
	{
		printf("fstat error\n");
	}

	printf("file size: %d\n", st.st_size);
	blkcnt = (st.st_size + blocksize - 1) / blocksize;
	sec_per_blk = blocksize/SECTOR_SIZE;
	printf("File size %d blockcnt %d blocksize %d\n", (int)st.st_size, blkcnt, blocksize);

	//printf("check major: %d %d\n", major(st.st_dev), minor(st.st_dev));
	//get start lba
//	get_dev_t_geometry (st.st_dev, NULL, NULL, NULL, &start_lba, NULL, &sector_bytes);
	start_lba=get_start_lba(st.st_dev);
	printf("start lba: 0x%lx\n", start_lba);	//device lba

	//get lba list
	block_bf=0xfffffffe;
	size=0;
	//lba_list_size=0;
	lba_num =0 ;
	for (i = 0; i < blkcnt; i++) {
		block = i;
		//printf("block_bf %u block %u\n", block_bf, block);

		//printf("offset: %d\n", lseek(fd, 0, SEEK_CUR)); 
		if (ioctl(fd, FIBMAP, &block)) {
			perror("FIBMAP ioctl failed");
		}
		if(block_bf+1 < block)
		{
			//printf("Not Contiguous!!!!!!!\n");
			lba = start_lba + block_bf*sec_per_blk - (size-sec_per_blk);
			printf("LBA/size: 0x%lx, %u\n", lba, SECTOR_SIZE*size);
			//printf("lba_listn_idx: %d\n", lba_list_size);
			//sprintf(&(lba_list[lba_list_size]), "%lu%u", lba, size);
			//*(unsigned long*)(&lba_list[lba_list_size]) = lba;
			//*(unsigned int*)(&lba_list[lba_list_size+BUF_LBA_SIZE]) = size;
			
			lba_list[lba_num].lba = lba;
			lba_list[lba_num].size = size;
			lba_num++;
			//lba_list_sizelba_list_size+=BUF_ELE_SIZE;
			size = 0;
		}
		//printf("block_bf %u block %u lba 0x%x\n", block_bf, block, start_lba + block_bf*sec_per_blk - (size-sec_per_blk));

		block_bf=block;
		size+=sec_per_blk;
		//printf("%3d %10lu\n", i, start_lba+block*sec_per_blk);	
	}	
	lba = start_lba + block*sec_per_blk -(size-sec_per_blk);
	printf("LBA/size: 0x%x, %u\n", lba, SECTOR_SIZE*size);
	//*(unsigned long*)(&lba_list[lba_list_size]) = lba;
	//*(unsigned int*)(&lba_list[lba_list_size+BUF_LBA_SIZE]) = size;
	lba_list[lba_num].lba = lba;
	lba_list[lba_num].size = size;
	lba_num++;
	//lba_list_size+=BUF_ELE_SIZE;			
	//sprintf(&lba_list[lba_list_size], "%lu%u", lba, size);
			
	//print_buf(lba_list, lba_list_size);

	recovery_request(fd, recovery_time);

	
}

static void recovery_request(int fd, unsigned int recovery_time)
{
	//Covert each lba list into the LPN
	for(int i=0; i<lba_num; i++)
	{
		recovery(fd, recovery_time, lba_list[i].lba, lba_list[i].size);
	}
	
}

static void recovery(int fd, unsigned int recovery_time, unsigned int lba, unsigned int num_sectors)
{
	unsigned int lpn, sect_offset;
	unsigned int remain_sects;

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
        vpn  = get_vpn(lpn);
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
			while (next_read_buf_id == GETREG(SATA_RBUF_PTR));	// wait if the read buffer is full (slow host)
			#endif

            // fix bug @ v.1.0.6
            // Send 0xFF...FF to host when the host request to read the sector that has never been written.
            // In old version, for example, if the host request to read unwritten sector 0 after programming in sector 1, Jasmine would send 0x00...00 to host.
            // However, if the host already wrote to sector 1, Jasmine would send 0xFF...FF to host when host request to read sector 0. (ftl_read() in ftl_xxx/ftl.c)
			mem_set_dram(RD_BUF_PTR(g_ftl_read_buf_id) + sect_offset*BYTES_PER_SECTOR,
                         0xFFFFFFFF, num_sectors_to_read*BYTES_PER_SECTOR);

            flash_finish();

			SETREG(BM_STACK_RDSET, next_read_buf_id);	// change bm_read_limit
			SETREG(BM_STACK_RESET, 0x02);				// change bm_read_limit

			g_ftl_read_buf_id = next_read_buf_id;
        }
        sect_offset   = 0;
        remain_sects -= num_sectors_to_read;
        lpn++;
    }
	
	//send lba_listfer to the system call
	//syscall(RECOVERY_KEY, fd, lba_list, size, recovery_time, dev_file);
	
}

static unsigned long get_start_lba(dev_t dev)
{
	unsigned long start_lba;
	//get start_lba
	struct hd_geometry{
		unsigned char	heads;
      unsigned char	sectors;
      unsigned short	cylinders;
      unsigned long	start;
	};

	//char dev_file[PATH_MAX]="/dev/sda4";
	//char dev_file[PATH_MAX];
	//get device file name
	find_dev_file(dev, dev_file);
	printf("Device file in %s\n", dev_file);

	int fd = open(dev_file, O_RDONLY|O_NONBLOCK); 
	if(fd==-1)
	{
		printf("device file open error\n");
	}

	struct hd_geometry geom;
	ioctl(fd, HDIO_GETGEO, &geom);
	start_lba = geom.start;

	close(fd);

	return start_lba;
}

static void find_dev_file(dev_t dev, char *dev_file)
{
	DIR *dp;
	struct dirent *entry;
	char *dev_dir="/dev";
	unsigned int major_ = major(dev), minor_ = minor(dev);
	

	*dev_file = '\0';
	if(!(dp = opendir(dev_dir)))
	{
		printf("device directory open error\n");
	}
	
	while((entry = readdir(dp)) != NULL)
	{
		if(entry->d_type == DT_UNKNOWN || entry->d_type == DT_BLK)
		{
			struct stat st;
			sprintf(dev_file, "%s/%s", dev_dir, entry->d_name);
			//printf("%s\n", dev_file);
			if(!stat(dev_file, &st) && S_ISBLK(st.st_mode))//S_ISBLK : Block device file
			{
			//	printf("%s: %d(%d), %d(%d)\n", dev_file, major_, minor_, major(st.st_rdev), minor(st.st_rdev));
				if((major_==(unsigned)major(st.st_rdev)) && (minor_ == (unsigned)minor(st.st_rdev)))
				{
				//	printf("hihi\n");
					closedir(dp);
					return;
				}
			}
		}
	}
}

static void print_buf(char *buf, unsigned int buf_size)
{
	int i,j;

	printf("lba_list_size: %u\n", buf_size);
	for (i=0; i<buf_size; i+=12)
	{
		//for(j=0; j<8; j++)
			printf("0x%lx %u", *(unsigned long*)(&lba_list[i]), *(unsigned int*)(&buf[i+8]));
		printf("\n");
	}
}

// static void request_recovery(char *buf, unsigned int buf_size)
// {
// 	int i,j;

// 	printf("lba_list_size: %u\n", buf_size);
// 	for (i=0; i<buf_size; i+=12)
// 	{
// 		//for(j=0; j<8; j++)
// 			printf("0x%lx %u", *(unsigned long*)(&lba_list[i]), *(unsigned int*)(&buf[i+8]));
// 		printf("\n");
// 	}
// }
