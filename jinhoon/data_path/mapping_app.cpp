#include <iostream>
#include <algorithm>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <map>
#include <vector>
#include <regex>
#include <vector>

const int BUF_MAX_SIZE = 1000;
const int para_MAX_SIZE = 3;
const int para_MAX_LEN = 100; //same as max directory size
#define DIR_LEN 100
#define CONFIG_PID_PATH "/home/jinu/Desktop/config_pid"
#define POLICY_LIST_PATH "/home/jinu/Desktop/policy_list"

//#define CONFIG_PID_PATH "/Users/ppp123/Desktop/config_pid"
//#define POLICY_LIST_PATH "/Users/ppp123/Desktop/policy_list"

typedef struct POLICY_INFO{
    char config[DIR_LEN];
    //std::map<char*, int> fpath_to_fid;
    //don't implement deleted_list
} policy_info;

class PN_DIR{
public:
    std::vector<policy_info*> p_info;
    
    PN_DIR(){
        FILE*fp = fopen(CONFIG_PID_PATH, "r");
        int i = 0, eof = 1;
        char dir[DIR_LEN];
        while (eof != EOF) {
            if(i == 0){
                eof = fscanf(fp, "%s", dir);
            }
            else{
                eof = fscanf(fp, "\n%s", dir);
            }
            if(eof == EOF) return;
            i++;
            policy_info*newInfo = new policy_info;
            strcpy(newInfo->config, dir);
            p_info.push_back(newInfo);
            printf("pid %d normalized %s\n", i, dir);
        }
    }
    
    ~PN_DIR(){
        FILE*fp = fopen(CONFIG_PID_PATH, "w");
        for(policy_info*a : p_info){
            fprintf(fp, "%s\n", a->config);
        }
        
        for(policy_info*a : p_info){
            delete a;
        }
    }
    
    int info_len(){
        return p_info.size();
    }
    
    void add_new_policy(char dir[DIR_LEN]){
        policy_info*newInfo = new policy_info;
        strcpy(newInfo->config, dir);
        p_info.push_back(newInfo);
    }
    
    int fpath_to_pid(char dir[DIR_LEN]){
        //path에 대응되는 정책 리턴
        int i = 0;
        
        for(policy_info*a : p_info){
            char text_for_regex[100];
            char text_for_config[100];
            strcpy(text_for_config, a->config);
            add_slash_to_dot(text_for_config);
            
            strcpy(text_for_regex, "[0-9a-zA-Z]+");
            strcpy(text_for_regex + strlen(text_for_regex), text_for_config);
            const std::regex txt_regex(text_for_regex);
            
            if(std::regex_match(dir, txt_regex)){
                printf("regex match between %s and %s\n", dir, txt_regex);
                return i;
            }
            
            i++;
        }
        return -1;
    }
    
    void add_slash_to_dot(char dir[DIR_LEN]){
        int l = strlen(dir);
        for(int i = l - 1; i >= 0; i--){
            if(dir[i] == '.'){
                char tmp[DIR_LEN * 3];
                strcpy(tmp, &dir[i]);
                dir[i] = '\\';
                stpcpy(&dir[i+1],tmp);
            }
        }
    }
};

typedef struct packet{
    int flag;
    int pid;
} _packet;

typedef struct packet_in{
    int flag;
    char dir[100];
} _packet_in;

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

int main() {
    FILE*fp = fopen(POLICY_LIST_PATH, "r+");
    if(fp == NULL){
        printf("There is no policy file\n");
        return 0;
    }
    
    int policy_cnt = 0;
    
    char policy_tb[10][3];
    int shmid_pid = shmget((key_t)0x1234, sizeof(_packet), IPC_CREAT | 0666);
    _packet*shmaddr_f_to_p = (_packet*)shmat(shmid_pid, NULL, 0);
    
    int shmid_fpth = shmget((key_t)0x1235, sizeof(_packet_fpth), IPC_CREAT | 0666);
    _packet_fpth*shmaddr_fpth = (_packet_fpth*)shmat(shmid_fpth, NULL, 0);
    
    int shmid_in = shmget((key_t)0x1236, sizeof(_packet_in), IPC_CREAT | 0666);
    _packet_in*shmaddr_in = (_packet_in*)shmat(shmid_in, NULL, 0);
    
    while(1){
        int eof, i = 0;
        char line[10];
        while(1){
            char tmp = 0 ;
            eof = fscanf(fp, "%c", &tmp);
            line[i++] = tmp;
            //printf("%c", tmp);
            if(tmp == '\n' || eof == EOF) break;
        }
        if(eof == EOF) break;
        
        policy_tb[policy_cnt][0] = line[0]-'0';
        policy_tb[policy_cnt][1] = line[2]-'0';
        policy_tb[policy_cnt][2] = line[4]-'0';
        
        policy_cnt++;
    }
    fclose(fp);
    
    printf("policy list\n");
    printf("pid   ret_time   backup_cycle\n");
    
    for(int i = 0; i < policy_cnt; i++){
        printf("  %d          %d              %d\n", policy_tb[i][0],policy_tb[i][1],policy_tb[i][2]);
    }
    
    PN_DIR pn_dir = PN_DIR();
    char dir[DIR_LEN];
    
    while(1){
        if(shmaddr_fpth->flag){
            shmaddr_fpth->flag = 0;
            strcpy(dir, shmaddr_fpth->dir);
            if(!strcmp(dir, "exit")){
                break;
            }
            pn_dir.add_new_policy(dir);
            printf("[policy add] policy added pid %d\n", pn_dir.info_len() - 1);
        }
        
        if(shmaddr_in->flag){
            shmaddr_in->flag = 0;
            strcpy(dir, shmaddr_in->dir);
            
            shmaddr_f_to_p->pid = pn_dir.fpath_to_pid(dir);
            shmaddr_f_to_p->flag = 1;
            printf("[policy find] %s is pid \"%d\"\n", dir, pn_dir.fpath_to_pid(dir));
        }
        scanf("%s\n", dir);
        printf("dir : %s\n", dir);
        printf("file path to pid : %d\n", pn_dir.fpath_to_pid(dir));
    }
    
    return 0;
}
