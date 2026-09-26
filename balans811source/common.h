#include "balans.h"
#include "inspect.h"

void renumber(mov_info_t *pmv, const int newp[], const int fix[],
              int Letters, int Permutation, int Mb, int Lowestboard);

void unify(mov_info_t *pmv);
int readfixers(FILE *ffixerfile, int N, int fix[N]);
int geth(mov_info_t *pmv, int do_warn);
