#include "readmove.h"
#include "orderb.h"
#include "inspect.h"

int inspect(mov_info_t *pmv, int printmode, int do_warn, double *Qo)
{
  int P1 = pmv->P1, B = pmv->b, Vacant = pmv->Vacant;
  SPUL *movement = pmv->movement;

  int boards[P1][pmv->b],opps[P1][P1],pairs[P1];
  int k,b,bmax=0,G1=0;

  for(int l=0; l<P1; l++) for(int m=0; m<B; m++) boards[l][m]=0;
  for(int l=0; l<P1; l++) for(int m=0; m<P1; m++) opps[l][m]=0;

  k=0;
  for(int nr=0; nr < pmv->r; nr++)
  {
    for(int l=0;l<P1; l++) pairs[l]=0;

    for(int nt=0; nt < pmv->t1; nt++)
    {
      int p1=  movement[k].noordzuid[0] -1;
      if(p1>=0)
      {
        int p2=  movement[k].noordzuid[1] -1;
        if(p1+1!=Vacant && p2+1!=Vacant) G1++;
        if(p1>=P1 || p2>=P1)
        {
          fprintf(stderr,"ERROR! Pair number too high: round %d table %d pair %d or %d\n",nr+1,nt+1,p1+1,p2+1);
          pmv->Status |= Invaliddat;
          return -1;
        }
        pairs[p1] += 1;
        pairs[p2] += 1;
        b=   orderb(pmv, pmv->movement[k].bord);
        if(b>bmax)bmax=b;
        if(b<0)
        {
          if(do_warn)
          {
            fprintf(stderr,"ERROR: illegal board, k=%d, b=%d\n", k,b);
            fprintf(stderr,"%d-%d:%d (%c)\n",
            movement[k].noordzuid[0],
            movement[k].noordzuid[1],
            movement[k].bord,
            movement[k].bord);
          }
          pmv->Status |= Invaliddat;
          return -1;
        }
        if(b>=B)
        {
          fprintf(stderr,"ERROR: %d or more boardsets, but only %d in header\n",b+1,B);
          pmv->Status |= Invaliddat;
          return -1;
        }
        if(p1+1!=Vacant && p2+1!=Vacant)
        {
          ++boards[p1][b];
          ++boards[p2][b];
          ++opps[p1][p2];
          ++opps[p2][p1];
        }
      }
      k++;
    }
    for(int l=0;l<P1; l++)
      if(pairs[l]>1)
    {
      if(printmode) fprintf(stderr,"WARNING! pair %d plays twice in round %d\n",l+1,nr+1);
      pmv->Status |= Dubbelpair;
    }
  }
  if(printmode)
  {
    printf("\n   ");
    for(int l=0; l<=bmax; l++) printf((pmv->Getallen ? "%2d" : " %c"), Letter(pmv,l));
    printf("    ");
    for(int m=0; m<P1; m++) printf("%2d",m+1);
  }
  for(int l=0; l<P1; l++)
  {
    if(printmode)printf("\n%2d:",l+1);
    for(int m=0; m<=bmax; m++)
    {
      if(printmode)
        {if(boards[l][m])printf(" %1d",boards[l][m]); else printf(" .");}
      if(boards[l][m]>1)pmv->Status|= Dubbelbord;
    }
    if(printmode)printf("    ");
    for(int m=0; m<P1; m++)
    {
      if(printmode)
      {
        if(opps[l][m])printf(" %1d",opps[l][m]);
        else { if(l==m)printf(" \\"); else printf(" ."); }
      }
      if(opps[l][m]>1)pmv->Status|= Dubbelopp;
    }
  }
  if(printmode)printf("\n");
  if((pmv->Status & Dubbelopp) && printmode)fprintf(stderr,"WARNING!! same opponents meet twice\n");
  if(pmv->Status >  Dubbelpair) return -1;

  double SS=0;
  double N=(double)((P1-1)*(Vacant ? P1-2 : P1)/2);
  double SN=(double)G1/N;
  for(int ip=0; ip<P1; ip++)
    for(int iq=0; iq<ip; iq++)
      if(ip+1!=Vacant && iq+1!=Vacant)
  {
    SS+= (opps[ip][iq]-SN) * (opps[ip][iq]-SN);
  }
  *Qo= 100.0*SN*SN;
  if(*Qo>0) *Qo/= (SS/N + SN*SN);

  return 0;
}
