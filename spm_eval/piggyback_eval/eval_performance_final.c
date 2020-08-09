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

typedef struct packet
{
	int flag;
	int pid;
	int fid;
} _packet;

typedef struct packet_out
{
	int flag;
	char dir[100];
} _packet_out;

#define TOTAL_WORKLOAD_SIZE 1000 // 1000MB
#define OPEN_KEY 334
#define CLOSE_KEY 335
#define MAX 1024

#define BIG_FILE_SIZE (512 * 1024 * 1024)
#define BUF_SIZE 4096
#define NUM_RATIO 5
#define DEFAULT_RET_TIME 123
//#define NAME_LEN 34
//#define NAME_END 23
//#define NAME_BEGIN 17

#define NAME_LEN 32
#define NAME_END 23
#define NAME_BEGIN 15

/*
 *	실험1. 성능 테스트.
 *	파일 타입 : retention time O(default), retention time X
 *	파일의 비율을 조정하며 테스트한다. 
 *	X:O= {100:0, 80:20, 60:40, 40:60, 20:80, 0:100}
 *	100:0은 일반 SSD, 0:100은 알마낙임
 *	파일의 총 size는 2GB
 *	각 파일 size는 400MB*5, 4KB*500*1024(50만개)
 *
 *
 * 사용자 입력:
 * 파일의 비율, 파일의 size, ...
 * */

//int num_total_files = 1; // num_total_files * file_size = 1GB
//	int file_size = 32 * 1024;
//int num_total_files = 1000; //
int num_total_files = 50;	//spm eval - 1GBfile(20MB*50)

//long long unsigned int file_size = 128*1024*1024;	//big file
//long long unsigned int file_size = 4 * 1024; //small file
long long unsigned int file_size = 20*1024*1024; //20MB
//int file_size = 64 * 1024 * 1024;
//int file_size = 1024 * 1024;
int thread_num = 1;

sem_t mysem0, mysem1;
int shmid_pid;
int shmid_out;
int baseline;
_packet *shmaddr_f_to_p;
_packet_out *shmaddr_out;

int set_file_name(char file_name[NAME_LEN]);
//void init_file_name(char file_name[NAME_LEN], unsigned long int thread_id);
int write_file(int fd, long long unsigned int file_size, char buf[BUF_SIZE], char file_pattern_flag, char th_num);

int my_id = 0;

char buf[BUF_SIZE] = {
	'X',
};
void *open_write_key(void *th_num)
{
	int i;
	int fd;
	struct timespec clock_s, clock_e;
	double time_s, time_e;
	int no;
	//printf("thread id : %d\n", pthread_self());
	//printf("th_num : %d\n", *(int *)th_num);
	// til '/' 14, til '0' 23
	//char file_name[NAME_LEN]="/home/jeewon/SSD/0000000.txt";
	char file_name[NAME_LEN] = "/home/jinu/SSD/000000000.ppt";

	sem_wait(&mysem1);
	no = my_id++;
	sem_post(&mysem1);
	//	clock_gettime(CLOCK_MONOTONIC, &clock_s);

	// 0 ->0000000
	// 1 ->1000000 00010000
	for (i = 0; i < num_total_files; i++)
	{
		//file_name[NAME_BEGIN] = *(char *)th_num;
		file_name[NAME_BEGIN+1] = no % 10 + '0';
		file_name[NAME_BEGIN] = no / 10 + '0';
		//init_file_name(file_name, pthread_self());

		//파일 이름 설정
		//file name이 바뀐다.
		set_file_name(file_name);
		printf("filename : %s\n", file_name);

		//printf("filename : %s\n", file_name);
		if (!baseline)
		{
			sem_wait(&mysem0);
			strcpy(shmaddr_out->dir, file_name);
			shmaddr_out->flag = 1;

			shmaddr_f_to_p->flag = 0;
			while (!shmaddr_f_to_p->flag)
				;
			unsigned int pid = shmaddr_f_to_p->pid;
			unsigned int fid = shmaddr_f_to_p->fid;
			sem_post(&mysem0);

			//printf("filename : %s\n", file_name);
			pid = 0x11223344;
			//unsigned int pid = 0x11223344;
			//unsigned int fid = 1;
			fd = syscall(OPEN_KEY, file_name, O_RDWR | O_CREAT, 0, pid, fid);
		}
		else
			fd = open(file_name, O_RDWR | O_CREAT);

		if (fd == -1)
		{
			printf("FD ERROR!! \n");
			return 0;
		}
		//printf("222222 : th_num : %c\n", *(char *)th_num);
		write_file(fd, file_size, buf, 'S', *(char *)th_num);
		//printf("666666 : th_num : %c\n", *(char *)th_num);
		//close(fd);
		//close(fd);
		if (!baseline)
			syscall(CLOSE_KEY, fd);
		else
			close(fd);

		//printf("777777 : th_num : %c\n", *(char *)th_num);
	}
}

//const char INITIAL_FILE_NAME[NAME_LEN] = "/home/jinu/SSD/0000000.txt";

int main(int argc, char **argv)
{
	//printf("hi\n");
	int fd;
	int err = 0;
	int i = 0;
	long int j = 0;
	int ret_time, ret_ratio, capacity;

	char file_name[NAME_LEN] = "/home/jeewon/SSD/0000000.txt";
	char init_file_name[NAME_LEN] = "/home/jeewon/SSD/0000000.txt";
	char init2_file_name[NAME_LEN] = "/home/jeewon/SSD/0000000.txt";

	struct timespec clock_s, clock_e;
	double time_s, time_e;

	//1. 사용자는 보호하고자하는 파일의(RT X)의 비율을 보낸다.
	//2. 사용자는 각 파일의 크기를 보낸다.
	//capacity = atoi(argv[1]); //0,20,40,60,80,100
	//ret_ratio = atoi(argv[2]);
	thread_num = atoi(argv[1]);
	baseline = atoi(argv[2]);
	printf("thread num : %d\n", thread_num);
	printf("buf %c %c %c %c %c\n", buf[0], buf[1], buf[2], buf[3], buf[4]);
	//int num_total_files = 50; // num_total_files * file_size = 1GB
	//int num_total_files = 50 * 5 * 1024 / 8; // num_total_files * file_size = 1GB
	//	int num_total_files = 10000; // num_total_files * file_size = 1GB

	int num_capacity_files = capacity * num_total_files / 100;
	int num_init_invalid_files = num_total_files - num_capacity_files;
	/*
	printf("capacity file num : %d, invalid file num : %d, retention file num : %d\n\n\n", num_capacity_files, num_init_invalid_files, num_capacity_files * ret_ratio / 100);
	srand(time(NULL));
	
	printf("*** Initialization ***\n");
	printf("Write %dMB file with no RT and %dMB files with RT\n\n\n", TOTAL_WORKLOAD_SIZE - TOTAL_WORKLOAD_SIZE * ret_ratio / 100, TOTAL_WORKLOAD_SIZE * ret_ratio / 100);
*/
	ret_ratio /= 20;

	printf("number of total file : %d, size of file %llu\n", num_total_files, file_size);

	shmid_pid = shmget((key_t)0x1237, sizeof(_packet), IPC_CREAT | 0666);
	shmaddr_f_to_p = (_packet *)shmat(shmid_pid, NULL, 0);

	shmid_out = shmget((key_t)0x1236, sizeof(_packet_out), IPC_CREAT | 0666);
	shmaddr_out = (_packet_out *)shmat(shmid_out, NULL, 0);
	//printf("hi\n");
	printf("start experiment!\n");
	clock_gettime(CLOCK_MONOTONIC, &clock_s);
	sem_init(&mysem0, 0, 1);
	sem_init(&mysem1, 0, 1);

	//printf("hi0\n");
	pthread_t p_thread[64];
	//char jin[64];
	int jin;
	for (i = 0; i < thread_num; i++)
	{
		//sem_wait(&mysem1);
		jin = i;
		int thr_id = pthread_create(&p_thread[i], NULL, open_write_key, (void *)&(jin));

		printf("thread %d made\n", i);

		if (thr_id < 0)
		{
			fprintf(stderr, "thread create error\n");
			exit(0);
		}
	}

	//printf("hi1\n");
	//	for(i = 0; i < num_capacity_files; i++)
	int status = 0;
	for (int i = 0; i < thread_num; i++)
	{
		//error  : i was changed in somewhere -> change scope
		int join_ret = pthread_join(p_thread[i], (void **)&status);
		printf("i : %d, join_ret : %d\n", i, join_ret);
	}
	printf("hi2\n");
	clock_gettime(CLOCK_MONOTONIC, &clock_e);
	time_s = (clock_s.tv_nsec / 1e9) + clock_s.tv_sec;
	time_e = (clock_e.tv_nsec / 1e9) + clock_e.tv_sec;
	printf("time: %.9lf ~ %.9lf = %.9lf\n", time_s, time_e, time_e - time_s);
}

int set_file_name(char file_name[NAME_LEN])
{
	int i;

	file_name[NAME_END]++;
	if (file_name[NAME_END] == '9' + 1)
	{
		file_name[NAME_END] = 'A';
	}
	for (i = NAME_END; i > NAME_BEGIN; i--)
	{
		if (file_name[i] > 'F' && i != NAME_BEGIN + 1)
		{
			file_name[i] = '0';
			file_name[i - 1]++;
			if (file_name[i - 1] == '9' + 1)
			{
				file_name[i - 1] = 'A';
			}
		}
	}
	return 1;
}

// void init_file_name(char file_name[NAME_LEN], unsigned long int thread_id)
// {
// 	printf("thread id : %lu\n", thread_id);
// 	for (int i = 9; i >= 0; i--)
// 	{
// 		file_name[NAME_BEGIN + i] = thread_id % 10 + '0';
// 		thread_id /= 10;
// 	}
// 	printf("file name : %s\n", file_name);
// }

int write_file(int fd, long long unsigned int file_size, char buf[BUF_SIZE], char file_pattern_flag, char th_num)
{
	int i;
	int err;
	int random_offset;
	int cur_offset;
	//printf("333333 : %c\n", th_num);
	lseek(fd, 0, SEEK_SET);

	//sequential write 수행
	if (file_pattern_flag == 'S')
	{
		for (i = 0; i < file_size / BUF_SIZE; i++)
		{
			if(baseline)
			err = write(fd, buf, BUF_SIZE);
			else{
				//printf("write\n");
				cur_offset = lseek(fd, 0, SEEK_CUR);
				err = read(fd, buf, BUF_SIZE);
				lseek(fd, cur_offset, SEEK_SET);
				err = write(fd, buf, BUF_SIZE);
				//printf("cur_offset: %d\n", cur_offset);
			}
		}
	}
	//printf("44444 : %c\n", th_num);
	fdatasync(fd);
	//fsync(fd);
	//printf("55555 : %c\n", th_num);
	return err;
}
