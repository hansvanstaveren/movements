/* Copyright (C) 2006-2022 Peter Smulders
 * Copyright (C) 2018-2023 Ulrik Dickow
 * The GNU General Public License (GPL-3.0-or-later)
 * applies to this software, see COPYING.txt
 */
/*
 * The program tries to improve the balance by swapping NS and EW pairs
 *
 * new in version 5.1 routine 'improve' converted from Fortran to C
 * new in version 5.2 routine 'readmove' updated
 * new in version 5.3 error check in 'improve'
 * new in version 5.4 option -L (in vernum) works differently
 *                    movements for odd number of pairs allowed
 *                    Output of Qo
 * new in version 5.5 MSWindow to show progress
 * new in version 5.6 routine 'optim' as a DLL
 * new in version 5.7 optimalisation when more board groups than rounds
 * new in version 5.9 universal begin pos also for odd number of pairs
 * new in version 6.1 made code more solid for large numbers
 *                    stack size and input buffer size increased.
 * new in version 6.3 silly routine minsq replaced
 *                    stack size increased some more.
 * new in version 6.4 calculation of Qf had become inaccurate
 * new in version 6.5 64-bit compiler proof
 * new in version 6.6 issue with commandline args in windows version
 * new in version 6.7 balans as subprogram for use with program fv
 *                    -z=random, -Q=quiet, -k. Options rtfv in vernum also
 *                    optimalisation when more rounds than board groups
 * new in version 6.8 .. 7.0 Major revision.
 *                    vacancy quality incorporated in improve().
 *                    Many speed-up improvements
 * new in version 7.1 systematic handling of fix options r t i R D I
 * new in version 7.4 new option S for slow cooling may find hard Qf faster
 *                    new options -j -J
 * new in version 7.5 optimising ensemble of states in parallel using OPENMP
 *                    new algorithm 2 (-S2) for slow adaptive cooling of ensemble
 *                    more options for verbosity level
 *                    options to read movement from stdin, write to stdout
 *                    movement handling improved, mvi struct
 * new in version 7.58 XGetopt
 * new in version 7.6 -V with argument, -Q without, to control verbosity
 * new in version 8.1 detect & use modern CPU features automatically if built so (AVX*, ...)
 */
#ifdef _OPENMP
#include <omp.h>
#endif

#include "common.h"

#ifdef XGETOPT
#include "XGetopt.h"
extern char *optarg;      //XGetopt globals defined in XGetopt.c
extern int  optind, opterr, optopt;
#endif

#define base10 10

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

#if 0
static char *strdouble(double number, char buf[16])  // No longer used in >7.4.2
// format a double, with sane representation of INFINITY and NAN
// the formatted string is put in buf. and a pointer to it is returned
{
  if (isnan(number)) strcpy(buf," nan  ");
  else if(isinf(number))
    strcpy(buf,(signbit(number)? " -inf " : " +inf  "));
  else snprintf(buf,15,"%-12.4g",number);
  return buf;
}
#endif

static void get_set_ensemble_params(int *pesize, int *pnthreads)
// User input parser must ensure that supplied nthreads <= abs(esize).
// Zero means that we set a suitable default here:
//    esize!=0 && nthreads>0  : we enforce the wanted numbers
//    esize!=0 && nthreads==0 : nthreads set to min of abs(esize) and sysnthreads
//    Both are zero          : both set to sysnthreads (e.g. OMP_NUM_THREADS)
{
#ifdef _OPENMP
  if(*pnthreads == 0)
  {
    int nsys = omp_get_max_threads();
    if(*pesize == 0)
      *pnthreads = nsys;
    else
      *pnthreads = (abs(*pesize)<nsys ? abs(*pesize) : nsys);
    if(*pnthreads != nsys) // Only bother to call if different from default
         omp_set_num_threads(*pnthreads);
  }
  else
    omp_set_num_threads(*pnthreads);
#else
  *pnthreads = 1;
#endif
  if(*pesize == 0)
    *pesize = *pnthreads;
}
#ifdef USE_DLL
void reportlasterror(char *title)
// copied from optim.c
{
  LPVOID lpMsgBuf;
  char top[128];
  unsigned int LastError;

  LastError= GetLastError();
  snprintf(top,128,"error %d in %s", LastError, title);

  FormatMessage(
    FORMAT_MESSAGE_ALLOCATE_BUFFER |
    FORMAT_MESSAGE_FROM_SYSTEM |
    FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL,
    LastError,
    0,                           // Default language
    (LPTSTR) &lpMsgBuf,
    0,
    NULL
    );
  MessageBox( NULL, (LPCTSTR)lpMsgBuf, top, MB_OK | MB_ICONINFORMATION );
  fprintf(stderr,"%s\n",(LPCTSTR)lpMsgBuf);
// Free the buffer.
  LocalFree( lpMsgBuf );
}
#endif

#ifdef SUBPROGRAM

int balans_main(int argc, char *argv[], QUAL *pQual, FILE **result)
{
  int K4=0;

#else

int main(int argc, char *argv[])
{
  int K4=1;
#endif                       //SUBPROGRAM

  char inputfile[PATH_MAX];
  char outputfile[PATH_MAX];
  char fixerfile[PATH_MAX];
  FILE *foutputfile=NULL;
  FILE *ffixerfile=NULL;

  const double undefined_algoc2=-1e30; // Negative values allowed for S2, even large, but use silly for undef

  int Verbose;
  int Letters=0;     // translate board group numbers to letters (option -L)
  int Permutation=0; // renumber pairs, give permutation on command line (option -p)
  int i,p,bad=0, nBest=0;
  int V;
  int rfix[MAXFIX], tfix[MAXFIX], ifix[MAXFIX], nrfix, ntfix, nifix, njfix, nfirst;
  int rrel[MAXFIX], trel[MAXFIX], irel[MAXFIX], nrrel, ntrel, nirel, njrel, fixi;
  unsigned int jfix[MAXFIX], jrel[MAXFIX];
  unsigned char boardgroup;

  int ier, checkonly;
  double ssorg, s4org, Qo;
  long int Samples_l; // Raw # of samples returned by strtol(3), before error checking
  int Samples=2000;
  int randomize=0;
  static int raninit_done=0;   // Avoid running raninit() repeatedly
  int maxtime= 0x7fffffff, vfirst= -1, vlast= -1, algoi=0;
  int esize=0, nthreads=0; // If not changed by user option, proper defaults >0 will be set
  int user_set_w1=0; // Did user give a Qf1max-weight with -W w1:w2?  Otherwise a default is used.
  int use_Qf1av=1;   // 1 means use Qf1av as part of optimalisation.  Set to 0 iff w2=0.
  double w1=0.0, w2=1.0, w12, algoc1=0.0, algoc2=undefined_algoc2;
  char *q, *pp;

  mov_info_t mvi; // Movement info structure.  The following 7 vars are just copies to make the code shorter.
  SPUL *movement;
  int P1, r, b, t1, G, Vacant=0; // Vacant also used to read command line arg before reading movement

  int P,t,h,S,N;

/* normally, P=P1, but if a pair is absent:  P= P1-1
 * t1 = number of tables, t = nr of simultaneously occupied tables
 * h = nr of times each board is played - 1
 * t=P/2; G=t1*r; S=t*r*h; N=P*(P-1)/2; */
  mvi.movement = NULL; // Because we may goto quit before calling read_movement()
  mvi.Status = 0; // Because we may goto error before calling read_movement()

  int n,ind;
  int argV;
  char dummy=0;
  int retval=0;                // return value from main
#ifdef USE_MVER
  char *cpu_info  = raninit_cpu_support_str(); // string for info only, optional
#endif

/* ------------  Linking to the DLL  ------------ */
#ifdef USE_DLL

  typedef double (*Qf_p)(int64_t SS, int S, int N);
  typedef int (*setmaxtime_p)(int maxtime);
  typedef void (*balance_p)(int mode, SPUL *movement, int P1, int G, int h, int Vacant, QUAL *pQ);
  #ifdef MS_DOS
  typedef int (*optim_W_p)(mov_info_t *pmv, int Samples, int esize, int fix[], int K4, int vfirst, int vlast,
                           int use_Qf1av, double weight, int algoi, double algoc1, double algoc2, int Verbose,
                           char *Label);
  #else
  typedef int (*optim_p)(mov_info_t *pmv, int Samples, int esize, int fix[], int K4, int vfirst, int vlast,
                         int use_Qf1av, double weight, int algoi, double algoc1, double algoc2, int Verbose);
  #endif
  // pointers to functions present in the DLL
  #ifdef MS_DOS
  optim_W_p optim_W;
  #else
  optim_p optim; // for "balans6", no progress window
  #endif
  setmaxtime_p setmaxtime;
  balance_p balance;
  Qf_p Qf;

  HANDLE hdll;

  hdll = LoadLibrary("baloptim.dll");
  if(!hdll)
  {
    reportlasterror("ERROR: Load Library baloptim.dll failed\n");
    goto error;
  }
  // assign addresses to the function pointers using intermediate cast to 'void *' to silence
  // gcc 8+ -Wcast-function-type warnings, see https://trac.nginx.org/nginx/ticket/1865
  #ifdef MS_DOS
  optim_W   = (optim_W_p)(void *)GetProcAddress(hdll, "optim_W");
  if(!optim_W)
  {
    fprintf(stderr,"ERROR: procedure optim_W not found in dll\n");
    goto error;
  }
  #else
  optim     = (optim_p)(void *)GetProcAddress(hdll, "optim");
  if(!optim)
  {
    fprintf(stderr,"ERROR: procedure optim not found in dll\n");
    goto error;
  }
  #endif
  balance   = (balance_p)(void *)GetProcAddress(hdll, "balance");
  Qf        = (Qf_p)(void *)GetProcAddress(hdll, "Qf");
  setmaxtime= (setmaxtime_p)(void *)GetProcAddress(hdll, "setmaxtime");
  if(!balance)
  {
    fprintf(stderr,"ERROR: procedure balance not found in dll\n");
    goto error;
  }
  if(!Qf)
  {
    fprintf(stderr,"ERROR: procedure Qf not found in dll\n");
    goto error;
  }
  if(!setmaxtime)
  {
    fprintf(stderr,"ERROR: procedure setmaxtime not found in dll\n");
    goto error;
  }
#endif
/* ---------------------------------------------- */
// remove or comment this line after test phase:
// echo(argc, argv);

  outputfile[0]='\0';
  inputfile[0]='\0';
  fixerfile[0]='\0';
  nrfix=0; ntfix=0; nifix=0; njfix=0; nfirst=0;
  nrrel=0; ntrel=0; nirel=0; njrel=0;
  checkonly=0;
  Verbose= V_regular;
// when calling balans repeatedly it is necessary to reset optind to 0:
  optind=0;
  while ((n=getopt(argc, argv,
  ":AD:I:J:KLNP:QR:S:T:V:W:a:cf:hi:j:k:m:npr:s:t:uv:wz"
  )) != -1)
  {

//// for testing getopt
//    if(optarg)
//     printf("getopt %c, optarg %s, optind %d, optopt %c\n", n,optarg, optind, optopt);
//    else
//     printf("getopt %c, optarg %s, optind %d, optopt %c\n", n,"NULL", optind, optopt);

    switch(n)
    {
      case ':': // required argument missing
        fprintf(stderr,"Option %c requires an argument\n",optopt);
        goto error;
      case 'Q':
        Verbose= V_quiet;
        break;
      case 'V':
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
        if P_misc printf("# option free round %d\n",rrel[nrrel]);
        nrrel++;
        break;
      case 'D':
        ier=sscanf(optarg,"%d",&trel[ntrel]);
        if(ier != 1)
        {
          fprintf(stderr,"option D missing table number\n");
          goto error;
        }
        if P_misc printf("# option free table %d\n",trel[ntrel]);
        ntrel++;
        break;
      case 'I':
        ier=sscanf(optarg,"%d",&irel[nirel]);
        if(ier != 1)
        {
          fprintf(stderr,"option I missing pair number\n");
          goto error;
        }
        if P_misc printf("# option free pair %d\n",irel[nirel]);
        nirel++;
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
        if P_misc printf("# option free board %d\n",jrel[njrel]);
        njrel++;
        break;
      case 's':
        Samples_l= strtol(optarg, &q, base10);
        if(optarg == q) // no value given
        {
          fprintf(stderr,"Syntax error in s option: missing nr. of interations\n");
          goto error;
        }
        if(Samples_l <= 0L)
        {
          fprintf(stderr,"option s nr. of (super) iterations must be positive\n");
          goto error;
        }
        if(Samples_l > (long int) INT_MAX)
        {
          fprintf(stderr,"option s nr. of (super) iterations must be <= %d\n", INT_MAX);
          goto error;
        }
        Samples = (int) Samples_l;
        if P_misc printf("# %d (super) iterations\n",Samples);
        if(!*q) break; // only Samples given (use default for esize, nthreads)
        if(*q != ':') // terminator other than ':' not allowed
        {
          fprintf(stderr,"trailing garbage in %s\n", optarg);
          goto error;
        }
        if(! *++q) // no value after ':'
        {
          fprintf(stderr,"Syntax error in s option: missing ensemble size\n");
          goto error;
        }
        pp=q;
        esize=(int)strtol(pp, &q, base10);
        if(pp == q) // no value given
        {
          fprintf(stderr,"Syntax error in s option: second arg must be an integer ensemble size\n");
          goto error;
        }
	// Any integer value accepted.  0 means default, negative means randomise ensemble.
        if P_misc printf("# %d parallel processes\n",esize);
        if(!*q) break; // only esize given, fine, finished (use default for nthreads)
// commented the following ... user input of nthreads not possible anymore
//        if(*q != ':')
//        {
//          fprintf(stderr,"trailing garbage after ensemble size in %s\n", optarg);
//          goto error;
//        }
//        if(! *++q) // no value after ':'
//        {
//          fprintf(stderr,"Syntax error in s option: missing nthreads\n");
//          goto error;
//        }
//        pp=q;
//        nthreads=strtol(pp, &q, base10);
//        if(q == pp)
//        {
//          fprintf(stderr,"Syntax error in s option: invalid nthreads\n");
//          goto error;
//        }
//        if P_misc printf("# %d threads\n",nthreads);
//        if(nthreads < 0) // Explicit 0 allowed to mean default, but negative is illegal
//        {
//          fprintf(stderr,"Invalid number of threads %d in s option: must be a non-negative integer\n", nthreads);
//          goto error;
//        }
        if(*q)
        {
          fprintf(stderr,"trailing garbage in s option: %s\n", pp);
          goto error;
        }
        break;
      case 'k':
        ier=sscanf(optarg,"%d",&K4);
        if(ier != 1 || K4 < 0 || K4 > 1)
        {
          fprintf(stderr,"option k requires argument 0 or 1\n");
          goto error;
        }
        if P_misc printf("# optimize d4 %s\n",K4?"Yes":"No");
        break;
      case 'c':
        checkonly=1;
        break;
      case 'L':
        Letters=1;
        break;
      case 'p':
        Permutation=1;
        break;
      case 'z':
        randomize=1;
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
        break;
      case 'T':
        ier=sscanf(optarg,"%d",&maxtime);
        if(ier != 1)
        {
          fprintf(stderr,"option T missing time (seconds)\n");
          goto error;
        }
        if P_misc printf("# maximum time %d s\n",maxtime);
        break;
      case 'P':
// split argument into 2 numbers:
        vfirst= (int)strtol(optarg, &q, base10);
        if(*q==':' || *q=='-')
        {
          if(! *++q)
          {
            fprintf(stderr,"Syntax error in P option\n");
            goto error;
          }
          vlast= (int)strtol(q, &pp, base10);
          if(*pp)
          {
            fprintf(stderr,"trailing garbage in %s\n",optarg);
            goto error;
          }
        }
        else
        {
          if(! *q) vlast=vfirst;
          else
          {
            fprintf(stderr,"option P expects first-last or first:last\n");
            goto error;
          }
        }
        break;
      case 'W':
        w1=strtod(optarg, &q);
        if(*q==':')
        {
          if(! *++q)
          {
            fprintf(stderr,"Syntax error in W option\n");
            goto error;
          }
          w2= strtod(q, &pp);
          if(*pp)
          {
            fprintf(stderr,"trailing garbage in %s\n", optarg);
            goto error;
          }
        }
        else if(*q)
        {
          fprintf(stderr,"trailing garbage in W option: %s\n", q);
          goto error;
        }
        user_set_w1=1;
        break;
      case 'S': // Syntax: algoi[:algoc1[:algoc2]] = int[:double[:double]]
        algoi= (int)strtol(optarg, &q, base10);
        if(optarg == q)
        {
          fprintf(stderr,"Syntax error in S option: first arg must be an integer\n");
          goto error;
        }
        // fprintf(stderr,"DEBUG: algoi = %d\n", algoi);
        if(algoi < 0 || algoi > NALGO-1)
        {
          fprintf(stderr,"Invalid algorithm number %d in S option: must be in 0..%d\n", algoi, NALGO-1);
          goto error;
        }
        if(!*q) break; // only -Salgo given, fine, finished
        if(*q != ':')
        {
          fprintf(stderr,"trailing garbage after algorithm index in %s\n", optarg);
          goto error;
        }
        if(! *++q)
        {
          fprintf(stderr,"Syntax error in S option: missing parameter(s) after algorithm index\n");
          goto error;
        }
        algoc1=strtod(q, &pp);
        // printf("DEBUG: algoc1=%.7g\n",algoc1);
        q=pp;
        if(*q==':')
        {
          if(! *++q)
          {
            fprintf(stderr,"Syntax error in S option: missing second algorithm parameter\n");
            goto error;
          }
          algoc2=strtod(q, &pp);
          // fprintf(stderr,"DEBUG: algoc2=%.7g\n",algoc2);
          if(*pp)
          {
            fprintf(stderr,"trailing garbage after second algorithm parameter in %s\n", optarg);
            goto error;
          }
        }
        else if(*q)
        {
          fprintf(stderr,"trailing garbage in S option after first algorithm parameter: %s\n", q);
          goto error;
        }
        break;
      case 'a':
        ier=sscanf(optarg,"%s",fixerfile);
        if(ier != 1)
        {
          fprintf(stderr,"option a requires a file name\n");
          goto error;
        }
        if P_misc printf("# Reading fixers from file %s\n", fixerfile);
        break;
      case 'm':
        strcpy(outputfile,optarg);
        if(strcmp(outputfile, "-")==0) Verbose=0;
        break;
      case 'A': case 'K': case 'N': case 'n': case 'u': case 'w':
        fprintf(stderr,"option -%c not implemented in balans\n", n);
        goto error;
      case 'h':
#ifdef USE_MVER
#ifdef _OPENMP
        printf("\nbalans version %s  --  OpenMP %d  --  %s\n",szVersion,_OPENMP,cpu_info);
#else
        printf("\nbalans version %s  --  %s\n", szVersion, cpu_info);
#endif
#else
#ifdef _OPENMP
        printf("\nbalans version %s  --  OpenMP %d\n",szVersion,_OPENMP);
#else
        printf("\nbalans version %s\n", szVersion);
#endif
#endif /* USE_MVER */
#ifndef SUBPROGRAM
        printf("call: balans [OPTION]... [MOVEMENTFILE]\n\n");
        printf("With no MOVEMENTFILE given, _balans.txt is read as input movement.\n");
        printf("When MOVEMENTFILE is -, read input movement from standard input.\n\n");
        printf("options:\n");
        printf(" -c : just check only, no optimalisation\n");
        printf(" -s m:n try m random (super) iterations during optimalisation\n");
        printf("    n: optional number of parallel optimalisations. example: -s5000:8\n");
        printf("       n<0: randomise all but first parallel state initially\n");
        printf("       n=0 or absent: use default, depends on OMP_NUM_THREADS or # of cpus\n");
#endif
        printf(" -r n: keep round n fixed\n");
        printf(" -t n: keep table n fixed\n");
        printf(" -i n: keep pair n fixed\n");
        printf(" -j n: keep board n fixed. (n=number or capital letter)\n");
        printf(" -f n: keep first n positions fixed\n");
        printf(" -R n: keep all rounds fixed except round n\n");
        printf(" -D n: keep all tables fixed except table n\n");
        printf(" -I n: keep all pairs fixed except pair n and opponents\n");
        printf(" -J n: keep all boards fixed except board n. (n=number or capital letter)\n");
        printf(" -a fixerfile: supply r * t fixers 0=free, 1=fixed\n");
#ifndef SUBPROGRAM
        printf(" -v n: pair n is absent\n");
        printf(" -z :  randomize compass directions before optimalisation\n");
        printf(" -L : indicate board groups by capital letters\n");
        printf(" -p : renumber pairs, give permutation on command line\n");
        printf(" -m FILENAME: write updated movement to file FILENAME\n");
        printf(" -m -       : write updated movement to standard output\n");
        printf(" -Q : quiet mode, print balance characteristics only\n");
        printf(" -V 0: no output to console at all\n");
        printf(" -V 1: same as -Q: quiet mode, print balance characteristics only\n");
        printf(" -V 2: limited output, fixed/switched tables and characteristics only\n");
        printf(" -V 3: default verbose mode\n");
        printf(" -V 7: extremely verbose mode, including expert output (e.g. energy statistics)\n");
        printf(" -V BITMASK: for possible values see help file (enable selected output types)\n");
        printf(" -P n1-n2 (e.g. -P 4-8):preferred range of absent pair nr.\n");
        printf(" -W w1:w2 (e.g. -W 0:1) relative weight of Qf1max and Qf1av\n");
        printf("       w1 and w2 notation example +1.2e-3\n");
        printf("       w1=0, w2 nonzero optimize Qf1av only\n");
        printf("       w1 nonzero, w2=0 optimize Qf1max only\n");
        printf("       w1=0, w2=0 don't optimize Qf1av, Qf1max\n");
        printf(" -k 0: don't optimize d4 / -k 1: optimize d4\n");
        printf(" -S n:c1:c2 (e.g. -S 2:400:0.01) algorithm + optional parameter(s)\n");
        printf("       n=0 use fast increases & decreases of T (c1=Tstart, c2=Tpeak)\n");
        printf("       n=1 use slow exponential cooling, c1=Tstart, c2=Tend\n");
        printf("       n=2 use adaptive ensemble cooling, c1=Tstart, c2=speed\n");
        printf(" -T n: maximum time in seconds\n");
        printf(" -h : show this help, then quit\n");
#endif
        printf("\nfor detailed help and description of the program see:\n");
        printf("Dutch:   http://www.pjms.nl/BALANS/README.html\n");
        printf("English: http://www.pjms.nl/BALANS/README_en.html\n");
        retval=0;
        goto quit;
      default:
        fflush(stdout);
        fprintf(stderr,"invalid option %c\n", optopt);
        bad++;
        break;
    }
  }
  if(bad){ fprintf(stderr,"bad options ... quitting\n"); goto error; }
  for(n=1;n<=optind;n++) ++argv;

// done with commandline options
  if (P_misc)
  {
    print_the_time();
#ifdef USE_MVER
    printf("balans %s (CPU support detected: %s)\n",szVersion,cpu_info);
#else
    printf("balans %s\n",szVersion);
#endif
  }
  setmaxtime(maxtime);

  ind=optind;
  if(ind>=argc)
  {
    strcpy(inputfile,"_balans.txt");
    if P_misc printf("No input file given, using %s\n", inputfile);
  }
  else if(ind < argc-1 && !Permutation) // Extra args only allowed if -p (permute pair numbers)
  {
    fprintf(stderr,"Error: only one input file allowed\n");
    goto error;
  }
  else // Exactly 1 input file given (possibly '-'), possibly with permuted pair numbers after that
  {
    strcpy(inputfile,*argv);
    ++ind; ++argv;
  }
  if(strlen(outputfile)==0)
  {
    strcpy(outputfile,"_balans.txt");
  }
// remove or comment this line after test phase:
//  printf("*** inputfile=%s, outputfile=%s\n", inputfile, outputfile);
  if(!raninit_done)
  {
    raninit();
    raninit_done=1;
  }

// open fixerfile
  if(strlen(fixerfile)>0)
  {
    if(!ffixerfile)ffixerfile= fopen(fixerfile,"r");
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
  N=P*(P-1)/2;
  QUAL Q, Q1;
  Q.r = Q1.r =r;

  get_set_ensemble_params(&esize, &nthreads);
  if P_misc
    printf("ensemble size: %d      number of threads: %d\n", esize, nthreads);
  if(randomize && esize>1)
    esize = -esize; // Will make improve2 randomise rest of ensemble members too

  if(algoi == 2 && abs(esize) == 1)
  {
    fprintf(stderr,"Error: Algorithm 2 (-S2) not allowed unless ensemble size larger than 1\n");
    goto error;
  }

  if(algoc1 <=  0.0) // Set default start temperature because user didn't supply it
  {
    algoc1 = (algoi == 1 ? P : algo_cdefault[algoi][0]);
    // Special case hack for S1 usually ok (Tstart = # of pairs), although too low on rare occasions.
    // For S2 do special lowering of Tstart if fewer than 2k iter, mostly to benefit silly benchmarks.
    // In that case lower by max fractional rate (0.5% per iter) for each iter below 2k, but no lower than 75.
    if(algoi == 2 && Samples < 2000)
    {
      algoc1 *= pow(0.995, 2000-Samples);
      if(algoc1 < 75.0) algoc1 = 75.0;
    }
  }
  if(algoc2 <= undefined_algoc2) // Set default Tpeak (S0), Tend (S1) or speed (S2) since user didn't set it
  {
    if(algoi == 0)
      algoc2 = algo_cdefault[algoi][1]; // S0 default is fixed
    else if(algoi == 1)
      algoc2 = P/4.0; // S1 default often too low, but not always -- hard to do any better for S1 in general
    else // S2: 0.28 is actually best fit instead of 0.35, but too low speed is much worse than a bit too high
      algoc2 = 0.35*pow(P*r, 1.361)/Samples; // Quite often a good default for S2, found by experiments
  }

  if(!user_set_w1) // set default weight now that we know if P is odd
  {
    if((P & 1) || (Vacant > 0))w1=0;
    else w1=2.0;
  }
  if (w2==0)
  {
    use_Qf1av=0;
    w12= (w1 != 0 ? 1 : 0);
  }
  else
  {
    w12= w1/w2;
  }
  if P_misc
  {
    // char buf1[16], buf2[16], buf12[16]; // For NaN/Inf, but avoided in >7.4.2 for safe fast-math
    // printf("w1=%s  w2=%s w1/w2=%s\n",
    //        strdouble(w1,buf1), strdouble(w2,buf2), strdouble(w12,buf12));
    printf("w1=%-12.4g  w2=%-12.4g w1/w2=%-12.4g  use_Qf1av=%d\n", w1, w2, w12, use_Qf1av);
  }

// open new level to enable dimensioning arrays properly
  {
    SPUL origmove[G];
    int fix[G];
    double Qf1av, Qf1max, Qf1[P1];
    int64_t SSsum;
    int Ssum;
    for(i=0;i<G;++i)fix[i]=0;

   // for(i=0; i<P1; ++i) Qf1[i] = -999999; // to silence warnings; but "h>1" check should be enough

    if((h = geth(&mvi, P_warn)) < 1)
    {
      if(!checkonly)
      {
        fprintf(stderr,"can't optimize this movement\n");
      }
    }
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
      printf("amount of competition S= %d\n",S);
      printf("number of pairs of pairs N= %d\n",N);
    }
    if P_inp_move
    {
      printf("\n%s\n","initial movement scheme:");
      print_movement(&mvi,stdout,0,0,0);
    }
    for(i=0; i<G; ++i)origmove[i]=movement[i];
    if(Permutation || Letters)
    {
      int newp[P1];
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
      if(Letters) mvi.Getallen=0;
      renumber(&mvi, newp, fix, Letters, Permutation, 0, 0);

      if P_inp_move printf("\n%s\n","renumbered movement scheme:");
      if P_inp_move print_movement(&mvi,stdout,0,0,0);
    }

    if(inspect(&mvi, P_inp_mat, P_warn, &Qo) < 0) goto error;
    balance(P_inp_mat,movement,P1,G,h,Vacant,&Q);
    if P_misc
    {
      printf("sum of squares original: %7.1f \n",Q.var);
      printf("sum of 4th powers: %7.1f \n",Q.s4);
      printf("sd= %.4g, d4= %.4g, Qc=%.4g, Qf=%.4g, Qo=%.4g\n",
        Q.sd, Q.d4, Q.Qc, Q.Qf, Qo);
      if(Vacant>0)printf("Pair %d is absent!!\n", Vacant);
    }
    if(ffixerfile)readfixers(ffixerfile,G,fix);
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
    {
      for(i=0;i<G;++i)
      {
        if(mvi.Getallen)
        {
          if(movement[i].bord == jfix[p]) fix[i]=1;
        }
        else
        {
          if(movement[i].bord - 'A' + 1== jfix[p]) fix[i]=1;
        }
      }
    }
// process options R, D, I, J;
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
        fixi=1;
        for(p=0;p<nrrel;++p)
          if( i/t1 == rrel[p]-1) fixi=0;
        if(fixi)fix[i]=1;
      }
    }
    if(ntrel)
    {
      for(i=0;i<G;++i)
      {
        fixi=1;
        for(p=0;p<ntrel;++p)
          if( i%t1 == trel[p]-1) fixi=0;
        if(fixi)fix[i]=1;
      }
    }
    if(nirel)
    {
      for(i=0;i<G;++i)
      {
        fixi=1;
        for(p=0;p<nirel;++p)
          if(movement[i].noordzuid[0]==irel[p] ||
          movement[i].noordzuid[1]==irel[p])
            fixi=0;
        if(fixi)fix[i]=1;
      }
    }
    if(njrel)
    {
      for(i=0;i<G;++i)
      {
        fixi=1;
        for(p=0;p<njrel;++p)
        {
          if(mvi.Getallen)
          {
            if(movement[i].bord == jrel[p]) fixi=0;
          }
          else
          {
            if(movement[i].bord - 'A' + 1 == jrel[p]) fixi=0;
          }
        }
        if(fixi)fix[i]=1;
      }
    }
    for(i=0;i<G;++i)
      if(movement[i].noordzuid[0]==0 ||
      movement[i].noordzuid[0]==Vacant ||
      movement[i].noordzuid[1]==Vacant)
        fix[i]=1;
    if (randomize)
    {
      int tmp;
      for(i=0;i<G;++i)
      {
        if(!fix[i] && genrand()&1)
        {
          tmp= movement[i].noordzuid[1];
          movement[i].noordzuid[1]= movement[i].noordzuid[0];
          movement[i].noordzuid[0]= tmp;
        }
      }
// removed in version 7.63, May 2022. Not clear why it ever got here.  Broke manipulate if used with -z.
//      for(i=0; i<G; ++i)origmove[i]=movement[i];
      if P_inp_move
      {
        printf("\nrandomized the movement\n");
        print_movement(&mvi,stdout,0,0,0);
      }
      balance(P_inp_mat,movement,P1,G,h,Vacant,&Q);
    }                          // end randomize
    if(vfirst <= 0) vfirst=1;
    if(vlast <= 0) vlast=P1;   // also counting Vacant pair
    if(vfirst > vlast)
    {
      fprintf(stderr,"invalid range of absent pairs %d-%d\n", vfirst,vlast);
      goto quit;
    }
    if(P_inp_swit && (ffixerfile || nfirst || nrfix || ntfix || nifix
      || njfix || nrrel || ntrel || nirel || njrel || Vacant))
    {
      printf("\nFixed tables, indicated by 1\n");
      for(i=0;i<G;++i)
      {
        printf("%2d",fix[i]);
        if((i%t1)==(t1-1))printf("\n");
      }
      printf("\n");
    }
    ssorg=Q.var;
    s4org=Q.s4;

    if (Verbose) fflush(stdout);
// done if no optimizing requested

    if(Samples>0 && !checkonly)
    {
      int show_out_mat = P_out_mat; //true if output matrix wanted
#ifdef MS_DOS
      nBest=optim_W(&mvi, Samples, esize, fix, K4, vfirst, vlast,
                    use_Qf1av, w12, algoi, algoc1, algoc2, Verbose, inputfile);
#else
      nBest=  optim(&mvi, Samples, esize, fix, K4, vfirst, vlast,
                    use_Qf1av, w12, algoi, algoc1, algoc2, Verbose);
#endif
      if(nBest < 0)goto error;

      if P_out_swit
      {
        printf("\nSwitched tables, indicated by 2 (while 1 are fixed tables)\n");
// keep in mind that this text is processed by "manipulate"
        for(i=0;i<G;++i)
        {
          if(movement[i].noordzuid[0]==origmove[i].noordzuid[0])
            printf("%2d",fix[i]);
          else printf("%2d",2);
          if((i%t1)==(t1-1))printf("\n");
        }
        printf("\n");
      }

      i=G;
      while(i>0)
      {
        i--;
        if(origmove[i].noordzuid[0]!=movement[i].noordzuid[0])i= -1;
      }
      if (i<0)                 // if any tables are switched
      {
        if P_out_move
        {
          printf("\n%s\n","optimized movement scheme:");
          print_movement(&mvi,stdout,0,0,0);
        }
      }
      else
      {
        if (P_out_move || P_out_swit) printf("no improvement\n");
        if (P_inp_mat) show_out_mat = 0; // matrix unchanged and already printed 
      }
// output balance characteristics
      balance(show_out_mat,movement,P1,G,h,Vacant,&Q);
      if P_Qf
      {
        printf("\nsum of squares    was %7.1f, now %7.1f \n",ssorg,Q.var);
        printf("sum of 4th powers was %7.1f, now %7.1f \n",s4org,Q.s4);
      }
    }
    Qf1av= 0;
    Qf1max= 0;
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
      Qf1av= Qf(SSsum, Ssum, P1*Q1.N);
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
    if P_Qf
    {
      printf("sd= %.4g, d4= %.4g, Qc=%.4g, Qf=%.4g, Qo=%.4g\n",
        Q.sd, Q.d4, Q.Qc, Q.Qf, Qo);
      if(Vacant>0)printf("Pair %d is absent!!\n", Vacant);
    }
    Q.Qf1av=Qf1av;
    Q.Qf1max=Qf1max;
    if(P_summary && Q.Qc > 100-eps && Qf1av > 100-eps)
    {
      printf("\n\nthe balance is perfect!!\n");
    }
    if P_Qf printf("\n");
#ifdef SUBPROGRAM
    *pQual = Q;
#else
    if(P_summary)
    {
      if(nBest == 0 || abs(esize) == 1) // In this case nBest is not useful info, so omit it
        printf("Qf=%6.2f, sd=%6.2f, d4=%6.2f, Qc=%6.2f, Qo=%6.2f, P=%d, S=%d, N=%d, Qf1av=%6.2f, Qf1max=%6.2f\n",
               Q.Qf, Q.sd, Q.d4, Q.Qc, Qo, Q.P, Q.S, Q.N, Q.Qf1av, Q.Qf1max);
      else
        printf("Qf=%6.2f, sd=%6.2f, d4=%6.2f, Qc=%6.2f, Qo=%6.2f, P=%d, S=%d, N=%d, Qf1av=%6.2f, Qf1max=%6.2f, nBest= %d/%d\n",
               Q.Qf, Q.sd, Q.d4, Q.Qc, Qo, Q.P, Q.S, Q.N, Q.Qf1av, Q.Qf1max, nBest, abs(esize));
    }
#endif
    foutputfile=NULL;
    if(strlen(outputfile)>0)
    {
      if(strcmp(outputfile,"-") == 0) foutputfile=stdout;
      else foutputfile= fopen(outputfile,"w+");
      if(!foutputfile)
      {
        fprintf(stderr,"failed to open file %s for output\n",outputfile);
        goto error;
      }
#ifdef SUBPROGRAM
      if(result)  *result= foutputfile;
      else { fclose(foutputfile); foutputfile=NULL; }
#endif
    }
    if(foutputfile)
    {
      if P_misc printf("input file is %s, output file is %s\n",
          inputfile,outputfile);
      print_movement(&mvi,foutputfile,0,0,0);
      if(Vacant<=0)
        fprintf(foutputfile,"\n#Qf=%.4g   Qo=%.4g\n",Q.Qf, Qo);
      else
        fprintf(foutputfile,"\n#Qf1(%d)=%.4g  Qo=%.4g\n",Vacant,Q.Qf, Qo);
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
    else { if P_misc  printf("input file %s\n",inputfile); }
  }
  fflush(stdout);
  if(mvi.Status & Unsuitable)
  {
    fprintf(stderr,"--- %s: Program not suitable for this movement ---\n", inputfile);
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
#ifdef USE_DLL
// FreeLibrary may be harmful, let the system do the clean up.
// See https://docs.microsoft.com/en-us/windows/win32/dlls/dllmain
// "In this case, it is not safe for the DLL to clean up the resources. Instead, the DLL should allow the operating system to reclaim the memory."
//  if(hdll) FreeLibrary(hdll); // commented out!
#endif

// make sure all output is processed before leaving main
// in particular when mysterious crash occurs
  fflush(stdout);
  fflush(stderr);
  return retval;
}
