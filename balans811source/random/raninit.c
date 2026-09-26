#include "raninit.h"

#ifdef USE_MVER     /* Then we need the definition of logf_fma(x) from logf_def.h */

#ifdef USE_FMA_LOGF /* Use fmaf(3) for all versions; clang -O3 -ffast-math replaces fmaf() with mul+add, good! */
#define ALWAYS_USE_FMA_LOGF
#else
#define USE_FMA_LOGF
#endif /* ifdef USE_FMA_LOGF */

#endif /* ifdef USE_MVER */

#include "logf_def.h"

// Global for entire program, allows inlining *genrand(), but thus not thread-safe
sfmt_t           raninit_sfmt;   // Basic generation and caching of 32-bit uints

/* @(#)gettime.c        2017 */
/* ======================== get_the_time ======================= */
#include <time.h>
#include <stdio.h>

#if defined MS_DOS || defined QPC
#include "../winver.h"
long ticks()
{
  LARGE_INTEGER li;
  QueryPerformanceCounter(&li);
  return li.QuadPart;
}
#else
long ticks()
{
  struct timespec tp;
  clock_gettime(CLOCK_MONOTONIC, &tp);
  return tp.tv_nsec;
}
#endif

static void get_the_time(int *day, int *month, int *year, int *hour, int *minute, int *second)
/* get the local date and time in the 6 integers */
/* pointed to by the arguments */
{
  struct tm *ltm;
  time_t now;
  now = time((time_t*)0);
  ltm = localtime ( &now );
  *day   = (int) ltm->tm_mday;
  *month = (int) (1 + ltm->tm_mon);
  *year  = (int) (1900 + ltm->tm_year); /* help fight the Y2K bug */
  *hour  = (int) ltm->tm_hour;
  *minute= (int) ltm->tm_min;
  *second= (int) ltm->tm_sec;
}
void print_the_time(void)
{
  int dd, mm, yy, hr, min, sec;
  get_the_time(&dd,&mm,&yy,&hr,&min,&sec);
  printf("%02d-%02d-%4d %02d:%02d:%02d\n",dd,mm,yy,hr,min,sec);
}


// raninit_gen_mlogfrand_all: eat values from our global cache of random uints and cache -logf(rnd) from them.
// Split loops here so gcc 7.3 on Sandybridge glibc will vectorise with AVX (256-bit _ZGVcN8v___logf_finite).
// Define via macro to allow for different implementations of LOGF() combined with optimising for different CPUs.
#define RANINIT_GEN_MLOGFRAND_ALL(suffix)                        \
void raninit_gen_mlogfrand_all ## suffix (raninit_state_t *rstp) \
{                                                                \
  float tmp_flt[RANINIT_NFLOAT] ALIGN_BEST;                      \
                                                                 \
  for(int i=0; i<RANINIT_NFLOAT; i++)                            \
    tmp_flt[i] = fgenrand_mt(rstp);                              \
                                                                 \
  for(int i=0; i<RANINIT_NFLOAT; i++)                            \
    rstp->fcache[i] = -LOGF(tmp_flt[i]);                         \
                                                                 \
  rstp->fidx = 0;                                                \
}

#ifndef USE_MVER
RANINIT_GEN_MLOGFRAND_ALL()
#else

__attribute__ ((flatten)) __attribute__ ((target ("avx2,fma"))) static RANINIT_GEN_MLOGFRAND_ALL(_avx2)
#ifndef ALWAYS_USE_FMA_LOGF
#undef LOGF
#define LOGF(x) logf(x)
#endif /* ifndef ALWAYS_USE_FMA_LOGF although in practice we'll almost always use logf_fma when USE_MVER */
static RANINIT_GEN_MLOGFRAND_ALL(_default)

// void raninit_gen_mlogfrand_all(raninit_state_t *rstp) // Defined inline static in raninit.h instead

#endif /* USE_MVER */

static void raninit_basic(sfmt_t *sfmtp)
{
  uint32_t seed_array[6];
  int day,month,year,hour,minute,second;

  get_the_time(&day,&month,&year,&hour,&minute,&second);

  seed_array[0]= (uint32_t)second;
  seed_array[1]= (uint32_t)(minute+second);
  seed_array[2]= (uint32_t)(hour+second+ticks());
  seed_array[3]= (uint32_t)(day*7-1+ticks());
  seed_array[4]= (uint32_t)(month*second*60);
  seed_array[5]= (uint32_t)(year*30/(second+1)+ticks());

  sfmt_init_by_array(sfmtp, seed_array, 6);
}


inline static void raninit_init_delayed_fcache(raninit_state_t *rstp) {

#ifdef USE_MVER
  enum cpu_targets cpu_target = raninit_cpu_support_enum();
  if (cpu_target == TARGET_AVX2)
    rstp->raninit_gen_func = &raninit_gen_mlogfrand_all_avx2;
  else // Intentionally don't use plain AVX since tests show that generic faster with clang (when no libmvec)
    rstp->raninit_gen_func = &raninit_gen_mlogfrand_all_default;
#endif
  rstp->fidx = RANINIT_NFLOAT; // Will then automatically refill cache at next call of mlogfrand_mt()
}


void raninit(void) { raninit_basic(&raninit_sfmt); }

void raninit_multi(raninit_state_t *rstp)
{
  raninit_basic(&(rstp->sfmt));
  raninit_init_delayed_fcache(rstp);
}
