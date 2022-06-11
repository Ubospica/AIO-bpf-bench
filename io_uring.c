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
#include <liburing.h>
#include <sys/time.h>

#define QD	64
#define BS	(32*1024)
#define ALIGN 1024

static int infd, outfd;
char *buf;

struct io_data {
	int read;
	off_t offset;
	struct iovec iov;
};

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

static void queue_prepped(struct io_uring *ring, struct io_data *data)
{
	struct io_uring_sqe *sqe;

	sqe = io_uring_get_sqe(ring);
	assert(sqe);

	if (data->read)
		io_uring_prep_readv(sqe, infd, &data->iov, 1, data->offset);
	else
		io_uring_prep_writev(sqe, outfd, &data->iov, 1, data->offset);

	io_uring_sqe_set_data(sqe, data);
}

// 读[offset, offset + size), 加一个sqe，sqe链接data
static void queue_sqe(struct io_uring *ring, off_t size, off_t offset, int read)
{
	struct io_data *data = calloc(1, sizeof(struct io_data));
	data -> read = read;
	data -> offset = offset;
	data -> iov.iov_base = buf + offset;
	data -> iov.iov_len = size;

	queue_prepped(ring, data);
}

static void read_file(struct io_uring *ring, off_t insize)
{
	unsigned long reads;
	struct io_uring_cqe *cqe;
	int ret;
	off_t offset;

	reads = offset = 0;

	DTRACE_PROBE(hello-usdt, probe-read);

	struct timeval t1, t2;
	gettimeofday(&t1, NULL);

	while (insize || reads) {
		int had_reads, got_comp;
	
		/*
		 * Queue up as many reads as we can
		 */
		had_reads = reads;
		while (insize) {
			off_t this_size = insize;

			if (reads >= QD)
				break;
			if (this_size > BS)
				this_size = BS;
			else if (!this_size)
				break;

			queue_sqe(ring, this_size, offset, 1);

			insize -= this_size;
			offset += this_size;
			reads++;
		}

		if (had_reads != reads) {
			ret = io_uring_submit(ring); //提交
			if (ret < 0) {
				fprintf(stderr, "io_uring_submit: %s\n", strerror(-ret));
				break;
			}
		}

		/*
		 * Queue is full at this point. Find at least one completion.
		 */
		// 等到一个 获取当前所有的
		got_comp = 0;
		while (reads) {
			if (!got_comp) {
				ret = io_uring_wait_cqe(ring, &cqe);
				got_comp = 1;
			} else {
				ret = io_uring_peek_cqe(ring, &cqe);
				if (ret == -EAGAIN) {
					cqe = NULL;
					ret = 0;
				}
			}
			if (ret < 0) {
				fprintf(stderr, "io_uring_peek_cqe: %s\n",
							strerror(-ret));
				return;
			}
			if (!cqe)
				break;

			--reads;
			free(io_uring_cqe_get_data(cqe));
			io_uring_cqe_seen(ring, cqe);
		}
	}
	gettimeofday(&t2, NULL);
	DTRACE_PROBE(hello-usdt, probe-read);
	printf("read:%lf\n", time_delta(t1, t2));
}


static void write_file(struct io_uring *ring, off_t insize)
{
	unsigned long writes;
	struct io_uring_cqe *cqe;
	off_t offset;
	int ret;

	writes = offset = 0;

	DTRACE_PROBE(hello-usdt, probe-write);
	while (insize || writes) {
		int had_writes, got_comp;
	
		/*
		 * Queue up as many writes as we can
		 */
		had_writes = writes;
		while (insize) {
			off_t this_size = insize;

			if (writes >= QD)
				break;
			if (this_size > BS)
				this_size = BS;
			else if (!this_size)
				break;

			queue_sqe(ring, this_size, offset, 0);

			insize -= this_size;
			offset += this_size;
			writes++;
		}

		if (had_writes != writes) {
			ret = io_uring_submit(ring); //提交
			if (ret < 0) {
				fprintf(stderr, "io_uring_submit: %s\n", strerror(-ret));
				break;
			}
		}

		/*
		 * Queue is full at this point. Find at least one completion.
		 */
		// 等到一个 获取当前所有的
		got_comp = 0;
		while (writes) {
			if (!got_comp) {
				ret = io_uring_wait_cqe(ring, &cqe);
				got_comp = 1;
			} else {
				ret = io_uring_peek_cqe(ring, &cqe);
				if (ret == -EAGAIN) {
					cqe = NULL;
					ret = 0;
				}
			}
			if (ret < 0) {
				fprintf(stderr, "io_uring_peek_cqe: %s\n",
							strerror(-ret));
				return;
			}
			if (!cqe)
				break;

			--writes;
			free(io_uring_cqe_get_data(cqe));
			io_uring_cqe_seen(ring, cqe);
		}
	}
	DTRACE_PROBE(hello-usdt, probe-write);
}

int main(int argc, char *argv[])
{

	if (argc < 3) {
		printf("%s: infile outfile\n", argv[0]);
		return 1;
	}

	
	off_t insize;
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

		
	struct io_uring ring;
	
	io_uring_queue_init(QD, &ring, 0);


	int sizealign = insize;
	if (sizealign % ALIGN) {
		sizealign += ALIGN - sizealign % ALIGN;
	}
	posix_memalign((void**)&buf, ALIGN, sizealign);	// buf = malloc(insize);

	read_file(&ring, insize);
	write_file(&ring, insize);

	free(buf);

	close(infd);
	close(outfd);

	io_uring_queue_exit(&ring);
	return 0;
}
