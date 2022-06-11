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
#include "liburing.h"

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

static int setup_context(unsigned entries, struct io_uring *ring)
{
	int ret;

	ret = io_uring_queue_init(entries, ring, 0);
	if (ret < 0) {
		fprintf(stderr, "queue_init: %s\n", strerror(-ret));
		return -1;
	}

	return 0;
}

static int get_file_size(int fd, off_t *size)
{
	struct stat st;

	if (fstat(fd, &st) < 0)
		return -1;
	if (S_ISREG(st.st_mode)) {
		*size = st.st_size;
		return 0;
	} else if (S_ISBLK(st.st_mode)) {
		unsigned long long bytes;

		if (ioctl(fd, BLKGETSIZE64, &bytes) != 0)
			return -1;

		*size = bytes;
		return 0;
	}

	return -1;
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

	// io_uring_sqe_set_data(sqe, data);
}

// 读[offset, offset + size), 加一个sqe，sqe链接data
static void queue_sqe(struct io_uring *ring, off_t size, off_t offset, int read)
{
	struct io_data data;
	data.read = read;
	data.offset = offset;
	data.iov.iov_base = buf + offset;
	data.iov.iov_len = size;

	queue_prepped(ring, &data);
}


// static void queue_write(struct io_uring *ring, struct io_data *data)
// {
// 	data->read = 0;
// 	data->offset = data->first_offset;

// 	data->iov.iov_base = data + 1;
// 	data->iov.iov_len = data->first_len;

// 	queue_prepped(ring, data);
// 	io_uring_submit(ring); // 直接提交
// }

static void read_file(struct io_uring *ring, off_t insize)
{
	unsigned long reads;
	struct io_uring_cqe *cqe;
	off_t offset;
	int ret;

	// buf = malloc(insize);
	int sizealign = insize;
	if (sizealign % ALIGN) {
		sizealign += ALIGN - sizealign % ALIGN;
	}
	posix_memalign((void**)&buf, ALIGN, sizealign);	// buf = malloc(insize);

	reads = offset = 0;

	while (insize) {
		int had_reads, got_comp;
	
		/*
		 * Queue up as many reads as we can
		 */
		while (insize) {
			off_t this_size = insize;

			if (this_size > BS)
				this_size = BS;
			else if (!this_size)
				break;

			// queue_sqe(ring, this_size, offset, 1);
			struct iovec tmp = {buf + offset, this_size};
			int res = preadv(infd, &tmp, 1, offset);

			insize -= this_size;
			offset += this_size;
			printf("read %ld\n", offset);
		}

		// if (had_reads != reads) {
		// 	ret = io_uring_submit(ring); //提交
		// 	if (ret < 0) {
		// 		fprintf(stderr, "io_uring_submit: %s\n", strerror(-ret));
		// 		break;
		// 	}
		// }

		/*
		 * Queue is full at this point. Find at least one completion.
		 */
		// 等到一个 获取当前所有的
		// got_comp = 0;
		// while (reads) {
		// 	if (!got_comp) {
		// 		ret = io_uring_wait_cqe(ring, &cqe);
		// 		got_comp = 1;
		// 	} else {
		// 		ret = io_uring_peek_cqe(ring, &cqe);
		// 		if (ret == -EAGAIN) {
		// 			cqe = NULL;
		// 			ret = 0;
		// 		}
		// 	}
		// 	if (ret < 0) {
		// 		fprintf(stderr, "io_uring_peek_cqe: %s\n",
		// 					strerror(-ret));
		// 		return;
		// 	}
		// 	if (!cqe)
		// 		break;

		// 	struct io_uring_cqe tmp = *cqe;

		// 	if (cqe -> res < 0) {
		// 		perror("readv");
		// 		exit(1);
		// 	}

		// 	--reads;
		// 	io_uring_cqe_seen(ring, cqe);
		// }
	}
}


static void write_file(struct io_uring *ring, off_t insize)
{
	unsigned long writes;
	struct io_uring_cqe *cqe;
	off_t offset;
	int ret;

	writes = offset = 0;

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
			io_uring_cqe_seen(ring, cqe);
		}
	}
}

int main(int argc, char *argv[])
{
	struct io_uring ring;
	off_t insize;

	if (argc < 3) {
		printf("%s: infile outfile\n", argv[0]);
		return 1;
	}

	infd = open(argv[1], O_RDONLY);// | O_DIRECT);
	if (infd < 0) {
		perror("open infile");
		return 1;
	}
	outfd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);// | O_DIRECT, 0644);
	if (outfd < 0) {
		perror("open outfile");
		return 1;
	}

	if (setup_context(QD, &ring))
		return 1;
	if (get_file_size(infd, &insize))
		return 1;

	read_file(&ring, insize);
	printf("%s\n", buf);
	// write_file(&ring, insize);

	close(infd);
	close(outfd);
	io_uring_queue_exit(&ring);
	return 0;
}
