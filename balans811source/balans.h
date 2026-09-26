/* General meaning of parameters.
 *
 * movement: the movement, array of "SPUL", of length G
 * P1      : number of pairs in the movement
 * t1      : number of tables in the movement
 * P       : actual number of pairs (may be P1 -1 if a pair is absent)
 * t       : number of tables occupied simultaneously <= t1
 * r       : number of rounds
 * G       : t1 x r, length of array movement
 * Vacant  : if non-zero, a pair that is absent
 * h       : score weight of an encounter
 *           number of times each board is played - 1
 * fix[G]  : array of fixers, if fix[j]!=0 table j is kept fixed
 * SN      : average value in score matrix
 * S4      : quality criterion based on 4th power
 */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <math.h>

#if defined(MS_DOS) || defined (USE_DLL)
#include "winver.h"
#endif

#include "Verbose.h"
#include "readmove.h"
#include "orderb.h"
#include "readmvi.h"
#include "random/raninit.h"

#define gewicht1 1
#define eps 1.0e-5
// max number of commandline arguments of the same type:
#define MAXFIX 128

#define szVersion "v8.1.1"

// number of algorithms for option -S (range 0..NALGO-1)
#define NALGO 3
// default values for (c1,c2) in -Sn:c1:c2 for each algo (c1 = start temperature for all of them)
static const double algo_cdefault[NALGO][3]=
{
  {   1, 2000},  // algo 0 (crazy fast): (Tmin,Tmax), must match the array 'temp' in improve2.c
  { 400,    4},  // algo 1 (slow exp):   (Tstart,Tend), but defaults instead set from # of pairs
  {2000,  0.1}   // algo 2 (slow adapt): (Tstart,speed), but defaults instead from NP*NR & niter
};

typedef struct
{
  int P;
  int S;
  int N;
  int r;
  int64_t SS;
  double var;
  double sd;
  double d4;
  double s4;
  double Qf;
  double Qc;
  double Qf1av;
  double Qf1max;
} QUAL;

int initf(void);

#ifndef USE_DLL
// if USE_DLL, then the user (balans.c/demo.c) will declare them, so avoid 'gcc -Wshadow' warning

double Qf(int64_t SS, int S, int N);
double Qc(int64_t SS, int S, int N); // Not used outside balans1.c, but wanted here for symmetry reasons anyway

int setmaxtime(int maxtime);

void balance(int mode, SPUL *movement, int P1, int G, int h, int Vacant, QUAL *pQ);

int optim_W(mov_info_t *pmv, int Samples, int esize, int fix[], int K4, int vfirst, int vlast,
            int use_Qf1av, double weight, int algoi, double algoc1, double algoc2, int Verbose, char *Label);

int optim(mov_info_t *pmv, int Samples, int esize, int fix[], int K4, int vfirst, int vlast,
          int use_Qf1av, double weight, int algoi, double algoc1, double algoc2, int Verbose);
#endif

#ifdef SUBPROGRAM
int balans_main(int argc, char *argv[], QUAL *Qual, FILE **result);
#endif
