#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<string.h>
#include<time.h>
//#include "frecovery.h"

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

// 
int main(int argc, char **argv)
{
    int fd;
    int key;
	int err=0;
    int i=0;
	long int j = 0;
    int ret_time=0;
 	int ret_ratio;
	int file_size;
	int ret_num_file;
	char buf[BUF_SIZE]={'A',};
	char file_name[NAME_LEN]="/home/jinu/SSD/0000000.txt";
	char init_file_name[NAME_LEN]="/home/jinu/SSD/0000000.txt";
	char init2_file_name[NAME_LEN]="/home/jinu/SSD/0000000.txt";
	int per_file_size;
	int num_file;
	char file_size_flag, file_pattern_flag;
	int ef;
	struct timespec clock_s, clock_e;
	double time_s, time_e;


	srand(time(NULL));
	//사용자 입력 받기
	//1. 사용자는 일반 파일(RT X)의 비율을 보낸다.
	//2. 사용자는 각 파일의 크기를 보낸다.
	ret_ratio = atoi(argv[1]);//0,20,40,60,80,100
	file_size_flag = *argv[2];
	//file_pattern_flag = *argv[3];
	
	printf("Write and overwrite a 32GB file once to fill up the disk\n");

	for (i=0; i<20; i++)
	{
		set_file_name(init_file_name);
		printf("file name, retention time: %s, %d\n", init_file_name, 73);
		fd = syscall(OPEN_KEY, init_file_name, O_RDWR|O_CREAT, 0, 73);
		
		write_file(fd, BIG_FILE_SIZE, buf, 'S');
		
		err = syscall(CLOSE_KEY, fd);

		if(err == -1)
		{
			printf("error! here2\n");
			return -1;
		}
	}

	printf("start experiment!\n");
	
	for (i=0; i<20; i++)
	{
		set_file_name(init2_file_name);
		printf("file name, retention time: %s, %d\n", init2_file_name, 73);
		fd = syscall(OPEN_KEY, init2_file_name, O_RDWR|O_CREAT, 0, 73);
		
		write_file(fd, BIG_FILE_SIZE, buf, 'S');
		
		err = syscall(CLOSE_KEY, fd);

		if(err == -1)
		{
			printf("error! here2\n");
			return -1;
		}
	}	

	///// EXPERIMENT START!!!
	if(file_size_flag=='B')
	{
		num_file = 4*5;
		file_size=200*1024*1024;
	}
		else if(file_size_flag=='S')
	{
		num_file = 200 * 5 * 1024;
		file_size = 4 * 1024;
	}

	printf("initalizing: ret_ratio %d, file_size_flag: %c, num_file: %d, file_size: %d\n", ret_ratio, file_size_flag, num_file, file_size);

	ret_ratio /= 20;	//0,1,2,3,4,5로 변형

	//시간을 받아온다.
	clock_gettime(CLOCK_MONOTONIC, &clock_s);

	//1. 파일을 열고 덮어쓰기를 시행한다.
	for(ef=0; ef<num_file; ef++)
	{
		//파일 이름 설정 
		//file name이 바뀐다.
		set_file_name(file_name);
		if(ef%NUM_RATIO < ret_ratio)	//ret없는 비율
		{
			ret_time=0;
		}
		else
			ret_time=DEFAULT_RET_TIME;

		printf("file name, retention time: %s, %d\n", file_name, ret_time);
		fd = syscall(OPEN_KEY, file_name, O_RDWR|O_CREAT, 0, ret_time);
	   	if(fd==-1)
		{
			printf("FD ERROR!! \n");
			return 0;
		}
	  	else
    		printf("FD SUCCESS!! FD is %d\n",fd);
	    
		// Initial write
		write_file(fd, file_size, buf, 'S');
	
		// overwrite
		write_file(fd, file_size, buf, 'S');
	
		//파일을 닫는다.
	  	err=syscall(CLOSE_KEY, fd);
   	 	printf("key close file return : %d\n", err);

	}

	//시간을 계산한다.
	clock_gettime(CLOCK_MONOTONIC, &clock_e);
	time_s = (clock_s.tv_nsec/10e9) + clock_s.tv_sec;
	time_e = (clock_e.tv_nsec/10e9) + clock_e.tv_sec;
	printf("time: %.9lf ~ %.9lf = %.9lf\n", time_s, time_e, time_e-time_s);
    return 0; 
}

// /SSD/0000000.txt
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

	fsync(fd);

	return err;
}
