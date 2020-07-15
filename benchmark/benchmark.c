#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>

//#include "frecovery.h"

#define OPEN_KEY 334
#define CLOSE_KEY 335

#define MAX 1024

int main(int *argc, char **argv)
{
    int fd;
    int fd_key;
    int key;
    int close;
    int readn = 0;
    int writen = 0;
    char bufR[MAX];
    char *bufW = "fklsdjfklsjdfkldsjflkjsdlkf";
    int i = 0;
    char file_path[3][20] = {"/SSD/foo7.txt", "/SSD/foo8.txt", "/SSD/foo9.txt"};
    char bufS[512];

    struct timespec clock_s, clock_e;
    double time_s, time_e;
    srand(time(NULL));
    unsigned int pid, fid;
    //    while(i<3){
    pid = 0x11223344;
    fid = 0x99999999;
    //key=11223344;
    //key file open
    fd_key = syscall(OPEN_KEY, "/home/jinu/SSD/foo7.txt", O_RDWR | O_CREAT, 0, pid, fid);
    //      fd_key = syscall(OPEN_KEY, file_path[i++], O_RDWR|O_CREAT, 0, key);
    //    fd_key = syscall(OPEN_KEY, "foo.txt", O_RDWR, 0, key);
    printf("pid, fid is 0x%x 0x%x\n", pid, fid);
    if (fd_key == -1)
        printf("FD ERROR!! \n");
    else
        printf("FD SUCCESS!! FD is %d\n", fd_key);

    //write
    clock_gettime(CLOCK_MONOTONIC, &clock_s);

    for (i = 0; i < 512; i++)
        bufS[i] = 0x55;

    //	for(i=0; i<64*1024*16; i++)
    for (i = 0; i < 1024 * 16; i++)
   //     for (i=0; i<1; i++)
    {
        ;
    	writen = write(fd_key, bufS, 512);
        //writen = syscall(WRITE_KEY, fd_key, bufS, 512);
        // lseek(fd_key, 0, SEEK_SET);
        //  fsync(fd_key);
    }
    // fsync(fd_key);
    // for (i=0; i<1024*8; i++)
    // //for (i=0; i<1; i++)
    // {
    // 	writen = syscall(WRITE_KEY, fd_key, bufS, 512);
    //    // lseek(fd_key, 0, SEEK_SET);
    //   //  fsync(fd_key);
    // }
    //	writen = syscall(WRITE_KEY, fd_key, bufW, strlen(bufW)+1);
    //   printf("WR fd is written with bufW : %s \n",bufW);
    //   printf("WR writen is : %d\n", writen);

    /*
	lseek(fd_key, 0, SEEK_SET);
    //read
    memset(bufR, 0x00, MAX);
    readn = syscall(READ_KEY, fd_key, bufR, MAX-1);

    printf("RD Buffer size : %d\n",(int)sizeof(bufR));
    printf("RD Readn is : %d\n", readn);
    printf("RD bufR is : %s\n",bufR);
*/
    //    sleep(1);//희한하게 싱크가 전혀 안맞는듯;:
    fsync(fd_key);

    //recovery
    frecovery(fd_key, 3);
    
    close = syscall(CLOSE_KEY, fd_key);
    clock_gettime(CLOCK_MONOTONIC, &clock_e);
    time_s = (clock_s.tv_nsec / 10e9) + clock_s.tv_sec;
    time_e = (clock_e.tv_nsec / 10e9) + clock_e.tv_sec;
    printf("time: %.9lf ~ %.9lf = %.9lf\n", time_s, time_e, time_e - time_s);

    //  printf("key close file return : %d\n", close);
    //  }
    /*
    //file open
    fd = open("./foo.txt", O_RDWR,0); //O_RDONLY, O_WRONLY
    if(fd == -1)
        printf("FD ERROR!! \n");
    else
        printf("FD SUCCESS!! FD is %d \n", fd);

    //write
    writen = write(fd, bufW, strlen(bufW));
    printf("WR fd is writeten with bufW : %s \n",bufW);
    printf("WR writen is : %d\n", writen);
    
    //lseek
    lseek(fd, 0, SEEK_SET);

    //read
    memset(bufR,0x00,MAX);
    readn = read(fd, bufR, MAX-1);

    printf("RD Buffer size : %d\n",(int)sizeof(bufR));
    printf("RD Readn is : %d\n", readn);
    printf("RD bufR is : %s\n",bufR);

    close(fd);
  */

    return 0;
}
