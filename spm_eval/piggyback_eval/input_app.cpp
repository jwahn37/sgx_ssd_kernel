#include <iostream>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

const int BUF_MAX_SIZE = 1000;
const int para_MAX_SIZE = 3;
const int para_MAX_LEN = 100; //same as max directory size

typedef struct packet_fpth{
    int flag;
    int pid;
    char dir[100];
} _packet_fpth;

int get_fileID(char*in){
    int len = 0;
    int ret = 0;
    for(; in[len] != '\0'; len++){
        ret += in[len];
    }
    return ret % 100;
}

void line_input(char in[BUF_MAX_SIZE]){
    char buf = 0;
    for(int i = 0; 1; i++){
        scanf("%c", &buf);
        in[i] = buf;
        if(buf == '\n'){
            in[i] = 0;
            break;
        }
        else if(i == BUF_MAX_SIZE && buf != '\n'){
            //buffer size error
            fprintf(stderr, "[error] MAX buffer size is %d\n", BUF_MAX_SIZE);
        }
    }
}

void parse_str(char in[BUF_MAX_SIZE], char out[para_MAX_SIZE][para_MAX_LEN]){
    char tmp[BUF_MAX_SIZE];
    int len = 0, para_size = 0, j = 0;
    //assume no space allows in first and last "in[]" components
    for(int i = 0; i < BUF_MAX_SIZE && in[i] != 0; i++){
        while(in[i] == ' ') i++;
        if(len != 0) tmp[len++] = ' ';
        while(in[i] != ' ' && in[i] != 0) {
            tmp[len++] = in[i];
            i++;
        }
    }
    
    for(int i = 0; i < len; i++){
        j = 0;
        while(tmp[i] != ' ' && i < len){
            out[para_size][j++] = tmp[i++];
        }
        out[para_size++][j] = 0;
    }
}

int main() {
    int shmid_fpth = shmget((key_t)0x1235, sizeof(_packet_fpth), IPC_CREAT | 0666);
    _packet_fpth*shmaddr_fpth = (_packet_fpth*)shmat(shmid_fpth, NULL, 0);
    
    printf("enter the *.something\n");
    printf("\n");
    printf("\n");
    printf("[enter exit to exit]\n");
    
    while(1){
        char in[1000];
        line_input(in);
        
        char para_arr[para_MAX_SIZE][para_MAX_LEN]; //save parameters
        parse_str(in, para_arr);
        
        printf("parameter lists : \n");
        for(int i = 0; i < para_MAX_SIZE; i++){
            printf("%s\n", para_arr[i]);
        }
        
        //shmaddr_fpth->pid = para_arr[0][0] - '0';
        strcpy(shmaddr_fpth->dir, para_arr[0]);
        shmaddr_fpth->flag = 1;
        
        if(!strcmp("exit", para_arr[0])) break;
    }
    
    return 0;
}
