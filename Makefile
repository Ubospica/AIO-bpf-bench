all: gen

.PHONY: gen
gen: generator.c
	gcc generator.c -o generator
	./generator 32M a.txt b.txt c.txt d.txt

io_uring: io_uring.c
	gcc io_uring.c -o io_uring -luring
	./io_uring a.txt a.new.txt

io_uring_gdb: io_uring.c
	gcc io_uring.c -o io_uring -luring -g3
	gdb --args ./io_uring a.txt a.new.txt

io_uring_valg: io_uring.c
	gcc io_uring.c -o io_uring -luring -g3
	valgrind --leak-check=full --show-leak-kinds=all ./io_uring a.txt a.new.txt 

io_uring_tmp_gdb: io_uring_tmp.c
	gcc io_uring_tmp.c -o io_uring_tmp -luring -g3
	gdb --args ./io_uring_tmp a.txt a.new.txt
	
io_uring_tmp: io_uring_tmp.c
	gcc io_uring_tmp.c -o io_uring_tmp -luring
	./io_uring_tmp a.txt a.new.txt

readv: readv.c
	gcc readv.c -o readv -g3
	./readv a.txt a.new.txt

bench: test.py gen
	gcc io_uring.c -o io_uring -luring
	gcc readv.c -o readv 
	python3 test.py &
	./readv a.txt a.new.txt &
	./io_uring a.txt a.new.txt &

compile:
	gcc generator.c -o generator -Wall -O2
	gcc pressure.c -Wall -O2 -o pressure -luring -laio
	gcc iops.c -Wall -O2 -o iops -luring -laio -pedantic

compile_gdb:
	gcc generator.c -o generator -Wall -g3
	gcc pressure.c -Wall -g3 -o pressure -luring -laio

clean:
	rm generator pressure a.out infile outfile