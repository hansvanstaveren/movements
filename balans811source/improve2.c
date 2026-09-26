/* Copyright (C) 1998,2000 Gerrit van der Velde
 * Copyright (C) 2006-2021 Peter Smulders
 * Copyright (C) 2017-2023 Ulrik Dickow
 *
 * The GNU General Public License (GPL-3.0-or-later)
 * applies to this software, see COPYING.txt
 */
/* improve the balance of a pairs movement.
 * based on the Fortran subroutine "improve" by Gerrit van der Velde (1998).
 *
 * This new/alternative version of improve.c by Ulrik Dickow 2017 optimizes these metrics
 * instead of the previous ones (almost like fv did in the balans 6.7* development versions):
 *   1st priority: ssq = Sum of Squares of balans elements without vacancy (minimizes Qf)
 *   2nd priority: Vacancy quality, calculated as a weighted sum of these two contributions:
 *       a) ssVall = sum of squares of balans elements when each of pairs 1..NP is absent
 *       b) ssVmin = sum of squares of balans elements when pair bestV absent, bestV a pair
 *                   number in requested range by user, the best number for current seatings
 *       Weighted together like this (minimization goal): Fw = ssVall + weight*ssVmin
 *   3rd priority: sumd4 = d4-related combination of 3rd & 4th powers of balans elements without vacancy
 *     (^optional)         Minimizing sumd4 will minimize the 4th central moment, punishing outliers.
 * All of these sums are for unique combinations of different pairs, i.e. half the sums for
 * the full matrices (diagonals are set to zero).
 * NB: Squares in this version are absolute integers, not fractional around any average.
 */
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <math.h>
#ifdef MS_DOS
#include "winver.h"
#include <commctrl.h>
#endif

#include "random/raninit.h"
#include "optim.h"

// Allow redefining ALB for test builds (to be empty or e.g. test 16-byte alignment instead of 32)
#ifndef ALB
#define ALB ALIGN_BEST
#endif

#define SQ(x) ((x)*(x))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define eps 1.0e-5

#define NREPEAT 100     /* 1 iter = this times # of free tables, at constant temperature */

/* Only for the non-simple "crazy fluctuating temperature on speed" algorithm: */
#define NTEMP 100       /* number of temperatures in fixed heating-cooling schedule array */
#define NDELAY_RETRY 50 /* number of iter to wait before retrying optimum w/ Temp reset */
#define NLIMIT_RETRY 75 /* Temp reset without delay & no retry when fewer iter remain */

/* For debugging and for the adaptive slow cooling strategies using collected statistics (algoi>1): */
#define NITER_E1 10         /* # of iterations per energy statistics sample (= sampling period) */
#define ALG1FRACT_MAX 0.005 /* Speed limit for temperature change, so ~5% per sampling period */

static int64_t sumsq_upper2d(int NP, const int a[NP][NP])
/* Sum of squares of upper half of elements of square matrix, excluding diagonal */
{
  int64_t sum=0;
  for(int p1=0; p1<NP-1; p1++)
    for(int p2=p1+1;p2<NP; p2++)
  {
    int64_t c = a[p1][p2];
    sum += c*c;
  }
  return sum;
}

static int64_t sum_part_d4_upper2d(int NP, const int a[NP][NP], int S, int N)
/* d4-relevant power sum of upper half of elements of square matrix, excluding diagonal,
 * see https://en.wikipedia.org/wiki/Central_moment#Relation_to_moments_about_the_origin :
 *          d4^4 = sum4/N - 4*S/N*sum3/N + 6*(S/N)^2*ssq/N - 3*(S/N)^4
 *    N^2 * d4^4 = N*sum4 - 4*S*sum3 + 6*S^2*ssq/N - 3*S^4/N^2  and therefore we'll minimize
 *   sum_part_d4 = N*sum4 - 4*S*sum3                            where sum3 = sum of 3rd powers
 *          d4^4 = (sum_part_d4 + 6*S^2*ssq/N - 3*S^4/N^2)/N^2
 * NB: Doesn't matter that ssq not const, because we always minimize ssq as first priority.
 */
{
  int64_t sum3=0, sum4=0;
  for(int p1=0; p1<NP-1; p1++)
    for(int p2=p1+1;p2<NP; p2++)
  {
    int64_t c = a[p1][p2];
    int64_t c3;
    sum3 += (c3 = c*c*c);
    sum4 += c3*c;
  }
  return N*sum4 - 4*S*sum3;
}

static double d4(int S, int N, int64_t ssq, int64_t sum_part_d4)
{
  double N2 = SQ((double)N); // Casts important to prevent integer overflow
  return sqrt(sqrt(((double) sum_part_d4 + 6.0*SQ((double)S)*((double)ssq)/N
                    - 3.0*SQ(SQ((double)S))/N2)/N2));
}

#if 0
static double d4_old(int NP, const int a[NP][NP], int S, int N, int Vacant)
/* 4th sample moment about the mean of upper half of elements of square matrix, excluding diagonal */
{
  const double av= (double)S/N;
  double sum=0;
  for(int p1=0; p1<NP-1; p1++)
    if(p1!=Vacant-1)
      for(int p2=p1+1;p2<NP; p2++)
        if(p2!=Vacant-1)
  {
    double c = a[p1][p2] - av;
    sum += (c*c)*(c*c); // Optimized to only 2 mul by gcc already at -O1, but not perf critical
  }
  return sqrt(sqrt(sum/(double)N));
}
#endif

static int64_t minsq64(int v, int n)
/* how to divide the integer value v over n boxes, such that the
 * sum of squares is minimal
 */
{
  int Nshort, vn;
  int64_t dbvn;
  vn= (n>0? v/n : 0);
  dbvn= vn;
  Nshort= v - n*vn;
  return dbvn*dbvn*n + (2*dbvn + 1)*Nshort;
}

static double Qf64(int64_t SS, int S, int N)
/* Qf as defined in Groot Schemaboek N.B.B. by F.C. Schiereck
 */
{
  return (SS > 0 ? 100.0 * (double)(minsq64(S,N)) / (double)SS : 0.0);
}

static inline void swap_ints(int *x, int *y) { int tmp = *x; *x = *y; *y = tmp; }

/* Helper function to copy a balans matrix to non-overlapping destination (at start/end of omp) */
static void copy_balans(int NP, const int bal_in[restrict NP][NP], int bal_out[restrict NP][NP])
{
  for(int p1=0; p1<NP; p1++)
    for(int p2=0; p2<NP; p2++)
      bal_out[p1][p2] = bal_in[p1][p2];
}

/* Helper function copies NS/EW directions from bp2d_in to bp2d_out (to save/restore boards) */
static void copy_bp2d(int NB, int NP, const int bp2d_in[restrict NB][NP], int bp2d_out[restrict NB][NP])
{
  // memcpy((void*) bp2d_out, (const void*) bp2d_in, NB*NP*sizeof(int)); // Slowdown, avoid in omp loop
  for(int b=0; b<NB; b++)
    for(int p=0; p<NP; p++)
      bp2d_out[b][p] = bp2d_in[b][p];
}

/* Helper function to calculate 2D balans array from given board settings (no extra vacancy) */
static void get_balans(int NB, int NP, int H, const int meetcnt[restrict NP][NP],
                       const int bp2d[restrict NB][NP], int balans[restrict NP][NP])
{
  // Arrange loops for optimal friendlyness to caching and vectorization, even though speed
  // here is only really important in the DEBUG2 case, not in the production builds.
  // Let's make it simplest for vectorization by calculating full matrix at once, not just upper half.
  for(int p1=0; p1<NP; p1++)
    for(int p2=0; p2<NP; p2++)
      balans[p1][p2] = 0;

  // Add the contribution from each boardset.  sum includes the p1-p2 meeting(s) in opposite
  // directions (-1*1), corrected via "+1" times meetcnt at the end.
  for(int b=0; b<NB; b++)
    for(int p1=0; p1<NP; p1++)
      for(int p2=0; p2<NP; p2++)
        balans[p1][p2] += bp2d[b][p1] * bp2d[b][p2]; // 1 when in same dir, -1 opposite, 0 p1/p2 not played

  for(int p1=0; p1<NP; p1++)
    for(int p2=0; p2<NP; p2++)
      balans[p1][p2] += (H+1)*meetcnt[p1][p2];

  for(int p=0; p<NP; p++)  // Parts of the code also depends on the diagonal being zero
    balans[p][p] = 0;
}

#ifndef USE_MVER

/* No multiversioning: we simply define improve2() directly, no dispatching depending on CPU type. */
#define IMPROVE2_DEF int improve2

/* We increase clang performance significantly, especially on AVX2, by disabling interleaving in VECT.
 * PGO would have the same effect for these loops but strangely, as of 16.0.4, makes balans run 20-26%
 * slower on AVX2 Linux in some tests than just disabling interleave and _not_ using PGO.
 * Note that -mllvm -force-vector-interleave=1 might be
 * even better for improve2.c but for raninit.c the default interleave 2 is clearly best on AVX2.
 * Note that VECT here must work for AVX and generic as well so don't set explicit vectorization width.
 * For gcc (or any other than clang) we just try to force vectorisation via OpenMP SIMD pragma.
 * For gcc this has the important effect of vectorizing the row-to-column loops.  clang already does that
 * by itself even when only SSE2 available.  This speeds up the -S2 24p19r test by a factor 1.08 on AMD Zen 3.
 */
#ifdef __clang__
#define VECT _Pragma("clang loop interleave_count(1)")
#else

#if defined(_OPENMP) && defined(__SSE2__)
#define VECT _Pragma("omp simd")
#else
#define VECT
#endif /* OpenMP and at least SSE2 (also defined for AVX and AVX2) */

#endif /* __clang__ */

#else /* USE_MVER */

/* Multiversioning.  For improve2() we only use makefile-based, no "target" function attribute, in order to
 *   to get good performance with clang as well as with gcc.  Added bonus is -mtune= flags in makefile
 *   (using tuning in target function attribute seems to destroy performance for at least gcc).
 *   In makefile-based multiversioning we arrange that exactly one of MVER_{AVX2,AVX,DEFAULT} is defined:
 *     If MVER_AVX2 is defined we define only public function improve2_avx2()
 *     If MVER_AVX  is defined we define only public function improve2_avx()
 *     If MVER_DEFAULT is defined
 *        we define private function improve2_default() and public improve2() dispatcher.
 */

/* Without help clang vectorises the critical loops terribly for AVX2, poorly for AVX and even for the
 * generic case we make it a few percent better by pragmas.  If target attribute-based multiversioning
 * is used, not even PGO and/or these pragmas can help clang sufficiently.  Otherwise PGO should be
 * sufficient for clang but let's speed up the makefile-based multiversioning for clang even without PGO.
 * gcc does a good job for the ssqDif loop already but needs "omp simd" to also vectorize row->column copy.
 * gcc 12.2.1 seems to ignore the "simdlen" clause so don't bother setting that.
 */
#ifdef __clang__
#define VECT_W8_I1 _Pragma("clang loop vectorize_width(8) interleave_count(1)")
#define VECT_W4_I1 _Pragma("clang loop vectorize_width(4) interleave_count(1)")
#else
#define VECT_W8_I1 _Pragma("omp simd")
#define VECT_W4_I1 _Pragma("omp simd")
#endif

#ifdef MVER_AVX2

#define IMPROVE2_DEF int improve2_avx2
#define VECT VECT_W8_I1

#elif defined MVER_AVX

#define IMPROVE2_DEF int improve2_avx
#define VECT VECT_W4_I1

#elif defined MVER_DEFAULT

#define IMPROVE2_DEF static int improve2_default
#define VECT VECT_W4_I1

#else

#error "When USE_MVER is defined you must also define either MVER_AVX2, MVER_AVX or MVER_DEFAULT for improve2.c"

#endif /* MVER_... */
#endif /* USE_MVER */

/* Helper function to calculate vacancy quality (ssVall, ssVmin, PssVmin) */
static void
get_vacancy_ints(int NB, int NP, int Vacant, int range1, int range2,
                 const int bp2d[NB][NP],
                 const int bp2p[NB][NP],
                 const int balans[NP][NP],
                 int64_t *pssVall, int64_t *pssVmin, int *pPssVmin)
{
// We speed up the calculation by starting from current balans, then finding diff from that
  int compSub[NP][NP] ALB;    // Amount of competition to subtract if pair V absent (only upper triangle used)
  int64_t ssVall = 0;
  int64_t ssVmin = INT64_MAX;
  int    PssVmin = -1;

  for(int V=0; V<NP; V++)
    if(V!=Vacant-1)
  {
    int64_t ssVtmp = 0;          // Sum of squares of upper half of balans matrix if pair V absent

    for(int p1=0; p1<NP-1; p1++) // Reset upper half of balans difference matrix, only used for p1 < p2
      for(int p2=p1+1; p2<NP; p2++)
        compSub[p1][p2] = 0;

    for(int b=0; b<NB; b++) // Add up contributions from each board to the balans diff matrix (pair V absent)
    {
      int p = bp2p[b][V]; // Opponent of absent pair
      if(p < 0)
        continue; // V didn't play this board so no change to competition, skip to next board

      int pd = bp2d[b][p]; // Direction of opponent to absent pair, always -1 or +1

      /* If p1 & p sit in same direction, subtract 1 from balans, else add 1 (0 if p1 didn't play it) */
      /* May include p1=V but doesn't matter; not checking enables vectorization */
      for(int p1=0; p1<p; p1++) compSub[p1][p] += pd*bp2d[b][p1];
      VECT for(int p2=p+1; p2<NP; p2++) compSub[p][p2] += pd*bp2d[b][p2]; // Same for pair numbers larger than p

      for(int p1=0; p1<NP; p1++) // Again include p1=V to reduce number of branches; V rows/cols don't matter
      {
        int p2 = bp2p[b][p1]; // p2 is -1 if p1 didn't play this board
        if(p2 > p1)           // So we might as well only update upper half instead of only checking >= 0
          compSub[p1][p2]++; // Competition of each meeting is 1 lower with vacancy
      }
    }
    // Set the V rows and columns of (upper) diff matrix to the balans so that total contribution becomes zero
    for(int p1=0;   p1<V;  p1++) compSub[p1][V] = balans[p1][V];
    for(int p2=V+1; p2<NP; p2++) compSub[V][p2] = balans[V][p2];

    // Use the now completed difference matrix (upper half) to calculate sum of squares if V absent
    for(int p1=0; p1<NP-1; p1++) // Only calculate upper half of matrix: "Vacant" contrib 0 via bp2d
      VECT for(int p2=p1+1; p2<NP; p2++)
        ssVtmp += SQ(balans[p1][p2] - compSub[p1][p2]);

    ssVall += ssVtmp;
    if(ssVtmp < ssVmin && range1 <= V && V <= range2)
    {
      ssVmin = ssVtmp;
      PssVmin = V;
    }
  }                             /* end loop V */
  *pssVall  = ssVall;
  *pssVmin  = ssVmin;
  *pPssVmin = PssVmin;
}

// Return value: -1 if error, nBest otherwise i.e. number of ensemble members with best found quality
IMPROVE2_DEF(int NB, int NR, int NT, int NP, int MAXIT, int esize, int Vacant, int scheme[3][NT][NR],
             int fixers[], int K4, int range1, int range2, int use_Qf1av, double w12,
             int algoi, double algoc1, double algoc2, int Verbose)
{
  // The variables in this initial block remain constant after the initialisation phase, except the first 3
  long double global_templ; // Current temperature at high precision for the slow algos, common to ensemble
  double alg1fract=0.0;     // Current exponential factor for slow temperature change (initially no change)
  double relaxtime_avg=0.0; // Rolling average of estimated relaxation times for algoi==2 (dampen fluctuations)
  double alg0scale=0.0; // To scale the fixed temp array in algo 0 (fast fluctuations; init due to stupid gcc)
  int ens_randomise=0; // If rest of ensemble should be randomised
  int meetmax;
  int interval;
  int alg0_do_rescale=0; /* algo 0: User gave significantly different temperatures, so do rescale */

  const int slow=algoi; /* 0 = crazy fast, 1 = slow exponential, 2 = slow adaptive piece-wise exponential */
  const int NPA = (Vacant<=0 || Vacant>NP ? NP : NP - 1);
  const int use_Estats = (algoi > 1 || P_expert); // Disable energy stats unless S2+ or very verbose
  const int use_Qf1 = (w12 != 0 || use_Qf1av); // If -W0:0, ignore Fw (Qf1*, ssV*) completely in optimization
  const long double weight = (long double)w12;

  static const float temp[NTEMP]=
  {
    1,    2,    4,    5,    7,   10,   10,   10,   20,   20,
    20,   20,   20,   20,   20,   20,   20,   20,   20,   20,
    20,   30,   30,   40,   40,   60,  100,  200,  400,  800,
    1200, 1600, 2000, 1600, 1200,  800,  400,  200,  100,   60,
    40,   40,   30,   30,   20,   20,   20,   10,   10,   10,
    7,    5,    4,    2,    1,    1,    2,    4,    5,    7,
    10,   10,   10,   20,   20,   20,   30,   30,   40,   40,
    60,  100,  200,  400,  800, 1200, 1600, 2000, 1600, 1200,
    800,  400,  200,  100,   60,   40,   40,   30,   30,   20,
    20,   20,   10,   10,   10,    7,    5,    4,    2,    1
  }; // consts above and below are for the old alg 0 only, but may be scaled
  const double temp0dflt =    1; // = algo_cdefault[0][0];
  const double temp1dflt = 2000; // = algo_cdefault[0][1];

  const long double temp0l = (long double) algoc1; // Initial temperature
  const long double temp1l = (long double) algoc2; // Final temperature if algoi == 1, else ignored
  const double speed = algoc2; // (Proportional to) the constant thermodynamic speed for algoi > 1

  global_templ = temp0l; // Only updated by the slow algos.  For alg0 only used to avoid undefined C.

  if(esize<0)
  {
    esize = -esize;
    ens_randomise = 1;
  }

  int progress_window = 0;
#ifdef MS_DOS
  progress_window = 1;
#endif

  if(algoi == 0)
  { /* Old algo using fixed temps; check if scaling needed and if so factor */
    alg0_do_rescale =
      (fabs(algoc1 - temp0dflt)/temp0dflt > eps ||
       fabs(algoc2 - temp1dflt)/temp1dflt > eps);
    if(alg0_do_rescale)
      alg0scale = (log(algoc2)    - log(algoc1))
                / (log(temp1dflt) - log(temp0dflt));
  }
  else if(algoi == 1)
  { /* exp cool: global_templ = maxtemp * (1-alg1fract)^(iter-1) */
    alg1fract = (double)(MAXIT > 3 ? -expm1l((logl(temp1l) - logl(temp0l))/(MAXIT-3)) : 1);
    if(P_expert) printf("(X) alg1fract = %.7g\n", alg1fract);
  } // else algoi > 1: keep default 0.0 for first periods to gather stabilised statistics

  if(NB > 32767 || NP > 32767) // Unrealistically large numbers, but just be on the safe side
  {
    fprintf(stderr,"improve2(): Too large # of pairs/boards, max is 32767: NP=%d, NB=%d\n", NP, NB);
    return -1;
  }
  interval= MAXIT/100;
  if(interval==0)interval=1;
  if(! P_expert) // When testing/expert we'd like at last ~100 lines of debugging statistics
    interval*= 5; // How often periodic progress is shown in Windows dialog box or "extended progress"

  {
    // Variables shared read-only by all ensemble members, not changed in the arrow switching loop
    int meetcnt[NP][NP] ALB; // Competition and number of meetings between pairs
    int bp2p[NB][NP]    ALB; // Board-pair-to-pair: bp2p[b][p] = opponent of pair p at board b (-1 = none)
    int bl_def[NR*NT]   ALB; // Free board-lowpair combis for random arrow switching (b << 16 + p)
    int S1[NP];              // Total amount of competition when pair V absent.  Part of the Qf1max calculation.
    int S1all;               // Sum of all the individual S1.  Also constant and part of the Qf1max calculation.

    // Variables particular to a single ensemble member, depending on arrow switching state:
    int   balans_e[esize][NP][NP] ALB; // 3D VLA's outside struct used because clang doesn't want 2D inside...
    int     bp2d_e[esize][NB][NP] ALB; // ... and 'gcc -fopenmp' crashes if VLA in struct
    int bp2dBest_e[esize][NB][NP] ALB; // Direction of pair at board: 1 = NS, -1 = EW, 0 = not played
    raninit_state_t rstate_e[esize]; // State of random number generator reserved for each member only
    struct ALB EMEMBER_STATE_T {     // We collect the simple state variables in an esize array of this struct
      /* longest variables declared first -- improves alignment and silences 'clang -Wpadded' */
      long double FwBest; // 80-bit floating point, lossless conversion to/from int64_t
      long double E2sum1rel; // Avoid int64_t overflow in first 10 iter of 120p30rM; minor bit loss not important
      int64_t ssq, ssqMin, ssVallBest, ssVminBest, sumd4Best; // "Best" = among states with minimal ssq (& Fw)
      /* ===== Begin for debugging and/or adaptive cooling (algoi > 1) (includes the E2sum1rel above): ===== */
      int64_t Emax1, Emin1, Ebase1, Esum1rel; // Energy statistics of recent accepted/stayed-in states
      int64_t Emax1rel_last, Emin1rel_last;   // Energy max/min of previous sampling period relative to base
      int64_t Ebase1_last;   // Energy base used in previous sampling period
      double Eavg1r, Evar1;  // Energy (relative) avrg and variance of previous sampling period (iter interval)
      double heatC, accRate; // Estimated heat capacity and measured acceptance rate in previous period
      int nacc; /* Number of accepted arrow switches in current period */
      /* =====  End for debugging and/or adaptive cooling ===== */
      int PssVminBest;
      int odd, newBest;
      int tidx, retry_delay, retry_at_iter;
      int eidx; // Index of this member in all of the ensemble arrays (for lookup in the 3D arrays)
      float tempiAtBest; // Temperature when the best state until now was found for this member
      float  dummy1; // Padding to improve alignment (clang -Wpadded)
      double dummy2; // Padding to improve alignment (clang -Wpadded)
      long double dummy3; // Padding to improve alignment to 32-byte (optimise for vectorisation)
    };
    typedef struct EMEMBER_STATE_T emember_state_t;
    emember_state_t ensemble[esize];     // Ensemble of states developing independently like an ideal gas

// Macro that's true iff x points to an ensemble member with better "best" quality parameters than y,
//   using "<=" in 2nd Fw term to circumvent warning about fp "==" use (FwBest selected that way anyway).
//   "Exactly equal to" for emps can then be implemented via "neither x better than y nor y better than x".
//   If the user has chosen not to optimise for Qf1, Qf1av and/or d4, the *Best remain constant and equal.
#define EMP_BETTER_THAN(x,y) \
    ( (x)->ssqMin    <  (y)->ssqMin || \
     ((x)->ssqMin    == (y)->ssqMin && \
     ((x)->FwBest    <  (y)->FwBest || \
     ((x)->FwBest    <= (y)->FwBest && \
      (x)->sumd4Best < (y)->sumd4Best))))

    // Variable that's "write only" by ensemble members (set 1 when new local best), reset to 0 outside ei-loop
    int newBest_any=1;   // When 1, we search for best ensemble member and show progress if new global improved

    // Variables used only outside main ensemble loop, to determine & show globally best ensemble members
    int eiBest_cnt=1;     // Number of ensemble members having found the present global optimum
    int64_t ssq_GBest=INT64_MAX; // Best ssq ever found until now.  Huge val ensures initialise @ iter=1 start.
    int64_t sumd4_GBest=0;  // Best for d4 in same sense as for Fw (= 0 just to silence gcc)
    long double Fw_GBest=0; // Best means "belonging to best member," i.e. ssq 1st priority
    emember_state_t *empB = ensemble; // Pointer to (one of) the latest known best ensemble member(s)

    // Variables only used for showing progress, derived from (one of) the globally best member(s)
    double Qf, Qf1av, Qf1max; // Minimize Fw = ssVall + weight*ssVmin (or just ssVmin if use_Qf1av=0)
    double d4Best = -1e-30;    // Initialise these to avoid warnings with MinGW and clang -W...uninitialized
    int PssVmin_GBest = -9999; // Pair number belonging to Qf1max of the globally best solution
    float tempi_at_GBest = -1e-30F;

    // Variables only used in the initialisation phase, not referenced at all in the arrow switching loop
    int bp2d_in[NB][NP] ALB; // Initial pair directions at each board
    int cnt[NB];         // For counting # of times each board is played
    int nblfree=0;       // Number of free (arrow switchable) board-lowpair combis (should be <= NB*NPA/2)
    int nerr;            // For counting errors, aborting if any found

// initialize
    for(int b=0;b<NB;++b)
      cnt[b]=0;

    for(int b=0; b<NB; b++)
      for(int p=0; p<NP; p++)
        bp2d_in[b][p] = 0;

    for(int b=0; b<NB; b++)
      for(int p=0; p<NP; p++)
        bp2p[b][p] = -1;

    for(int p1=0; p1<NP; p1++)
      for(int p2=0; p2<NP; p2++)
        meetcnt[p1][p2] = 0;

    for(int V=0; V<NP; V++)
      S1[V]=0;

// Get the opponents on each boardset, eliminate b==0 and Vacant tables,
// count number of meetings of each pair of pairs and check for consistency.
    meetmax=1;  // set to 1 to silence zero-length array warning
    nerr=0;
    for(int r=0; r<NR; r++)
      for(int t=0; t<NT; t++)
    {
      int b=  scheme[0][t][r] - 1;
      int ns= scheme[1][t][r] - 1;
      int ew= scheme[2][t][r] - 1;
      if(b<0 || ns==Vacant-1 || ew==Vacant-1 || ns>=NP || ew>=NP)
        continue; // These cases are skipped, but not considered to be errors

      if(b>NB || ns<0 || ew <0 || ns == ew) // But the cases here are errors
      {
        nerr++; continue;
      }
      else if(bp2d_in[b][ns] || bp2d_in[b][ew])
      {
        nerr++; continue; // ns and/or ew already played this board at another table!
      }
      bp2d_in[b][ns] =  1;
      bp2d_in[b][ew] = -1;
      bp2p[b][ns] = ew;
      bp2p[b][ew] = ns;

      if(!fixers[r*NT + t]) // If not fixed, i.e. free to be arrow switched
        bl_def[nblfree++] = (b << 16) + (ns < ew ? ns : ew); // Pack {b,lowestpair} in 2*16 bits

      if((meetcnt[ns][ew] = ++meetcnt[ew][ns]) > meetmax)
        meetmax++;

      for(int V=0; V<NP; V++)
        if(V!=Vacant-1 && V!=ns && V!=ew)
          S1[V]++; // Count another encounter; multiplied by (H-1) later

      cnt[b]++;
    }
#ifdef DEBUG2
    for(int ei=0; ei<MIN(2,esize); ei++)
      printf("DEBUG: Address of rstate_e[%d] = 0x%lx\n", ei, (unsigned long) &rstate_e[ei]);

    for(int b=0; b<NB; b++)
    {
      printf("DEBUG: Board %3d:\n", b+1);
      printf("DEBUG: Pair: "); for(int p=0; p<NP; p++) printf(" %2d", p+1);           printf("\n");
      printf("DEBUG:  bp2d:"); for(int p=0; p<NP; p++) printf(" %2d", bp2d_in[b][p]); printf("\n");
      printf("DEBUG:  bp2p:"); for(int p=0; p<NP; p++) printf(" %2d", bp2p[b][p]+1);  printf("\n");
    }
#endif

// Now we know the number of free board-lowpair combis, declare constant to help compilers / make safe code
    const int NBLF = nblfree;
    const int NEnergy1 = NITER_E1*NREPEAT*NBLF; // Number of sampled states in an energy statistics period

// Find the maximum number of times any boardset is played
    int NTA_cnt=0;
    for(int b=0; b<NB; ++b)
    {
      if(cnt[b] > NTA_cnt) NTA_cnt=cnt[b];
    }
    const int NTA = NTA_cnt;

// Check whether all boards are played the same number of times
    for(int b=0; b<NB; ++b)
      if(cnt[b] != NTA) ++nerr;

// Note. This program can only optimize movements where all boards are
// played the same number of times.
    if(nerr>0)
    {
      fprintf(stderr,"\nCAN NOT OPTIMIZE THIS MOVEMENT!!\nCheck if all boards are played the same number of times.\n\n");
#ifdef MS_DOS
      MessageBox(NULL,
        "CAN NOT OPTIMIZE THIS MOVEMENT!!\nCheck if all boards are played the same number of times",
        "balans",MB_ICONERROR);
#endif
      return -1;
    }

// new in version 5.7: H is no longer input parameter
    const int H=NTA-1;
    const int S=NTA*NB*H;       // amount of competition (= half the sum of the balans elements of full matrix)
    const int N=NPA*(NPA-1)/2;  // Number of pairs of pairs (unordered) without vacancy
    const int N1=(NPA-1)*(NPA-2)/2; // Number of pairs of pairs (unordered) excluding an absent pair

// calculate vacancy sums.  Normally all S1[V]=(NTA-1)*NB*(H-1), but not for e.g. NPA odd.
    S1all=0;
    for(int V=0; V<NP; V++)
      S1all += (S1[V]*=(H-1));

// finished with the preliminaries, now initialise all ensemble members.  Each remember own best values.
    for(int ei=0; ei<esize; ei++)
    {
      raninit_multi(&rstate_e[ei]); // Seed individual thread-safe RNG and prepare its float cache
      emember_state_t *emp=&ensemble[ei];
      emp->eidx = ei;
      copy_bp2d(NB, NP, bp2d_in, bp2d_e[ei]); // bp2d = current NS/EW directions.  Never randomise ei=0.
      if(ens_randomise && ei>0)   // When user used -z, we randomise directions for all but first member
        for(int j=0; j<NBLF; j++)
      {
        int b   = (bl_def[j] >> 16);     // board is high 16 bits, lowpair is the low 16 bits
        int p1  = (bl_def[j] & 0xFFFF);
        int p2  = bp2p[b][p1];           // p1 meets p2 here, p1<p2
        int p1d = bp2d_e[ei][b][p1];     // 1 when p1 currently NS, -1 when EW (can't be 0 for this p1)
        if(iigenrand(2))
        {
          bp2d_e[ei][b][p1] = -p1d; // Arrow switch with 50% probability
          bp2d_e[ei][b][p2] =  p1d;
        }
      }
      copy_bp2d(NB, NP, bp2d_e[ei], bp2dBest_e[ei]); // bp2dBest = saved best optimum (for this member)
      get_balans(NB, NP, H, meetcnt, bp2d_e[ei], balans_e[ei]); // get the initial balance
      emp->ssqMin = emp->ssq = sumsq_upper2d(NP, balans_e[ei]); // sumd4best init, but only changed if K4 set
      emp->sumd4Best = (K4 ? sum_part_d4_upper2d(NP, balans_e[ei], S, N) : 0);

      get_vacancy_ints(NB, NP, Vacant, range1, range2, bp2d_e[ei], bp2p, balans_e[ei],
                       &(emp->ssVallBest), &(emp->ssVminBest), &(emp->PssVminBest));
      emp->FwBest = (use_Qf1 ? use_Qf1av*emp->ssVallBest + weight*emp->ssVminBest : 0);
      if(P_expert)
        printf("(X) #%d: Initial sums of squares etc: Fw=%.7g weight=%.16g ssq=%" PRId64
               " ssVall=%" PRId64 " ssVmin=%" PRId64 " sumd4=%" PRId64
               " K4=%d S=%d N=%d S1[]=%d..%d S1all=%d N1=%d NPA=%d\n", ei+1, (double) emp->FwBest,
               (double) weight, emp->ssqMin, emp->ssVallBest, emp->ssVminBest,  emp->sumd4Best,
               K4, S, N, S1[0], S1[NP-1], S1all, N1, NPA);
      // Initialise variables for debugging and adaptive cooling
      emp->Emin1 = emp->Emax1 = emp->Ebase1 = emp->Ebase1_last = emp->ssqMin;
      emp->Esum1rel = 0; emp->E2sum1rel = 0;
      emp->nacc = 0;

      emp->tidx = -1;          // So tidx=0 in first iteration
      emp->newBest = 1;        // So odd=1 _and_ deep freeze initially (temperature 0 effectively)
      emp->retry_at_iter = 0;  // Iteration at which to retry last optimum w/ Temp reset
      emp->odd = emp->retry_delay = 0; // Just to silence warnings from stupid MinGW (gcc+clang ok)
      emp->tempiAtBest = 0.0F; // Temperature at last optimum, informational only, so low precision fine
    }
    Qf = Qf1av = Qf1max = 0.0; // Just to silence a gcc that can't see that we _do_ initialise them at iter 1

//  MAIN LOOP STARTS HERE
//  check next super-round of arrow switches (1 iter tries esize*100*NBLF arrow switches)
    for(int iter=1; iter<=MAXIT+1; iter++) // The extra iter is very small, to show initial+final status
    {
      if(esize>1 && newBest_any) // At least one member improved, but did it beat the formerly best global qual?
      {
// Find best member of ensemble and how many now have the same best quality.  After that compare with old vals.
        int ecnt=0;
        emember_state_t *emp=ensemble;
        for(int ei=0; ei<esize; ei++, emp++)
          if(EMP_BETTER_THAN(emp, empB))
          {
            empB = emp; ecnt = 1;
          }
          else if(!EMP_BETTER_THAN(empB, emp))
            ecnt++; // Neither better than the other, so they are equally good
        if(ecnt != eiBest_cnt)
          eiBest_cnt = ecnt;   // Number of best members changed, so keep newBest state (i.e. show progress)
        else if(iter>1)  // When not initialising in iter==1, see if global quality really improved
          newBest_any = ( empB->ssqMin    <  ssq_GBest ||
                         (empB->ssqMin    == ssq_GBest &&
                         (empB->FwBest    <  Fw_GBest  ||
                         (empB->FwBest    <= Fw_GBest &&
                          empB->sumd4Best < sumd4_GBest))));
      }
      if(newBest_any) // Always true at start of iter==1, so they become initialised
      {
        ssq_GBest      = empB->ssqMin;
        Fw_GBest       = empB->FwBest;
        sumd4_GBest    = empB->sumd4Best;
        PssVmin_GBest  = empB->PssVminBest;
        tempi_at_GBest = empB->tempiAtBest;
// Derive Qf, Qf1av and Qf1max for printing (for optimizing we just use the ss*)
        Qf       = Qf64(empB->ssqMin,         S,              N);
        if(use_Qf1 || iter==1)
        {
          if (PssVmin_GBest < 0)
          {
            printf("++++++ PROGRAMMING ERROR: PssVmin_GBest=%d (setting to 0 now)\n", PssVmin_GBest);
            PssVmin_GBest = 0;
          }
          Qf1av  = Qf64(empB->ssVallBest, S1all,         N1*NPA);
          Qf1max = Qf64(empB->ssVminBest, S1[PssVmin_GBest], N1);
        }
        d4Best = d4(S, N, ssq_GBest, sumd4_GBest);
        if(P_expert && iter>1) // Prefix to level 1 showprogress2() output, so a mix of expert and normal
          printf("(x) ssq=%" PRId64 ", sumd4=%" PRId64 ", ", empB->ssqMin, empB->sumd4Best);
      }

      if((P_progress_ext && !P_expert) || progress_window)
      {
        if((iter-1) % interval == 0) // Windows dialog box update regularly even when no new best optimum
          newBest_any = 1;           // Pretend for a very short while that we _did_ find a new optimum
      }

      if(newBest_any)
      {
        showprogress2(iter-1, (slow ? (double)global_templ : (double)tempi_at_GBest), Qf, Qf1av, Qf1max,
		      PssVmin_GBest, d4Best, eiBest_cnt, esize, 1, Verbose);
        newBest_any=0;
        // We calc & print Qf1* initially even when not optimizing for them; but won't _during_ optimization
        if(!use_Qf1) Qf1av = Qf1max = -1.0; // This will make showprogress2() print dashes the rest of the time
      }

      int early_iter = (iter == 11 || iter == 101 || iter == 1001 || iter == 10001);
      if(use_Estats
	 && ((iter > NITER_E1 && ((algoi > 1 && (iter-1) % NITER_E1 == 0) || (iter-1) % interval == 0))
	     || early_iter))
      {
        if(esize>1)
        {
          // Calculate aggregated energy statistics over entire ensemble, rescaling to globally best ssq as base
          double ens_Esum1rel  = 0.0; // Sum of member time-averaged energies, relative to now globally best ssq
          double ens_E2sum1rel = 0.0; // As above, but for squared energy.  Use doubles for simplicity & safety.
          double ens_Evar1sum  = 0.0; // Sum of member energy variances (to calculate mean of that)
//          double ens_heatCsum  = 0.0; // Sum of member heat capacities (to etc.) ** not used **
          double ens_accRsum   = 0.0; // Sum of member acceptance rate (to etc.)
          int64_t ens_Emax1rel = 0;   // Globally highest relative (to base) ssq seen during last sampling period
          int64_t ens_Emin1rel = INT64_MAX; // Globally lowest above base similarly seen
          for(emember_state_t *emp=ensemble; emp<ensemble+esize; emp++)
          {
            int64_t Ebase1rel = emp->Ebase1_last - ssq_GBest; // Base energy for member stats relative to global
            ens_Esum1rel  +=   (emp->Eavg1r + (double)Ebase1rel);
            ens_E2sum1rel += SQ(emp->Eavg1r + (double)Ebase1rel);
            ens_Evar1sum  +=    emp->Evar1;
//            ens_heatCsum  +=    emp->heatC; ** not used **
            ens_accRsum   +=    emp->accRate;
            ens_Emin1rel = MIN(emp->Emin1rel_last + Ebase1rel, ens_Emin1rel);
            ens_Emax1rel = MAX(emp->Emax1rel_last + Ebase1rel, ens_Emax1rel);
          }
          double ens_Eavg1r   = ens_Esum1rel/esize; // Total mean energy, averaged over time as well as ensemble
          double ens_Evar1avg = ens_Evar1sum/esize; // Mean individual member variance (mean of variances)
          double ens_Evar1tot = ens_Evar1avg + (ens_E2sum1rel/esize - SQ(ens_Eavg1r)); // Total var, NEnergy1>>1
          //printf("   ens_Evar1tot/ens_Evar1avg=%8.5f\n", ens_Evar1tot/ens_Evar1avg);
          // Variance of the means, would be ~ NEnergy1 times smaller than sample variance if no correlation:
          double ens_Evar1 = (ens_E2sum1rel - SQ(ens_Esum1rel)/esize) / (esize-1);
          // But even at high T, several steps (arrow switches) are needed for E(n+i) to be almost uncorrelated
          // to E(n).  Therefore the following ratio is normally somewhat higher than 1, e.g. around 8 at
          // T=9999 for 26p25rbarometer.how for large ensemble.  It  will explode to much higher values if/when
          // members freeze out at low T to essentially disconnected states at different energy levels:
          double sdevRatio = (ens_Evar1sum > 0 ? sqrt(NEnergy1 * ens_Evar1 / ens_Evar1avg) : 9999.9);
          double ens_heatC = ens_Evar1tot/SQ((double)global_templ); // Heat cap. estimated from total variance
          if(algoi == 2 && iter > 2*NITER_E1) // Skip first statistics period to get more stable statistics
          {
            // Use sdevRatio as a rough estimate of the relaxation time for use at cooling at roughly constant
            // thermodynamic speed (better but slower estimate would be by collecting energy transition matrix),
            // but rolling averaged to stabilise it somewhat more (weight 50-50 for past-vs-current values):
            relaxtime_avg = (iter <= 3*NITER_E1 ? sdevRatio : (relaxtime_avg + sdevRatio) / 2);
            // alg1fract = speed / (relaxtime_avg * sqrt(ens_heatC));
            // Ref: Formula (6) in
            // https://www.fys.ku.dk/~andresen/BAhome/ownpapers/perm-annealSched.pdf
            double denominator = relaxtime_avg * sqrt(ens_heatC);
            if(ens_Evar1sum <= 0.0) // In the crazy sdevRatio=9999.9 case don't lower T any further right now
              alg1fract = 0.0;
            else if(ALG1FRACT_MAX * denominator < fabs(speed)) // Enforce speed limit and guard against div by 0
              alg1fract = ALG1FRACT_MAX;
            else
              alg1fract = speed / denominator; // Exponential cooling by this during the next stats period
          }
          if(P_expert && ((iter-1) % interval == 0 || early_iter))
            printf("(X) %7d %6.1f Tot aR=%.2e, Er=%6.1f +-%6.1f (%4" PRId64 "..%4" PRId64
		   "), C=%7.2f, sR=%7.2f, fr=%.4g\n",
                   iter-1, (double) global_templ, ens_accRsum/esize, ens_Eavg1r,
                   sqrt(ens_Evar1avg), ens_Emin1rel, ens_Emax1rel, ens_heatC, sdevRatio, alg1fract);
          //if(P_expert) printf("  NEnergy1=%d, std.dev(Emean) = %8.2f\n", NEnergy1, sqrt(ens_Evar1));
        }
        if(P_expert && ((iter-1) % interval == 0 || early_iter))
        { // Dump the best one too, empB, as extra check (and the only one if ensemble has only 1 member):
          int64_t dE = empB->Ebase1_last - ssq_GBest; // Convert energy values to best global base too here
          printf("(X) %7d %6.1f #%-2d aR=%.2e, Er=%6.1f +-%6.1f (%4" PRId64 "..%4" PRId64 "), C=%7.2f\n",
                 iter-1, (double) global_templ, empB->eidx+1, empB->accRate, empB->Eavg1r+(double)dE,
                 sqrt(empB->Evar1), empB->Emin1rel_last+dE, empB->Emax1rel_last+dE, empB->heatC);
        }
      }
      if(iter > MAXIT                       // Finished with the extra partial iter for summarising
         || (Qf == 100.0 && Qf1av == 100.0) // The balance is perfect :)
         || qinterrupt() != 0) break;

      if(iter > 1 && slow)
        global_templ -= global_templ*((long double)alg1fract); // Slow ~ exp cooling of ensemble

#ifdef _OPENMP
#pragma omp parallel for
#endif
      for(int ei=0; ei<esize; ei++)
      {
        // First we declare arrays to hold local copies of the ei part of the most contended ensemble arrays
        int balans[NP][NP] ALB;
        int bp2d[NB][NP] ALB;     // No need to copy bp2dBest too, since that is rarely updated
#ifdef DEBUG2
        int balansC[NP][NP] ALB;  // For slow check of balans by complete recalculation
#endif
        raninit_state_t rstate; // Internally contains arrays to hold random uints and (-logf of) floats
        emember_state_t estate; // No arrays here, but also heavily written to, so copy in-out too
        emember_state_t * const emp=&estate;
        float tempi; // Temperature for fast calculations, set from either global_templ or the temp array
        // Begin copy ensemble-specific parts of shared arrays, making multi-threaded version much faster
        //memcpy((void*) balans,  (void*)  balans_e[ei], sizeof(balans)); // memcpy with VLA not inlinable
        //memcpy((void*) bp2d,    (void*)    bp2d_e[ei], sizeof(bp2d));   // and not nice in parallel section
        copy_balans(NP, balans_e[ei], balans);
        copy_bp2d(NB, NP, bp2d_e[ei], bp2d);
        memcpy((void*) &rstate, (void*) &rstate_e[ei], sizeof(rstate));
        memcpy((void*) &estate, (void*) &ensemble[ei], sizeof(estate));
        // End copying parts of shared arrays
        if(slow)
        {
          if(iter >= MAXIT-1)
            emp->odd=1; // Deep freeze at last 2 iter (value of tempi doesn't matter then)
          tempi = (float) global_templ;
          if(tempi <= 0) tempi = 0.01F;  // In case of numerical instability; shouldn't ever happen
        } else { // Fast, crazy fluctuating temp with possible delayed retries with temp reset
          if(iter == emp->retry_at_iter)
          { // Jump back to last saved optimum
            copy_bp2d(NB, NP, bp2dBest_e[ei], bp2d);
            get_balans(NB, NP, H, meetcnt, bp2d, balans);
            emp->ssq   = emp->ssqMin;
            emp->odd = 1;  // So that odd=0 in first rep because this optimum already deep frozen
            emp->tidx = 0; // Reset temperature, i.e. slow heating from a well established optimum
            emp->retry_at_iter += (emp->retry_delay <<= 1); // Try same again after ever doubling delay
          }
          else
            emp->tidx = (emp->tidx+1)%NTEMP;
          tempi = temp[emp->tidx];
          if(alg0_do_rescale) // Rescale fixed array logarithmically as requested by user (-S)
            tempi = (float) exp(log(algoc1) + alg0scale*(log((double)tempi) - log(temp0dflt)));
        } // end if(slow)
// Copy free board-lowpair combis to new compact array.  Doesn't need to survive iterations, friendly to OpenMP.
        int bl[NBLF+1]; // +1 added to silence zero-length array warning
        for(int i=0; i<NBLF; i++)
          bl[i] = bl_def[i];

        for(int nrep=1; nrep<=NREPEAT; nrep++)
        {
          if(!slow) emp->odd = (emp->newBest || !emp->odd); // Freeze at new optimum, otherwise warm-cool-warm...
          emp->newBest = 0; // Only set to 1 again if new optimum found in this repetition

// randomize the board-lowpair access array unless slow and not yet at final freezing (odd=1)
          if(!slow || emp->odd)
            for(int i=0; i<NBLF; i++)
            {
              int j = iigenrand_mt(&rstate, NBLF);
              swap_ints(bl + i, bl + j);
            }

// fast/freezing: loop over all switchable boards and lowpairs: try and switch each in random order
// slow & warm: loop same # of times, but some b/l many times, others not at all (reversible neighbour func)
          for(int i=0; i<NBLF; i++)
          {
            int j   = (slow && !emp->odd ? iigenrand_mt(&rstate, NBLF) : i);
            int b   = (bl[j] >> 16);     // board is high 16 bits, lowpair is the low 16 bits
            int p1  = (bl[j] & 0xFFFF);
            int p2  = bp2p[b][p1];       // p1 meets p2 here, p1<p2
            int p1d = bp2d[b][p1]; // 1 when p1 currently NS, -1 when EW (can't be 0 for this p1)
#ifdef DEBUG2
            if(iter==1 && nrep ==1)
              printf("#%d: iter=1, nrep=1, ifree=%3d, j=%3d, b=%2d, p1=%3d, p2=%3d, p1d=%2d, p2d=%2d, p2o=%3d\n",
                     ei+1, i, j, b, p1, p2, p1d, bp2d[b][p2], bp2p[b][p2]);
#endif
// how much would this arrow switch increase the sum of squares without vacancy (ssq)?
            int ssqDif=0; // p1d * bp2d[p3][b] is 1 for p1-p3 NS-NS or EW-EW, -1 opposite, 0 if p3 didn't play
            VECT for(int p3=0; p3<NP; p3++)  // also: if p1 in same direction as p3, then p2 opposite p3
              ssqDif +=  bp2d[b][p3] * (balans[p2][p3] - balans[p1][p3]);
            // Almost corresponds to this old version:
            // ssqDif += balans[ew][ns2] - balans[ew][ew2] +
            //           balans[ns][ew2] - balans[ns][ns2];
            // except that we:
            //   1) still need to correct the sign by multiplying with p1d
            //   2) included p3=p1 and p3=p2, giving 2*balans[p1][p2] too much, correct both now:
            ssqDif = 4*(p1d*ssqDif - 2*balans[p1][p2]) + (NTA-1)*16;

// printf("DEBUG: Considering ssqDif=%d\n", ssqDif);
// do we accept this arrow switch?
//       Original test (ver<=6.79) spends lots of time in exp(3) (double):
//          if ((exp(-( ssqDif/tempi)) > dgenrand() && odd == 0) ||
//            (ssqDif < 0 && odd != 0))
//       Optimized version by Ulrik Dickow (ukd) 20170207. logf(3) faster than expf(3):
//            (odd || ssqDif >= -tempi*logf(fgenrand()))
//          Further optimised by ukd 20190720 to use vectorised batch generation of logf's (e.g. AVX or AVX2)
//          Also note that when ssqDif <= 0, the exp(...) would always be >= 1.
            if ((ssqDif > 0) &&    // On odd (freezing) reps never accept switches that would increase Qf
                (emp->odd || (float)ssqDif >= tempi*mlogfrand_mt(&rstate)))
            { // Switch not accepted, stay in current state.  But must still be counted in time avrg of states.
	      if (!use_Estats) continue;
              emp->Esum1rel  +=   (emp->ssq - emp->Ebase1); // For average energy of states we were in recently
              emp->E2sum1rel += SQ(emp->ssq - emp->Ebase1); // For variance & std.dev. (+ approx. heat capacity)
              continue;            // On non-odd reps accept increasing Qf likelihood given by tempi
            }

// arrow switch accepted, maybe temporarily worse than before, but searching for a better one
#ifdef DEBUG2
            // printf("DEBUG2: Accepted ssqDif=%d\n", ssqDif);
            int debug_cp1p2 = balans[p1][p2];
#endif
            // Update balans matrix.  Calculate only for p1/p2 rows, then mirror to columns afterwards.
            // Shortest -- but with gcc not fastest -- code to do the initial work is these 2 loops:
            //   for(int p3=0; p3<NP; p3++) balans[p1][p3] -= 2 * p1d * bp2d[b][p3];
            //   for(int p3=0; p3<NP; p3++) balans[p2][p3] += 2 * p1d * bp2d[b][p3];
            // Is faster to avoid the multiplications by splitting the loops on whether p1d is +1 or -1
            // _and_ also replace the doublings by double addition/subtraction (both are needed for speedup).
            if(p1d>0) // p1d is always either +1 or -1, see back when set
            {
              VECT for(int p3=0; p3<NP; p3++) balans[p1][p3] = balans[p1][p3] - bp2d[b][p3] - bp2d[b][p3];
              VECT for(int p3=0; p3<NP; p3++) balans[p2][p3] = balans[p2][p3] + bp2d[b][p3] + bp2d[b][p3];
            } else {
              VECT for(int p3=0; p3<NP; p3++) balans[p1][p3] = balans[p1][p3] + bp2d[b][p3] + bp2d[b][p3];
              VECT for(int p3=0; p3<NP; p3++) balans[p2][p3] = balans[p2][p3] - bp2d[b][p3] - bp2d[b][p3];
            }
            // Included p3=p1 and p3=p2, now correct for that (p2>p1 so only need to fix [p1][p2] here)
            balans[p1][p1] = balans[p2][p2] = 0;
            balans[p1][p2] -= 2;

            // Now copy the p1/p2 rows to columns
            VECT for(int p3=0; p3<NP; p3++) balans[p3][p1] = balans[p1][p3]; // Includes p2.p1 = p1.p2
            VECT for(int p3=0; p3<NP; p3++) balans[p3][p2] = balans[p2][p3];

            bp2d[b][p1] = -p1d; // Update arrow switches and sum of squares
            bp2d[b][p2] =  p1d;
            emp->ssq += ssqDif;

#ifdef DEBUG2
            get_balans(NB, NP, H, meetcnt, bp2d, balansC); // True balans as result of arrow switch
            int64_t ssqC = sumsq_upper2d(NP, balansC);           // True ssq after arrow switch
            int64_t ssqN = sumsq_upper2d(NP, balans);            // ssq of the quickly updated balans
            if(emp->ssq != ssqC || emp->ssq != ssqN
               || balans[p1][p2] != debug_cp1p2 || balans[p2][p1] != debug_cp1p2)
            {
              fprintf(stderr,"!!! BUG: ssq=%" PRId64 ", ssqC=%" PRId64 ", ssqN=%" PRId64
                     ", Old12 = %d, New12 = %d, New21 = %d\n",
                     emp->ssq, ssqC, ssqN, debug_cp1p2, balans[p1][p2], balans[p2][p1]);
              exit(2);
            }
#endif
	    if(use_Estats)
	    {
	      if(emp->ssq < emp->Emin1) emp->Emin1 = emp->ssq;
	      if(emp->ssq > emp->Emax1) emp->Emax1 = emp->ssq;
	      emp->Esum1rel  +=   (emp->ssq - emp->Ebase1); // For avrg energy of the states we were in recently
	      emp->E2sum1rel += SQ(emp->ssq - emp->Ebase1); // For variance & std.dev. (+ approx. heat capacity)
	      emp->nacc++; // For calculating acceptance rate (the above E* are also updated for unchanged states)
	    }
            if(emp->ssq > emp->ssqMin)
              continue;            // Worse than best known result, so go on searching new switches

// Same or new Qf optimum found.  Get vacancy quality to see if _that_ has improved (if wanted).
            int64_t ssVall = -9999, ssVmin = -9999;
            long double Fw=0;
            int PssVmin = -9999;
            if(use_Qf1)
            {
              get_vacancy_ints(NB, NP, Vacant, range1, range2, bp2d, bp2p, balans, &ssVall, &ssVmin, &PssVmin);
              if(use_Qf1av)
                Fw = ssVall + weight*ssVmin;
              else
                Fw = ssVmin;
              if(emp->ssq == emp->ssqMin && Fw > emp->FwBest)
                continue;            // Worse than best known result, so go on searching new switches
            }

// Same or new (Qf, Fw) optimum found.  If wanted, then get d4-related sum to see if _that_ has improved.
            int64_t sumd4 = (K4 ? sum_part_d4_upper2d(NP, balans, S, N) : 0);

            if(!(emp->ssq < emp->ssqMin || Fw < emp->FwBest || sumd4 < emp->sumd4Best))
              continue;            // Worse or equal to best known result, so go on searching new switches

// save the seatings because this _is_ the best result sofar (better ssq or Fw (same ssq) or sumd4 (same ssq+Fw))
            emp->ssqMin      = emp->ssq;
            emp->FwBest      = Fw;       // Maybe worse than before, but associated with currently best ssq
            emp->sumd4Best   = sumd4;    // Ditto for d4-related power sum
            emp->ssVallBest  = ssVall;
            emp->ssVminBest  = ssVmin;
            emp->PssVminBest = PssVmin;
            emp->tempiAtBest = tempi;
            copy_bp2d(NB, NP, bp2d, bp2dBest_e[ei]);
            newBest_any = 1;

            if (slow)  // Only in the crazy fast, mixed algorithm we do the deep freeze & retry stuff
              continue;
            emp->newBest = emp->odd = 1; // Deep freeze: temporarily only accept further improv.
            if (iter <= MAXIT-NLIMIT_RETRY)
              emp->retry_at_iter = iter + (emp->retry_delay = NDELAY_RETRY); // Delay Temp reset, retry later
            else
            {
              emp->retry_at_iter = 0; // Cancel any waiting earlier optimum for retry
              emp->tidx = 0;          // Immediately reset temperature to lowest above zero
              tempi = (float)algoc1;  // After deeep freeze, we then continue with slow heating (usually)
            }
          }                        /* loop over all boards and tables */
        }                          /* end loop nrep */
        if(use_Estats && iter % NITER_E1 == 0)
        { // Calculate aggregated statistics for now complete period before resetting counters for the next one.
          // "Average" is just "time average" (of recent small dt) for the current ensemble member
          emp->Eavg1r = ((double) emp->Esum1rel)/NEnergy1; // Relative to base = saved optimum at period start
          emp->Evar1 = (double)((emp->E2sum1rel - SQ((long double) emp->Esum1rel)/NEnergy1)
                                / (NEnergy1 - 1)); // Variance
          emp->heatC = emp->Evar1/SQ((double)tempi);  // Theory estimate if slow-varying T and good statistics
          emp->accRate = ((double) emp->nacc)/NEnergy1; // Fraction of attempted arrow switches that we accepted
          emp->Ebase1_last   = emp->Ebase1;
          emp->Emin1rel_last = emp->Emin1 - emp->Ebase1;
          emp->Emax1rel_last = emp->Emax1 - emp->Ebase1;

          // Reset statistics counters for the next period
          emp->Emin1 = emp->ssq; // Smallest energy (ssq) of accepted states recently (is always >= ssqMin)
          emp->Emax1 = emp->ssq; // Largest energy of accepted states recently
          emp->Ebase1 = emp->ssqMin; // Sums & printed E* are relative for easy read & minimise risk of overflow
          emp->Esum1rel = 0; emp->E2sum1rel = 0; // Summing of (E-Ebase1) and (E-Ebase1)**2 for average etc.
          emp->nacc = 0;
        }
        // Copy back ensemble-loop local arrays to the shared ones
        //memcpy((void*) balans_e[ei],  (void*) balans,  sizeof(balans));
        //memcpy((void*) bp2d_e[ei],    (void*) bp2d,    sizeof(bp2d));
        copy_balans(NP, balans, balans_e[ei]);
        copy_bp2d(NB, NP, bp2d,   bp2d_e[ei]);
        memcpy((void*) &rstate_e[ei], (void*) &rstate, sizeof(rstate));
        memcpy((void*) &ensemble[ei], (void*) &estate, sizeof(estate));
      }                            /* end loop esize (ensemble members)*/
    }                              /* end loop iter */
    if(P_progress && esize>1)
    { // Print best values for all members, normally not much more than 10
      printf("=== Results for all %3d members of ensemble: ======\n", esize);
      emember_state_t *emp=ensemble;
      for(int ei=0; ei<esize; ei++, emp++)
      {
        double Qf_e     = Qf64(emp->ssqMin, S, N);
        double Qf1av_e  = (use_Qf1 ? Qf64(emp->ssVallBest, S1all, N1*NPA)            : -1);
        double Qf1max_e = (use_Qf1 ? Qf64(emp->ssVminBest, S1[emp->PssVminBest], N1) : -1);
        double d4_e     = d4(S, N, emp->ssqMin, emp->sumd4Best);
        showprogress2(ei+1, (double) emp->tempiAtBest, Qf_e, Qf1av_e, Qf1max_e, emp->PssVminBest, d4_e,
                      0, 0, 0, Verbose);
      }
      printf("===================================================\n");
    }
    int eiB = empB->eidx; // Index of best solution
#ifdef DEBUG2
    for(int b=0; b<NB; b++)
    {
      printf("DEBUG: Board %3d:\n", b+1);
      printf("DEBUG:   Pair:"); for(int p=0; p<NP; p++) printf(" %2d", p+1);               printf("\n");
      printf("DEBUG:   bp2d:"); for(int p=0; p<NP; p++) printf(" %2d", bp2d_e[eiB][b][p]); printf("\n");
    }
      printf("DEBUG: Changed ns-ew board:\n");
#endif
// change the scheme to account for arrow switches
    for(int r=0; r<NR; r++)
      for(int t=0; t<NT; t++)
    {
      int b = scheme[0][t][r] - 1;
      int ns= scheme[1][t][r] - 1;
      int ew= scheme[2][t][r] - 1;
      if(b<0 || ns==Vacant-1 || ew==Vacant-1 || ns>=NP || ew>=NP)
        continue; // These cases are skipped, but not considered to be errors

      if(bp2dBest_e[eiB][b][ew] > 0)
      {
        scheme[1][t][r] = ew+1;
        scheme[2][t][r] = ns+1;
#ifdef DEBUG2
        printf("DEBUG:  %2d-%2d %2d\n", ew+1, ns+1, b+1);
#endif
      }
    }
    return eiBest_cnt;
  }
}

#ifdef MVER_DEFAULT

#ifndef USE_MVER
#error "When MVER_DEFAULT is defined for improve2.c you must also define USE_MVER for the entire build"
#endif

/* Define prototypes for the versions in external object files */
int improve2_avx2(int NB, int NR, int NT, int NP, int MAXIT, int esize, int Vacant, int scheme[3][NT][NR],
                  int fixers[], int K4, int range1, int range2, int use_Qf1av, double w12,
                  int algoi, double algoc1, double algoc2, int Verbose);
int improve2_avx(int NB, int NR, int NT, int NP, int MAXIT, int esize, int Vacant, int scheme[3][NT][NR],
                 int fixers[], int K4, int range1, int range2, int use_Qf1av, double w12,
                 int algoi, double algoc1, double algoc2, int Verbose);

/* ================== Dispatcher choosing the right one of the 3 versions =========================== */
// Public entrypoint is here in the multiversioning case; optimised & default functions are then private
int improve2(int NB, int NR, int NT, int NP, int MAXIT, int esize, int Vacant, int scheme[3][NT][NR],
             int fixers[], int K4, int range1, int range2, int use_Qf1av, double w12,
             int algoi, double algoc1, double algoc2, int Verbose)
{
  // Dispatch manually depending on detected CPU feature.  Only called once per balans main() invocation
  // so no need to cache or use function pointers.  Call directly instead, possibly helping clang optimisation.
  enum cpu_targets cpu_target = raninit_cpu_support_enum();
  if (cpu_target == TARGET_AVX2)
    return improve2_avx2(NB, NR, NT, NP, MAXIT, esize, Vacant, scheme, fixers, K4, range1, range2,
                         use_Qf1av, w12, algoi, algoc1, algoc2, Verbose);
  else if (cpu_target == TARGET_AVX)
    return improve2_avx(NB, NR, NT, NP, MAXIT, esize, Vacant, scheme, fixers, K4, range1, range2,
                        use_Qf1av, w12, algoi, algoc1, algoc2, Verbose);
  else
    return improve2_default(NB, NR, NT, NP, MAXIT, esize, Vacant, scheme, fixers, K4, range1, range2,
                            use_Qf1av, w12, algoi, algoc1, algoc2, Verbose);
}

#endif /* MVER_DEFAULT */
