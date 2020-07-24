/*
 * Copyright (C) 2011-2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <pwd.h>
#define MAX_PATH FILENAME_MAX
#include "sgx_trts.h"
#include "sgx_tseal.h"
#include "sgx_urts.h"
#include "App.h"
#include "Enclave_u.h"

const int BUF_MAX_SIZE = 1000;
const int para_MAX_SIZE = 3;
const int para_MAX_LEN = 100; //same as max directory size
#define __NR_enc_rdafwr 333

#define SECTOR_SIZE 512
#define SECTOR_BIT 9
#define KEY_SIZE 16
#define MAC_SIZE (KEY_SIZE * 2)
#define VERSION_SIZE 4
#define FD_SIZE 4

#define P_SIZE 4096
#define K_SIZE 1024
#define IO_SIZE 8192
#define RESPONSE_SIZE SECTOR_SIZE
#define PAGE_BIT 12
#define NAME_LEN 16
#define NODE_SIZE 4096
//DS_param, ds_cmd는 개발에 따라 정책이 수정될 수 있다.
//fs_open.c와 openssd jasmine펌웨어 sata_table.c, sata.h 와 반드시 맞춰줄것.
typedef struct DS_param
{
    unsigned int fd;
    unsigned char cmd;
    unsigned long offset; //여기가 LBA영역에 들어감 6bytes
    unsigned int size;    //이건 lba처럼 count영역에 들어가니, 섹터단위일듯.
    unsigned int ret_time;
} DS_PARAM;

enum ds_cmd
{
    DS_WR_RANGE_MIN = 0x43,
    DS_CREATE_WR = 0x44,
    DS_OPEN_WR = 0x45,
    DS_CLOSE_WR = 0x46,
    DS_REMOVE_WR = 0x47,
    DS_RITE_WR = 0x48,
    DS_WR_RANGE_MAX = 0x49,
    DS_RD_RANGE_MIN = 0x4A,
    DS_READ_RD = 0x4B,
    DS_AUTH_RD = 0x4C,
    DS_CREATE_RD = 0x4D,
    DS_OPEN_RD = 0x4E,
    DS_CLOSE_RD = 0x4F,
    DS_REMOVE_RD = 0x50,
    DS_WRITE_RD = 0x51,
    DS_RD_RANGE_MAX = 0x52
};

enum spm_cmd
{
    SPM_CREATE = 101,
    SPM_CHANGE,
    SPM_DELETE,
    SPM_RECOVERY
};

int cntw = 0;
double timewr = 0;

typedef struct PM_param
{
    //	unsigned int fd;
    unsigned char cmd;
    //	unsigned long offset; //여기가 LBA영역에 들어감 6bytes
    unsigned int size; //이건 lba처럼 count영역에 들어가니, 섹터단위일듯.
                       //	unsigned int ret_time;
} PM_PARAM;

//8KB가 넘으면, 쪼개주는정도만..아닌가.....
//넘으면 mac다시계산해야할거같은데.
//얘를 호출할당시부터 8KB쪼개졌다가정 즉 u_buf사이즈 <=8KB
int enc_rdafwr(DS_PARAM *ds_param, char *u_buf, char *response, int count)
{
    uint32_t response_size = 40;

    //direct I/O 할 버퍼에 저장.
    if (((uint64_t)u_buf & 0x01ff) != 0) //sector단위라 마지막 9bit 가 0이어야함.
    {
        fprintf(stdout, "Error wrong I/O buffer address\n");
    }

    PM_PARAM pm_param;
    pm_param.cmd = ds_param->cmd;
    pm_param.size = ds_param->size;
    //char *response = (char*) c (sizeof(char)*RESPONSE_SIZE);
    int ret;
     //실험용printf("syscall!\n");
    ret = (int)syscall(__NR_enc_rdafwr, &pm_param, u_buf, count);

    return ret;
}

//fd는 0으로
//이건 ocall이라 packing만 해주는 함수라고 생각해야함
//fileID는 write할때 필요한거라 여기에선 필요가 없음
int spm_send_cmd(int fd, char *buffer, int node_size, char *response, int pid, int spm_param[4])
{
    uint64_t offset = 0;
    uint32_t ret_msg;
    uint32_t file_size = node_size;
    uint32_t reserved;
    DS_PARAM *ds_param = (DS_PARAM *)malloc(sizeof(DS_PARAM));
    char *u_buf_ = (char *)malloc(sizeof(char) * (IO_SIZE + SECTOR_SIZE)); //IO_SIZE??
    char *u_buf = (char *)((((unsigned long)u_buf_ + SECTOR_SIZE - 1) >> SECTOR_BIT) << SECTOR_BIT);

    int cmd = ds_param->cmd = spm_param[3];
    ds_param->fd = fd;
    ds_param->offset = offset;

    memcpy((char *)u_buf, buffer, file_size);

    if ((reserved = file_size % 512) != 0) //섹터단위로 안나뉘면 dummy값넣어줘야
    {
        fprintf(stdout, "never be called!!!\n");
        memset((char *)(u_buf + file_size), 0x00, 512 - reserved);
        file_size += (512 - reserved);
    }

    //cmd는 register fis로 보낼거
    spm_param[2] = 10;
    switch (cmd)
    {
    case SPM_CREATE:
        //memcpy((char*)(u_buf+file_size), (char*)(&sp->pid), 4);
        memcpy((char *)(u_buf + file_size), (char *)(&pid), 4);
        memcpy((char *)(u_buf + file_size + 4), (char *)(&spm_param[0]), 4);  //ret
        memcpy((char *)(u_buf + file_size + 8), (char *)(&spm_param[1]), 4);  //backup cycle
        memcpy((char *)(u_buf + file_size + 12), (char *)(&spm_param[2]), 4); //version num
        ds_param->size = (((node_size + 16) + SECTOR_SIZE - 1) >> SECTOR_BIT) << SECTOR_BIT;
        break;
    case SPM_DELETE:
    case SPM_CHANGE:
        //pid의 정책이 파라미터들로 바뀜
        //memcpy((char*)(u_buf+file_size), (char*)(&sp->pid), 4);
        memcpy((char *)(u_buf + file_size), (char *)(&pid), 4);
        memcpy((char *)(u_buf + file_size + 4), (char *)(&spm_param[0]), 4);  //ret
        memcpy((char *)(u_buf + file_size + 8), (char *)(&spm_param[1]), 4);  //backup cycle
        memcpy((char *)(u_buf + file_size + 12), (char *)(&spm_param[2]), 4); //version num
        ds_param->size = (((node_size + 16) + SECTOR_SIZE - 1) >> SECTOR_BIT) << SECTOR_BIT;
        break;
    default:
        printf("[APP] Invalid SPM command\n");
        return -1;
        break;
    }
     //실험용printf("cmd %d, ret %d, backup %d\n", cmd, spm_param[0], spm_param[1]);
    enc_rdafwr(ds_param, u_buf, response, ds_param->size);
    free(u_buf_);
    free(ds_param);
    return 0;
}

void line_input(char in[BUF_MAX_SIZE])
{
    char buf = 0;
    for (int i = 0; 1; i++)
    {
        scanf("%c", &buf);
        in[i] = buf;
        if (buf == '\n')
        {
            in[i] = 0;
            break;
        }
        else if (i == BUF_MAX_SIZE && buf != '\n')
        {
            //buffer size error
            fprintf(stderr, "[error] MAX buffer size is %d\n", BUF_MAX_SIZE);
        }
    }
}

void parse_str(char in[BUF_MAX_SIZE], char out[para_MAX_SIZE][para_MAX_LEN])
{
    char tmp[BUF_MAX_SIZE];
    int len = 0, para_size = 0, j = 0;
    //assume no space allows in first and last "in[]" components
    for (int i = 0; i < BUF_MAX_SIZE && in[i] != 0; i++)
    {
        while (in[i] == ' ')
            i++;
        if (len != 0)
            tmp[len++] = ' ';
        while (in[i] != ' ' && in[i] != 0)
        {
            tmp[len++] = in[i];
            i++;
        }
    }

    for (int i = 0; i < len; i++)
    {
        j = 0;
        while (tmp[i] != ' ' && i < len)
        {
            out[para_size][j++] = tmp[i++];
        }
        out[para_size++][j] = 0;
    }
}

/* Global EID shared by multiple threads */
sgx_enclave_id_t global_eid = 0;

typedef struct _sgx_errlist_t
{
    sgx_status_t err;
    const char *msg;
    const char *sug; /* Suggestion */
} sgx_errlist_t;

/* Error code returned by sgx_create_enclave */
static sgx_errlist_t sgx_errlist[] = {
    {SGX_ERROR_UNEXPECTED,
     "Unexpected error occurred.",
     NULL},
    {SGX_ERROR_INVALID_PARAMETER,
     "Invalid parameter.",
     NULL},
    {SGX_ERROR_OUT_OF_MEMORY,
     "Out of memory.",
     NULL},
    {SGX_ERROR_ENCLAVE_LOST,
     "Power transition occurred.",
     "Please refer to the sample \"PowerTransition\" for details."},
    {SGX_ERROR_INVALID_ENCLAVE,
     "Invalid enclave image.",
     NULL},
    {SGX_ERROR_INVALID_ENCLAVE_ID,
     "Invalid enclave identification.",
     NULL},
    {SGX_ERROR_INVALID_SIGNATURE,
     "Invalid enclave signature.",
     NULL},
    {SGX_ERROR_OUT_OF_EPC,
     "Out of EPC memory.",
     NULL},
    {SGX_ERROR_NO_DEVICE,
     "Invalid SGX device.",
     "Please make sure SGX module is enabled in the BIOS, and install SGX driver afterwards."},
    {SGX_ERROR_MEMORY_MAP_CONFLICT,
     "Memory map conflicted.",
     NULL},
    {SGX_ERROR_INVALID_METADATA,
     "Invalid enclave metadata.",
     NULL},
    {SGX_ERROR_DEVICE_BUSY,
     "SGX device was busy.",
     NULL},
    {SGX_ERROR_INVALID_VERSION,
     "Enclave version was invalid.",
     NULL},
    {SGX_ERROR_INVALID_ATTRIBUTE,
     "Enclave was not authorized.",
     NULL},
    {SGX_ERROR_ENCLAVE_FILE_ACCESS,
     "Can't open enclave file.",
     NULL},
};

/* Check error conditions for loading enclave */
void print_error_message(sgx_status_t ret)
{
    size_t idx = 0;
    size_t ttl = sizeof sgx_errlist / sizeof sgx_errlist[0];

    for (idx = 0; idx < ttl; idx++)
    {
        if (ret == sgx_errlist[idx].err)
        {
            if (NULL != sgx_errlist[idx].sug)
                printf("Info: %s\n", sgx_errlist[idx].sug);
            printf("Error: %s\n", sgx_errlist[idx].msg);
            break;
        }
    }

    if (idx == ttl)
        printf("Error code is 0x%X. Please refer to the \"Intel SGX SDK Developer Reference\" for more details.\n", ret);
}

/* Initialize the enclave:
 *   Step 1: try to retrieve the launch token saved by last transaction
 *   Step 2: call sgx_create_enclave to initialize an enclave instance
 *   Step 3: save the launch token if it is updated
 */
int initialize_enclave(void)
{
    char token_path[MAX_PATH] = {'\0'};
    sgx_launch_token_t token = {0};
    sgx_status_t ret = SGX_ERROR_UNEXPECTED;
    int updated = 0;

    /* Step 1: try to retrieve the launch token saved by last transaction
     *         if there is no token, then create a new one.
     */
    /* try to get the token saved in $HOME */
    const char *home_dir = getpwuid(getuid())->pw_dir;

    if (home_dir != NULL &&
        (strlen(home_dir) + strlen("/") + sizeof(TOKEN_FILENAME) + 1) <= MAX_PATH)
    {
        /* compose the token path */
        strncpy(token_path, home_dir, strlen(home_dir));
        strncat(token_path, "/", strlen("/"));
        strncat(token_path, TOKEN_FILENAME, sizeof(TOKEN_FILENAME) + 1);
    }
    else
    {
        /* if token path is too long or $HOME is NULL */
        strncpy(token_path, TOKEN_FILENAME, sizeof(TOKEN_FILENAME));
    }

    FILE *fp = fopen(token_path, "rb");
    if (fp == NULL && (fp = fopen(token_path, "wb")) == NULL)
    {
        printf("Warning: Failed to create/open the launch token file \"%s\".\n", token_path);
    }

    if (fp != NULL)
    {
        /* read the token from saved file */
        size_t read_num = fread(token, 1, sizeof(sgx_launch_token_t), fp);
        if (read_num != 0 && read_num != sizeof(sgx_launch_token_t))
        {
            /* if token is invalid, clear the buffer */
            memset(&token, 0x0, sizeof(sgx_launch_token_t));
            printf("Warning: Invalid launch token read from \"%s\".\n", token_path);
        }
    }
    /* Step 2: call sgx_create_enclave to initialize an enclave instance */
    /* Debug Support: set 2nd parameter to 1 */
    ret = sgx_create_enclave(ENCLAVE_FILENAME, SGX_DEBUG_FLAG, &token, &updated, &global_eid, NULL);
    if (ret != SGX_SUCCESS)
    {
        print_error_message(ret);
        if (fp != NULL)
            fclose(fp);
        return -1;
    }

    /* Step 3: save the launch token if it is updated */
    if (updated == FALSE || fp == NULL)
    {
        /* if the token is not updated, or file handler is invalid, do not perform saving */
        if (fp != NULL)
            fclose(fp);
        return 0;
    }

    /* reopen the file with write capablity */
    fp = freopen(token_path, "wb", fp);
    if (fp == NULL)
        return 0;
    size_t write_num = fwrite(token, 1, sizeof(sgx_launch_token_t), fp);
    if (write_num != sizeof(sgx_launch_token_t))
        printf("Warning: Failed to save launch token to \"%s\".\n", token_path);
    fclose(fp);
    return 0;
}

/* OCall functions */
void ocall_print_string(const char *str)
{
    /* Proxy/Bridge will check the length and null-terminate
     * the input string to prevent buffer overflow.
     */
    printf("%s", str);
}
unsigned char *newLine;
void ocall_pass_string(unsigned char *str)
{
    /* Proxy/Bridge will check the length and null-terminate
     * the input string to prevent buffer overflow.
     */
    // printf("%s", str);
    /*
     printf("for hex\n");
     for(int i = 0; i < 566; i++){
     printf("%.2x", str[i]);
     }
     printf("\n");*/
    memcpy(newLine, str, 566);
}

int covert_char_to_hex(char a)
{
    if ('0' <= a && a <= '9')
    {
        return a - '0';
    }
    else
    {
        return 10 + a - 'a';
    }
}

//#define POLICY_LIST "/home/lass/jinhoon/policy_list"
#define POLICY_LIST "/home/jinu/Desktop/policy_list"
unsigned char policy_arr[1001][566];
/* Application entry */
int SGX_CDECL main(int argc, char *argv[])
{
    (void)(argc);
    (void)(argv);
    int N;
    /* Initialize the enclave */
    if (initialize_enclave() < 0)
    {
        printf("Enter a character before exit ...\n");
        getchar();
        return -1;
    }
    struct timespec clock_s, clock_e;
    double time_s, time_e;

    scanf("%d\n", &N);
    clock_gettime(CLOCK_MONOTONIC, &clock_s);
    for(int ii = 0; ii <N; ii++)
    {
        //실험용으로 잠시 삭제
        // printf("**************************************************************\n");
        // printf("* SGX-SSD policy manager                                     *\n");
        // printf("*                                                            *\n");
        // printf("* press command and parameter                                *\n");
        // printf("*                                                            *\n");
        // printf("* ex) {create|change|delete} {ret_time} {Backup_cycle}       *\n");
        // printf("*                                                            *\n");
        // printf("* units:                                                     *\n");
        // printf("* retention time : day                                       *\n");
        // printf("* Backup cycle   : day                                       *\n");
        // printf("**************************************************************\n");
        //실험용으로 잠시 삭제

        FILE *fp = fopen(POLICY_LIST, "r+");

        int policy_cnt = 0;
        int pair = 0;
        //printf("policy : \n");
        while (1)
        {
            int eof, i = 0;
            char tmp[2];
            unsigned char line[567];
            while (1)
            {
                eof = fscanf(fp, "%c", &tmp[pair]);
                if (eof == EOF || tmp[pair] == '\n')
                {
                    pair = 0;
                    break;
                }
                pair++;
                if (pair == 2)
                {
                    pair = 0;
                    line[i++] = (covert_char_to_hex(tmp[0])) * 16 + covert_char_to_hex(tmp[1]);
                    //printf("%.2x", line[i-1]);
                }
            }
            //printf("\n");
            memcpy(policy_arr[policy_cnt], line, 566);
            policy_cnt++;
            if (eof == EOF)
                break;
        }
        //실험용printf("\n\n");

        /*
     printf("policy arr check : \n");
     for(int i = 0; i < policy_cnt; i++){
     for(int j = 0; j < 566; j++){
     printf("%.2x", policy_arr[i][j]);
     }
     printf("\n");
     }*/
        //perfectly excuted
        //실험용printf("*** policy list ***\n");
        for (int i = 0; i < policy_cnt - 1; i++)
        {
            print_unseal_data(global_eid, &policy_arr[i][0]);
            //실험용printf("\n");
        }
         //실험용printf("*******************\n");
         //실험용printf("\ninput : \n");
        //assume aurora input
        char in[1000];
        line_input(in);
        //debug
        //printf("%s\n", in);

        char para_arr[para_MAX_SIZE][para_MAX_LEN]; //save parameters
        parse_str(in, para_arr);

        //debug

         //실험용printf("parameter lists : \n");
        for (int i = 0; i < para_MAX_SIZE; i++)
        {
            ;
             //실험용printf("%s\n", para_arr[i]);
        }

        char path[100];
        int command, retention_time, backup_cycle, version_number, pid;

        int branch = 0;

        if (para_arr[0][0] == 'c' && para_arr[0][1] == 'r')
        {
            command = SPM_CREATE;
        }
        else if (para_arr[0][0] == 'c' && para_arr[0][1] == 'h')
        {
            command = SPM_CHANGE;
        }
        else if (para_arr[0][0] == 'd' || para_arr[0][0] == 'D')
        {
            command = SPM_DELETE;
        }
        else if (para_arr[0][0] == 'r' || para_arr[0][0] == 'R')
        {
            //no recovery
            command = 3;
            branch = 1;
        }
        else
        {
            fprintf(stderr, "error : command\n");
            return 0;
        }

        if (command == SPM_CHANGE)
        {
            //what pid to change?
             //실험용printf("what pid to change : ");
            scanf("%d", &pid);
        }
        else if (command == SPM_DELETE)
        {
             //실험용printf("what pid to delete : ");
            scanf("%d", &pid);
        }
        strcpy(path, para_arr[1]);

        if (command != SPM_DELETE)
        {
            retention_time = atoi(para_arr[1]);
            backup_cycle = atoi(para_arr[2]);
        }
        //debug
        //debug
        //printf("data : %d%s%d%d%d\n", command, path, retention_time, backup_cycle, version_number);
         //실험용printf("data : %d%d%d\n", command, retention_time, backup_cycle);

        //start
        

        int spm_param[4];
        spm_param[0] = retention_time;
        spm_param[1] = backup_cycle;
        spm_param[2] = version_number;
        spm_param[3] = command;
        //printf("%d",sizeof(sgx_sealed_data_t));
        newLine = (unsigned char *)malloc(566);
        printf_helloworld(global_eid, policy_cnt, spm_param, newLine);
        fclose(fp);
        if (command == SPM_CREATE)
        {
            fp = fopen(POLICY_LIST, "a+");
            //fprintf(fp, "%d %d %d\n", retention_time, backup_cycle, 0);
            //fprintf(fp, "%s\n", newLine); //not work
            for (int i = 0; i < 566; i++)
            {
                fprintf(fp, "%.2x", newLine[i]);
            }
            fprintf(fp, "\n");
            fclose(fp);
        }
        else if (command == SPM_CHANGE)
        {
            fp = fopen(POLICY_LIST, "w+");
             //실험용printf("policy_cnt : %d\n", policy_cnt);
            policy_cnt--;
            for (int i = 0; i < policy_cnt; i++)
            {
                if (i == pid)
                {
                    for (int j = 0; j < 566; j++)
                    {
                        fprintf(fp, "%.2x", newLine[j]);
                    }
                }
                else
                {
                    for (int j = 0; j < 566; j++)
                    {
                        fprintf(fp, "%.2x", policy_arr[i][j]);
                    }
                }
                fprintf(fp, "\n");
            }
            fclose(fp);
        }
        else if (command == SPM_DELETE)
        {
            fp = fopen(POLICY_LIST, "w+");
             //실험용printf("policy_cnt : %d\n", policy_cnt);
            policy_cnt--;
            for (int i = 0; i < policy_cnt; i++)
            {
                if (i != pid)
                {
                    for (int j = 0; j < 566; j++)
                    {
                        fprintf(fp, "%.2x", policy_arr[i][j]);
                    }
                    fprintf(fp, "\n");
                }
            }
            fclose(fp);
        }

        char buf[100];
        char resp[100];
        
        if (spm_send_cmd(0, buf, 0, resp, policy_cnt, spm_param) == -1)
        {
             //실험용printf("[spm] error command didn't reach to ssd");
            return 0;
        }
        
        //end
        
        /* Destroy the enclave */

        free(newLine);
    }
    clock_gettime(CLOCK_MONOTONIC, &clock_e);
    time_s = (clock_s.tv_nsec / 1e9) + clock_s.tv_sec;
    time_e = (clock_e.tv_nsec / 1e9) + clock_e.tv_sec;
    printf("time: %.9lf ~ %.9lf = %.9lf\n", time_s, time_e, time_e - time_s);
    sgx_destroy_enclave(global_eid);
    //free(policy_arr);
    return 0;
}
