#include "SFMT.h"

#ifdef HAVE_SSE2
#define ALIGN_BEST __attribute__((aligned(32)))
#else
#define ALIGN_BEST
#endif

#ifdef USE_MVER
#include <stdlib.h>  // getenv(3) used for disabling AVX2/AVX funcs via env, useful when profiling (PGO)

enum cpu_targets {
  TARGET_GENERIC = 0,
  TARGET_AVX,
  TARGET_AVX2
};

inline static enum cpu_targets raninit_cpu_support_enum(void) {
  if (__builtin_cpu_supports("avx2") && __builtin_cpu_supports("fma") && ! getenv("NO_AVX2"))
    return TARGET_AVX2;
  else if (__builtin_cpu_supports("avx") && ! getenv("NO_AVX"))
    return TARGET_AVX;
  return TARGET_GENERIC;
}

inline static char *raninit_cpu_support_str(void) {
  enum cpu_targets cpu_target = raninit_cpu_support_enum();
  if      (cpu_target == TARGET_AVX2) return "AVX2,FMA";
  else if (cpu_target == TARGET_AVX)  return "AVX";
  else                                return "generic";
}
#endif /* USE_MVER */

/* === Begin global variable with associated functions that are NOT thread-safe */

extern sfmt_t raninit_sfmt; // Defined in raninit.c

void raninit(void);  // Defined in raninit.c

inline static uint32_t genrand(void) { return sfmt_genrand_uint32(&raninit_sfmt); }

/* Return (signed) int in interval 0..N-1 with _almost_ uniform distribution; assume N>1*/
inline static int iigenrand(int N) { return (int) (genrand() % ((uint32_t)N)); }

/* No longer used in balans itself, only on old stand-alone test program raninittest: */
inline static double dgenrand(void) { return sfmt_genrand_real2(&raninit_sfmt); }

/* === End of non-thread-safe section (seemed to work with OpenMP anyway, but slow due to contention) */

/* === Begin caching of (minus) logf(3) of randoms, allowing the logs to be calculated in vectorised loops.
 *     Only used for ensembles, so needs thread-safe implementation via separate state for each member.
 *     The ensembles need random uints as well, so full state combines a sfmt_t with our own float cache.
 */

// 20190721: Intended for when random in the hot function is smaller than 9, increasing precision near 0.
//   In that case we consume a new random uint32 and map that to this small region like ]0;8.5] of orig ints.
//   No inline hint because only called about once in 500 million random floats (cold path)
static float raninit_uint32_to_float_small(uint32_t v) {
  return 2.303929616e-19F + (float)v * 4.607859233e-19F;
  // 0.5 * 8.5*2**(-64) + v * 8.5*2**(-64) corresponding to uints 0-8 in fgenrand()
}

/* Float version of similar in SFMT.h, by Ulrik Dickow 2017-2019, faster than casting */
inline static float raninit_uint32_to_float(uint32_t v) {
  // return 0.5F/4294967296.0F + v * (1.0F/4294967296.0F); // Interval ]0;1] 20190720 more useful for logf(3)
  return (float)v * (float) (1.0/4294967295.0); // 20190721: Intended for use only with v > 8 (hot path)
  /* divided by 2**32 */
}

/* Number of floats in a mlogfrand() cache.  Would have to be at least 624 if sfmt_fill_array32 were used. */
#define RANINIT_NFLOAT 256

typedef struct ALIGN_BEST RANINIT_STATE_T raninit_state_t;
struct ALIGN_BEST RANINIT_STATE_T {
  float fcache[RANINIT_NFLOAT];  // Cache of random -logf's.  Define first to maximise chance of good alignment
  int fidx;    // Index into fcache of next available (minus logf of) float
  int     dummy1; // Padding to improve alignment (clang -Wpadded)
  void (*raninit_gen_func)(raninit_state_t *rstp); // CPU-dependent func if USE_MVER, otherwise just padding
  sfmt_t sfmt; // SFMT's own uint cache, used for returning uints as well as caching floats
};

inline static uint32_t genrand_mt(raninit_state_t *rstp) {
  return sfmt_genrand_uint32(&(rstp->sfmt));
}

/* Return (signed) int in interval 0..N-1 with _almost_ uniform distribution; assume N>1*/
inline static int iigenrand_mt(raninit_state_t *rstp, int N) {
  return (int) (genrand_mt(rstp) % ((uint32_t)N));
}

#ifdef USE_MVER
inline static void raninit_gen_mlogfrand_all(raninit_state_t *rstp)
{
  rstp->raninit_gen_func(rstp); // Generator function already initialised by raninit_init_delayed_fcache()
}
#else
void raninit_gen_mlogfrand_all(raninit_state_t *rstp); // Defined in raninit.c
#endif

void raninit_multi(raninit_state_t *rstp);             // Defined in raninit.c

inline static float fgenrand_mt(raninit_state_t *rstp) {
  uint32_t u = genrand_mt(rstp); // For u in 0..8 we will instead eat a new random and map that like u in 0..8.5
  if (u > 8)                     // This way -logf of the float may reach almost 43 instead of only 22.2
    return raninit_uint32_to_float(u);
  else
    return raninit_uint32_to_float_small(genrand_mt(rstp));
}

/**
 * This function generates and returns -logf(rnd) where rnd is a pseudorandom float in ]0;1].
 * The supplied raninit_state must be initialised via raninit_multi() before first use of this function.
 * Modeled after sfmt_genrand_uint32().
 */
inline static float mlogfrand_mt(raninit_state_t *rstp) {
    if (rstp->fidx >= RANINIT_NFLOAT) {
        raninit_gen_mlogfrand_all(rstp);
        rstp->fidx = 0;
    }
    float f = rstp->fcache[rstp->fidx++];
    return f;
}

/* === End thread-safe caching of (minus) logf(3) of randoms and uints in general */

long ticks(void);
void print_the_time(void);
