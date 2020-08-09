#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <semaphore.h>
#include <pthread.h>
//#include "frecovery.h"

typedef struct packet{
    int flag;
    int pid;
    int fid;
} _packet;

typedef struct packet_out{
    int flag;
    char dir[100];
} _packet_out;

#define OPEN_KEY 334
#define CLOSE_KEY 335

#define MAX 1024

#define TOTAL_WORKLOAD_SIZE 1000 // 1000MB
#define BIG_FILE_SIZE (200*1024*1024)
#define BUF_SIZE 4096
#define NUM_RATIO 5
#define DEFAULT_RET_TIME 123
#define NAME_LEN 30
#define NAME_END 21
#define NAME_BEGIN 15
void*open_write_key(void*th_num);
int set_file_name(char file_name[NAME_LEN]);
int write_file(int fd, int file_size, char buf[BUF_SIZE], char file_pattern_flag);

sem_t mysem0, mysem1;
int shmid_pid;
int shmid_out;
_packet*shmaddr_f_to_p;
_packet_out*shmaddr_out;

int main(int *argc, char **argv)
{
    shmid_pid = shmget((key_t)0x1234, sizeof(_packet), IPC_CREAT | 0666);
    _packet*shmaddr_f_to_p = (_packet*)shmat(shmid_pid, NULL, 0);
    
    shmid_out = shmget((key_t)0x1236, sizeof(_packet_out), IPC_CREAT | 0666);
    _packet_out*shmaddr_out = (_packet_out*)shmat(shmid_out, NULL, 0);
    
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
    char*in = "/home/jinu/SSD/foo7.txt";
    strcpy(shmaddr_out->dir, in);
    shmaddr_out->flag = 1;
    shmaddr_f_to_p->flag = 0;
    while(!shmaddr_f_to_p->flag);
    pid = shmaddr_f_to_p->pid;
    fid = shmaddr_f_to_p->fid;
    printf("pid : %d, fid : %d\n", pid, fid);
    //key file open
    sem_init(&mysem0, 0, 1);
    sem_init(&mysem1, 0, 1);
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
    //writen = write(fd_key, bufS, 512);
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

int set_file_name(char file_name[NAME_LEN])
{
    int i;
    
    file_name[NAME_END]++;
    for(i=NAME_END; i>NAME_BEGIN; i--)
    {
        if(file_name[i]>'9' && i != NAME_BEGIN+1)
        {
            file_name[i]='0';
            file_name[i-1]++;
        }
    }
    
    return 1;
}

void*open_write_key(void*th_num){
    int i;
    printf("th_num : %c", *(char*)th_num);
    char file_name[NAME_LEN]="/home/jinu/SSD/0000000";

    for(i = 0; i < num_total_files; i++)
    {
        file_name[15] = *(char*)th_num;
        if('9' < (char*)th_num){
            file_name[15] = 'A' + (char*)th_num - '9' + 1;
        }
        //파일 이름 설정
        //file name이 바뀐다.
        set_file_name(file_name);
         // printf("key close file return : %d\n", err);
        //printf("A, %c\n", *(char*)th_num);
        sem_wait(&mysem0);
        //printf("B, %c\n", *(char*)th_num);
        strcpy(shmaddr_out->dir, file_name);
        shmaddr_out->flag = 1;
        
            
        shmaddr_f_to_p->flag = 0;
        while(!shmaddr_f_to_p->flag);
        int pid = shmaddr_f_to_p->pid;
        sem_post(&mysem0);
            
        int fd = write(file_name, O_RDWR|O_CREAT, 0, pid);
        if(fd == -1)
        {
            printf("FD ERROR!! \n");
            return 0;
        }

        write_file(fd, file_size, buf, 'S');
        close(fd);
    }
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
