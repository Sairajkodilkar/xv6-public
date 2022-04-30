#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "swap.h"

void usage() {
	printf("mkswp <filename> <blocksize> <count>\n");
	return;
}

int main(int argc, char **argv) {

	struct swap_header sh;
	int fd,
			block_size,
			count;	

  if(argc != 4) {
		usage();
		return 0;
	}

	if((fd = open(argv[1], O_WRONLY | O_CREAT, 0666)) < 0) {
		perror("mkswp");
		return errno;
	}
	block_size = atoi(argv[2]);
	count = atoi(argv[3]);
	ftruncate(fd, block_size * count);

	sh.count = count;
	sh.block_size = block_size;
	sh.data_start = 1;
	write(fd, &sh, sizeof(sh));
	close(fd);
	return 0;
}
