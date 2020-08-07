#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <semaphore.h>
#include <fcntl.h>

typedef struct packet{
    int flag;
    int pid;
    int fid;
} _packet;

typedef struct packet_out{
    int flag;
    char dir[100];
} _packet_out;

sem_t mysem0, mysem1;
int shmid_pid;
int shmid_out;
_packet*shmaddr_f_to_p;
_packet_out*shmaddr_out;

#define DIRRECTORY "/home/jinu/SSD2/eval"
#define dstDIR "/home/jinu/SSD/eval"
#define BUF_SIZE 4096

char buf[BUF_SIZE];

struct stat st = {0};

void dfs(char*currPath, char*currDstPath){
    struct dirent *de;  // Pointer for directory entry
    DIR *dr = opendir(currPath);// opendir() returns a pointer of DIR type.
    if (dr == NULL)  // opendir returns NULL if couldn't open directory
    {
        printf("Could not open current directory" );
        return;
    }
    char srcPath[200];
    char dstPath[200];
    while ((de = readdir(dr)) != NULL)
    {
        if (de->d_type == DT_REG){
            //printf("%s\n", de->d_name);
            int len = strlen(currPath);
            memcpy(srcPath, currPath, len);
            memcpy(&srcPath[len], "/", 1);
            memcpy(&srcPath[len+1], de->d_name, strlen(de->d_name));
            memcpy(&srcPath[len+1 + strlen(de->d_name)], "\0", 1);
            //printf("src path : %s\n", srcPath);
            
            len = strlen(currDstPath);
            memcpy(dstPath, currDstPath, len);
            memcpy(&dstPath[len], "/", 1);
            memcpy(&dstPath[len+1], de->d_name, strlen(de->d_name));
            memcpy(&dstPath[len+1 + strlen(de->d_name)], "\0", 1);
            //printf("dst path : %s\n", dstPath);
            //  sem_wait(&mysem0);
            //  strcpy(shmaddr_out->dir, dstPath);
            //  shmaddr_out->flag = 1;
             
            //  shmaddr_f_to_p->flag = 0;
            //  while(!shmaddr_f_to_p->flag);
            //  int pid = shmaddr_f_to_p->pid;
            //  pid = 0x11223344;
            //  int fid = shmaddr_f_to_p->fid;
            //  sem_post(&mysem0);
            
            
            int srcFd = open(srcPath, O_RDWR|O_CREAT);
            int dstFd = open(dstPath, O_RDWR|O_CREAT);
            //int dstFd = syscall(334, dstPath, O_RDWR|O_CREAT, 0, pid ,fid);
            long long int file_size =  lseek(srcFd, 0, SEEK_END) + 1;
            lseek(srcFd, 0, SEEK_SET);
            lseek(dstFd, 0, SEEK_SET);
            
            for(int i=0; i < file_size/BUF_SIZE; i++){
                read(srcFd, buf, BUF_SIZE);
                write(dstFd, buf, BUF_SIZE);
            }
            if(file_size % BUF_SIZE > 0){
                read(srcFd, buf, file_size % BUF_SIZE);
                write(dstFd, buf, file_size % BUF_SIZE);
            }
	    //fsync(dstFd);
            close(srcFd);
            //syscall(335, dstFd);
            close(dstFd);
        }
        else if (de->d_type == DT_DIR && strcmp(de->d_name, ".") &&  strcmp(de->d_name, "..")){
            char nextPath[200];
            char nextDstPath[200];
            
            int len = strlen(currPath);
            memcpy(nextPath, currPath, len);
            memcpy(&nextPath[len], "/", 1);
            memcpy(&nextPath[len+1], de->d_name, strlen(de->d_name));
            memcpy(&nextPath[len+1 + strlen(de->d_name)], "\0", 1);
            //printf("nextPath : %s\n", nextPath);
            
            len = strlen(currDstPath);
            memcpy(nextDstPath, currDstPath, len);
            memcpy(&nextDstPath[len], "/", 1);
            memcpy(&nextDstPath[len+1], de->d_name, strlen(de->d_name));
            memcpy(&nextDstPath[len+1 + strlen(de->d_name)], "\0", 1);
            //printf("nextDstPath : %s\n", nextDstPath);
            
            if (stat(nextDstPath, &st) == -1) {
                mkdir(nextDstPath, 0700);
            }
            
            dfs(nextPath, nextDstPath);
        }
    }
    closedir(dr);
}

int main(void)
{
    // shmid_pid = shmget((key_t)0x1237, sizeof(_packet), IPC_CREAT | 0666);
    // shmaddr_f_to_p = (_packet*)shmat(shmid_pid, NULL, 0);
    
    // shmid_out = shmget((key_t)0x1236, sizeof(_packet_out), IPC_CREAT | 0666);
    // shmaddr_out = (_packet_out*)shmat(shmid_out, NULL, 0);
    
    
    sem_init(&mysem0, 0, 1);
    dfs(DIRRECTORY, dstDIR);
   
    sync(); 
    return 0;
}
