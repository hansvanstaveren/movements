#include "common.h"

void renumber(mov_info_t *pmv, const int newp[], const int fix[],
              int Letters, int Permutation, int Mb, int Lowestboard)
{
  SPUL tmpmove[pmv->G];
  SPUL *movement = pmv->movement;

  for(int k=0; k < pmv->G; k++)
  {
    tmpmove[k]= movement[k];
    if(movement[k].noordzuid[0])
    {
      if(Letters)
      {
        tmpmove[k].bord= 'A' + movement[k].bord - pmv->Bias;
      }
      if(Mb) //  board groups as given sequence of numbers (option -n)
      {
        int j= orderb(pmv, movement[k].bord);
        tmpmove[k].bord= (unsigned int) (j+Lowestboard);
      }
      if(Permutation) // option -p
      {
        tmpmove[k].noordzuid[0]= newp[movement[k].noordzuid[0]-1];
        tmpmove[k].noordzuid[1]= newp[movement[k].noordzuid[1]-1];
      }
      if(fix[k]==2) // vernum options -R D I J w
      {
        tmpmove[k].noordzuid[0]= movement[k].noordzuid[1];
        tmpmove[k].noordzuid[1]= movement[k].noordzuid[0];
      }
    }
    else
    {
      if(pmv->Getallen)tmpmove[k].bord= 0;
      if(Letters)tmpmove[k].bord= '0';
// the order of these 2 statements matters
// both Getallen and Letters may be true!
    }
  }

  for(int k=0; k < pmv->G; k++) movement[k]= tmpmove[k];
  if(Letters || Mb) pmv->Nb = 0; // make orderb()/Letters() reinitialise at next call
}

void unify(mov_info_t *pmv)
{
  SPUL *movement = pmv->movement;
  int P1 = pmv->P1; // number of pairs (including possible absent one)
  int uni[P1 + 1];  // size is highest pair nummber + 1
  int k=0;

  for(int i=0; i<P1+1; i++) uni[i]=P1;

  for(int j=0; j < pmv->t1; j++)
    if(movement[j].noordzuid[0]>0)
  {
    if(movement[j].noordzuid[0] > P1 || movement[j].noordzuid[1] > P1)
    {
      fprintf(stderr,"Pair number too large %d or %d\n",
        movement[j].noordzuid[0], movement[j].noordzuid[1]);
      return;
    }
    uni[movement[j].noordzuid[0]] = ++k;
    uni[movement[j].noordzuid[1]] = ++k;
  }

  for(int j=0; j < pmv->G; j++)
    if(movement[j].noordzuid[0]>0)
  {
    movement[j].noordzuid[0] = uni[movement[j].noordzuid[0]];
    movement[j].noordzuid[1] = uni[movement[j].noordzuid[1]];
  }

  if(pmv->Vacant > 0) pmv->Vacant = uni[pmv->Vacant];
}

int readfixers(FILE *ffixerfile, int N, int fix[N])
{
  int ier;
  char c, i[2]=" ";

  rewind(ffixerfile);
  for (int m=0; m<N; ++m)
  {
    c='\0';
/* skip blanks, linefeeds, and other junk */
    while((c < '0' || c > '1'))
    {
      c= (char)getc(ffixerfile);
      if(c==EOF){ fprintf(stderr,"error in fixer file!"); return(-1);}
    }
/* decode character */
    i[0]=c;
    ier= sscanf(i,"%1d",&fix[m]);
    if(ier!=1){ fprintf(stderr,"error in fixer file!"); return(-1);}
  }
  return 0;
}

int geth(mov_info_t *pmv, int do_warn)
{
// Sets in hh[] the number of times each board is played minus 1 and returns the lowest one.
// If not all boards are played the same number of times a warning message is given (if warnings enabled)
// and the Unsuitable flag set in *pStatus.  If orderb() returns an out-of-bounds number
// we unconditionally print an error and set the Unsuitable flag too.
  SPUL *movement = pmv->movement;
  int b = pmv->b, Vacant = pmv->Vacant;
  int P = (Vacant ? pmv->P1 - 1 : pmv->P1); // Number of pairs excluding vacancy
  int hh[b];
  int hmin=0, hmax=0;

  for(int i=0; i<b; ++i) hh[i]=-1;
  for(int i=0; i < pmv->G; i++)
    if(movement[i].noordzuid[0]>0 &&
       movement[i].noordzuid[0]!=Vacant &&
       movement[i].noordzuid[1]!=Vacant)
    {
      int spel = orderb(pmv, movement[i].bord);
      if(spel < 0)
      {
        pmv->Status |= Unsuitable;
        return -1; // orderb() already printed error unconditionally
      }
      if(spel > b)
      {
        fprintf(stderr,"ERROR: orderb() returned %d, higher than b=%d (i=%d)\n", spel, b, i);
        pmv->Status |= Unsuitable;
        return -1;
      }
      // After those checks we're finally sure that we won't access hh[] out-of-bounds
      hh[spel] += 1;
    }
  if(b > 0) hmin = hmax = hh[0];
  for(int i=0; i<b; ++i)
  {
    if(hh[i]>hmax)hmax=hh[i];
    if(hh[i]<hmin)hmin=hh[i];
  }
  if(hmin!= P/2-1 && do_warn) fprintf(stderr,"WARNING! Players do not play all boards.\n");
  if(hmax!=hmin)
  {
    pmv->Status |= Unsuitable;
    if(do_warn) fprintf(stderr,"WARNING! Not all boards are played the same number of times.\n");
  }
  if(hmin<1 && do_warn) fprintf(stderr,"WARNING! h= %d, can't calculate MP scores\n", hmin);
  return hmin;
}
