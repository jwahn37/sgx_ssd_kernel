#include <iostream>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

typedef struct packet{
    int flag;
    int pid;
} _packet;

typedef struct packet_out{
    int flag;
    char dir[100];
} _packet_out;

int main() {
    int shmid_pid = shmget((key_t)0x1234, sizeof(_packet), IPC_CREAT | 0666);
    _packet*shmaddr_f_to_p = (_packet*)shmat(shmid_pid, NULL, 0);
    
    int shmid_out = shmget((key_t)0x1236, sizeof(_packet_out), IPC_CREAT | 0666);
    _packet_out*shmaddr_out = (_packet_out*)shmat(shmid_out, NULL, 0);
    
    char*in = "apid.pdf";
    strcpy(shmaddr_out->dir, in);
    shmaddr_out->flag = 1;
    shmaddr_f_to_p->flag = 0;
    while(!shmaddr_f_to_p->flag);
    printf("pid : %d\n", shmaddr_f_to_p->pid);
    
    return 0;
}
