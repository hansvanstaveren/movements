#ifdef MS_DOS
#include "winver.h"
#include <commctrl.h>
#include <process.h>
#include "resource.h"
#endif                           /* MS_DOS */

#include "balans.h"
#include "optim.h"

#ifdef MS_DOS
#define colorBkd  RGB(0xF0,0xF0,0x7E)
#define IDM_OPTIMALISEREN 40022
static TCHAR szAppName[] = TEXT ("balans");
static char szCaption[64]= " balans";
static mov_info_t *pmv_W;
static int Samples_W, esize_W;
static DWORD ret_W;
static int* fix_W;
static int K4_W;
static int vfirst_W;
static int vlast_W;
static int use_Qf1av_W;
static int algoi_W;
static double weight_W, algoc1_W, algoc2_W;
static int Verbose_W;

static void reportlasterror(char *title)
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

/*=================== Thread:  OptDlgProc ======================*/
static HWND hwndTijd=NULL, hwndO=NULL;

#ifdef CREATETHREAD
static DWORD WINAPI Thread(LPVOID lpParam)
{
  INTERRUPT=FALSE;
  ret_W=(DWORD)optim(pmv_W, Samples_W, esize_W, fix_W, K4_W, vfirst_W, vlast_W,
                     use_Qf1av_W, weight_W, algoi_W, algoc1_W, algoc2_W, Verbose_W);
  SendMessage(hwndO,WM_COMMAND,IDOK,0);
  if(lpParam == NULL) return ret_W; // useless use lpParam to silence warning
  return ret_W;
}
#else
static VOID Thread(PVOID pvoid)
{
  INTERRUPT=FALSE;
  ret_W=(DWORD)optim(pmv_W, Samples_W, esize_W, fix_W, K4_W, vfirst_W, vlast_W,
                     use_Qf1av_W, weight_W, algoi_W, algoc1_W, algoc2_W, Verbose_W);
  if(pvoid) printf("Thread argument non-zero?\n"); // useless use pvoid to silence warning
  SendMessage(hwndO,WM_COMMAND,IDOK,0);
}
#endif
static LRESULT CALLBACK OptDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch(msg)
  {
    case WM_INITDIALOG:
      if(lParam)break; // to prevent "unused" warning
      break;
    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case IDC_BUTTONSTART:
          hwndTijd= GetDlgItem(hwnd, IDC_PROGRESS1);
          hwndO= hwnd;
#ifdef CREATETHREAD
          CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)Thread,&lParam,0,NULL);
#else
          _beginthread(Thread,0,NULL);
#endif
          break;
        case IDC_BUTTONSTOP:
        case IDCANCEL:
          INTERRUPT=TRUE;
          break;
        case IDOK:
          SendMessage(GetParent(hwnd), WM_CLOSE, 0, 0);
          break;
      }
      break;
    default:
      return FALSE;
  }
  return TRUE;
}

/*=================== WndProc ======================*/
static LRESULT CALLBACK WndProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  HDC         hdc ;
  PAINTSTRUCT ps ;
  RECT        rect ;
  INITCOMMONCONTROLSEX icex;
  HWND hwndButton;
  RECT Rect;

  switch (message)
  {
    case WM_CREATE:

      icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
      icex.dwICC  = ICC_DATE_CLASSES | ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS;
      InitCommonControlsEx(&icex);

      return 0;
    case WM_COMMAND:
      switch(LOWORD(wParam))
      {
        case IDM_OPTIMALISEREN:
        {
          HRSRC hResLoad;        // handle to loaded resource
          HANDLE hDLL=NULL;      // handle to existing .exe or .dll file
          HRSRC hRes;            // handle/ptr. to res. info. in hDLL
          char *lpResLock;       // pointer to resource data
// there was some strange stuff here. Loading the library you are building?

// Locate the dialog box resource in the .EXE file.
          hRes = FindResource(hDLL, MAKEINTRESOURCE(IDD_DIALOG_PROGRESS), RT_DIALOG);
          if (hRes == NULL) { reportlasterror("FindResource"); }

// Load the dialog box into global memory.
          hResLoad = LoadResource(hDLL, hRes);
          if (hResLoad == NULL) { reportlasterror("LoadResource"); }

// Lock the dialog box into global memory.
          lpResLock = LockResource(hResLoad);
          if (lpResLock == NULL) { reportlasterror("LockResource"); }
          hwndO = CreateDialogIndirect(GetModuleHandle(NULL),
            (DLGTEMPLATE *)lpResLock, hwnd, (DLGPROC)OptDlgProc);
          if(hwndO == NULL)reportlasterror("CreateDialog call in WndProc");
          SendMessage(hwndO, WM_SETICON, ICON_SMALL,
            (LPARAM)LoadImage(GetModuleHandle(NULL),
            MAKEINTRESOURCE(IDI_ICONSM),IMAGE_ICON,0,0,LR_DEFAULTSIZE));
          hwndButton= GetDlgItem(hwndO, IDC_BUTTONSTART);
          ShowWindow(hwndButton,SW_HIDE);
          hwndTijd= GetDlgItem(hwndO, IDC_PROGRESS1);
          SendMessage(hwndTijd, PBM_SETBARCOLOR,0,RGB(0x18,0xE8,0x68));
          GetWindowRect(hwndO,&Rect);
          MoveWindow(GetParent(hwndO), Rect.left, Rect.top,
            1,1, TRUE);
          ShowWindow(hwndO, SW_SHOW);
          SetForegroundWindow(hwndO);
          SendMessage(hwndO,WM_COMMAND,IDC_BUTTONSTART,0);
          GetWindowText(hwnd,szCaption,64);
          SetWindowText(hwndO,szCaption);
        }
        break;
      }
      break;
    case WM_PAINT:
      hdc = BeginPaint (hwnd, &ps) ;
      GetClientRect (hwnd, &rect) ;
      DrawText (hdc, TEXT (szVersion), -1, &rect, DT_SINGLELINE | DT_CENTER | DT_VCENTER) ;
      EndPaint (hwnd, &ps) ;
      return 0 ;

    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;
    case WM_DESTROY:
      PostQuitMessage (0) ;
      return 0 ;
  }
  return DefWindowProc (hwnd, message, wParam, lParam) ;
}

/************************* optim_W  ********************************/
int optim_W(mov_info_t *pmv, int Samples, int esize, int fix[], int K4, int vfirst, int vlast,
            int use_Qf1av, double weight, int algoi, double algoc1, double algoc2, int Verbose, char *Label)
/*
 * pmv:       Pointer to movement info structure that contains this, along with more:
 * - P1:      Number of pairs (including vacant pair, if any)
 * - G:       Number of elements in movement (tables x rounds)
 * - r:       Number of rounds
 * - b:       Number of boardsets (according to header of movement file)
 * - Vacant:  if non-zero: a pair number that is vacant
 * - movement: (input / output)
 *            array of size G, where each element is a structure of
 *            3 integers: North, South, Boardset
 *            Boardsets may be given as numbers from 1 .. 255,
 *            or as ASCII codes
 *            For non-used tables give 3 zeroes
 * Samples:   Number of iterations
 * esize:     Number of states in ensemble (developing more or less in parallel)
 * fix:       array of G integers, 0 or 1, where 1 indicates that
 *            the corresponding element should be kept fixed
 * K4:        non-zero if 4th moment optimization is wanted
 * vfirst, vlast: range of preferred vacant pair numbers
 * use_Qf1av: non-zero if Qf1av is wanted as part of the weight function
 * weight:    weight to be assigned to Qf1max relative to Qf1av
 * algoi:     algorithm index (0 = fast fluctuating, 1/2 = slow exp/adaptive cooling)
 * algoc1:    algorithm const1 (initial temperature)
 * algoc2:    algorithm const2 (near-final/max temperature for algoi<2, else speed)
 * Verbose:   verbosity flags (see Verbose.h for interpretation)
 * Label:     zero-terminated string used in the caption of the
 *            progress window
 * return value: nBest on normal termination (positive number), -1 on error
 */
{
  HWND     hwnd;
  MSG      msg;
  WNDCLASSEX wndclass;
  BOOL     bRet;

  pmv_W=     pmv;
  Samples_W= Samples;
  esize_W=   esize;
  fix_W= fix;
  K4_W= K4;
  vfirst_W= vfirst;
  vlast_W=  vlast;
  use_Qf1av_W= use_Qf1av;
  weight_W= weight;
  algoi_W=  algoi;
  algoc1_W= algoc1;
  algoc2_W= algoc2;
  Verbose_W=Verbose;

  wndclass.cbSize        = sizeof(WNDCLASSEX);
  wndclass.style         = CS_HREDRAW | CS_VREDRAW;
  wndclass.lpfnWndProc   = WndProc;
  wndclass.cbClsExtra    = 0;
  wndclass.cbWndExtra    = 0;
  wndclass.hInstance     = GetModuleHandle(NULL);
  wndclass.hCursor       = LoadCursor (NULL, IDC_ARROW);
  wndclass.hbrBackground = CreateSolidBrush(colorBkd);
  wndclass.lpszMenuName  = NULL;
  wndclass.lpszClassName = szAppName;
  wndclass.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICONSM));
  wndclass.hIconSm = (HICON)LoadImage(GetModuleHandle(NULL),
    MAKEINTRESOURCE(IDI_ICONSM), IMAGE_ICON, 16, 16, 0);

  if (!RegisterClassEx (&wndclass))
  {
    MessageBox (NULL, TEXT ("Window Registration Failed"),
      szAppName, MB_ICONERROR);
    ret_W= -1;
    return (int)ret_W;
  }
  snprintf(szCaption,63," balans %s %s",szVersion, Label);
  hwnd = CreateWindow (szAppName, szCaption,
    WS_CAPTION | WS_SYSMENU,
    CW_USEDEFAULT, CW_USEDEFAULT,
    453,110,
    NULL, NULL, GetModuleHandle(NULL), NULL);
  ShowWindow (hwnd, SW_HIDE);
  UpdateWindow (hwnd);
  PostMessage(hwnd, WM_COMMAND, IDM_OPTIMALISEREN, 0);

  while ((bRet = GetMessage (&msg, NULL, 0, 0)))
  {
    if(bRet == -1)
    {
      reportlasterror("GetMessage");
    }
    else
    {
      TranslateMessage (&msg);
      DispatchMessage (&msg);
    }
  }
  return (int)ret_W;
}
#endif                           /* MS_DOS */

/************************* optim **********************************/
int optim(mov_info_t *pmv, int Samples, int esize, int fix[], int K4, int vfirst, int vlast,
          int use_Qf1av, double weight, int algoi, double algoc1, double algoc2, int Verbose)
/*
 * Similar to optim_W, but does not display a progress window
 * For a description of the parameters, see optim_W
 *
 */
{
  int Vacant = pmv->Vacant, t1 = pmv->t1, r = pmv->r;
  int k;
  int b, t0=0, nBest;
  int schema[3][t1][r];

  static int initdone= 0;
#ifdef MS_DOS
  Samples_W = Samples;
#endif                         /* MS_DOS */

  if(!initdone)
  {
    initf();
#ifdef BUILD_DLL
    raninit();
#endif                       /* BUILD_DLL */
    initdone=1;
  }

/* Convert the "movement" structure to "schema" */
  k=0;
  for(int i=0; i<r; i++)
  {
    int t=0;
    for(int j=0; j<t1; j++)
    {
      schema[1][j][i]=pmv->movement[k].noordzuid[0];
      schema[2][j][i]=pmv->movement[k].noordzuid[1];
      if(pmv->movement[k].noordzuid[0]==0)
      {
        schema[0][j][i]=0;
      }
      else
      {
        b=orderb(pmv, pmv->movement[k].bord);
        if(b >= pmv->b)
        {
          fprintf(stderr, "ERROR: can't optimize this movement, more boardsets than specified\n");
#ifdef MS_DOS
          MessageBox(NULL,
            " can't optimize this movement, more boardsets than specified\n",
            szCaption,MB_ICONWARNING);
#endif
          pmv->Status |= Invaliddat;
          return -1;
        }
        schema[0][j][i]=1+b;
        if(pmv->movement[k].noordzuid[0]!=Vacant &&
          pmv->movement[k].noordzuid[1]!=Vacant) t++;
      }

      k++;
    }
    if(i==0) t0=t;
    else
    {
      if(t0 != t)
      {
        fprintf(stderr, "ERROR: can't optimize this movement, different number of tables in different rounds\n");
#ifdef MS_DOS
        MessageBox(NULL,
          "Can't optimize this movement, different number of tables in different rounds\n",
          szCaption,MB_ICONINFORMATION);
#endif
        pmv->Status |= Unsuitable;

        return -1;
      }
    }
  }

  if(P_misc)
  {
    printf("\nTrying to Improve the Balance %s\n", szVersion);

    if(algoi == 0)
      printf("algo 0: fast temperature fluctuations between T=%.7g and T=%.7g\n", algoc1, algoc2);
    else if(algoi == 1)
      printf("algo 1: slow exponential %s from T=%.7g to T=%.7g (+ T=0 in last 2 iter)\n",
             (algoc1 > algoc2 ? "cooling" : "heating"), algoc1, algoc2);
    else
      printf("algo 2: slow %s from T=%.7g at approximated constant thermodynamic speed=%.7g\n",
             (algoc2 >= 0.0 ? "cooling" : "heating"), algoc1, algoc2);

    if(weight == 0 && !use_Qf1av)
    {
      printf("Qf1max and Qf1av not optimized\n");
    }
    else
    {
      printf("range for Qf1max: %d - %d\n",vfirst, vlast);
      if(use_Qf1av)
        printf("relative weight of Qf1max: %10.4g\n", weight);
      else
        printf("Qf1av not optimized\n");
    }
  }
  fflush(stdout);
  nBest = improve2(pmv->b,r,t1,pmv->P1,Samples,esize,Vacant,schema,fix,K4,
                   vfirst-1,vlast-1,use_Qf1av,weight,algoi,algoc1,algoc2, Verbose);
  if(nBest < 0)
    return nBest; // Don't bother converting "schema" structure in case of error

/* Convert the "schema" structure back to "movement" */
  k=0;
  for(int i=0; i<r; i++)
    for(int j=0; j<t1; j++)
  {
    pmv->movement[k].noordzuid[0]=schema[1][j][i];
    pmv->movement[k].noordzuid[1]=schema[2][j][i];
    k++;
  }
  return nBest;
}

/* showprogress2 reports the current state during the iterations */
void showprogress2(int iter, double T, double Qf, double Qf1av, double Qf1max, int bestV, double d4,
                   int nebest, int esize, int opt_busy, int Verbose)
{
  const int bufsiz=72; // Large enough to silence gcc 9 warning about possible snprintf truncation
  static int first=1;
  static int first2=1;
  char head[64], buf[bufsiz];
#ifdef MS_DOS
  static HWND hwndB;
  Verbose = Verbose & 0xffffffff; // Silence -Wunused-parameter and -Wself-assign; we _do_ use it when not MS_DOS via P_progress
#else
  if(! P_progress) return;
#endif
  if(first)  // once only
  {
    snprintf(head,63, "%7s %6s %6s %7s %6s %5s %5s %5s",
             "iter","T","Qf","Qf1av","Qf1max","pair","d4",(esize>1 ? "nBest" : ""));
  }
  else if(first2 && !opt_busy)
  {
      snprintf(head,63, "%7s %6s %6s %7s %6s %5s %5s",
               "#","T","Qf","Qf1av","Qf1max","pair","d4");
  }
  // if(isnan(Qf1max))  // In >7.4.2 we avoid NaN and Inf to allow safe compilation with fast-math
  if(Qf1max < 0)
  {
    if(esize > 1)
      snprintf(buf,bufsiz-1, "%7d %6.1f %6.2f     -     -     -   %6.3f %2d/%-4d",
               iter,T,Qf, d4, nebest, esize);
    else
      snprintf(buf,bufsiz-1, "%7d %6.1f %6.2f     -     -     -   %6.3f",
               iter,T,Qf, d4);
  }
  else
  {
    if(esize > 1)
      snprintf(buf,bufsiz-1, "%7d %6.1f %6.2f %7.3f %6.2f %4d %6.3f %2d/%-4d",
               iter, T, Qf, Qf1av, Qf1max, bestV+1, d4, nebest, esize);
    else
      snprintf(buf,bufsiz-1, "%7d %6.1f %6.2f %7.3f %6.2f %4d %6.3f",
               iter, T, Qf, Qf1av, Qf1max, bestV+1, d4);
  }

  if(first || ( first2 && !opt_busy)) // write header
  {
#ifdef MS_DOS
    if(opt_busy) // write header to window
    {
      hwndB= GetDlgItem(hwndO, IDC_BALANS1);
      SendMessage(hwndB, LB_RESETCONTENT, 0, 0);
      SendMessage(hwndB, LB_ADDSTRING, 0, (LPARAM)head);
    }
    else puts(head); // write header to console terminal
#else
    puts(head);
#endif
    first=0;
    if(!opt_busy) first2=0;
  }
#ifdef MS_DOS
  if(opt_busy) // write progress status to window
  {
    SendMessage(hwndB, LB_DELETESTRING, 1, 0);
    SendMessage(hwndB, LB_ADDSTRING, 0, (LPARAM)buf);
    SendMessage(hwndTijd,PBM_SETPOS,(WPARAM)(iter*100/Samples_W),0);
    InvalidateRect(hwndO,NULL,TRUE);
  }
  else
  {
    puts(buf); // write survey to console terminal
    fflush(stdout);
  }
#else
  puts(buf); // always write to console terminal
  fflush(stdout);
#endif
}
