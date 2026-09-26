/* Copyright (C) 2006-2018 Peter Smulders
 * The GNU General Public License (GPL-3.0-or-later)
 * applies to this software, see COPYING.txt
 */

#include "common.h"
#ifdef XGETOPT
#include "XGetopt.h"
extern char *Xoptarg;         //XGetopt globals defined in XGetopt.c
extern int  Xoptind, Xopterr, Xoptopt;
#endif
#define MANY 40000

#if 0
static void echo(int argc, char *argv[])
{
// echo the call, used for testing
  int j;
  char *p; // substitute for misbehaving basename()
  p= strrchr(argv[0], '/');         // MinGW
  if(!p) p= strrchr(argv[0], '\\'); // Windows
  if(!p) p= argv[0];                // Linux
  else p++;
  printf("########################################### %s",p);
  for (j=1; j<argc; j++) printf(" %s",argv[j]);
  printf("\n");
}
#endif

/* Random50 is attempt to introduce at random boards with a flat (=0) score (experimental) */
static int Random50=0;

struct score
{
  int p;
  int s;
  int MP;
};

static int Top;
static double Tops,Subtop;
static int Histo,Pp;
static int Extensive;
static int Een;
static int Bus;
static int WBus, VBus, SBus, Samples;
static int RandomSpel[100];

static double percent(double Score, double top, int rr)
{
  if(top*rr > 0)
  {
    return Score*100.0/(top*rr);
  }
  else return 0.0;
}

static void shuffle(int N[], int n)
{
/* the Knuth shuffle */
  int i,ip, j;

  for(j=0; j<n; ++j)
  {
    i=iigenrand(j+1);
    ip=N[j];
    N[j]=N[i];
    N[i]=ip;
  }
}

static void sortscore(int Nn, double s[], int n[])
{
  int tmpn, i, j, nswap, same; double tmps;

  nswap=1;
  while(nswap)
  {
    nswap=0;
    for(j=1; j<Nn; ++j)
      for(i=0; i<j;  ++i)
      {
        if(s[j] > s[i])
        {
          tmps=s[j]; s[j]=s[i]; s[i]=tmps;
          tmpn=n[j]; n[j]=n[i]; n[i]=tmpn;
          ++nswap;
        }
      }
  }
  same=0;
  for(j=1; j<Nn; ++j)
  {
    if (fabs(s[j] - s[j-1]) < 0.001)
    {
      if(!same)same=j;
    }
    else if (same)
    {
      shuffle(&n[same-1],j-same+1);
      same=0;
    }
  }

  if (same)
  {
    shuffle(&n[same-1],Nn-same+1);
  }
}

static int makescores(mov_info_t *pmv, int mode, int h, const char *inputfile, int Verbose)
{
  SPUL *movement = pmv->movement;
  int b = pmv->b, P = pmv->P1, G = pmv->G;
  int k,ip,ip1,ip2, held, beer,i,j;
  int spel, dir[b], bir[b], ind, indw, histo[51], nscore, del1, del2;
  double s1, s2, Score[P+1], top, left, scores[P*P*(P-1)/2],
  scoresw[P*P*(P-1)/2], scr[P*P*(P-1)/2];
  int Nscr[P*P*(P-1)/2], Nn;
  int rr[P+1];
  double var, avg, nv;

  top=h*2;
  for(k=1; k<=P; ++k)rr[k]=0;
  for(k=1; k<=P; ++k)Score[k]=0.0;
  for(k=0; k<b; ++k) dir[k]=0;
  for(k=0; k<G; ++k)
  {
    ip=movement[k].noordzuid[0];
    if(ip)
    {
      ip2=movement[k].noordzuid[1];
      ++rr[ip];
      ++rr[ip2];
    }
  }
// rr[ip] now is number of boards played by pair ip

  if(mode == 1)
  {
  // only 1 strong pair
    if P_out_mini
    {
      printf("\n");
      for(i=1; i<=P; ++i)printf("%7d",i);
      printf("\n");
    }

    for(held=1; held<=P; ++held)
    {
      for(k=1; k<=P; ++k)Score[k]=0.0;
      for(k=0; k<b; ++k) dir[k]=0;

      for(k=0; k<G; ++k)
      {
        ip=movement[k].noordzuid[0];
        if(ip)
        {
          ip2=movement[k].noordzuid[1];
          spel=orderb(pmv, movement[k].bord);
          if(ip==held){dir[spel]= 1;}
          if(ip2==held){dir[spel]= -1;}
        }
      }

      for(k=0; k<G; ++k)
      {
        ip=movement[k].noordzuid[0];
        if(ip>0)
        {
          ip2=movement[k].noordzuid[1];
          spel=orderb(pmv, movement[k].bord);
          if(ip==held)
          {
            del1= h*2;
            del2= 0;
          }
          else
          {
            if(ip2==held)
            {
              del1= 0;
              del2= h*2;
            }
            else
            {
              del1= h-dir[spel];
              del2= h+dir[spel];
            }
          }
          Score[ip]+=  del1;
          Score[ip2]+= del2;
        }
      }
      if P_out_mini
      {
        for(k=1; k<=P; ++k)printf("%7.2f",percent(Score[k],top,rr[k]));
        printf("\n");
      }
    }
    return 1;
  }
  //end one strong pair

  ind=0;
  indw=0;
  nscore=P*P*(P-1)/2;

  for(held=1; held<P; ++held)
    for(beer=held+1; beer<=P; ++beer)
  {
    for(k=1; k<=P; ++k)Score[k]=0;
    for(k=0; k<b; ++k) dir[k]=0;
    for(k=0; k<b; ++k) bir[k]=0;

    for(k=0; k<G; ++k)
    {
      ip1=movement[k].noordzuid[0];
      if(ip1)
      {
        ip2=movement[k].noordzuid[1];
        spel=orderb(pmv, movement[k].bord);
        if(ip1==held){dir[spel]= 1;}
        if(ip2==held){dir[spel]= -1;}
        if(ip1==beer){bir[spel]= 1;}
        if(ip2==beer){bir[spel]= -1;}
      }
    }

    for(k=0; k<G; ++k)
    {
      ip1=movement[k].noordzuid[0];
      if(ip1)
      {
        ip2=movement[k].noordzuid[1];
        spel=orderb(pmv, movement[k].bord);
        if(ip1==held)
        {
          if (ip2==beer)
          {
            s1= h; s2= h;
          }
          else
          {
            if(bir[spel]!=dir[spel])
            {
              s1= Tops; s2= h*2-Tops;
            }
            else {s1= Subtop; s2= h*2-Subtop; }
          }
        }
        else if(ip2==held)
        {
          if (ip1==beer)
          {
            s1= h; s2= h;
          }
          else
          {
            if(bir[spel]!=dir[spel])
            {
              s1= h*2-Tops; s2= Tops;
            }
            else {s1= h*2-Subtop; s2= Subtop;}
          }
        }
        else if(ip1==beer)
        {
          if(bir[spel]!=dir[spel])
          {
            s1= Tops; s2= h*2-Tops;
          }
          else {s1= Subtop; s2= h*2-Subtop; }
        }
        else if(ip2==beer)
        {
          if(bir[spel]!=dir[spel])
          {
            s1= h*2-Tops; s2= Tops;
          }
          else {s1= h*2-Subtop; s2= Subtop;}
        }
        else
        {
          if(dir[spel]!=bir[spel]){ s1=h; s2=h; }
          else
          {
            if(h>1)
            {
              left= ((double)(h*(h+1)) -2.0*Subtop)/(h-1);
            }
            else
              left=1;
            if(dir[spel]>0){ s1= left; s2=h*2-left;}
            else           { s2= left; s1=h*2-left;}
          }
        }
        Score[ip1]+= s1;
        Score[ip2]+= s2;
      }
    }

    if(Extensive &&!P_null)
    {
      printf("%2d%3d ",held, beer);
      for(k=1; k<=P; ++k)
      {
        printf("%7.2f", percent(Score[k],top,rr[k]));
      }
      printf("\n");
    }

    for(k=1; k<=P; ++k) scores[ind++]=percent(Score[k],top,rr[k]);
    for(k=1; k<=P; ++k)
    {
      if(k!=held && k!=beer)scoresw[indw++]=percent(Score[k],top,rr[k]);
    }
  }
  // scoresw contains all the scores of the weak pairs
  avg=0.0;
  nv=indw;
  for(k=0; k<indw; ++k)avg+= scoresw[k];
  avg= avg/nv;
  var=0.0;
  for(k=0; k<indw; ++k)var+= (scoresw[k]-avg) * (scoresw[k]-avg);
  var= var/nv;

  Nn=0;
  for(i=0;i<nscore;i++)
  {
    for(j=0; j<Nn; j++)
      if(fabs(scores[i] - scr[j]) < 0.002)
        { ++Nscr[j]; goto next;}
    scr[Nn]=scores[i];
    Nscr[Nn]=1;
    ++Nn;
    next: ;
  }
  sortscore(Nn, scr, Nscr);
  if P_out_mini for(j=0; j<Nn; j++)
    printf("%4d Scores %7.2f\n",Nscr[j],scr[j]);

  for(k=0;k<=50;k++)histo[k]=0;
  for(i=0;i<nscore;i++)
  {
    j=(int)(scores[i]/2);
    if(j<=50)++histo[j];
  }
  if (P_out_mini || P_summary)
  {
    if(var == 0)printf("%s: sdw = 0\n", inputfile);
    else printf("%s: sdw = %6.02f\n", inputfile, sqrt(var));
  }
  if(Histo && !P_null)
  {
    s1=0;
    for(k=0;k<=50;k++) { if (histo[k]>s1) s1=histo[k]; }
    if(s1>72) s2=36/s1; else s2=0.5;
    for(k=0;k<=50;k++)
    {
      printf("\n%3d%4d",k*2,histo[k]);
      if(histo[k]>0)
      {
        for(i=0;i<histo[k]*s2;++i)printf(" ");
        printf("+");
      }
    }
    printf("\n");
  }
  if(Pp)
  {
    FILE *fhist;

    fhist= fopen("hist.txt","w");
    if(!fhist) printf("failed to open file hist.txt");
    else
    {
      for(k=0;k<=50;k++) fprintf(fhist,"%3d ",k*2);
      fprintf(fhist,"\n");
      for(k=0;k<=50;k++) fprintf(fhist,"%3d ",histo[k]);
      fprintf(fhist,"\n");
      for(k=0;k<=50;k++)fprintf(fhist,"\n%3d %3d",k*2,histo[k]);
      fprintf(fhist,"\n");
      fclose(fhist);
    }

  }
  return 0;
}

static void sorteer(int b, struct score *N)
{
  int tmpn, i, j, nswap, tmps;

  nswap=1;
  while(nswap)
  {
    nswap=0;
    for(j=1; j<b; ++j)
      for(i=0; i<j;  ++i)
        if(N[j].s < N[i].s)
        {
          tmps=N[j].s; N[j].s=N[i].s; N[i].s=tmps;
          tmpn=N[j].p; N[j].p=N[i].p; N[i].p=tmpn;
          ++nswap;
        }
  }
}

static void MP(int b, struct score *N)
{
  int i, j,j0;

  j0=0;

  for(j=0; j<=b; ++j)
  {
    if(j==b || (j>0 && N[j].s !=N[j-1].s) )
    {
      for(i=j0; i<j; ++i)N[i].MP=j+j0-1;
      j0=j;
    }
  }
#ifdef DEBUG
  printf("Pair ");
  for(j=0; j<b; ++j)printf("%5d", N[j].p);
  printf("\n");
  printf("score");
  for(j=0; j<b; ++j)printf("%5d", N[j].s);
  printf("\n");
  printf("MP s ");
  for(j=0; j<b; ++j)printf("%5d", N[j].MP);
  printf("\n");
  printf("\n");
#endif
}

static void SetRandomSpel(int b)
{
  int j;
  for(j=0; j<b; ++j)
  {
    if(genrand()&1)RandomSpel[j]=0;
    else RandomSpel[j]=1;
  }
}

// Bussemaker model
static int makebus(mov_info_t *pmv, int wg[], int A, int Verbose)
{
  SPUL *movement = pmv->movement;
  int P = pmv->P1, b = pmv->b, G = pmv->G;
  int k,ip,ip2, s, q, bA=0;
  int spel,nr[b], n, pairnr[P+1], pp[P+1], stat[P][P], plek[A+1], k1;
  int mins[P+1], maxs[P+1], bord[G];
  double sum=0, scores[P+1], tmpscores[P+1], diag, fs;
  double sumscore[P+1], var[P+1], NN, tmp, sumvar;

  struct score NZ[b][P/2], OW[b][P/2];

  for(k=1; k<=P; ++k)pairnr[k]=k;
  for(k=1; k<=P; ++k)sumscore[k]=0;
  for(k=1; k<=P; ++k)var[k]=0;
  for(k=1; k<=P; ++k)mins[k]=10000;
  for(k=1; k<=P; ++k)maxs[k]= -1000;
  for(k=0; k<P; ++k) for(q=0; q<P; ++q)stat[k][q]=0;
  NN=0;
  if(A) { bA=P/A; } else { bA=P; }

  for(q=0; q<Samples; ++q)
  {
    if(q!=0)
    { /* randomize pair numbers */
      if(A)
      {
        for(k=0; k<A; ++k) shuffle(&pairnr[k*bA+1],bA);
      }
      else
      {
        shuffle(&pairnr[1],P);
      }
      if(Random50)SetRandomSpel(b);
    }


    for(k=1; k<=P; ++k)scores[k]=0;
    for(k=0; k<b; ++k)nr[k]=0;
    for(k=0; k<b; ++k)
      for(n=0; n<P/2; ++n)
    {
      NZ[k][n].p=0; NZ[k][n].s=0; NZ[k][n].MP=0;
      OW[k][n].p=0; OW[k][n].s=0; OW[k][n].MP=0;
    }

    for(k=0; k<G; ++k)
    {
      ip=movement[k].noordzuid[0];
      if(ip)
      {
        ip2=movement[k].noordzuid[1];
        spel=orderb(pmv, movement[k].bord);
        bord[spel]=movement[k].bord;
        n=nr[spel];
        ++nr[spel];
        NZ[spel][n].p=pairnr[ip];
        OW[spel][n].p=pairnr[ip2];
        s=(wg[pairnr[ip]]-wg[pairnr[ip2]]);
        if(Random50 && RandomSpel[spel]) s=0;
        NZ[spel][n].s= -s*100;
        OW[spel][n].s=  s*100;
      }
    }

    for(spel=0; spel<b; ++spel)
    {
      sorteer(nr[spel],NZ[spel]);
      sorteer(nr[spel],OW[spel]);
      MP(nr[spel],NZ[spel]);
      MP(nr[spel],OW[spel]);
      for(n=0; n<nr[spel]; ++n)
      {
        ip=NZ[spel][n].p;
        s=NZ[spel][n].MP;
        scores[ip]+=s;
        ip=OW[spel][n].p;
        s=OW[spel][n].MP;
        scores[ip]+=s;
      }
    }
    sum=0;
    for(k=1; k<=P; ++k) sum+= scores[k];
    for(k=1; k<=P; ++k)tmpscores[k]=scores[k];
    for(k=1; k<=P; ++k)pp[k]=k;
    sortscore(P, &tmpscores[1], &pp[1]);
    if(!A) for(k=1; k<=P; ++k) stat[k-1][pp[k]-1]++;
    else
    {
      for(k1=0;k1<A;++k1)plek[k1]=0;
      for(k=1; k<=P; ++k)
      {
        k1=(pp[k]-1)/bA;
        stat[plek[k1]][pp[k]-1]++;
        plek[k1]++;
      }
    }
    if(q<VBus && P_misc)
    {
      printf("\nordering:");
      for(k=1; k<=P; ++k)printf("%3d",pairnr[k]);
      printf("\nstrength:");
      for(k=1; k<=P; ++k)printf("%3d",wg[pairnr[k]]);

      if(SBus)
      for(spel=0; spel<b; ++spel)
      {
        if(pmv->Getallen)
          printf("\nScores for board set %d\n",bord[spel]);
        else
          printf("\nScores for board set %c\n",bord[spel]);
        printf("Pair   score    MP\n");
        for(n=0; n<nr[spel]; ++n)
        {
          printf("%4d%8d%6d\n",NZ[spel][n].p,
          NZ[spel][n].s, NZ[spel][n].MP);
        }
        for(n=0; n<nr[spel]; ++n)
        {
          printf("%4d%8d%6d\n",OW[spel][n].p,
          OW[spel][n].s, OW[spel][n].MP);
        }
      }

      printf("\nPair str. MP percent\n");
      for(k=1; k<=P; ++k)
      {
        printf("%3d %3d%5.0f%8.2f\n",
                pp[k], wg[pp[k]], tmpscores[k], tmpscores[k]*100*P/(2*sum));
      }
      printf("=================================================\n");
    }
    for(k=1; k<=P; ++k)
    {
      s=(int)scores[k];
      if(s<mins[k])mins[k]=s;
      if(s>maxs[k])maxs[k]=s;
      tmp=scores[k]*100*P/(2*sum);
      sumscore[k] += tmp;
      var[k] += tmp*tmp;
    }
    NN+=1;
  }

  if(Samples>1)
  {
    sumvar=0;
    if P_out_mat printf("\nPair Str. average s.d.   av/3  sd/3     min.  max\n");
    for(n=1; n<=P; ++n)
    {
      var[n] -= sumscore[n]*sumscore[n]/NN;
      if(var[n]<0)var[n]=0;
      sumvar+=var[n];
      var[n]= sqrt(var[n]/NN);
      sumscore[n] = sumscore[n]/NN;

      if P_out_mat printf("%3d %4d %6.1f %5.1f  %6.1f %5.1f   %6.1f %5.1f\n", n,
      wg[n],sumscore[n], var[n], (sumscore[n]-50)/3+50, var[n]/3,
      ((double)(mins[n]*100*(P/2))/sum),
      ((double)(maxs[n]*100*(P/2))/sum));
    }
    if P_out_mat printf("\n"); // The blank line should be part of table, not summary
    sumvar=sqrt(sumvar/(NN*P));
    if P_summary printf("        mean   %6.1f        %6.1f     Qb: %6.2f\n",
    sumvar, sumvar/3, 100.0/(1 + sumvar/3));
    diag=0;
    for(n=0; n<P; n+=32)
    {
      ip=n+32;
      if(ip>P)ip=P;
      if P_out_mat
      {
        printf("\n\nPair");
        for (k=1+n; k<=ip; ++k)printf("%3d",k);
        printf("\nPos\n");
      }
      for(q=0; q<bA; ++q)
      {
        if P_out_mat printf("%2d  ",q+1);
        for(k=n;k<ip;++k)
        if(stat[q][k]>0)
        {
          fs = 100.0*stat[q][k]/Samples;
          if P_out_mat
          {
            if(fs>=0.5)printf("%3d",(int)(fs+0.5));
            else printf("  .");
          }
          if(A>0)
          {
            if( q/A == (k%(P/A))) diag+= fs;
          }
          else
          {
            if(q==k)diag+= fs;
          }
        }
        else if P_out_mat printf("   ");
        if P_out_mat printf("\n");
      }
    }
    if P_summary printf("Qd %.1f\n",diag/P);
  }
  return 0;
}

int main(int argc, char *argv[])
{
  char inputfile[PATH_MAX];

  int i, ier, bad=0, Div;

  int n,ind;
  int argV;
  char dummy=0;

  mov_info_t mvi;   // Movement info structure
  int h;
  double Qo;

// echo(argc, argv); // For development/testing only

  int Verbose = V_regular | V_summary; // summary is Qb/Qd-lines
  mvi.movement = NULL; // Because we may goto error before calling read_movement()
  Top=100;
  Histo=0;
  Pp=0;
  Extensive=0;
  Een=0;
  Bus=0;
  WBus=0;
  VBus=10;
  SBus=0;
  Samples=MANY;
  Div=0;
  optind=0;
  strcpy(inputfile,"_balans.txt");
  while ((n=getopt(argc, argv, ":QV:T:dex1bwv:D:Ss:h")) != -1)
  {
    switch(n)
    {
      case ':': // required argument missing
        fprintf(stderr,"Option %c requires an argument\n",optopt);
        goto error;
      case 'Q':
        Verbose= V_quiet;
        break;
      case 'V':
// V followed by some optarg
        argV= -1;
        ier=sscanf(optarg,"%i%c",&argV, &dummy);
// %i conversion: integer in base 16 if it begins with 0x or 0X,
// in base 8 if it begins with 0 and in base 10 otherwise.

        if(ier != 1 || argV < 0) // negative or absent number or garbage
        {
          fprintf(stderr,"Option V must be followed by a nonnegative number\n");
          goto error;
        }
        Verbose=argV;
        argV = argV & 0xfffffff8L;        //argV without 3 last bits
        Verbose = Verbose & 7;            //3 last bits of argV
        if(P_quiet) argV|= V_quiet;       //convert to bits of Verbose
        if(P_mini) argV|= V_mini;
        if(P_regular) argV|= V_regular;
        if(P_all) argV|= V_all;
        Verbose= argV;
        break;
      case 'T':
        ier=sscanf(optarg,"%d",&Top);
        if(ier != 1)
        {
          fprintf(stderr,"option T requires a numerical argument\n");
          goto error;
        }
        if P_misc printf("# score per board of the 2 top pairs = %d per cent\n",Top);
        if(Top>100 || Top<0)
        {
          fprintf(stderr,"??value must be 0..100\n");
          goto error;
        }
        break;
      case 'd':
        Histo=1;
        break;
      case 'e':
        Extensive=1;
        break;
      case '1':
        Een=1;
        break;
      case 'b':
        Bus=1;
        break;
      case 'w':
        WBus=1;
        break;
      case 'S':
        SBus=1;
        break;
      case 'v':
        ier=sscanf(optarg,"%d",&VBus);
        if(ier != 1)
        {
          fprintf(stderr,"option v requires a numerical argument\n");
          goto error;
        }
        if P_misc printf("# %d cases with detailed output in Bussemaker model\n",VBus);
        break;
      case 'D':
        ier=sscanf(optarg,"%d",&Div);
        if(ier != 1)
        {
          fprintf(stderr,"option D requires a numerical argument\n");
          goto error;
        }
        fprintf(stderr,"Bussemaker model with %d divisions\n",Div);
        break;
      case 's':
        ier=sscanf(optarg,"%d",&Samples);
        if(ier != 1)
        {
          fprintf(stderr,"option s requires a numerical argument\n");
          goto error;
        }
        if P_misc printf("# Bussemaker model with %d samples\n",Samples);
        break;
      // case 'g':
        // Getallen=1; // Completely useless, readmove() overrode that previously global variable!
        // break;
      case 'x':
        Pp=1;
        break;
      case 'h':
        printf("call : %s [OPTION]... [MOVEMENTFILE]\nVersion %s\n\n",argv[0], szVersion);
        printf("With no MOVEMENTFILE given, _balans.txt is read as input movement.\n");
        printf("When MOVEMENTFILE is -, read input movement from standard input.\n\n");
        printf("options:\n");
        printf(" -Q : quiet mode");
        printf(" -V n: Verbosity control, for options see balans -h and help file\n");
        printf(" -T n: specify score of the 2 pairs (in percent)\n");
        printf(" -e : extensive survey of all possible scores\n");
        printf(" -d : print Distribution of scores\n");
        printf(" -x : export some results\n");
        printf(" -1 : only 1 strong pair, other options above ignored\n");
        printf(" -b : Bussemaker model\n");
        printf("    -s n: Bussemaker: n=number of samples (default 40000)\n");
        printf("    -w :  Weighted Bussemaker, give weights on command line\n");
        printf("    -D n: Bussemaker model with n divisions\n");
        printf("    -S:   Bussemaker: detailed output includes scores per board\n");
        printf("    -v n: Bussemaker: n=number of cases with detailed output (default 10)\n");
        // printf(" -g : input board sets as numbers\n"); // Undocumented and broken, readmove overrides!
        printf(" -h : show this help, then quit\n");
        printf("\nsee: http://www.pjms.nl/BALANS/README.html\n");
        printf("and: http://www.pjms.nl/BALANS/README_en.html\n");
        return 0;
      default:
        bad++;
        break;
    }
  }
  if(bad){ fprintf(stderr,"bad options ... quitting\n"); goto error; }
  for(n=1;n<=optind;n++) ++argv;

  ind=optind;
  if(ind>=argc)
  {
   if P_misc printf("No input file given, using %s\n", inputfile);
  }
  else if(ind < argc-1 && !WBus) // Extra args only allowed if -b (Bussemaker weights)
  {
    fprintf(stderr,"Error: only one input file allowed\n");
    goto error;
  }
  else // Exactly 1 input file given (possibly '-'), possibly with Bussemaker weights after that
  {
    strcpy(inputfile,*argv);
    ++ind; ++argv;
  }

  raninit();

  if(read_movement(&mvi, inputfile) < 0) goto error;
  if((h = geth(&mvi, P_warn)) < 1) goto error;
  if(mvi.b != mvi.Nb)
  {
    fprintf(stderr,"wrong value of nr of boards in header: %d, in fact %d\n", mvi.b, mvi.Nb);
    goto error;
  }

  if(inspect(&mvi, P_inp_mat, P_warn, &Qo) < 0) goto error;
  if  P_inp_move
  {
    printf("\nmovement scheme:\n");
    print_movement(&mvi,stdout,0,0,0);
  }
  if(Een)
  {
    makescores(&mvi,1,h,inputfile,Verbose);
    goto exit;
  }
  if(Bus)
  {
    int P = mvi.P1;
    int wt[P+1];
    for(i=1;i<=P;i++)wt[i]=i;
    if(WBus)
    {
      for(i=1;i<=P;i++)
      {
        if((ind>=argc) || (sscanf(*argv, "%d", &wt[i]))<=0)
        {
          fprintf(stderr,"not enough weights\n");
          goto error;
        }
        ++argv; ++ind;
      }
    }
    makebus(&mvi,wt,Div,Verbose);
    goto exit;
  }
  Tops= ((double)(2*h*Top))/100.0;
  Subtop=Tops - (Tops-h)/h;
  if P_misc printf("The 2 pairs score\n %5.1f%% if they play in opposite direction,\n %5.1f%% if they play in the same direction\n",100*Tops/(2*h), 100*Subtop/(2*h));
  if P_misc printf("  50.0%% if they play against each other\n");
  makescores(&mvi,2,h,inputfile,Verbose);
exit:
  if(mvi.Status & Unsuitable)
  {
    fprintf(stderr,"--- Not all boards are played the same number of times!\n");
    fprintf(stderr,"--- Program not suitable for this movement\n");
    fprintf(stderr,"--- RESULTS INVALID!!! ---\n");
  }
  free_movement(&mvi);
  return 0;
error:
  free_movement(&mvi);
  return 2;
}
