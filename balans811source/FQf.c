/* @(#)FQf.c        v3.57 Mei 2017*/
/* edit mei 2010: niet langer h=t-1, maar call Seth() */
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include "readmove.h"

int Fbalance(SPUL *movement, int P1, int G,
int h, int Vacant, double SN, double *S4)
{
  int k,m,ip,iq,ip2,iq2,SS, mat[P1][P1];
  double ftmp;

  for(ip=0;ip<P1;++ip)
    for(iq=0;iq<P1;++iq)
      mat[iq][ip]=0;

  for(k=0;k<G;++k)
  {
    ip= movement[k].noordzuid[0];
    if(ip!=0 && ip!=Vacant)
    {
      ip2=movement[k].noordzuid[1];
      if(ip2!=Vacant)
      {
        mat[ip-1][ip2-1]+=h;
        for(m=0;m<k;++m)
          if(movement[k].bord == movement[m].bord)
        {
          iq= movement[m].noordzuid[0];
          iq2=movement[m].noordzuid[1];
          if(iq!=Vacant && iq2!=Vacant)
          {
            mat[ip-1][iq-1]  += 1;
            mat[ip-1][iq2-1] -= 1;
            mat[ip2-1][iq2-1]+= 1;
            mat[ip2-1][iq-1] -= 1;
          }
        }
      }
    }
  }

  SS=0;
  *S4=0;
  for(ip=0;ip<P1;++ip)
    for(iq=0;iq<ip;++iq)
  {
    ftmp=mat[ip][iq]+mat[iq][ip];
    SS+= ftmp*ftmp;
    ftmp= ftmp-SN;
    ftmp= ftmp*ftmp;
    *S4+= ftmp*ftmp;
  }
  return SS;
}

int minsq(int S, int N)
/* how to divide the integer value S over N boxes, such that the
 * sum of squares is minimal
 */
{
  int iavg, nhigh, nlow;
  int high, low;

  iavg= S/N;
  nhigh= S - iavg*N;
  nlow= N - nhigh;
  high= iavg+1;
  low= iavg;
  return nhigh*high*high + nlow*low*low;
}

double FQf(SPUL *movement, int P, int G, int r, int b)
/* Qf as defined in Groot Schemaboek N.B.B. by F.C. Schiereck
 */
{
  int t,h,S,N;
  double S4, SN;
  int SS;

  t=P/2;
// h = half a top on each board
//  h = P/2 - 1; // not correct if nr of board groups != nr of rounds
  h = P*r/(2*b) - 1; // we assume all boards have the same top
  S=t*r*h;
  N=P*(P-1)/2;
  SN=(double)S/(double)N;
  SS=Fbalance(movement,P,G,h,0,SN, &S4);
  if(SS > 0) return (double)minsq(S,N) / (double)(SS)*100.0;
  else return 0;
}
