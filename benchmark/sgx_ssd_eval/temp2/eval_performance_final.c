#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<string.h>
#include<time.h>
//#include "frecovery.h"

#define TOTAL_WORKLOAD_SIZE 1000 // 1000MB
#define OPEN_KEY 333
#define CLOSE_KEY 334
#define READ_KEY 335
#define WRITE_KEY 336
#define MAX 1024

#define BIG_FILE_SIZE (200*1024*1024)
#define BUF_SIZE 4096
#define NUM_RATIO 5
#define DEFAULT_RET_TIME 123
#define NAME_LEN 30
#define NAME_END 21
#define NAME_BEGIN 15
/*
 *	실험1. 성능 테스트.
 *	파일 타입 : retention time O(default), retention time X
 *	파일의 비율을 조정하며 테스트한다. 
 *	X:O= {100:0, 80:20, 60:40, 40:60, 20:80, 0:100}
 *	100:0은 일반 SSD, 0:100은 알마낙임
 *	파일의 총 size는 2GB
 *	각 파일 size는 400MB*5, 4KB*500*1024(50만개)
 *
 *
 * 사용자 입력:
 * 파일의 비율, 파일의 size, ...
 * */

int set_file_name(char file_name[NAME_LEN]);
int write_file(int fd, int file_size, char buf[BUF_SIZE], char file_pattern_flag);

//const char INITIAL_FILE_NAME[NAME_LEN] = "/home/jinu/SSD/0000000.txt";

int main(int argc, char **argv)
{
	int fd;
	int err=0;
	int i=0;
	long int j = 0;
	int ret_time, ret_ratio, capacity;
	char buf[BUF_SIZE]={'X',};
	char file_name[NAME_LEN]="/home/jinu/SSD/0000000.txt";
	char init_file_name[NAME_LEN]="/home/jinu/SSD/0000000.txt";
	char init2_file_name[NAME_LEN]="/home/jinu/SSD/0000000.txt";
	
	struct timespec clock_s, clock_e;
	double time_s, time_e;
	
	//1. 사용자는 보호하고자하는 파일의(RT X)의 비율을 보낸다.
	//2. 사용자는 각 파일의 크기를 보낸다.
	capacity = atoi(argv[1]); //0,20,40,60,80,100
	ret_ratio = atoi(argv[2]);

	
	//int num_total_files = 50; // num_total_files * file_size = 1GB
	int num_total_files = 50 * 5 * 1024 / 8; // num_total_files * file_size = 1GB
	int file_size = 32 * 1024;

	int num_capacity_files = capacity * num_total_files / 100;
	int num_init_invalid_files = num_total_files - num_capacity_files; 

	printf("capacity file num : %d, invalid file num : %d, retention file num : %d\n\n\n", num_capacity_files, num_init_invalid_files, num_capacity_files * ret_ratio / 100);
	srand(time(NULL));
	
	printf("*** Initialization ***\n");
	printf("Write %dMB file with no RT and %dMB files with RT\n\n\n", TOTAL_WORKLOAD_SIZE - TOTAL_WORKLOAD_SIZE * ret_ratio / 100, TOTAL_WORKLOAD_SIZE * ret_ratio / 100);

	ret_ratio /= 20;

	for (i=0; i < num_total_files; i++)
	{
	
		if (i < num_init_invalid_files)
		{
			if ( capacity / 20 == 1 && i % num_capacity_files == 0)
			{
//				printf("ret_ratio : %d, i %d, num_capacity_files : %d\n\n\n", ret_ratio, i, num_capacity_files);
				sprintf(init_file_name, "/home/jinu/SSD/0000000.txt");
			}
			set_file_name(init_file_name);
			
//			printf("init !! file name, retention time: %s, %d\n", init_file_name, 0);
		
			fd = syscall(OPEN_KEY, init_file_name, O_RDWR|O_CREAT, 0, 0);
			if(fd==-1)
			{
				printf("FD ERROR!! \n");
				return 0;
			}
// 		else
//    			printf("FD SUCCESS!! FD is %d\n",fd);
			
			write_file(fd, file_size, buf, 'S');
			err = syscall(CLOSE_KEY, fd);
	 	//    printf("key close file return : %d\n", err);
		}
		else
		{
			set_file_name(init2_file_name);

			if( (i-num_init_invalid_files) % NUM_RATIO >= ret_ratio)
				ret_time=0;
			else
				ret_time=DEFAULT_RET_TIME;

//			printf("fd[%d] : file name, retention time: %s, %d\n", i-num_init_invalid_files, init2_file_name, ret_time);			
			fd = syscall(OPEN_KEY, init2_file_name, O_RDWR|O_CREAT, 0, ret_time);
			if(fd == -1)
			{
				printf("FD ERROR!! \n");
				return 0;
			}

				
			write_file(fd, file_size, buf, 'S');
			err = syscall(CLOSE_KEY, fd);
		}
	}

	printf("start experiment!\n");
	clock_gettime(CLOCK_MONOTONIC, &clock_s);

	for(i = 0; i < num_capacity_files; i++)
	{
		//파일 이름 설정 
		//file name이 바뀐다.
		set_file_name(file_name);
		if(i % NUM_RATIO >= ret_ratio)	
			ret_time=0;
		else
			ret_time=DEFAULT_RET_TIME;

//		printf("fd[%d], file name, retention time: %s, %d\n", i, file_name, ret_time);
		fd = syscall(OPEN_KEY, init2_file_name, O_RDWR|O_CREAT, 0, ret_time);
			if(fd == -1)
			{
				printf("FD ERROR!! \n");
				return 0;
			}

		write_file(fd, file_size, buf, 'S');
		err = syscall(CLOSE_KEY, fd);
	 	// printf("key close file return : %d\n", err);
	}
	
	clock_gettime(CLOCK_MONOTONIC, &clock_e);
	time_s = (clock_s.tv_nsec/10e9) + clock_s.tv_sec;
	time_e = (clock_e.tv_nsec/10e9) + clock_e.tv_sec;
	printf("time: %.9lf ~ %.9lf = %.9lf\n", time_s, time_e, time_e-time_s);
}

int set_file_name(char file_name[NAME_LEN])
{
	int i;
	
	file_name[NAME_END]++;
	for(i=NAME_END; i>NAME_BEGIN; i--)
	{
		if(file_name[i]>'9')
		{
			file_name[i]='0';
			file_name[i-1]++;
		}
	}
	
	return 1;
}

int write_file(int fd, int file_size, char buf[BUF_SIZE], char file_pattern_flag)
{
	int i;
	int err;
	int random_offset;

	lseek(fd, 0, SEEK_SET);

	//sequential write 수행
	if(file_pattern_flag=='S')
	{
		for(i=0; i<file_size/BUF_SIZE; i++)
    		err = syscall(WRITE_KEY, fd, buf, BUF_SIZE);
	}

	fdatasync(fd);

	return err;
}
