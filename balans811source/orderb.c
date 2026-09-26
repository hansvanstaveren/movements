#include "readmove.h"
#include "orderb.h"

static int fill_orderb_arrays_if_needed(mov_info_t *pmv)
{
  int Nb = pmv->Nb; // Handy shorthand, stored back before we return

  if(Nb != 0) return 0; // Already initialised

  for(int i=0; i<256; i++) pmv->letter[i] = (unsigned char)0;
  for(int i=0; i<256; i++) pmv->order_b[i] = -1;

  pmv->Bias = 0x7fffffff;
  for(int k=0; k < pmv->G; k++)
  {
    if(!pmv->movement[k].noordzuid[0]) continue;

    unsigned int c = pmv->movement[k].bord;
    if(pmv->order_b[c] >= 0) continue;

    set_letter(pmv, Nb++, c);
    if(pmv->Bias > c) pmv->Bias= c;

    if(Nb>256)
    {
      pmv->Nb = 256;
      fprintf(stderr,"error!! too many different board sets!!\n");
      return -1;
    }
  }
  // Commented out because caller checks and takes care of warning/error instead:
  // if(Nb != pmv->b) fprintf(stderr,"WARNING: %d board sets, but %d expected\n",Nb,pmv->b);
  pmv->Nb = Nb;
  return 0;
}

int orderb(mov_info_t *pmv, unsigned int L)
{
  if(fill_orderb_arrays_if_needed(pmv) < 0)
    return -1;

  if(pmv->order_b[L] < 0)
    fprintf(stderr,"error: letter not found: '%c' %2x\n",L,L);

  return pmv->order_b[L];
}

int Letter(mov_info_t *pmv, int k)
{
  fill_orderb_arrays_if_needed(pmv);

  return pmv->letter[k];
}
