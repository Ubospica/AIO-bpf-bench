
BPF_HASH(time_read, u64, u64);
BPF_HASH(time_write, u64, u64);

void timer_read(struct pt_regs *ctx) {
	u64 key = 0, *ptr, delta, curtm = bpf_ktime_get_ns();

	ptr = time_read.lookup(&key);
	if (ptr == NULL) {
		time_read.update(&key, &curtm);
	} else {
		delta = curtm - *ptr;
		time_read.update(&key, &delta);
	}
}

void timer_write(struct pt_regs *ctx) {
	u64 key = 0, *ptr, delta, curtm = bpf_ktime_get_ns();

	ptr = time_write.lookup(&key);
	if (ptr == NULL) {
		time_write.update(&key, &curtm);
	} else {
		delta = curtm - *ptr;
		time_write.update(&key, &delta);
	}
}