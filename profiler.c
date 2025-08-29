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

	printf("\n===== TOP PROCESSES (interval %d ms =====\n", interval);
