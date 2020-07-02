#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<string.h>

#define OPEN_KEY 332
#define CLOSE_KEY 333
#define READ_KEY 334
#define WRITE_KEY 335
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
    char *bufW = "fsdlkjflksdjflksdjflksdjflksjdflkjsdflk";
//    char *bufW = "IamRANSOMWARE!!!HAHAHAHAHAHA";
    int i=0;
    char file_path[3][20] = {"/mount_ssd/foo7.txt", "/mount_ssd/foo8.txt", "/mount_ssd/9.txt"};

//    while(i<3){
        key= 0x11223344;
    //key=11223344;
    //key file open
  fd_key = syscall(OPEN_KEY, "/mount_ssd/dong.txt", O_RDWR|O_CREAT, 0, key);
//    fd_key = open("/mount_ssd/dong.txt", O_RDWR|O_CREAT, 0);
//      fd_key = syscall(OPEN_KEY, file_path[i++], O_RDWR|O_CREAT, 0, key);
//    fd_key = syscall(OPEN_KEY, "foo.txt", O_RDWR, 0, key);
        printf("key is %x\n",key);
    if(fd_key==-1)
            printf("FD ERROR!! \n");
    else
            printf("FD SUCCESS!! FD is %d\n",fd_key);

    //write
 
/*   
    writen = syscall(WRITE_KEY, fd_key, bufW, strlen(bufW));
    printf("WR fd is written with bufW : %s \n",bufW);
    printf("WR writen is : %d\n", writen);

    fsync(fd_key);
 */  
    
    //overwrite
/*    lseek(fd_key, (off_t)0, SEEK_SET);
    writen = syscall(WRITE_KEY, fd_key, bufW, strlen(bufW));
    printf("WR fd is written with bufW : %s \n",bufW);
    printf("WR writen is : %d\n", writen);

    fsync(fd_key);
  */  
    //read

    lseek(fd_key, (off_t)0, SEEK_SET);
    memset(bufR, 0x00, MAX);
    readn = syscall(READ_KEY, fd_key, bufR, MAX);

    printf("RD Buffer size : %d\n",(int)sizeof(bufR));
    printf("RD Readn is : %d\n", readn);
    printf("RD bufR is : %s\n",bufR);
    printf("%c%c%c\n",bufR[0],bufR[1],bufR[2]);

    //    sleep(1);//희한하게 싱크가 전혀 안맞는듯;:
  
    close=syscall(CLOSE_KEY, fd_key);
    printf("key close file return : %d\n", close);

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
