#include <limits.h>
#include "balans.h"

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

double Qf(int64_t SS, int S, int N)
/* Qf as defined in Groot Schemaboek N.B.B. by F.C. Schiereck
 */
{
  return (SS > 0 ? 100.0 * (double)(minsq64(S,N)) / (double)SS : 0.0);
}

double Qc(int64_t SS, int S, int N)
{
  return (SS > 0 ? 100.0 * (double)S*(double)S/(N*(double)SS) : 0.0);
}

void balance(int mode, SPUL *movement, int P1, int G,
  int h, int Vacant, QUAL* pQ)
{
  int64_t SS, tmp64;
  long double SNl;
  double SN, var, S4, ftmp;
  int k,m,ip,iq,ip2,iq2,mat[P1][P1], S=0, N=0;

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
        S++;
        for(m=0;m<k;++m)
          if(movement[k].bord == movement[m].bord)
        {
          iq= movement[m].noordzuid[0];
          iq2=movement[m].noordzuid[1];
          if(iq!=Vacant && iq2!=Vacant)
          {
            mat[ip-1][iq-1]  += gewicht1;
            mat[ip-1][iq2-1] -= gewicht1;
            mat[ip2-1][iq2-1]+= gewicht1;
            mat[ip2-1][iq-1] -= gewicht1;
          }
        }
      }
    }
  }
  if (mode)
  {
    printf("\nthe score matrix:\n");
    for(ip=0;ip<P1;++ip)
    {
      for(iq=0;iq<P1;++iq)
        if (ip==iq) printf("   *");
        else printf("%4d",mat[ip][iq]+mat[iq][ip]);
      printf("\n");
    }
    fflush(stdout);
  }

  S*= h;
  pQ->S= S;
  N=P1;
  if(Vacant)N--;
  pQ->P= N;
  N= N*(N-1)/2;
  pQ->N= N;
  SN=(double)(SNl=(long double)S/N);
  SS=0;
  S4=0;
  for(ip=0;ip<P1;++ip)
  if(ip!=Vacant-1)
  {
    for(iq=0;iq<ip;++iq)
    if(iq!=Vacant-1)
    {
      tmp64=mat[ip][iq]+mat[iq][ip];
      SS+= tmp64 * tmp64;
      ftmp=mat[ip][iq]+mat[iq][ip]-SN;
      S4+= ftmp*ftmp*ftmp*ftmp;
    }
  }
  var= (double)(SS - SNl*SNl*N);
  pQ->Qc= h>0?Qc(SS,S,N) : 0;
  pQ->Qf= h>0?Qf(SS,S,N) : 0;
  pQ->sd= (var<0)? 0:sqrt(var/N);
  pQ->d4= (S4<0)? 0:sqrt(sqrt(S4/N));
  pQ->var= var;
  pQ->SS = SS;
  pQ->s4= S4;
  return;
}
