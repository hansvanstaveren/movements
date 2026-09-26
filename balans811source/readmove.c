#include "readmove.h"
#ifdef GUI
#include "winver.h"
#endif

#define MAXBORD 255
#define MAXBUF 4096

char* ugets(char *buf,int count,FILE *stream, int *lastEOL);

/*
 * movement is een array van SPUL waarbij elk element bevat:
 *   char bord        =  spelgroep
 *   int noordzuid[2] =  paarnummer NZ, paarnummer OW
 *
 * P = aantal paren
 * r = aantal ronden
 * t = aantal tafels
 * Getallen = boolean
 *  indien true dan bevat bord integer in de range 0 .. MAXBORD
 *  indien false dan bevat bord een ASCII symbool
 *
 * parameters van readmove():
 *  movement   = pointer naar array (zie boven)
 *  inputfile  = string inputfile,      INPUT
 *  *P         = pointer naar P,        OUTPUT
 *  *r         = pointer naar r,        OUTPUT
 *  *b         = pointer naar b,        OUTPUT
 *  *t         = pointer naar t,        OUTPUT
 *  *Getallen  = pointer naar Getallen, OUTPUT
 * return value:
 *  movement   = pointer naar array (zie boven), ==NULL bij fouten
 *
 * bij aanroep van readmove wordt de array movement gealloceerd.
 * Deze geheugenruimte wordt later weer vrijgegeven bij een volgende
 * aanroep, als de input parameter movement niet NULL is.
 *
 * voor de layout van de schemafiles zie:
 *  http://www.pjms.nl/BALANS/schemafile.html
 *
 */

/*================== intgetb ========================*/
/* decode an integer from an ASCII string */
/* arguments:  line (input) = string                    */
/*            *len  (input) = nr of characters in line  */
/*            *ipt (input/output) = index within line   */
/*            *ianswer (output) = result (integer)      */
/*            *term (output)    = terminating character */
/* stat:         0 = number decoded, both results O.K.  */
/*      notinteger = number decoded, not an integer     */
/*      nonumber   = no number, terminator only         */
/*      EOString   = no number, end of string           */
/*      alpha      = alphanumeric terminator            */
/* if no number is decoded, ianswer is unchanged        */
/* ipt input value : 0 for first character of line      */
/* ipt output value: index of character after terminator*/

/* NB: Current policy is that the only acceptable changes to this very old function are those that
 * eliminate compiler/analyzer warnings and do nothing more than that.  Example: a dead assignment
 * to 'finale' has been removed (flagged by the clang static analyzer) but eliminating 'finale'
 * completely is not allowed ... until possibly some day an analyzer flags that too ("use break").
 */

int intgetb (char *line,int *len,int *ipt,int *ianswer,int* term)
{
  int ch= -1,where,finale,i,digit;
  int number, oldnumber, base=10,flag;

  i= *ipt; *term=0; where=0; finale=0; number=0; flag=0;

  while ( ! finale )
  {
    ch=0;
    if(i >= *len) { *ipt=i; *term=0; break; }
    ch=line[i++];
    *term=ch; *ipt=i;
    digit= ch - '0';
    if ( digit >= 0 && digit < base)
    {
      if (where < 3) where=3;
      number= (oldnumber=number)*base + digit;
      if(number < oldnumber) flag|=(notinteger|errstat);
    }
    else { if( ! (ch==' ' && where==0))finale=1; }
  }
  if (where < 3) flag=nonumber;
  if ((where == 0 && ch==0) || ch == '#')flag|=EOString;
  if (where >= 3) *ianswer = number;
  if(isalnum(*term)) flag|=alpha;
  return(flag);
}

static SPUL *getmove(FILE *finputfile, const char *inputfile, int *Howell, int *Mitchell,
                     int *Getallen, int *pP, int *pr, int *pb, int *pt)
{
  int len=0, ipt=0, ianswer=0, stat=0, k, ns, n;
  int term;
  char buf[MAXBUF];
  SPUL *movement=NULL;
  int nheader,need, header[5], qheader;
  int i, ipt0, ipt1, r, t=1, P, b, I, firstboard, tmpbord, j;
  int lastEOL=0;

  if(!finputfile)
  {
    fprintf(stderr,"input file %s not found!?\n",inputfile);
    return NULL;
  }

  for(n=0;n<5;n++)header[n]=0;
  k=0; ns=0; r=-1; P=-1; b=-1; firstboard=1;
  nheader=0;
  qheader=1;
  need=0;
  rewind(finputfile);

  while(ugets(buf,MAXBUF,finputfile, &lastEOL))
  {
    stat=0;
    len=(int)strlen(buf);
    if(len < 0) len=9999999; // Guard against strlen>INT_MAX, silence 'clang -Weverything'
    if(len > MAXBUF) stat= EOString|nonumber;
//    else if(buf[len-1] != '\n' && buf[len-2] != '\n') stat= EOString|nonumber;
    if(stat&EOString)
    {
      if(len > MAXBUF)
        fprintf(stderr,"%s: buffer overflow, line length>%d, max=%d\n",
          inputfile,len,MAXBUF);
      else
        fprintf(stderr,"%s: no numbers, input line = %s\n",inputfile, buf);
      goto fout;
    }
    ipt=0;
    while(! (stat&EOString))
    {
      stat=intgetb(buf, &len, &ipt, &ianswer, &term);
// jan 2020: better test for text before the header
//           comment starting with # is not standard, but we allow it
      if(stat & nonumber && term != '#' && nheader < 2)
      {
/*allow end-of-line after first number (backwards compatible)*/
        if(stat&EOString)goto skip;
        fprintf(stderr, "getmove header expected but text ecountered: %s\n", buf);
        goto fout;
      }
/*no number and not alphanumeric: skip white space*/
      if(stat==nonumber && term != '#')goto skip;
/*allow end-of-line after first number (backwards compatible)*/
      if(stat&EOString && nheader<2)goto skip;
      if(qheader)
      {
        if(stat==0)
        {
          if(nheader<5)header[nheader]=ianswer;
          nheader++;
        }
        if(stat&alpha)
        {
          if(term == 'H') *Howell=1;
          if(term == 'M') *Mitchell=1;
          if(term == 'g') *Getallen=1;
        }
        if(term == '#') stat=EOString;
        if(stat&EOString)
        {
          P=header[0];
          *pP=P;
          if(nheader > 2)
          {
            t=header[1];
            r=header[2];
            b=header[3];
            I=header[4];
          }
          else
          {
            r=header[1];
            t=P/2;
            b=r;
            I=0;
          }
          *pr=r;
          *pb=b;
          *pt=t;
          if(I==1){ fprintf(stderr,"Individual movement (not implemented) or other error in header\n"); goto fout; }
          if(P<2||r<=0||t<=0||b<=0||I!=0)
          {
            fprintf(stderr,"%s: empty movement or error in header\n",inputfile);
            fprintf(stderr,"header:");
            for(n=0;n<nheader;n++)fprintf(stderr," %d ",header[n]);
            fprintf(stderr,"\n");
            goto fout;
          }
          need=t;
          if(!(*Howell || *Mitchell))need*= r;
          k=0; ns=0;
          if(!(movement= malloc((unsigned int)(t*r)*sizeof(SPUL))))
            { fprintf(stderr,"getmove: malloc failed\n"); exit(-1); }
          qheader=0;
        }
      }
      else
      {
        if(ns<2)
        {
          if(stat==0)
          { movement[k].noordzuid[ns]=ianswer; ++ns; }
          else if (stat&alpha) goto fout;
        }
        else if(ns==2)
        {
          if(*Getallen)
          {
            if(stat==0)
            {
              if(ianswer >=0 && ianswer <= MAXBORD)
              {
                movement[k].bord=(unsigned int)ianswer;
              }
              else
              {
                fprintf(stderr,"%s: illegal board group %d, max=%d\n", inputfile, ianswer, MAXBORD);
                goto fout;
              }
            }
            else
            {
              fprintf(stderr,"%s: numeric board group expected\n",inputfile);
              goto fout;
            }
          }
          else
          {
            tmpbord=term;
            if(stat==0)
            {
//numeric boardset but Getallen not set
//is it the first non-zero?? Then we switch to numeric board groups
              if (ianswer != 0 && firstboard)
              {
                *Getallen=1;
                tmpbord=ianswer;    // numeric value
                for(j=k-1; j>=0; j--) movement[j].bord=0;
                firstboard=0;
              }
              else
                tmpbord='0'+ianswer; // ASCII value
            }
            else if(stat&alpha)
            {
              firstboard=0;
            }
            if(tmpbord >=0 && tmpbord <= MAXBORD)
            {
              movement[k].bord=(unsigned int)tmpbord;
            }
            else
            {
              fprintf(stderr,"%s: illegal board group %d, max=%d\n", inputfile, tmpbord, MAXBORD);
              goto fout;
            }
          }
          k++;
          ns=0;
          if(k>=need)goto exit;
        }
      }
      skip: ;
    }
  }
  exit:
  if(k<need)
  {
    fprintf(stderr,"%s: need %d, found %d: not enough data!\n", inputfile,need,k);
    goto fout;
  }
  return movement;
fout:
// jan 2020: better error reporting
  if(P<0)P=0;
  if(r<0)r=0;
  if(qheader) fprintf(stderr,"error in movement %s\nstat=%08x %d pairs, %d rounds, %d boards\n",inputfile,stat,P,r,b);
  else        fprintf(stderr,"error in movement %s\nstat=%08x %d pairs, %d rounds, round %d, table %d\n",inputfile,stat,P,r,(k/t)+1,(k%t)+1);
  ipt0=ipt-20; if(ipt0<0)ipt0=0;
  ipt1=ipt+10; if(ipt1>=len)ipt1=len;
  for(i=ipt0;i<ipt1;++i)fprintf(stderr,"%c",buf[i]);
  fprintf(stderr,"\n");
  for(i=ipt0;i<ipt ;++i)fprintf(stderr,".");
  fprintf(stderr,"^\n");
  if(movement) { free(movement); movement=NULL; }
  return NULL;
}

SPUL *readmove(SPUL *movement, const char *inputfile, int *pP,int *pr, int *pb, int *pt, int *pGetallen)
{
  int k,l,m,i,j,q;
  unsigned int tops;
  FILE *finputfile;
  int Howell, Mitchell, Getallen;
  int P,r,b,t,G;

  if(movement) { free(movement); movement=NULL; }
  Howell=0;
  Mitchell=0;
  Getallen=0;
  *pP=0;
  *pr=0;
  *pb=0;
  *pt=0;
  *pGetallen=0;
  finputfile=stdin;
  fflush(stdout); // flush output before possible messages from readmove
// if inputfile is "-" stdin will be used
  if(strlen(inputfile)>0 && strcmp(inputfile,"-"))
  {
    finputfile= fopen(inputfile,"r");
    if(!finputfile)
    {
      fprintf(stderr,"file %s not found, or not readable\n",inputfile);
      goto errorreadmove;
    }
  }
  movement=getmove(finputfile, inputfile, &Howell, &Mitchell, &Getallen, pP, pr, pb, pt);
  if(movement==NULL) goto errorreadmove;

  *pGetallen=Getallen;
  P= *pP;
  r= *pr;
  b= *pb;
  t= *pt;
  G=t*r;
  if(P <= 0)
  {
    fprintf(stderr,"ERROR: number of pairs must be positive\n");
    goto errorreadmove;
  }
  if(P/2*2 != P)
  {
//    printf("WARNING: number of pairs is odd\n");
  }
  if(r <=0 )
  {
    fprintf(stderr,"ERROR: number of rounds must be positive\n");
    goto errorreadmove;
  }
  if(b <=0 )
  {
    fprintf(stderr,"ERROR: number of board sets must be positive\n");
    goto errorreadmove;
  }
  if (Howell || Mitchell)
  {
    if(b>MAXBORD)
    {
      fprintf(stderr,"too many board groups\n");
      goto errorreadmove;
    }
    k=t;
    tops= 'A'+(unsigned int)r;
    if(Getallen)tops=1+(unsigned int)r;
    for(l=1; l<r; ++l)
    {
      for(m=0; m<t; ++m)
        if(Howell)
      {
        q=k-t;
        for(i=0;i<2;i++)
        {
          movement[k].noordzuid[i]=movement[q].noordzuid[i];
          if(movement[q].noordzuid[i]<=r)
          {
            movement[k].noordzuid[i]++;
            if(movement[k].noordzuid[i]>r)
              movement[k].noordzuid[i]=1;
          }
        }
        movement[k].bord=movement[q].bord;
        ++movement[k].bord;
        if(movement[k].bord == tops)  movement[k].bord-= (unsigned int)r;
        ++k;
      }
      else if (Mitchell)
      {
        q=k-t;
        movement[k].noordzuid[0]=movement[q].noordzuid[0];
        i=q-1; if (m==0) i+=t;
        movement[k].noordzuid[1]=movement[i].noordzuid[1];
//        i=q+1; if (m==t-1) i-=t;
//        movement[k].bord=movement[i].bord;
// feb 2009 changed the above to the following. Also works for relay Mitchells
        movement[k].bord= movement[q].bord+1;
        if(movement[k].bord == tops)  movement[k].bord-= (unsigned int)r;
        ++k;
      }
    }
  }

  for(k=0; k<G; ++k)
  {
    j=movement[k].noordzuid[0];
    i=movement[k].noordzuid[1];
    if(j<0|| j>P){fprintf(stderr,"illegal pair: %d\n",j); goto errorreadmove;}
    if(i<0|| i>P){fprintf(stderr,"illegal pair: %d\n",i); goto errorreadmove;}
  }
  fclose(finputfile);
  finputfile=NULL;
  return movement;
errorreadmove:
  fflush(stderr);
  if(finputfile) fclose(finputfile);
  finputfile=NULL;
  return NULL;
}

static int bb(int number, int base, int offodd, int offeven)
{
// ??  if(number==0)return 0;
  int offset = ((number&1) ? offodd : offeven);
  return offset + (base>0
                   ? 1 + (number-1)%base
                   : number);
}

void printmove(FILE *f, const SPUL *movement, int P1, int r, int b, int t1,
               int Getallen, int base, int offodd, int offeven)
{
  int k=0;

  if(!f)
  {
    fprintf(stderr,"printmove: non-existent file pointer!\n");
    return;
  }
  fprintf(f, "%2d %2d %2d %2d%2d",P1,t1,r,b,0);
  if(Getallen) fprintf(f," g");

  for(int l=0; l<r; ++l)
  {
    fprintf(f, "\n");
    for(int m=0; m<t1; ++m, ++k)
      fprintf(f, (Getallen ? "%2d-%2d %2d  " : "%2d-%2d %c "),
              bb(movement[k].noordzuid[0], base, offodd, offeven),
              bb(movement[k].noordzuid[1], base, offodd, offeven), movement[k].bord);
  }
  fprintf(f, "\n");
  fflush(f);
}

#ifdef standalone
/* ============ test en demo programma  ============================ */
static void check(char *inputfile)
{
  int P, r, b, t, Getallen;
  SPUL *movement = NULL;

  printf("\ninputfile: %s\n",inputfile);

  fflush(stdout);

  if((movement=readmove(movement, inputfile, &P, &r, &b, &t, &Getallen)))
  {
    printmove(stdout, movement, P, r, b, t, Getallen, 0,0,0);
    printf("\n");
    free(movement);
    movement = NULL;
  }
  else
  {
    printf("readmove error\n");
  }

  fflush(stdout);
}

int main()
{
  check("examples/nbb14.7.txt");
  check("examples/test.txt");
  check("examples/short10.7.how");
  check("examples/slordig.how");
  check("examples/mitchell18.mit");
  check("examples/fout.how");
  check("examples/10HOWL18.ASC");
  check("examples/10MULT06.ASC");
  check("examples/nonexistent.txt");
  return 0;
}
#endif

#ifdef pairs
/* ============ output number of pairs and other information  ============================ */
static int pp(char *inputfile)
{
  int P, r, b, t, Getallen;
  SPUL *movement = NULL;
  double FQf(SPUL *movement, int P, int G, int r, int b);
  double Qf;

  if((movement=readmove(movement, inputfile, &P, &r, &b, &t, &Getallen)))
  {
     Qf= FQf(movement,P,r*t,r,b);
     printf("%25s:pairs=%d, rounds=%d, boards=%d, tables=%d, Qf=%5.2f\n",inputfile,P, r, b, t, Qf);
    fflush(stdout);
    free(movement); movement=NULL;
    return 0;
  }
  else
    return 1;
}

int main(int argc, char * argv[])
{
  int n=1;
  while(argc>n) { pp(argv[n++]); }
  return 0;
}
#endif
