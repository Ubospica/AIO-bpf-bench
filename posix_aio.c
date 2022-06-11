/**
 * Usage: ./posix-aio infile outfile
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <aio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

#define ALIGN 1024

void test_read(int fd, int size) {
    int ret;

    char *buf;
	posix_memalign((void**)&buf, ALIGN, size);
    assert(buf >= 0);

    struct aiocb my_aiocb = {
        .aio_buf = buf,
        .aio_fildes = fd,
        .aio_nbytes = size,
        .aio_offset = 0
    };

    ret = aio_read(&my_aiocb);
    assert(ret >= 0);

    struct aiocb *aiocb_list[1] = {&my_aiocb};
    aio_suspend(aiocb_list, 1, NULL);
    
}

void test_write(int fd, int size, char *content) {

}

int main(int argc, char *argv[])
{
    assert(argc == 3);
    int infd = open(argv[1], O_RDONLY | O_DIRECT);
    int outfd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC | O_DIRECT, 0644);
    assert(infd >= 0 && outfd >= 0);

    struct stat st;
	fstat(infd, &st);
	int size = st.st_size;

    char *content = malloc(size);
    memset(content, 'a', size);

    test_read(infd, size);
    test_write(outfd, size, content);

    free(content);


    // int fd, ret;

    // fd = open(__FILE__, O_RDONLY);
    // if (fd < 0) {
    //     perror("open");
    //     return -1;
    // }

//     memset(&my_aiocb, 0, sizeof(struct aiocb));

//     my_aiocb.aio_buf = malloc(BUFSIZE);
//     if (!my_aiocb.aio_buf) {
//         perror("malloc");
//         goto end;
//     }

//     my_aiocb.aio_fildes = fd;
//     my_aiocb.aio_nbytes = BUFSIZE;
//     my_aiocb.aio_offset = 0;

//     ret = aio_read(&my_aiocb);
//     if (ret < 0) {
//         perror("aio_read");
//         goto end;
//     }

//     while (aio_error(&my_aiocb) == EINPROGRESS);

//     if ((ret = aio_return(&my_aiocb)) > 0)
//         printf("read %d bytes.\n", ret);
//     else
//         perror("aio_read");

//     free((void*)(my_aiocb.aio_buf));
// end:
//     close(fd);
//     return ret;
}