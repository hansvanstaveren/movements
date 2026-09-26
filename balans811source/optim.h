#include "Verbose.h"

int improve2(int NB, int NR, int NT, int NP, int MAXIT, int esize, int Vacant, int scheme[3][NT][NR],
             int fixers[], int K4, int range1, int range2, int use_Qf1av, double weight,
             int algoi, double algoc1, double algoc2, int Verbose);

/* showprogress2 reports the current state during the iterations */
void showprogress2(int iter, double T, double Qf, double Qf1av, double Qf1max, int bestV, double d4,
                   int nebest, int esize, int opt_busy, int Verbose);

/* qinterrupt checks for a user interrupt request */
  int qinterrupt(void);

int setmaxtime(int max);  

int initf(void);

extern int INTERRUPT;
