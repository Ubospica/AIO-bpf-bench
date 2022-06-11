from bcc import BPF, USDT 
import argparse
import os

infile = 'infile'
outfile = 'outfile'
exec_name = 'pressure'

# 	./pressure lib input output QD BLK_size(KB)
#   ./pressure io_uring infile outfile 64 32
def test(ty, sz, QD, blk_size):
	usdt = USDT(path = "./" + exec_name)
	usdt.enable_probe(probe = "probe-read", fn_name = "timer_read") 
	usdt.enable_probe(probe = "probe-write", fn_name = "timer_write") 

	bpf = BPF(src_file="./bpf_source.c", usdt_contexts = [usdt])

	# gen data
	os.system("./generator %s %s" % (sz, infile))

	# test
	os.system("./%s %s %s %s %d %d" % (exec_name, ty, infile, outfile, QD, blk_size))

	print("read time(s):", *[v.value / 1e9 for k, v in bpf["time_read"].items()])
	print("write time(s):", *[v.value / 1e9 for k, v in bpf["time_write"].items()])



ty_list = ["sync", "io_uring", "libaio", "posix_aio"]
sz_list = ["32M", "512M"]
QD_blk_list = [(64, 32), (128, 32), (512, 32), (64, 1024), (512, 1024)]

if __name__ == '__main__':
	for ty in ty_list:
		for sz in sz_list:
			for QD, blk in QD_blk_list:
				print("ty=%s sz=%s QD=%d blk=%d" % (ty, sz, QD, blk))
				test(ty, sz, QD, blk)
	# test("posix_aio", "32M", 64, 32)







#     parser = argparse.ArgumentParser()
#     parser.add_argument("--type", type=str)
#     parser.add_argument("--sz", type=str)
#     # args = parser.parse_args(["--type", "io_uring", "--sz", "32M"])
#     args = parser.parse_args()

