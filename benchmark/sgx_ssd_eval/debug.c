#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

#define OPEN_KEY 333
#define CLOSE_KEY 334
#define READ_KEY 335
#define WRITE_KEY 336

#define BUF_SIZE (4*1024)
#define VPN_SIZE (32*1024)
#define PPN_SIZE (4*1024)

int write_file(int fd, int file_size, char buf[BUF_SIZE]);

int main(){

	
	int i, x;
	int fd;
	char buf[BUF_SIZE] = {'A'};

	while(1)
	{
		printf("Enter the command\n");
		printf("0 : 32KB File(file0.txt) Write\n");
		printf("1 : 4KB File(file1.txt) Write\n");
//		printf("2 : Information about what??\n");
		printf("9 : QUIT\n");
		scanf("%d", &x);
		

		switch(x){
		
			case 0:
				fd = syscall(OPEN_KEY, "/home/jinu/SSD/file0.txt", O_RDWR|O_CREAT, 0, 35);
				write_file(fd, VPN_SIZE, buf);
				printf("close key return : %ld\n", syscall(CLOSE_KEY, fd));
				break;
			case 1:
				fd = syscall(OPEN_KEY, "/home/jinu/SSD/file1.txt", O_RDWR|O_CREAT, 0, 35);
				write_file(fd, VPN_SIZE, buf);
				printf("close key return : %ld\n", syscall(CLOSE_KEY, fd));
				break;
			case 2:
				fd = syscall(OPEN_KEY, "/home/jinu/SSD/file3.txt", O_RDWR|O_CREAT, 0, 35);
				write_file(fd, VPN_SIZE, buf);
				printf("close key return : %ld\n", syscall(CLOSE_KEY, fd));
				break;
			case 9: 
				exit(-1);
			default:
				break;
		}	
	}
}

int write_file(int fd, int file_size, char buf[BUF_SIZE])
{
	int i, err, random_offset;

	lseek(fd, 0, SEEK_SET);
	
	for (i=0; i<file_size/BUF_SIZE; i++)
	{
		printf("%dth write\n", i);
		err = syscall(WRITE_KEY, fd, buf, BUF_SIZE);
	}

	fsync(fd);

	return err;
}

