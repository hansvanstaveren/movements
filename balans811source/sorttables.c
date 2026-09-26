#include "readmove.h"
#include "orderb.h"
#include "sorttables.h"

void sorttables(SPUL *movem, int Nt, int r, int t1)
{
  int i, j, nswap,nr, b;
  SPUL movetmp;

  for(nr=0; nr<r; ++nr)
  {
    b=nr*t1;
    nswap=1;
    while(nswap)
    {
      nswap=0;
      for(j=1; j<Nt; ++j)
        for(i=0; i<j;  ++i)
          if(movem[j+b].bord < movem[i+b].bord)
          {
            movetmp = movem[i+b];
            movem[i+b]= movem[j+b];
            movem[j+b]= movetmp;
            ++nswap;
          }
    }
  }
}

#if 0
void sortboardgroups(mov_info_t *pmv)
{
  int did_swap;
  int nb_unsorted = pmv->Nb;

  do // Simple bubble sort, moderately optimised version
  {
    did_swap=0;
    for (int j=1; j < nb_unsorted; j++)
    {
      int i=j-1;
      if(pmv->letter[i] <= pmv->letter[j]) continue;
      unsigned int tmp = pmv->letter[i];
      set_letter(pmv, i, pmv->letter[j]);
      set_letter(pmv, j, tmp);
      did_swap = 1;
    }
    nb_unsorted--; // The top element(s) already sorted
  } while(did_swap);
}
#else
// Instead use qsort for letter[] and set order_b[] accordingly afterwards.
// So here we consider ourselves a friend of orderb.* and write to the arrays directly.

static int cmpchrp(const void *a, const void *b)
{
  unsigned char c = *((const unsigned char *)a);
  unsigned char d = *((const unsigned char *)b);
  return (c > d) - (c < d); // -1 if c<d, 0 if c==d, +1 if c>d
}

void sortboardgroups(mov_info_t *pmv)
{
  qsort(pmv->letter, pmv->Nb, sizeof(unsigned char), cmpchrp);

  for(int i=0; i<256; i++) pmv->order_b[i] = -1; // Clear for extra safety
  for(int i=0; i<pmv->Nb; i++) pmv->order_b[pmv->letter[i]] = i; // Refill cache
}
#endif
