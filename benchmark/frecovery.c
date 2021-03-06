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
#define SECTOR_BIT 9
#define BUF_SIZE 16384 * 4
#define BUF_ELE_SIZE 12
#define BUF_LBA_SIZE 8
#define NUM_LBA 16384 * 4
#define RECOVERY_KEY 336

#define SSD_PAGE_SIZE (32 * 1024)
#define SECTORS_PER_PAGE (SSD_PAGE_SIZE / SECTOR_SIZE)

int frecovery(int fd, unsigned int recovery_time, unsigned int fid);
static void recovery_request(int fd, unsigned int recovery_time, int rec_fd);
static void recovery(int fd, unsigned int recovery_time, unsigned int lba, unsigned int num_sectors, int rec_fd);
static int make_recfile(int file_size);
static void write_recfile(int rec_fd, char* rec_buf, int cur_offset, int size);
static unsigned long get_start_lba(dev_t dev);
static void find_dev_file(dev_t dev, char *dev_file);
static void print_buf();
static void write_recfile(int rec_fd, char* rec_buf, int cur_offset, int size);

//static unsigned char lba_list[BUF_SIZE];

typedef struct
{
	unsigned int lba;
	unsigned int size;
} LBA_LIST;

LBA_LIST lba_list[NUM_LBA];
int lba_num = 0;
char dev_file[PATH_MAX];
//char rec_buf[SSD_PAGESIZE + SSD_PAGESIZE];
char *rec_buf;
char *rec_buf_;
unsigned int fid;
unsigned int recovery_time;

int frecovery(int fd, unsigned int recovery_time_, unsigned int fid_)
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

	printf("frecovery (%x %x)\n", fid_, recovery_time_);

	fid = fid_;
	recovery_time = recovery_time_;

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
	sec_per_blk = blocksize / SECTOR_SIZE;
	printf("File size %d blockcnt %d blocksize %d\n", (int)st.st_size, blkcnt, blocksize);

	//printf("check major: %d %d\n", major(st.st_dev), minor(st.st_dev));
	//get start lba
	//	get_dev_t_geometry (st.st_dev, NULL, NULL, NULL, &start_lba, NULL, &sector_bytes);
	start_lba = get_start_lba(st.st_dev);
	printf("start lba: 0x%lx\n", start_lba); //device lba

	//get lba list
	block_bf = 0xfffffffe;
	size = 0;
	//lba_list_size=0;
	lba_num = 0;
	for (i = 0; i < blkcnt; i++)
	{
		block = i;
		//printf("block_bf %u block %u\n", block_bf, block);

		//printf("offset: %d\n", lseek(fd, 0, SEEK_CUR));
		if (ioctl(fd, FIBMAP, &block))
		{
			perror("FIBMAP ioctl failed");
		}
		if (block_bf + 1 < block)
		{
			//printf("Not Contiguous!!!!!!!\n");
			lba = start_lba + block_bf * sec_per_blk - (size - sec_per_blk);
			//printf("LBA/size: 0x%lx, %u\n", lba, SECTOR_SIZE * size);
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

		block_bf = block;
		size += sec_per_blk;
		//printf("%3d %10lu\n", i, start_lba+block*sec_per_blk);
	}
	lba = start_lba + block * sec_per_blk - (size - sec_per_blk);
	//printf("LBA/size: 0x%x, %u\n", lba, SECTOR_SIZE * size);
	//*(unsigned long*)(&lba_list[lba_list_size]) = lba;
	//*(unsigned int*)(&lba_list[lba_list_size+BUF_LBA_SIZE]) = size;
	lba_list[lba_num].lba = lba;
	lba_list[lba_num].size = size;
	lba_num++;
	//lba_list_size+=BUF_ELE_SIZE;
	//sprintf(&lba_list[lba_list_size], "%lu%u", lba, size);

	print_buf(lba_list);

	
	//direct i/o 할 버퍼는 반드시 512BYTE단위로 주소를 초기화해야함.
	rec_buf_ = (char*)malloc(sizeof(char)*SSD_PAGE_SIZE*2 + SECTOR_SIZE);
	rec_buf = (char*) ((((unsigned long)rec_buf_ + SECTOR_SIZE -1 ) >> SECTOR_BIT)<<SECTOR_BIT);

	int rec_fd = make_recfile(st.st_size);
	recovery_request(fd, recovery_time, rec_fd);
	fsync(rec_fd);
	close(rec_fd);

	free(rec_buf_);
}

static void recovery_request(int fd, unsigned int recovery_time, int rec_fd)
{
	//Covert each lba list into the LPN
	for (int i = 0; i < lba_num; i++)
	{
		recovery(fd, recovery_time, lba_list[i].lba, lba_list[i].size, rec_fd);
	}
}

static void recovery(int fd, unsigned int recovery_time, unsigned int lba, unsigned int num_sectors, int rec_fd)
{
	//follow open ssd jaemine ftl_read code(greedy_ftl)
	unsigned int lpn, sect_offset, num_sectors_to_recovery;
	unsigned int remain_sects;
	unsigned int cur_lbn, cur_size, cur_offset;
	//unsigned int fid;


	lpn = lba / SECTORS_PER_PAGE;
	sect_offset = lba % SECTORS_PER_PAGE;
	remain_sects = num_sectors;

	//eval code(temp)
	//emcpy(rec_buf, &, 4);
	//for(int i=SSD_PAGE_SIZE; i<SSD_PAGESIZE*2; i++)
	//	rec_buf[i] = 0x55;
	cur_offset = 0;

	//페이지 (이하) 단위로 복구 요청
	while (remain_sects != 0)
	{
		if ((sect_offset + remain_sects) < SECTORS_PER_PAGE)
		{
			num_sectors_to_recovery = remain_sects;
		}
		else
		{
			num_sectors_to_recovery = SECTORS_PER_PAGE - sect_offset;
		}

		//LPN의 sect_offset으로부터 num_sectors_to_read만큼 읽어온다.
		//		syscall(RECOVERY_KEY, fd, lba_list, size, rerecovery_time, dev_file);
		cur_lbn = lpn * SECTORS_PER_PAGE + sect_offset;
		cur_size = num_sectors_to_recovery * SECTOR_SIZE;

		if(( (unsigned long)rec_buf & 0x01ff) != 0)
		{
			printf("ERROR : wrong I/O buffer address\n");
		}

		printf("[RECOVERY_KEY] cur_lbn: 0x%lx, cur_size: %u\n", cur_lbn, cur_size);
		syscall(RECOVERY_KEY, rec_buf, fid, cur_lbn, cur_size, recovery_time, dev_file);
		memcpy(&cur_offset, rec_buf, 4);
		printf("[RECOVERY_KEY] offset : 0x%x\n", cur_offset);
		
		write_recfile(rec_fd, rec_buf+SSD_PAGE_SIZE, cur_offset, cur_size);
	//	cur_offset += cur_size;	//eval_code

		sect_offset = 0;
		remain_sects -= num_sectors_to_recovery;
		lpn++;
	}
}

static void write_recfile(int rec_fd, char* rec_buf, int offset, int size)
{
	//printf("write recfile: offset/size %u/%u\n", offset, size);
	lseek(rec_fd, offset, SEEK_SET);
	write(rec_fd, rec_buf, size);
}

static int make_recfile(int file_size)
{
	int fd = open("recovery_file1.txt", O_RDWR | O_CREAT, 0); //O_RDONLY, O_WRONLY
	if (fd == -1)
		printf("rec FD ERROR!! \n");
	else
		printf("rec FD SUCCESS!! FD is %d \n", fd);

	ftruncate(fd, file_size);
	return fd;

}

static unsigned long get_start_lba(dev_t dev)
{
	unsigned long start_lba;
	//get start_lba
	struct hd_geometry
	{
		unsigned char heads;
		unsigned char sectors;
		unsigned short cylinders;
		unsigned long start;
	};

	//char dev_file[PATH_MAX]="/dev/sda4";
	//char dev_file[PATH_MAX];
	//get device file name
	find_dev_file(dev, dev_file);
	printf("Device file in %s\n", dev_file);

	int fd = open(dev_file, O_RDONLY | O_NONBLOCK);
	if (fd == -1)
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
	char *dev_dir = "/dev";
	unsigned int major_ = major(dev), minor_ = minor(dev);

	*dev_file = '\0';
	if (!(dp = opendir(dev_dir)))
	{
		printf("device directory open error\n");
	}

	while ((entry = readdir(dp)) != NULL)
	{
		if (entry->d_type == DT_UNKNOWN || entry->d_type == DT_BLK)
		{
			struct stat st;
			sprintf(dev_file, "%s/%s", dev_dir, entry->d_name);
			//printf("%s\n", dev_file);
			if (!stat(dev_file, &st) && S_ISBLK(st.st_mode)) //S_ISBLK : Block device file
			{
				//	printf("%s: %d(%d), %d(%d)\n", dev_file, major_, minor_, major(st.st_rdev), minor(st.st_rdev));
				if ((major_ == (unsigned)major(st.st_rdev)) && (minor_ == (unsigned)minor(st.st_rdev)))
				{
					//	printf("hihi\n");
					closedir(dp);
					return;
				}
			}
		}
	}
}

static void print_buf()
{
	int i, j;

	//printf("lba_list_size: %u\n", buf_size);
	for(i=0; i<lba_num; i++)
		printf("0x%x, %u\n", lba_list[i].lba, lba_list[i].size);
}
