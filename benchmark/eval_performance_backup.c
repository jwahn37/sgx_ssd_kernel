#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<string.h>

//#include "frecovery.h"

#define OPEN_KEY 333
#define CLOSE_KEY 334
#define READ_KEY 335
#define WRITE_KEY 336
#define MAX 1024

#define TOTAL_SIZE 2*1024*1024*1024
#define BUF_SIZE 512
#define NUM_RATIO 5
#define DEFAULT_RATIO 1
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

int main(int *argc, char **argv)
{
    int fd;
    int fd_key;
    int key;
    int close;
    int readn = 0;
    int writen = 0;
    int i=0;
    int ret_time=0;
 	int ret_ratio;
	int file_size;
	int ret_num_file;
	char buf[BUF_SIZE]={'A',};
	char file_name[NAME_LEN]="/home/jinu/SSD/0000000.txt";
	int per_file_size;
	int num_file;
	char file_size_flag;
	int ef;

	//사용자 입력 받기
	//1. 사용자는 일반 파일(RT X)의 비율을 보낸다.
	//2. 사용자는 각 파일의 크기를 보낸다.
	ret_ratio = atoi(argv[1]);//0,20,40,60,80,100
	file_size_flag = *argv[2];

	//1. 파일을 열고 덮어쓰기를 시행한다.
	if(file_size_flag=='B')
	{
		num_file = 5;
		file_size=400*1024*1024;
	}
	else if(file_size_flag=='S')
	{
		num_file = 500*1024;
		file_size = 4*1024;
	}

	ret_ratio /= 20;	//0,1,2,3,4,5로 변형
	for(ef=0; ef<num_file; ef++)
	{
		//파일 이름 설정 
		//file name이 바뀐다.
		printf("before set name\n");
		set_file_name(file_name);
		if(ef%NUM_RATIO < ret_ratio)	//ret없는 비율
		{
			ret_ratio=0;
		}
		else
			ret_ratio=DEFAULT_RATIO;

		printf("file name, retention time: %s, %d\n", file_name, ret_time);
		fd = syscall(OPEN_KEY, file_name, O_RDWR|O_CREAT, 0, ret_time);
	   	if(fd==-1)
		{
			printf("FD ERROR!! \n");
			return 0;
		}
    	else
        	printf("FD SUCCESS!! FD is %d\n",fd_key);
	   //write
	   //file size만큼
		for(i=0; i<file_size/BUF_SIZE; i++)
    		writen = syscall(WRITE_KEY, fd, buf, BUF_SIZE);
   		//printf("WR fd is written with bufW : %s \n",bufW);
    	//printf("WR writen is : %d\n", writen);
    	fsync(fd);
	
		//overwrite1
		lseek(fd, 0, SEEK_SET);
		for(i=0; i<file_size/BUF_SIZE; i++)
    		writen = syscall(WRITE_KEY, fd, buf, BUF_SIZE);
		fsync(fd);
		//overwrite2
		lseek(fd, 0, SEEK_SET);
		for(i=0; i<file_size/BUF_SIZE; i++)
    		writen = syscall(WRITE_KEY, fd, buf, BUF_SIZE);
		fsync(fd);

		//파일을 닫는다.
	  	close=syscall(CLOSE_KEY, fd_key);
   	 	printf("key close file return : %d\n", close);

	}
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
