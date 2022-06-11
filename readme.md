## AIO-bpf-bench

### Usage

```
sudo mount -t debugfs debugfs /sys/kernel/debug
make compile
sudo python3 test.py
```

### Test Design
**Libraries**
- `read`/`write` syscalls
- IO_uring
- posix aio
- linux native aio

**Tasks and Variables**
1. Pressure: lib, file size, blk_size, queue_depth
2. Throughput: the same as 1
3. IOPS: lib, qd

**Target**
- time(ms)
- syscalls
- IOPS 

### Result: Pressure

**Syscall Count**
```
Setting:
512M SEQ
64Q blk=32k
16384 blocks in total
```


|  Library  | Syscalls | Context Switches |
|:---------:|----------|------------------|
|   Native  | 32828    | 32771            |
|  IO_uring | 17663    | 18282            |
| POSIX_aio | 31862    | 32800            |
| Linux_aio | 1652     | 16991            |

**Improvement:**
```
usleep(1000);
```
Before:  
- Read time: 392.646000  
- Write time: 1896.780000  
- Syscall: 16542

After:  
- Read time: 476.732000  
- Write time: 1968.523000  
- Syscall: 2023

### Result: IOPS

```
Setting:
512M RANDOM
64Q/128Q blk=4k
Calculate IOPS in 10s
```

**QD=64**

|  Library  | IOPS read | IOPS write | Syscalls |
|:---------:|-----------|------------|----------|
|   Native  | 7233      | 9063       | 16296    |
|  IO_uring | 124604    | 9948       | 14811    |
| POSIX_aio | 6051      | 8378       | 14402    |
| Linux_aio | 134911    | 11238      | 16495    |

**QD=128**

|  Library  | IOPS read | IOPS write | Syscalls |
|:---------:|-----------|------------|----------|
|  IO_uring | 114679    | 11214      | 12041    |
| POSIX_aio | 6384      | 9574       | 15915    |
| Linux_aio | 155398    | 11443      | 15183    |

*\* QD is meaningless for sync IO.*

## Throughput

A thorough test with python.

```
Setting:
512M RANDOM
ty_list = ["sync", "io_uring", "libaio", "posix_aio"]
sz_list = ["32M", "512M"]
QD_blk_list = [(64, 32), (128, 32), (512, 32), (64, 1024), (512, 1024)]
```

*Time Unit: GBytes/s*  
*Block Size Unit: KBytes*

**32MB Read**

| Lib / (QD, blk) | (64, 32) | (128, 32) | (512, 32) | (64, 1024) | (512, 1024) |
|:---------------:|----------|-----------|-----------|------------|-------------|
|      Native     | 0.20     | 0.19      | 0.21      | 1.14       | 1.34        |
|     IO_uring    | 1.08     | 1.60      | 2.97      | 2.93       | 2.79        |
|    POSIX_aio    | 0.13     | 0.13      | 0.12      | 1.18       | 1.36        |
|    Linux_aio    | 0.21     | 0.24      | 0.29      | 0.38       | 0.25        |

**32MB Write**

| Lib / (QD, blk) | (64, 32) | (128, 32) | (512, 32) | (64, 1024) | (512, 1024) |
|:---------------:|----------|-----------|-----------|------------|-------------|
|      Native     | 0.21     | 0.25      | 0.23      | 1.09       | 1.07        |
|     IO_uring    | 0.20     | 0.20      | 0.23      | 1.05       | 1.04        |
|    POSIX_aio    | 0.12     | 0.17      | 0.14      | 0.99       | 1.12        |
|    Linux_aio    | 0.14     | 0.13      | 0.13      | 0.25       | 0.32        |

**512MB Read**

| Lib / (QD, blk) | (64, 32) | (128, 32) | (512, 32) | (64, 1024) | (512, 1024) |
|:---------------:|----------|-----------|-----------|------------|-------------|
|      Native     | 0.18     | 0.16      | 0.18      | 1.67       | 1.43        |
|     IO_uring    | 1.13     | 1.91      | 3.04      | 3.64       | 4.37        |
|    POSIX_aio    | 0.13     | 0.14      | 0.11      | 1.18       | 1.55        |
|    Linux_aio    | 1.47     | 2.03      | 2.37      | 2.11       | 2.44        |


**512MB Write**

| Lib / (QD, blk) | (64, 32) | (128, 32) | (512, 32) | (64, 1024) | (512, 1024) |
|:---------------:|----------|-----------|-----------|------------|-------------|
|      Native     | 0.22     | 0.21      | 0.23      | 0.52       | 0.52        |
|     IO_uring    | 0.20     | 0.21      | 0.19      | 0.38       | 0.39        |
|    POSIX_aio    | 0.19     | 0.18      | 0.12      | 0.53       | 0.53        |
|    Linux_aio    | 0.22     | 0.21      | 0.21      | 0.49       | 0.44        |
