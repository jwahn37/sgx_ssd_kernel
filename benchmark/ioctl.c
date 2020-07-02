/*
 * fibmap - List blocks of a file
 *
 * Written in 2012 by Prashant P Shah <pshah.mumbai@gmail.com>
 *
 * To the extent possible under law, the author(s) have dedicated
 * all copyright and related and neighboring rights to this software
 * to the public domain worldwide. This software is distributed
 * without any warranty.
 *
 * You should have received a copy of the CC0 Public Domain Dedication
 * along with this software.
 * If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <assert.h>

//int recovery(int fd, int recovery_time);

int main(int argc, char **argv)
{
	int fd, i, block, blocksize, blkcnt;
	int pos;
	int recov;
	char input[4096];
	struct stat st;

	assert(argv[1] != NULL);

	fd = open(argv[1], O_RDWR);
	if (fd <= 0) {
		perror("error opening file");
		goto end;
	}

	if (ioctl(fd, FIGETBSZ, &blocksize)) {
		perror("FIBMAP ioctl failed");
		goto end;
	}

	if (fstat(fd, &st)) {
		perror("fstat error");
		goto end;
	}
//	write(fd, "hello world!", 13);
//	ftruncate(fd, 1024);
	//printf("%d\n", lseek(fd,0,SEEK_END));
	for(i=0; i<4096; i++)
		input[i]=1;
	for(i=0; i<1024; i++)
		write(fd, input, 4096);
	
//	rqecovery(fd, recov);
	

	blkcnt = (st.st_size + blocksize - 1) / blocksize;
	printf("File %s size %d blocks %d blocksize %d\n",
			argv[1], (int)st.st_size, blkcnt, blocksize);

	int block_bf=0;
	for (i = 0; i < blkcnt; i++) {
		block = i;
		//printf("offset: %d\n", lseek(fd, 0, SEEK_CUR)); 
		if (ioctl(fd, FIBMAP, &block)) {
			perror("FIBMAP ioctl failed");
		}
		if(block_bf+1 < block)
			printf("Not Contiguous!!!!!!!\n");
		block_bf=block;
		printf("%3d %10d\n", i, block);
	}

	
end:
	close(fd);
	return 0;
}




