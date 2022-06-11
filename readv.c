/* SPDX-License-Identifier: MIT */
/*
 * gcc -Wall -O2 -D_GNU_SOURCE -o io_uring-cp io_uring-cp.c -luring
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/sdt.h> 
#include <sys/uio.h>

#define QD	64
#define BS	(32*1024)
#define ALIGN 1024

static int infd, outfd;
static char *buf;


static int get_file_size(int fd, off_t *size)
{
	struct stat st;

	fstat(fd, &st);
	*size = st.st_size;
	return 0;
}


double time_delta(struct timeval tv1, struct timeval tv2) {
	return tv2.tv_sec - tv1.tv_sec + (tv2.tv_usec - tv1.tv_usec) / 1000000.0;
}


int main(int argc, char *argv[])
{
	off_t insize;
	int ret;

	if (argc < 3) {
		printf("%s: infile outfile\n", argv[0]);
		return 1;
	}

	infd = open(argv[1], O_RDONLY | O_DIRECT);
	if (infd < 0) {
		perror("open infile");
		return 1;
	}
	outfd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC | O_DIRECT, 0644);
	if (outfd < 0) {
		perror("open outfile");
		return 1;
	}

	if (get_file_size(infd, &insize))
		return 1;

	// buf = malloc(insize);
	int sizealign = insize;
	if (sizealign % ALIGN) {
		sizealign += ALIGN - sizealign % ALIGN;
	}
	posix_memalign((void**)&buf, ALIGN, sizealign);
	
	struct iovec vec;


	DTRACE_PROBE("hello_usdt", probe-read);
	
	struct timeval t1, t2;
	gettimeofday(&t1, NULL);

	int reminder = insize, offset = 0;
	while (reminder) {
		int sz = reminder;
		if (sz > BS) sz = BS;
		vec.iov_base = buf + offset;
		vec.iov_len = sz;
		int tmp = readv(infd, &vec, 1);
		if (tmp < 0) {
			perror("readv");
		}
		offset += sz;
		reminder -= sz;
	}
	gettimeofday(&t2, NULL);
	DTRACE_PROBE("hello_usdt", probe-read);
	printf("read:%lf\n", time_delta(t1, t2));

	DTRACE_PROBE("hello_usdt", probe-write);
	reminder = insize, offset = 0;
	while (reminder) {
		int sz = reminder;
		if (sz > BS) sz = BS;
		vec.iov_base = buf + offset;
		vec.iov_len = sz;
		writev(outfd, &vec, 1);
		offset += sz;
		reminder -= sz;
	}
	DTRACE_PROBE("hello_usdt", probe-write);

	close(infd);
	close(outfd);

	return ret;
}
