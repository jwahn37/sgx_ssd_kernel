#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<string.h>
#include<time.h>

#define MAX 1024
//int ii;
int main(int *argc, char **argv)
{
    int fd;
    int readn = 0;
    int writen = 0;
    char bufR[MAX];
    char *bufW = "hello my name is jinwoo!!";
    char bufS[512];
    int i;

    struct timespec clock_s, clock_e;
	double time_s, time_e;
    srand(time(NULL));
    //file open
   // fd = open("/mount_ssd/foo.txt", O_RDWR,0); //O_RDONLY, O_WRONLY
    fd = open("/home/jinu/SSD/foo.txt", O_RDWR | O_CREAT,0); //O_RDONLY, O_WRONLY
    if(fd == -1)
        printf("FD ERROR!! \n");
    else
        printf("FD SUCCESS!! FD is %d \n", fd);

    clock_gettime(CLOCK_MONOTONIC, &clock_s);

    //write
    for(i=0; i<512; i++)
	//	bufS[i]=i%256+1;
		bufS[i]=0x55;
	for(i=0; i<64*1024*16; i++)
    //for(i=0; i<1024*16; i++)
    	writen = write(fd, bufS, 512);

   // printf("WR fd is writeten with bufW : %s \n",bufW);
   // printf("WR writen is : %d\n", writen);
    fsync(fd);
   /* 
    //lseek
    lseek(fd, 0, SEEK_SET);

    //read
    memset(bufR,0x00,MAX);
    readn = read(fd, bufR, MAX-1);

    printf("RD Buffer size : %d\n",(int)sizeof(bufR));
    printf("RD Readn is : %d\n", readn);
    printf("RD bufR is : %s\n",bufR);
    */
    close(fd);
     clock_gettime(CLOCK_MONOTONIC, &clock_e);
	time_s = (clock_s.tv_nsec/10e9) + clock_s.tv_sec;
	time_e = (clock_e.tv_nsec/10e9) + clock_e.tv_sec;
	printf("time: %.9lf ~ %.9lf = %.9lf\n", time_s, time_e, time_e-time_s);
    

    return 0; 
}
