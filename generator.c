
/**
 * usage: ./generator 512M infile
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#include <stdio.h>
// 1 GBytes
#define SZ (1024*1024*1024)

char buf[SZ];

int main(int argc, char *argv[]) {
	assert(argc >= 3);
	int sz0;
	char c;
	sscanf(argv[1], "%d%c", &sz0, &c);
	sz0 *= (c == 'M' ? 1024 * 1024 : c == 'K' ? 1024 : 1);
	
	for (int i = 0; i < sz0; ++i) buf[i] = 'a';
	for (int i = 2; i < argc; ++i) {
		int fd = open(argv[i], O_CREAT | O_RDWR | O_TRUNC, S_IRWXU);
		assert(fd >= 0);
		int res = write(fd, buf, sz0);
		assert(res == sz0);
		fsync(fd);
		close(fd);
	}
}