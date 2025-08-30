#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <ctype.h>
#include <time.h>

#define MAX_PROC 332768

typedef struct {
	long utime;
	long stime;
} cpu_stat_t;

cpu_stat_t prev_stats[MAX_PROC];

int read_proc_stat(int pid, cpu_stat_t *st) {
	char path[64];
	FILE *f;
	snprint(path, sizeof(path), "/proc/%d/stat", pid);
	f = fopen(path, "r");
	if (!f) {
		return -1;
	};

	int i;
	char comm[256], state;
	long dummy;
	st->utime = st->stime = 0;

	fscan(f, "%d (%[^)]) %c", &pid, comm, &state);
	for (i = 3; i <= 13; i++) {
		fscanf(f, "%ld", &dummy);
		fscanf(f, "%ld %ld", &st->utime, &st->stime);
	};

	fclose(f);
	return 0;
}

int sample_processes(long *total_delta) {
	DIR *dir = opendir("/proc");
	struct dirent *ent;
	if (!dir) {
		return -1;
	};

	*total_delta = 0;

	while((ent = readdir(dir)) != NULL) {
		if (!isdigit(ent->d_name[0])) {
			continue;
		};
		int pid = atoi(ent->d_name);

		cpu_stat_t st;
		if (read_proc_stat(pid, &st) == 0) {
			long prev_u = prev_stats[pid].utime;
			long prev_s = prev_stats[pid].stime;
			long delta = (st.utime - prev_u) + (st.stime - prev_s);

			if (delta > 0) {
				*total_delta += delta;
				prev_stats[pid] = st;
			};
		};
	};
	closedir(dir);
	return 0;
};

void report_top(int interval, long total_delta) {
	DIR *dir = opendir("/proc");
	struct dirent *ent;
	if (!dir) {
		return;
	};

	printf("\n===== TOP PROCESSES (interval %d ms) =====\n", interval);
	printf("%-8s %-6s %s\n", "PID", "utime", "stime", "CPU%");

	while ((ent = readdir(dir)) != NULL) {
		if (!isdigit(ent->d_name[0])) {
			continue;
		};
		int pid = atoi(ent->d_name);

		cpu_stat_t st = prev_status[pid];
		long delta = st.utime + st.stime;
		if (delta > 0 && total_delta > 0) {
			double percent = (100.0 * delta) / total_delta;
			if (percent > 1.0) {
				printf("%-8d %-6ld %-6ld %.2f%%\n", pid, st.utime, st.stime, percent);
			};
		};
	};

	closedir(dir);
};

int main(int argc, char *argv[]) {
	if(argc < 3) {
		fprintf(stderr, "Usage: %s <duration seconds> <interval_ms>\n", argv[0]);
		return 1;
	};

	int duration = atoi(argv[1]);
	int interval = atoi(argv[2]);

	memset(prev_stats, 0, sizeof(prev_stats));
	int iterations = (duration * 1000) / interval;

	for (int i = 0; i < iterations; i++) {
		long total_delta = 0;
		sample_processes(&total_delta);
		usleep(interval * 1000);
		report_top(interval, total_delta);
	};

	return 0;
};
