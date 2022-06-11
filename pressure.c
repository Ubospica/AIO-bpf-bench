/**
 * Compile:
 * 	gcc pressure.c -Wall -O2 -o pressure -luring -laio
 * 
 * Usage:
 * 	./pressure lib input output QD BLK_size(KB)
 * 	./pressure io_uring infile outfile 64 32
 *
 * 	The size of input file should be multiple of ALIGN
 */
#define _GNU_SOURCE

#include <aio.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <libaio.h>
#include <liburing.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/sdt.h> 
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define ALIGN 1024

int QD;
int blk_size;


// a, b > 0
int my_ceil(int a, int b) {
	return (a - 1) / b + 1;
}

int my_min(int a, int b) {
	return a < b ? a : b;
}

#define ADD_WR_PROBE() { \
	if (is_read) DTRACE_PROBE(my-probe, probe-read); \
	else DTRACE_PROBE(my-probe, probe-write);}

double time_delta(struct timeval tv1, struct timeval tv2) {
	return (tv2.tv_sec - tv1.tv_sec) * 1000.0 + (tv2.tv_usec - tv1.tv_usec) / 1000.0;
}

#define START_LATENCY() struct timeval t1, t2; gettimeofday(&t1, NULL);
#define END_LATENCY() (gettimeofday(&t2, NULL), time_delta(t1, t2))


int check(char *buf, int size) {
	for (int i = 0; i < size; ++i) {
		if (buf[i] != 'a') {
			fprintf(stderr, "read failed at %d\n", i);
			exit(1);
		}
	}
	return 0;
}


void sync_test(int fd, int size, char *buf, int is_read) {
	int ret, offset = 0;

	ADD_WR_PROBE();
	struct iovec vec;
	while (size) {
		int cur_size = my_min(size, blk_size);
		vec.iov_base = buf + offset;
		vec.iov_len = cur_size;
		if (is_read)
			ret = readv(fd, &vec, 1);
		else
			ret = writev(fd, &vec, 1);
		assert(ret >= 0);
		offset += cur_size;
		size -= cur_size;
	}
	ADD_WR_PROBE();
}

void io_uring_test(int fd, int size, char *buf, int is_read) {
	int ret;
	
	ADD_WR_PROBE();

	START_LATENCY();

	struct io_uring ring;
	struct io_uring_sqe *sqe;
	struct io_uring_cqe *cqe;
	io_uring_queue_init(QD, &ring, 0);

	int offset = 0, cnt = 0;

	while (size || cnt) {
		int prev_cnt = cnt;

		while(size && cnt < QD) {
			int cur_size = my_min(size, blk_size);

			sqe = io_uring_get_sqe(&ring);
			struct iovec *my_iovec = calloc(1, sizeof(struct iovec));
			my_iovec -> iov_base = buf + offset;
			my_iovec -> iov_len = cur_size;
			if (is_read) {
				io_uring_prep_readv(sqe, fd, my_iovec, 1, offset);
			} else {
				io_uring_prep_writev(sqe, fd, my_iovec, 1, offset);
			}
			io_uring_sqe_set_data(sqe, my_iovec);

			offset += cur_size;
			size -= cur_size;
			++cnt;
		}

		if (prev_cnt != cnt) {
			assert((ret = io_uring_submit(&ring)) >= 0);
		}

		int got_comp = 0;
		usleep(1000);
		while (cnt) {
			if (!got_comp) {
				ret = io_uring_wait_cqe(&ring, &cqe);
				got_comp = 1;
			} else {
				ret = io_uring_peek_cqe(&ring, &cqe);
				if (ret == -EAGAIN) {
					break;
				}
			}
			assert(ret >= 0);
			if (!cqe)
				break;

			--cnt;
			free(io_uring_cqe_get_data(cqe));
			io_uring_cqe_seen(&ring, cqe);
		}
	}
	
	io_uring_queue_exit(&ring);

	ADD_WR_PROBE();
	
	// double lat = END_LATENCY();
	// printf("time:%lf\n", lat);
}

void posix_aio_test(int fd, int size, char *buf, int is_read) {
    int ret;

	ADD_WR_PROBE();

    int offset = 0, cnt = 0;
	struct aiocb** my_aiocb = calloc(QD, sizeof(struct aiocb));
	for (int i = 0; i < QD; ++i) {
		if (size == 0) {
			my_aiocb[i] = NULL;
			continue;
		}
		int cur_size = my_min(size, blk_size);
		my_aiocb[i] = calloc(1, sizeof(struct aiocb));
		*my_aiocb[i] = (struct aiocb){
			.aio_buf = buf + offset,
			.aio_fildes = fd,
			.aio_nbytes = cur_size,
			.aio_offset = offset,
			.aio_lio_opcode = is_read ? LIO_READ : LIO_WRITE
		};
		offset += cur_size;
		size -= cur_size;
		++cnt;
	}
	
	lio_listio(LIO_NOWAIT, my_aiocb, QD, NULL);

	while(cnt > 0) {
		ret = aio_suspend(my_aiocb, QD, NULL);
		assert(ret == 0);

		for (int i = 0; i < QD; ++i) {
			if (my_aiocb[i] == NULL)
				continue;
			
			ret = aio_error(my_aiocb[i]);

			if (ret != 0)
				continue;

			assert((ret = aio_return(my_aiocb[i])) > 0);

			if (size) {
				int cur_size = my_min(size, blk_size);
				
				*my_aiocb[i] = (struct aiocb){
					.aio_buf = buf + offset,
					.aio_fildes = fd,
					.aio_nbytes = cur_size,
					.aio_offset = offset,
					// .aio_lio_opcode = is_read ? LIO_READ : LIO_WRITE
				};
				offset += cur_size;
				size -= cur_size;
				if (is_read)
					assert((ret = aio_read(my_aiocb[i])) == 0);
				else // bug: !is_read ---> aio_write
					assert((ret = aio_write(my_aiocb[i])) == 0);
			} else {
				free(my_aiocb[i]);
				my_aiocb[i] = NULL;
				--cnt;
			}
		}
	}
	free(my_aiocb);

	ADD_WR_PROBE();
}

void libaio_test(int fd, int size, char *buf, int is_read) {
    int ret;
	
	ADD_WR_PROBE();

	io_context_t myctx;
	memset(&myctx, 0, sizeof(myctx));
	io_setup(QD, &myctx);

	struct io_event *events = calloc(QD, sizeof(struct io_event));

	int offset = 0, cnt = 0;

	while(size || cnt) {
		int restcnt = my_min(QD - cnt, my_ceil(size, blk_size));
		if (restcnt > 0) {
			struct iocb *ioq[restcnt];
			for (int i = 0; i < restcnt; ++i) {
				int cur_size = my_min(size, blk_size);
				ioq[i] = calloc(1, sizeof(struct iocb));
				assert(ioq[i]);
				if (is_read) {
					io_prep_pread(ioq[i], fd, buf + offset, cur_size, offset);
				} else {
					io_prep_pwrite(ioq[i], fd, buf + offset, cur_size, offset);
				}
				offset += cur_size;
				size -= cur_size;
				++cnt;
			}

			ret = io_submit(myctx, restcnt, ioq);
			assert(ret >= 0);
		}

		ret = io_getevents(myctx, 1, QD, events, NULL);
		for (int i = 0; i < ret; ++i) {
			// printf("offset=%lld ret=%ld buf=%ld\n", events[i].obj -> u.c.offset, events[i].res, (char*)events[i].obj->u.c.buf - buf);
			free(events[i].obj);
			--cnt;
		}
	}

	free(events);
	io_destroy(myctx);
	
	ADD_WR_PROBE();
}

int main(int argc, char *argv[]) {
	int ret;

    assert(argc == 6);
    int infd = open(argv[2], O_RDONLY | O_DIRECT);
    int outfd = open(argv[3], O_WRONLY | O_CREAT | O_TRUNC | O_DIRECT, 0644);
    assert(infd >= 0 && outfd >= 0);

    struct stat st;
	fstat(infd, &st);
	int size = st.st_size;

    char *buf;
	ret = posix_memalign((void**)&buf, ALIGN, size);
    assert(buf && ret == 0);

	sscanf(argv[4], "%d", &QD);
	// sscanf(argv[5], "%d", &threads);
	sscanf(argv[5], "%d", &blk_size);
	blk_size *= 1024;

	if (strcmp(argv[1], "sync") == 0) {
		sync_test(infd, size, buf, 1);
		check(buf, size);
    	memset(buf, 'a', size);
		sync_test(outfd, size, buf, 0);
	} else if (strcmp(argv[1], "io_uring") == 0) {
		io_uring_test(infd, size, buf, 1);
		check(buf, size);
		memset(buf, 'a', size);
		io_uring_test(outfd, size, buf, 0);
	} else if (strcmp(argv[1], "posix_aio") == 0) {
		posix_aio_test(infd, size, buf, 1);
		check(buf, size);
		memset(buf, 'a', size);
		posix_aio_test(outfd, size, buf, 0);
	} else if (strcmp(argv[1], "libaio") == 0) {
		libaio_test(infd, size, buf, 1);
		check(buf, size);
		memset(buf, 'a', size);
		libaio_test(outfd, size, buf, 0);
	}

    free(buf);
	close(infd);
	close(outfd);

	// system("grep ctxt /proc/$(pidof pressure)/status | awk \'{ print $2 }\'");

    return 0;
}