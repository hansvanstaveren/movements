/* Copyright (C) 2006-2020 Peter Smulders
 * Copyright (C) 2020-2021 Ulrik Dickow
 * The GNU General Public License (GPL-3.0-or-later)
 * applies to this software, see COPYING.txt
 */

#include "common.h"
#include "sorttables.h"
#ifdef XGETOPT
#include "XGetopt.h"
extern char *optarg;      //XGetopt globals defined in XGetopt.c
extern int  optind, opterr, optopt;
#endif

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

int main(int argc, char *argv[])
{
  char inputfile[PATH_MAX];
  char outputfile[PATH_MAX];
  char fixerfile[PATH_MAX];
  FILE *foutputfile=NULL;
  FILE *ffixerfile=NULL;

  int UBP=0;         // renumber to Universal Begin Position (option -u)
  int Letters=0;     // translate board group numbers to letters (option -L)
  int Permutation=0; // renumber pairs, give permutation on command line (option -p)
  int Mb=0;          // Use numbers for board groups in the output but specify sequence (option -n)
  int Lowestboard=0; // Lowest board number for the output of option -n (arg to -n)
  int i, ier, bad=0;
  int grnum;
  int randomize;
  int stat, len, ipt, ianswer, term;

  int ASCII=0;
  int Base=0;
  int Offodd=0;
  int Offeven=0;
  int K=0, ntables=0;

  mov_info_t mvi; // Movement info structure.  The following 7 vars are just copies to make the code shorter.
  SPUL *movement;
  int P1, r, b, t1, G, Vacant=0; // Vacant also used to read command line arg before reading movement

  mvi.movement = NULL; // Because we may goto error before calling read_movement()
  mvi.Status = 0;      // Ditto
  mvi.b = -1; // to check if b is used unitialized

  int P,t,h,S;
  int rfix[MAXFIX], tfix[MAXFIX], ifix[MAXFIX], nrfix, ntfix, nifix, njfix, nfirst;
  int rrel[MAXFIX], trel[MAXFIX], irel[MAXFIX], nrrel, ntrel, nirel, njrel, fixi;
  unsigned int jfix[MAXFIX], jrel[MAXFIX];
  double Qo;

  optind=0;
  int n,ind;
  int argV;
  char dummy=0;
  int retval=0;                // return value

  int Arrow;
  int Verbose=V_regular;
  grnum=0; Arrow=0;
  Base=0; Offodd=0; Offeven=0;
  randomize=0;
  int Change=0;                  // any option that may change the movement sets Change
  outputfile[0]='\0';
  inputfile[0]='\0';
  fixerfile[0]='\0';
  nrfix=0; ntfix=0; nifix=0; njfix=0; nfirst=0;
  nrrel=0; ntrel=0; nirel=0; njrel=0;
  unsigned char boardgroup;

// echo(argc, argv); // For development/testing only

  Verbose= V_regular;
// process commandline
  while ((n=getopt(argc, argv,
    ":AD:I:J:K:LNPQR:TV:Wa:b:ce:f:hi:j:km:n:o:pr:st:uv:wz"
    )) != -1)
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

        if(ier != 1 || argV < 0 ) // negative or absent number or garbage
        {
          fprintf(stderr,"Option V must be followed by a nonnegative number\n");
          goto error;
        }
        Verbose=argV;
        argV = argV & 0xfffffff8L;        //argV without 3 last bits
        Verbose = Verbose & 7;            //3 last bits of argV
        if(P_quiet) argV|= V_quiet;       //convert to bits of Verbose
        if(P_mini) argV|= V_out_move|V_quiet;
        if(P_regular) argV|= V_regular;
        if(P_all) argV|= V_all;
        Verbose= argV;
        break;
      case 'f':
        ier=sscanf(optarg,"%d",&nfirst);
        if(ier != 1)
        {
          fprintf(stderr,"option f missing nr. of tables\n");
          goto error;
        }
        if P_misc printf("# fix first %d positions\n",nfirst);
        break;
      case 'r':
        ier=sscanf(optarg,"%d",&rfix[nrfix]);
        if(ier != 1)
        {
          fprintf(stderr,"option r missing round number\n");
          goto error;
        }
        if P_misc printf("# option fix round %d\n",rfix[nrfix]);
        nrfix++;
        break;
      case 't':
        ier=sscanf(optarg,"%d",&tfix[ntfix]);
        if(ier != 1)
        {
          fprintf(stderr,"option t missing table number\n");
          goto error;
        }
        if P_misc printf("# option fix table %d\n",tfix[ntfix]);
        ntfix++;
        break;
      case 'i':
        ier=sscanf(optarg,"%d",&ifix[nifix]);
        if(ier != 1)
        {
          fprintf(stderr,"option i missing pair number\n");
          goto error;
        }
        if P_misc printf("# option fix pair %d\n",ifix[nifix]);
        nifix++;
        break;
      case 'j':
        ier=sscanf(optarg,"%d",&jfix[njfix]);
        if(ier != 1)
        {
// board group not a number, should be capital letter
          ier=sscanf(optarg,"%c",&boardgroup);
          if(ier == 1)
          {
            if( ! (boardgroup >= 'A' && boardgroup <= 'Z'))
            {
              fprintf(stderr,"option j, board group should be number or capital letter!\n");
              goto error;
            }
            jfix[njfix]= boardgroup-'A'+1;
          }
          else
          {
            fprintf(stderr,"option j missing board group\n");
            goto error;
          }
        }
        if P_misc printf("# option fix board group %d\n",jfix[njfix]);
        njfix++;
        break;
      case 'R':
        ier=sscanf(optarg,"%d",&rrel[nrrel]);
        if(ier != 1)
        {
          fprintf(stderr,"option R missing round number\n");
          goto error;
        }
        if P_misc printf("# option switch round %d\n",rrel[nrrel]);
        nrrel++;
        Change=1;
        break;
      case 'D':
        ier=sscanf(optarg,"%d",&trel[ntrel]);
        if(ier != 1)
        {
          fprintf(stderr,"option D missing table number\n");
          goto error;
        }
        if P_misc printf("# option switch table %d\n",trel[ntrel]);
        ntrel++;
        Change=1;
        break;
      case 'I':
        ier=sscanf(optarg,"%d",&irel[nirel]);
        if(ier != 1)
        {
          fprintf(stderr,"option I missing pair number\n");
          goto error;
        }
        if P_misc printf("# option switch pair %d\n",irel[nirel]);
        nirel++;
        Change=1;
        break;
      case 'J':
        ier=sscanf(optarg,"%d",&jrel[njrel]);
        if(ier != 1)
        {
// board group not a number, should be capital letter
          ier=sscanf(optarg,"%c",&boardgroup);
          if(ier == 1)
          {
            if( ! (boardgroup >= 'A' && boardgroup <= 'Z'))
            {
              fprintf(stderr,"option J, board group should be number or capital letter!\n");
              goto error;
            }
            jrel[njrel]= boardgroup-'A'+1;
          }
          else
          {
            fprintf(stderr,"option J missing board group\n");
            goto error;
          }
        }
        if P_misc printf("# option switch board %d\n",jrel[njrel]);
        njrel++;
        Change=1;
        break;
      case 'A':
        ASCII=1;
        grnum=1;
        Change=1;
        break;
      case 'L':
        Letters=1;
        Change=1;
        break;
      case 'u':
        UBP=1;
        Change=1;
        break;
      case 'N':
        grnum=1;
        Change=1;
        break;
      case 'z':
        randomize=1;
        Change=1;
        break;
      case 'c':
// option c ignored
        break;
      case 'v':
        if(Vacant)
        {
          fprintf(stderr,"option v may only be given once\n");
          goto error;
        }
        ier=sscanf(optarg,"%d",&Vacant);
        if(ier != 1)
        {
          fprintf(stderr,"option v missing pair number\n");
          goto error;
        }
        Change=1;
        break;
      case 'n':
        grnum=1;
        Mb=1;
        ier=sscanf(optarg,"%d",&Lowestboard);
        if(ier!=1)
        {
          fprintf(stderr,"option n requires numerical argument (lowest board number)\n");
          goto error;
        }
        Change=1;
        break;
      case 'p':
        Permutation=1;
        Change=1;
        break;
      case 'w':
        Arrow=1;
        Change=1;
        break;
      case 'K':
        K=1;
        ier=sscanf(optarg,"%d",&ntables);
        if P_misc printf("# ntables= %d\n", ntables);
        if(ier!=1)
        {
          fprintf(stderr,"option K requires numerical argument (number of tables to sort)\n");
          goto error;
        }
        Change=1;
        break;
      case 'b':
        ier=sscanf(optarg,"%d",&Base);
        if(ier!=1)
        {
          fprintf(stderr,"option b requires numerical argument (lowest number)\n");
          goto error;
        }
        Change=1;
        break;
      case 'o':
        ier=sscanf(optarg,"%d",&Offodd);
        if(ier!=1)
        {
          fprintf(stderr,"option o requires numerical argument\n");
          goto error;
        }
        Change=1;
        break;
      case 'e':
        ier=sscanf(optarg,"%d",&Offeven);
        if(ier!=1)
        {
          fprintf(stderr,"option e requires numerical argument\n");
          goto error;
        }
        Change=1;
        break;
      case 'a':
        ier=sscanf(optarg,"%s",fixerfile);
        if(ier != 1)
        {
          fprintf(stderr,"option a requires a file name\n");
          goto error;
        }
        if P_misc printf("# Reading fixers from file %s\n", fixerfile);
        Change=1;
        break;
      case 'm':
        strcpy(outputfile,optarg);
        if(strcmp(outputfile, "-")==0) Verbose=0;
        break;
      case 'P': case 'T': case 'W': case 'k': case 's':
        fprintf(stderr,"Option -%c not implemented in vernum\n",n);
        goto error;
      case 'h':
        printf("\nvernum %s by Peter Smulders and Ulrik Dickow.\n", szVersion);
        printf("call : %s [OPTION]... [MOVEMENTFILE]\n\n",argv[0]);
        printf("With no MOVEMENTFILE given, _balans.txt is read as input movement.\n");
        printf("When MOVEMENTFILE is -, read input movement from standard input.\n\n");
        printf("options:\n");
        printf(" -Q : quiet mode, print balance characteristics only\n");
        printf(" -Vn: Verbosity control, for options see balans -h and help file\n");
        printf(" -A : ASC Specification Compliant output\n");
        printf(" -L : output board groups as capital letters\n");
        printf(" -p : renumber pairs, give permutation on command line\n");
        printf(" -u : renumber pairs to Universal Begin Position\n");
        printf(" -n <Lowest board number>: print board groups as numbers, give sequence on command line\n");
        printf(" -N : print board groups as numbers, default sequence\n");
        printf(" -K <n>: sort the first n tables of each round according to boardgroup\n");
        printf(" -m FILENAME: write updated movement to file FILENAME\n");
        printf(" -m -       : write updated movement to standard output\n");
        printf(" -b <base>: print movement as set of base movements\n");
        printf(" -o <offset odd>: add offset to each odd pair number\n");
        printf(" -e <offset even>: add offset to each even pair number\n");
        printf(" -v <pair number> calculate Qf1 with this pair absent.\n");
        printf(" -a fixerfile: supply r * t fixers 0=free, 1=fixed\n");
        printf(" -r n: keep round n fixed\n");
        printf(" -t n: keep table n fixed\n");
        printf(" -i n: keep pair n fixed\n");
        printf(" -j n: keep board n fixed. (n=number or capital letter)\n");
        printf(" -f n: keep first n positions fixed\n");
        printf(" -R n: arrow switch round n\n");
        printf(" -D n: arrow switch table n\n");
        printf(" -I n: arrow switch all seatings of pair n\n");
        printf(" -J n: arrow switch board n. (n=number or capital letter)\n");
        printf(" -w : switch all NS-EW. except tables fixed or already switched by one of the above\n");
        printf(" -z : random switch NS-EW. except tables fixed or already switched by one of the above\n");
        printf(" -h : show this help, then quit\n");
        printf("\nfor detailed help and description of the program see:\n");
        printf("Dutch:   http://www.pjms.nl/BALANS/README.html\n");
        printf("English: http://www.pjms.nl/BALANS/README_en.html\n");
        retval=0;
        goto quit;
      default:
        fprintf(stderr,"invalid option %c\n", optopt);
        bad++;
        break;
    }
  }
  if(bad){ fprintf(stderr,"bad options ... quitting\n"); goto error; }
  for(n=1;n<=optind;n++) ++argv;

// done with commandline options
  if(P_misc)
  {
    print_the_time();
    printf("vernum %s\n", szVersion);
  }
  ind=optind;
  if(ind>=argc)
  {
    strcpy(inputfile,"_balans.txt");
    if P_misc printf("No input file given, using %s\n", inputfile);
  }
  else if(ind < argc-1 && !(Permutation || Mb)) // Extra args only allowed if -p or -n
  {
    fprintf(stderr,"Error: only one input file allowed\n");
    goto error;
  }
  else // Exactly 1 input file given (possibly '-'), possibly followed by pair or board group numbers (-p/-n)
  {
    strcpy(inputfile,*argv);
    ++ind; ++argv;
  }
  if(strlen(outputfile)==0)
  {
    strcpy(outputfile,"_balans.txt");
  }

  raninit();

// open fixerfile
  if(strlen(fixerfile)>0)
  {
    ffixerfile= fopen(fixerfile,"r");
    if(!ffixerfile)
    {
      fprintf(stderr,"file %s not found, or not readable\n",fixerfile);
      goto error;
    }
    else if P_misc printf("fixer file is %s\n",fixerfile);
  }
// read movement
  if(read_movement(&mvi, inputfile) < 0)
    goto error;
  movement = mvi.movement;
  P1 = mvi.P1; r = mvi.r; b = mvi.b; t1 = mvi.t1; G = mvi.G;
  P=P1;
  if (Vacant)
  {
    if(Vacant > P1)Vacant=P1;
    P=P-1;
    if P_misc printf("Pair %d is absent!\n", Vacant);
    mvi.Vacant = Vacant;
  }

  t=P/2;
  QUAL Q, Q1;
  Q.r = Q1.r =r;

// open new level to enable dimensioning arrays properly
  {
    int fix[G];
    int p, V;
    double Qf1av=0, Qf1max=0, Qf1[P1];
    int64_t SSsum;
    int Ssum;

    //if((h = geth(&mvi, P_warn)) < 0) goto error; // Already printed error unconditionally
    h = geth(&mvi, P_warn); // Continue merrily even when h<0 since 7.4.2 did that -- really needed??
    if(b!=mvi.Nb)
    {
      fprintf(stderr,"ERROR! nr of boardgroups in header: %d, in fact %d\n", b, mvi.Nb);
      mvi.Status |= Invaliddat;
      goto error;
    }

    S=r*h*t;
    if P_misc
    {
      printf("%s: %d pairs, %d rounds %d tables, h = %d, S = %d\n",inputfile,P,r,t1,h,S);
    }
    if (P_inp_move || (P_out_move && !Change))
    {
      printf("\n%s\n","initial movement scheme:");
      print_movement(&mvi,stdout,Base,Offodd,Offeven);
    }

// another level, for processing special options
    if(Change)
    {
      int newp[P1];

      for(i=0;i<G;++i)fix[i]=0;
      if(ffixerfile)
      {
        readfixers(ffixerfile,G,fix);
        if(!Arrow && !randomize)fprintf(stderr,"Warning: fixers only have effect on options -w and -z!\n");
      }
      Qf1max=0;
      Qf1av =0;
// process options f, r, t, i, j;
      if(nfirst > G)     { fprintf(stderr,"\n!! -f%d: too big\n",nfirst);  goto error;}
      for(p=0;p<nrfix;++p)
        if(rfix[p] > r)  { fprintf(stderr,"\n!! -r%d: too big\n",rfix[p]); goto error;}
      for(p=0;p<ntfix;++p)
        if(tfix[p] > t1) { fprintf(stderr,"\n!! -t%d: too big\n",tfix[p]); goto error;}
      for(p=0;p<nifix;++p)
        if(ifix[p] > P1) { fprintf(stderr,"\n!! -i%d: too big\n",ifix[p]); goto error;}
      for(p=0;p<njfix;++p)
        if(jfix[p] > (unsigned) b)  { fprintf(stderr,"\n!! -j%d: too big\n",jfix[p]); goto error;}
      for(i=0;i<nfirst;++i)fix[i]=1;
      for(p=0;p<nrfix;++p)for(i=0;i<t1;++i)fix[(rfix[p]-1)*t1+i]=1;
      for(p=0;p<ntfix;++p)for(i=0;i<r;++i)fix[i*t1+tfix[p]-1]=1;
      for(p=0;p<nifix;++p)
      {
        for(i=0;i<G;++i)
          if(movement[i].noordzuid[0] == ifix[p] ||
          movement[i].noordzuid[1] == ifix[p])
            fix[i]=1;
      }
      for(p=0;p<njfix;++p)
        for(i=0;i<G;++i)
          if((mvi.Getallen ? movement[i].bord : movement[i].bord - 'A' + 1) == jfix[p])
            fix[i]=1;

// process options R, D, I, J;
      if( (nrrel || ntrel || nirel || njrel) && Permutation)
      {
        fprintf(stderr, "*** options R,D,I,J incompatible with p ***"); goto error;
      }  
      for(p=0;p<nrrel;++p)
        if(rrel[p] > r)  { fprintf(stderr,"\n!! -R%d: too big\n",rrel[p]); goto error;}
      for(p=0;p<ntrel;++p)
        if(trel[p] > t1) { fprintf(stderr,"\n!! -D%d: too big\n",trel[p]); goto error;}
      for(p=0;p<nirel;++p)
        if(irel[p] > P1) { fprintf(stderr,"\n!! -I%d: too big\n",irel[p]); goto error;}
      for(p=0;p<njrel;++p)
        if(jrel[p] > (unsigned)b)  { fprintf(stderr,"\n!! -J%d: too big\n",jrel[p]); goto error;}
      if(nrrel)
      {
        for(i=0;i<G;++i)
        {
          fixi=0;
          for(p=0;p<nrrel;++p)
            if( i/t1 == rrel[p]-1) fixi=1;
          if(fixi && !fix[i])fix[i]=2;      //fix[i]=2: candidate for arrow switch
        }
      }
      if(ntrel)
      {
        for(i=0;i<G;++i)
        {
          fixi=0;
          for(p=0;p<ntrel;++p)
            if( i%t1 == trel[p]-1) fixi=1;
          if(fixi && !fix[i])fix[i]=2;
        }
      }
      if(nirel)
      {
        for(i=0;i<G;++i)
        {
          fixi=0;
          for(p=0;p<nirel;++p)
            if(movement[i].noordzuid[0]==irel[p] ||
            movement[i].noordzuid[1]==irel[p])
              fixi=1;
          if(fixi && !fix[i])fix[i]=2;
        }
      }
      if(njrel)
      {
        for(i=0;i<G;++i)
        {
          fixi=0;
          for(p=0;p<njrel;++p)
            if((mvi.Getallen ? movement[i].bord : movement[i].bord - 'A' + 1) == jrel[p])
              fixi=1;

          if(fixi && !fix[i])fix[i]=2;
        }
      }

      for(i=0;i<G;++i)
        if(movement[i].noordzuid[0]==0 ||
        movement[i].noordzuid[0]==Vacant ||
        movement[i].noordzuid[1]==Vacant)
          fix[i]=1;
      if(Permutation)
        for (i=0; i<P1; ++i)
      {
        if((ind>=argc) ||
          (sscanf(*argv, "%d", &newp[i]))<=0)
        {
          fprintf(stderr,"not enough permuted pair numbers\n");
          goto error;
        }
        ++argv; ++ind;
      }
      if(Mb)
      {
        if P_misc printf("old: ");
        for (i=0; i<b; ++i)
        {
          if(ind>=argc)
          {
            fprintf(stderr,"not enough board groups\n");
            goto error;
          }
          len=(int)strlen(*argv);
          if(len < 0) len=9999999; // Guard against strlen>INT_MAX, silence 'clang -Weverything'
          ipt=0;
          ianswer=0;
          stat= intgetb(*argv, &len, &ipt, &ianswer, &term);
          if(mvi.Getallen && stat==0) set_letter(&mvi, i, (unsigned int)ianswer);
          else if(stat&alpha) set_letter(&mvi, i, (unsigned int)term);
          else
          {
            fprintf(stderr,"Getallen=%d ianswer=%d term=%d\n", mvi.Getallen, ianswer,term);
            fprintf(stderr,"ERROR: illegal boardgroup %s\n", *argv);
            goto error;
          }
          ++argv; ++ind;
          if P_misc printf((mvi.Getallen ? " %2d" : "  %c"), Letter(&mvi, i));
        }
        if P_misc
        {
          printf("\n");
          printf("new: ");
          for (i=0; i<b; ++i)
          {
            printf(" %2d",i+Lowestboard);
          }
          printf("\n");
        }
        mvi.Getallen=1;
      }
      else if(grnum)
      {
        Lowestboard=1; Mb=1; mvi.Getallen=1;
        sortboardgroups(&mvi);
        if(Letters)
        {
          Letters=0;
          if(!ASCII)
          {
            fprintf(stderr,"\nWarning! option -L ignored: should not be used in combination with -N or -n\n");
          }
        }
        else
          ASCII=0;
      }

      if(Arrow)
      {
        if (Letters || Mb || Permutation)
        {
          fprintf(stderr,"\nError! option -w should not be used in combination with -L -n or -p\n");
          goto error;
        }
        for(i=0;i<G;i++)
        {
          if(! fix[i]) fix[i]=2;
        }
      }

      if(randomize)
      {
        if (Arrow)
          fprintf(stderr,"\nWarning! option -z has no effect in combination with -w\n");
        for(i=0;i<G;i++)
        {
          if((! fix[i]) && (genrand()&1)) fix[i]=2;
        }
      }

      if (P_out_swit && (ffixerfile || nfirst || nrfix || ntfix || nifix
        || njfix || nrrel || ntrel || nirel || njrel || Vacant || Arrow))
      {
        printf("\nFixed tables, indicated by 1\n");
        printf("Switched tables, indicated by 2\n");
        for(i=0;i<G;++i)
        {
          printf("%2d",fix[i]);
          if((i%t1)==(t1-1))printf("\n");
        }
        printf("\n");
      }

      renumber(&mvi, newp, fix, Letters, Permutation, Mb, Lowestboard);

      if(ASCII && (b<=26))
      {
        mvi.Getallen=0;
        for(i=0;i<G;i++)
        {
          if(movement[i].noordzuid[0])
            movement[i].bord+= 'A'-1;
          else
            movement[i].bord='0';
        }
      }

      if(UBP)
      {
        if(P1 > t1*2+1)
        {
          fprintf(stderr,"Warning! Universal Begin Position may not work for this movement: too many pairs\n");
        }
        unify(&mvi);
        Vacant = mvi.Vacant; // unify() may have changed the vacant pair number
      }
      if(K)
      {
        sorttables(movement,ntables,r,t1);
      }
      if(Letters){ mvi.Getallen=0; }
      if P_out_move
      {
        printf("\n%s\n","renumbered movement scheme:");
        print_movement(&mvi,stdout,Base,Offodd,Offeven);
      }
    }
    if(inspect(&mvi, P_out_mat|P_inp_mat, P_warn, &Qo) < 0) goto error;
    balance(P_out_mat|P_inp_mat,movement,P1,G,h,Vacant,&Q);
    if(!Vacant && h>1)
    {
      if P_Qf1 printf("pair     Qf1\n");
      SSsum=0;
      Ssum=0;
      for(V=1;V<=P1;V++)
      {
        balance(0,movement,P1,G,h-1,V,&Q1);
        if P_Qf1 printf("%4d %7.2f\n",V,Q1.Qf);
        SSsum += Q1.SS;
        Ssum += Q1.S;
        if(Q1.Qf>Qf1max)Qf1max=Q1.Qf;
        Qf1[V-1]=Q1.Qf;
      }
      Q.Qf1av= Qf1av= Qf(SSsum, Ssum, P1*Q1.N);
      Q.Qf1max= Qf1max;
      if P_misc
      {
        printf("Qf1av=%7.3f   Qf1max=%7.2f, for pair(s)", Qf1av, Qf1max);
        for(V=1;V<=P1;V++)
        {
          if(fabs(Qf1[V-1] - Qf1max) < eps) printf(" %d",V);
        }
        printf("\n");
      }
    }
    if(Vacant && P_misc)
    {
      printf("balance characteristics pair %d ABSENT:\n", Vacant);
    }
    if P_misc printf("sum of squares:   %6.1f\n",Q.var);
    if P_misc printf("sum of 4th powers:%6.1f\n",Q.s4);
    if P_misc printf("%24s sd= %.4g, d4= %.4g, Qc=%.4g, Qf=%.4g, Qo=%.4g\n",
        inputfile, Q.sd, Q.d4, Q.Qc, Q.Qf, Qo);
    if(P_summary)
    { printf("Qf=%6.2f, sd=%6.2f, d4=%6.2f, Qc=%6.2f, Qo=%6.2f, P=%d, S=%d, N=%d, Qf1av=%6.2f, Qf1max=%6.2f\n",
        Q.Qf, Q.sd, Q.d4, Q.Qc, Qo, Q.P, Q.S, Q.N, Q.Qf1av, Q.Qf1max);
    }
    if(strlen(outputfile)>0)
    {
      if(strcmp(outputfile,"-") == 0) foutputfile=stdout;
      else foutputfile= fopen(outputfile,"w");
      if(!foutputfile)
      {
        fprintf(stderr,"failed to open file %s for output\n",outputfile);
        goto error;
      }
      else
      {
        if P_misc printf("input file is %s, output file is %s\n",
            inputfile,outputfile);
        print_movement(&mvi,foutputfile,Base,Offodd,Offeven);
        if(Vacant)
        {
          fprintf(foutputfile,"\n#when pair %d absent:",Vacant);
          fprintf(foutputfile," Qf1=%.4g, Qo1=%.4g else",Q.Qf, Qo);
        }
      }
    }
    else if P_misc { printf("input file %s\n",inputfile); }
    if (Vacant)
    {
      Vacant=mvi.Vacant=0;
      h = geth(&mvi, P_warn); // Continue merrily even when h<0 since 7.4.2 did that -- really needed??
      if(inspect(&mvi, 0, 0, &Qo) < 0) goto error;
      balance(0,movement,P1,G,h,Vacant,&Q);
    }
    if(foutputfile)
    {
      fprintf(foutputfile,"\n#Qf=%.4g   Qo=%.4g\n",Q.Qf, Qo);
      if(Qf1av>0 && h>1)
      {
        fprintf(foutputfile, "#Qf1av=%.4g Qf1max=%.4g, for pair(s)", Qf1av, Qf1max);
        for(V=1;V<=P1;V++)
        {
          if(fabs(Qf1[V-1] - Qf1max) < eps) fprintf(foutputfile, " %d",V);
        }
        fprintf(foutputfile, "\n");
      }
    }
  }
  if(mvi.Status & Unsuitable)
  {
    fprintf(stderr,"--- %s: Program not suitable for this movement ---\n",inputfile);
    fprintf(stderr,"--- Not all boards are played the same number of times! ---\n");
    fprintf(stderr,"--- %s: BALANCE CHARACTERISTICS INVALID!!! ---\n",inputfile);
    retval=1;
  }
  if(mvi.Status & Dubbelpair)
  {
    fprintf(stderr,"*** %s: pairs playing twice in same round ***\n", inputfile);
    goto quit;
  }
  if((mvi.Status & Dubbelopp) && P_warn)
  {
    fprintf(stderr,"--- %s: pairs meeting twice ---\n", inputfile);
  }
  if(mvi.Status<Dubbelbord)goto quit;
error:
  retval= 2;
  if(mvi.Status & Invaliddat)
  {
    fprintf(stderr,"*** %s: invalid movement ***\n", inputfile);
    goto quit;
  }
  if(mvi.Status & Illegalmov)
  {
    fprintf(stderr,"*** %s: illegal movement ***\n", inputfile);
    goto quit;
  }
  if(mvi.Status & Dubbelbord)
  {
    fprintf(stderr,"*** %s: pairs playing same board twice ***\n", inputfile);
    goto quit;
  }
  fprintf(stderr,"*** %s: error(s) encountered ***\n", inputfile);
quit:
  free_movement(&mvi);
  return retval;
}
