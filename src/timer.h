#ifndef GPUSOCK_TIMER_H
#define GPUSOCK_TIMER_H

typedef struct timer {
	unsigned long long total;
	unsigned long long val;
	unsigned long cnt;
} gs_timer;


#ifdef TIMERS_ENABLED

#include <time.h>

static inline unsigned long long get_time()
{
	struct timespec ts;
	unsigned long long time;

	if(clock_gettime(CLOCK_MONOTONIC, &ts) == -1) {
		perror("clock gettime");
		exit(EXIT_FAILURE);
	}
	time = ts.tv_sec * 1000000000 + ts.tv_nsec;
	//printf("time: %llu ns\n", time);

	return time;
}


#define TIMER_START(tm)	do { (tm)->val = get_time(); } while (0)
#define TIMER_STOP(tm)	do { (tm)->total += get_time() - (tm)->val; ++(tm)->cnt; } while (0)
#define TIMER_GET(tm)	do { (tm)->total += get_time() - (tm)->val; (tm)->val = get_time(); } while (0)
#define TIMER_RESET(tm)	do { (tm)->total = (tm)->val = 0; (tm)->cnt = 0; } while (0)
#define TIMER_TOTAL(tm)	((tm)->total)
#define TIMER_COUNT(tm)	((tm)->cnt)
#define TIMER_AVG(tm)	((tm)->cnt ? ((tm)->total / (tm)->cnt) : 0)

#define TIMER_TO_SEC(t) (t / 1000000000.0)
#define TIMER_TO_USEC(t) (t / 1000.0)

#else

#define TIMER_START(a)
#define TIMER_STOP(a)
#define TIMER_GET(a)
#define TIMER_RESET(a)
#define TIMER_TOTAL(a) 0ULL
#define TIMER_COUNT(a) 0UL
#define TIMER_AVG(a) 0ULL

#define TIMER_TO_SEC(a) 0
#define TIMER_TO_USEC(a) 0

#endif	/* TIMER_ENABLED */

#endif	/* GPUSOCK_TIMER_H */
