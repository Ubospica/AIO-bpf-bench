/**
 * 4K IO test
 * 
 * Compile:
 * 	gcc iops.c -Wall -O2 -o iops -luring -laio -pedantic
 * 
 * Usage:
 * 	./iops lib input output QD threads
 * 	lib = [sync | io_uring | posix_aio | libaio]
 * 	./iops io_uring infile outfile_rand 64 1
 *
 * 	The size of input file should be multiple of ALIGN
 */
#define _GNU_SOURCE

// #include "/usr/include/asm-generic/errno.h"
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
int threads;
const int blk_size = 4096;

// #define eprintf(...) fprintf(stderr, __VA_ARGS__)
#define eprintf(...) 


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

#undef ADD_WR_PROBE
#define ADD_WR_PROBE()

int check(char *buf, int size) {
	for (int i = 0; i < size; ++i) {
		if (buf[i] != 'a') {
			fprintf(stderr, "read failed at %d\n", i);
			exit(1);
		}
	}
	return 0;
}

// [l, r]
int rand_between(int l, int r) {
	return rand() % (r - l + 1) + l;
}

int get_rand_offset(int size) {
	return rand() % (size / blk_size) * blk_size;
}

const int timer_sec = 10;
int timer_end = 0;

void handle(int signo){
    __atomic_add_fetch(&timer_end, 1, __ATOMIC_SEQ_CST);
	// printf("time ok\n");
}

void init_sigaction(){
    struct sigaction act;
	memset(&act, 0, sizeof(act));
    act.sa_handler = handle;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask); 
    sigaction(SIGPROF, &act, NULL);
}

void init_time() { 
    // struct itimerval value = {
	// 	.it_value.tv_sec = timer_sec,
	// 	.it_value.tv_usec = 0,
	// 	.it_interval.tv_sec = 0,
	// 	.it_interval.tv_usec = 0
	// }; 
    // setitimer(ITIMER_REAL, &value, NULL);
	int ret;
	timer_end = 0;
	ret = alarm(timer_sec);
	// printf("ret=%d\n", ret);
	assert(ret == 0);
	assert(signal(SIGALRM, handle) >= 0);
} 



void sync_test(int fd, int size, char *buf, int is_read) {
	int ret, iocnt = 0;

	init_time();
	while (!__atomic_load_n(&timer_end, __ATOMIC_SEQ_CST)) {
		int offset = get_rand_offset(size);
		if (is_read) {
			ret = posix_memalign((void**)&buf, ALIGN, blk_size);
			assert(ret == 0);
			ret = pread(fd, buf, blk_size, offset);
			free(buf);
		} else {
			ret = pwrite(fd, buf, blk_size, offset);
		}
		assert(ret > 0);
		++iocnt;
	}
	printf("sync state:%s iocnt=%d\n", is_read ? "read" : "write", iocnt);
}

void io_uring_test(int fd, int size, char *buf, int is_read) {
	int ret, iocnt = 0;
	
	struct io_uring ring;
	struct io_uring_sqe *sqe;
	struct io_uring_cqe *cqe;
	io_uring_queue_init(QD, &ring, 0);

	int cnt = 0;

	init_time();
	while (!__atomic_load_n(&timer_end, __ATOMIC_SEQ_CST)) {
		int prev_cnt = cnt;

		while(size && cnt < QD) {
			sqe = io_uring_get_sqe(&ring);
			int offset = get_rand_offset(size);
			if (is_read) {
				ret = posix_memalign((void**)&buf, ALIGN, blk_size);
				assert(ret == 0);
				io_uring_prep_read(sqe, fd, buf, blk_size, offset);
				io_uring_sqe_set_data(sqe, buf);
			} else {
				io_uring_prep_write(sqe, fd, buf, blk_size, offset);
			}
			++cnt;
		}

		if (prev_cnt != cnt) {
			assert((ret = io_uring_submit(&ring)) >= 0);
		}

		int got_comp = 0;
		while (cnt && !__atomic_load_n(&timer_end, __ATOMIC_SEQ_CST)) {
			if (!got_comp) {
				ret = io_uring_wait_cqe(&ring, &cqe);
				got_comp = 1;
			} else {
				ret = io_uring_peek_cqe(&ring, &cqe);
				if (ret == -EAGAIN) {
					break;
				}
			}
			if (ret == -EINTR) {
				break;
			}
			assert(ret >= 0);
			if (!cqe)
				break;

			--cnt;
			++iocnt;
			if(is_read){
				check(io_uring_cqe_get_data(cqe), blk_size);
				free(io_uring_cqe_get_data(cqe));
			}
			io_uring_cqe_seen(&ring, cqe);
		}
	}
	
	io_uring_queue_exit(&ring);

	printf("io_uring state:%s iocnt=%d\n", is_read ? "read" : "write", iocnt);
}

void posix_aio_test(int fd, int size, char *buf, int is_read) {
    int ret, iocnt = 0;

	init_time();

    int offset = 0;
	struct aiocb** my_aiocb = calloc(QD, sizeof(struct aiocb));
	for (int i = 0; i < QD; ++i) {
		offset = get_rand_offset(size);
		my_aiocb[i] = calloc(1, sizeof(struct aiocb));
		*my_aiocb[i] = (struct aiocb) {
			// .aio_buf = ,
			.aio_fildes = fd,
			.aio_nbytes = blk_size,
			.aio_offset = offset,
			.aio_lio_opcode = is_read ? LIO_READ : LIO_WRITE
		};
		if (is_read) {
			ret = posix_memalign((void**)&(my_aiocb[i] -> aio_buf), ALIGN, blk_size);
			assert(ret == 0);
		}
		else 
			my_aiocb[i] -> aio_buf = buf;
	}
	
	lio_listio(LIO_NOWAIT, my_aiocb, QD, NULL);

	while (!__atomic_load_n(&timer_end, __ATOMIC_SEQ_CST)) {
		ret = aio_suspend(my_aiocb, QD, NULL);
		assert(ret == 0);
		eprintf("chk\n");
		// usleep(50);

		for (int i = 0; i < QD; ++i) {
		eprintf("p\n");
			ret = aio_error(my_aiocb[i]);

		eprintf("t\n");
			if (ret == EINPROGRESS) {
		eprintf("u\n");
				continue;
			}
			if (ret != 0) {
				eprintf("ret=%d\n", ret);
			}
			assert(ret == 0);
			// if (ret != 0)
			// 	continue;

			if (is_read) {
				check(my_aiocb[i] -> aio_buf, blk_size);
			}
			
		eprintf("q\n");
			assert((ret = aio_return(my_aiocb[i])) > 0);
			++iocnt;

			offset = get_rand_offset(size);
			char *tmp = my_aiocb[i] -> aio_buf;
			*my_aiocb[i] = (struct aiocb) {
				.aio_buf = tmp,
				.aio_fildes = fd,
				.aio_nbytes = blk_size,
				.aio_offset = offset,
			};
			
		eprintf("r\n");
			if (is_read)
				assert((ret = aio_read(my_aiocb[i])) == 0);
			else // bug: !is_read ---> aio_write
				assert((ret = aio_write(my_aiocb[i])) == 0);
				
		eprintf("s\n");
		}
	}

	//wait for the rest to finish
	// usleep(1000);

	// if (is_read) {
	// 	for (int i = 0; i < QD; ++i) {
	// 		free(my_aiocb[i] -> aio_buf);
	// 	}
	// }
	// for (int i = 0; i < QD; ++i) {
	// 	free(my_aiocb[i]);
	// }
	// free(my_aiocb);

	printf("posix_aio state:%s iocnt=%d\n", is_read ? "read" : "write", iocnt);
}

void libaio_test(int fd, int size, char *buf, int is_read) {
    int ret, iocnt = 0;
	
	init_time();

	io_context_t myctx;
	memset(&myctx, 0, sizeof(myctx));
	io_setup(QD, &myctx);

	struct io_event *events = calloc(QD, sizeof(struct io_event));

	int offset = 0, cnt = 0;

	// while(size || cnt) {
	while (!__atomic_load_n(&timer_end, __ATOMIC_SEQ_CST)) {
		int restcnt = QD - cnt;
		if (restcnt > 0) {
			struct iocb *ioq[restcnt];
			for (int i = 0; i < restcnt; ++i) {
				offset = get_rand_offset(blk_size);
				ioq[i] = calloc(1, sizeof(struct iocb));
				assert(ioq[i]);
				if (is_read) {
					ret = posix_memalign((void**)&buf, ALIGN, blk_size);
					io_prep_pread(ioq[i], fd, buf, blk_size, offset);
				} else {
					io_prep_pwrite(ioq[i], fd, buf, blk_size, offset);
				}
				++cnt;
			}

			ret = io_submit(myctx, restcnt, ioq);
			assert(ret >= 0);
		}

		ret = io_getevents(myctx, 1, QD, events, NULL);
		for (int i = 0; i < ret; ++i) {
			// printf("offset=%lld ret=%ld buf=%ld\n", events[i].obj -> u.c.offset, events[i].res, (char*)events[i].obj->u.c.buf - buf);
			if (is_read) {
				check(events[i].obj -> u.c.buf, blk_size);
				free(events[i].obj -> u.c.buf);
			}
			free(events[i].obj);
			--cnt;
			++iocnt;
		}
	}

	free(events);
	io_destroy(myctx);
	
	printf("libaio state:%s iocnt=%d\n", is_read ? "read" : "write", iocnt);
}

int main(int argc, char *argv[]) {
	int ret;
	
	srand(1234);
	// init_sigaction();

    assert(argc == 6);
    int infd = open(argv[2], O_RDONLY | O_DIRECT);
    int outfd = open(argv[3], O_WRONLY | O_CREAT | O_TRUNC | O_DIRECT, 0644);
    assert(infd >= 0 && outfd >= 0);

    struct stat st;
	fstat(infd, &st);
	int size = st.st_size;

    char *buf;
	ret = posix_memalign((void**)&buf, ALIGN, blk_size);
    assert(buf && ret == 0);
    memset(buf, 'a', blk_size);

	sscanf(argv[4], "%d", &QD);
	sscanf(argv[5], "%d", &threads);

	if (strcmp(argv[1], "sync") == 0) {
		sync_test(infd, size, buf, 1);
		sync_test(outfd, size, buf, 0);
	} else if (strcmp(argv[1], "io_uring") == 0) {
		io_uring_test(infd, size, buf, 1);
		io_uring_test(outfd, size, buf, 0);
	} else if (strcmp(argv[1], "posix_aio") == 0) {
		posix_aio_test(infd, size, buf, 1);
		posix_aio_test(outfd, size, buf, 0);
	} else if (strcmp(argv[1], "libaio") == 0) {
		libaio_test(infd, size, buf, 1);
		libaio_test(outfd, size, buf, 0);
	}

    free(buf);
	close(infd);
	close(outfd);

	printf("syscalls:\n");
	ret = system("grep ctxt /proc/$(pidof iops)/status | awk \'{ print $2 }\'");

    return 0;
}