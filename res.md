## Todo
1. posix aio
2. native aio
3. IOPS latency
4. syscall
5. 其他测试：read 文章内容（加速io_uring） 多线程

测试：
1. pressure & throughput: lib, sz, qd, thread
1. latency: lib, qd
2. IOPS & context switch: lib, qd 

syscalls:

512M 64Q blk=32

16384 blocks

sync: 32828
io_uring: 17663
posix_aio: 31892
libaio: 1652

context switches:
sync: 32771
io_uring: 18282
posix_aio: 32800
libaio: 16991

## Ques
1. io_uring 事件？
2. 分块？
3. IOPS and latency:
	1. IOPS: 队列, finished / time
	2. latency: 单个, avg time

## Latency

ty=sync sz=32M QD=64 blk=32
read time(s): 0.136768418
write time(s): 0.123670437
ty=sync sz=32M QD=128 blk=32
read time(s): 0.144716307
write time(s): 0.120943467
ty=sync sz=32M QD=512 blk=32
read time(s): 0.135203025
write time(s): 0.105852482
ty=sync sz=32M QD=64 blk=1024
read time(s): 0.01614789
write time(s): 0.02668741
ty=sync sz=32M QD=512 blk=1024
read time(s): 0.016218133
write time(s): 0.052806825
ty=sync sz=512M QD=64 blk=32
read time(s): 2.110610681
write time(s): 1.980956294
ty=sync sz=512M QD=128 blk=32
read time(s): 2.075518512
write time(s): 2.009417228
ty=sync sz=512M QD=512 blk=32
read time(s): 2.050625915
write time(s): 1.990008713
ty=sync sz=512M QD=64 blk=1024
read time(s): 0.231216333
write time(s): 0.949920809
ty=sync sz=512M QD=512 blk=1024
read time(s): 0.227365884
write time(s): 0.969042238
ty=io_uring sz=32M QD=64 blk=32
read time(s): 0.011910011
write time(s): 0.125513517
ty=io_uring sz=32M QD=128 blk=32
read time(s): 0.010634302
write time(s): 0.119824361
ty=io_uring sz=32M QD=512 blk=32
read time(s): 0.008346569
write time(s): 0.116001024
ty=io_uring sz=32M QD=64 blk=1024
read time(s): 0.007182685
write time(s): 0.026819379
ty=io_uring sz=32M QD=512 blk=1024
read time(s): 0.007267819
write time(s): 0.056068116
ty=io_uring sz=512M QD=64 blk=32
read time(s): 0.152973686
write time(s): 2.004801073
ty=io_uring sz=512M QD=128 blk=32
read time(s): 0.108977138
write time(s): 1.940577877
ty=io_uring sz=512M QD=512 blk=32
read time(s): 0.091695261
write time(s): 1.947431704
ty=io_uring sz=512M QD=64 blk=1024
read time(s): 0.088856097
write time(s): 0.989037432
ty=io_uring sz=512M QD=512 blk=1024
read time(s): 0.094902105
write time(s): 0.95332893
ty=libaio sz=32M QD=64 blk=32
read time(s): 0.115128109
write time(s): 0.191308949
ty=libaio sz=32M QD=128 blk=32
read time(s): 0.112503943
write time(s): 0.214017685
ty=libaio sz=32M QD=512 blk=32
read time(s): 0.133610375
write time(s): 0.186224602
ty=libaio sz=32M QD=64 blk=1024
read time(s): 0.127725038
write time(s): 0.115149072
ty=libaio sz=32M QD=512 blk=1024
read time(s): 0.113900001
write time(s): 0.124318638
ty=libaio sz=512M QD=64 blk=32
read time(s): 0.232752936
write time(s): 1.92121681
ty=libaio sz=512M QD=128 blk=32
read time(s): 0.194784683
write time(s): 1.864218989
ty=libaio sz=512M QD=512 blk=32
read time(s): 0.160962926
write time(s): 1.869796029
ty=libaio sz=512M QD=64 blk=1024
read time(s): 0.169440418
write time(s): 1.029616929
ty=libaio sz=512M QD=512 blk=1024
read time(s): 0.17034087
write time(s): 1.060998319
ty=posix_aio sz=32M QD=64 blk=32
read time(s): 0.158765641
write time(s): 0.137403648
ty=posix_aio sz=32M QD=128 blk=32
read time(s): 0.158826596
write time(s): 0.13586638
ty=posix_aio sz=32M QD=512 blk=32
read time(s): 0.170743114
write time(s): 0.180158702
ty=posix_aio sz=32M QD=64 blk=1024
read time(s): 0.017134911
write time(s): 0.026135053
ty=posix_aio sz=32M QD=512 blk=1024
read time(s): 0.016074352
write time(s): 0.055934387
ty=posix_aio sz=512M QD=64 blk=32
read time(s): 2.327393028
write time(s): 2.144257659
ty=posix_aio sz=512M QD=128 blk=32
read time(s): 2.303401143
write time(s): 2.140017977
ty=posix_aio sz=512M QD=512 blk=32
read time(s): 3.367355854
write time(s): 3.43193437
ty=posix_aio sz=512M QD=64 blk=1024
read time(s): 0.239432849
write time(s): 1.047982157
ty=posix_aio sz=512M QD=512 blk=1024
read time(s): 0.233110855
write time(s): 0.971263285

| Lib / Size | 32M R(s) | 32M W(s) | 128M R(s) | 128M W(s) | 512M R(s) | 512M W(s) | MB/s |
|:----------:|----------|----------|-----------|-----------|-----------|-----------|------|
|   Native   | fd       |          |           |           |           |           |      |
|  IO_uring  |          |          |           |           |           |           |      |
|  POSIX_aio |          |          |           |           |           |           |      |
|  Linux_aio |          |          |           |           |           |           |      |





|  Library  | 32M time(s) | 32M syscalls | 128M time(s) | MB/s | 4k IOPS |
|:---------:|-------------|--------------|--------------|------|---------|
|   Native  | fd          |              |              |      |         |
|  IO_uring |             |              |              |      |         |
| POSIX_aio |             |              |              |      |         |
| Linux_aio |             |              |              |      |         |

