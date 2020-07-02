#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<string.h>

#define MAX 1024
//int ii;
int main(int *argc, char **argv)
{
    int fd;
    int readn = 0;
    int writen = 0;
    char bufR[MAX];
    char *bufW = "hello my name is jinwoo!!";
    int i;

    //file open
   // fd = open("/mount_ssd/foo.txt", O_RDWR,0); //O_RDONLY, O_WRONLY
    fd = open("/mount_ssd/foo.txt", O_RDWR | O_CREAT,0); //O_RDONLY, O_WRONLY
    if(fd == -1)
        printf("FD ERROR!! \n");
    else
        printf("FD SUCCESS!! FD is %d \n", fd);

    //write
    writen = write(fd, bufW, strlen(bufW));
    printf("WR fd is writeten with bufW : %s \n",bufW);
    printf("WR writen is : %d\n", writen);
    
    fdatasync(fd);
    //lseek
    lseek(fd, 0, SEEK_SET);

    //read
    memset(bufR,0x00,MAX);
    readn = read(fd, bufR, MAX-1);

    printf("RD Buffer size : %d\n",(int)sizeof(bufR));
    printf("RD Readn is : %d\n", readn);
    printf("RD bufR is : %s\n",bufR);

    close(fd);
       
    return 0; 


}
