#ifndef ON2_TIMER_H
#define ON2_TIMER_H

#if defined(_MSC_VER)
/*
 * Win32 specific includes
 */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
/*
 * POSIX specific includes
 */
#include <sys/time.h>
#endif


struct on2_usec_timer {
#if defined(_MSC_VER)
    LARGE_INTEGER  begin, end;
#else
    struct timeval begin, end;
#endif
};


static INLINE void
on2_usec_timer_start(struct on2_usec_timer *t) {
#if defined(_MSC_VER)
    QueryPerformanceCounter(&t->begin);
#else
    gettimeofday(&t->begin, NULL);
#endif
}


static INLINE void
on2_usec_timer_mark(struct on2_usec_timer *t) {
#if defined(_MSC_VER)
    QueryPerformanceCounter(&t->end);
#else
    gettimeofday(&t->end, NULL);
#endif
}


static INLINE long
on2_usec_timer_elapsed(struct on2_usec_timer *t) {
#if defined(_MSC_VER)
    LARGE_INTEGER freq, diff;
    
    diff.QuadPart = t->end.QuadPart - t->begin.QuadPart;
    if(QueryPerformanceFrequency(&freq) && diff.QuadPart < freq.QuadPart)
        return (long)(diff.QuadPart * 1000000 / freq.QuadPart);
    return 1000000;
#else
    struct timeval diff;
    
    timersub(&t->end, &t->begin, &diff);
    return diff.tv_sec?1000000:diff.tv_usec;
#endif
}


#endif
