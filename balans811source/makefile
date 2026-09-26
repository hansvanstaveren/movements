# makefile for MSYS2, MINGW64, MINGW32, CLANG64, UCRT64 and old MinGW
# To create only the distributed exe files give command 'make dist'
BALANSDIR= $(PWD)
RANDOM= $(BALANSDIR)/random
SFMT= $(RANDOM)/SFMT
MAKEFILE = makefile

RANDOMFLAGS = -DSFMT_MEXP=19937 -I$(SFMT)

# These only used if MAKE_MVER defined.  Assumed to benefit most users without hurting anyone too much.
#   haswell-tuning may be a bit better for znver3 but hurts Intel Skylake family CPUs more than it benefits AMD.
AVX2TUNE = -mtune=skylake
AVXTUNE = -mtune=sandybridge

# Machine/architecture dependent C flags (global default empty since 32-bit don't have SSE2 but all 64-bit do).
ifdef MAKE_MVER   # Makefile-based multiversioning
# Note that for CLANG64 we change this default below.  This first default is for multiversioning with gcc.
MFLAGS = -DUSE_MVER -DHAVE_SSE2
else
#   You don't need to edit here to change.  Just override on command line with "make MFLAGS='<your-flags>'".
MFLAGS =
# If you plan to run only on the same 64-bit pc that you build on, then as a minimum these are safe:
#MFLAGS = -march=native -DHAVE_SSE2
# -DUSE_FMA_LOGF speeds up if your arch has hardware-based fused multiply-add (FMA) or you use CLANG64:
#MFLAGS = -march=skylake -DUSE_FMA_LOGF -DHAVE_SSE2
endif

ifeq  ($(MSYSTEM),CLANG64)
# Enable multiversioning, meant to speed up on modern CPUs but still run correctly on any 64-bit.
#   Certainly works and speeds up with gcc too, not only clang, but for now only make it default for clang.
MFLAGS = -DUSE_MVER -DUSE_FMA_LOGF -DHAVE_SSE2
MAKE_MVER = 1
endif

OPT = -O3
DFLAGS = $(OPT) -ffast-math -Wall -Wextra -Wshadow -ggdb -DQPC
#DFLAGS = -ftrapv -Wall -Wextra -Wshadow -g -m32 -mwin32 -DQPC

BFLAGS = -D_FORTIFY_SOURCE=2 \
-fstack-protector \
-fasynchronous-unwind-tables \
-fexceptions \
-fstack-protector-strong \
-fno-omit-frame-pointer
## -fstack-clash-protection removed
# define SAN if address and undefined behaviour sanitizing is wanted. Only works in CLANG64 shell (Feb. 2023)
ifdef SAN
BFLAGS += -fsanitize=address -fsanitize=undefined
endif

WINFLAGS = -DMS_DOS -mthreads
ifeq  ($(MSYSTEM),MSYS)
 WINFLAGS = -DMS_DOS -mthreads -DCREATETHREAD
endif
ifeq  ($(MSYSTEM),UCRT64)
 WINFLAGS = -DMS_DOS -mthreads -DCREATETHREAD
endif
# clang doesn't need -mthreads and warns about it if used for building object files
ifeq  ($(MSYSTEM),CLANG64)
 WINFLAGS = -DMS_DOS
endif

OMPFLAGS = -fopenmp
CFLAGS = $(MFLAGS) $(DFLAGS) $(BFLAGS) $(RANDOMFLAGS) -D_MINGW

DLLFLAGS = -DUSE_DLL

GETOPT=XGetopt.c
GETOPTFLAG = -DXGETOPT

#LFLAGS =
LFLAGS = -static
ifeq  ($(MSYSTEM),CLANG64)
# work-around for missing EnumProcessModules
LFLAGS = -static -lpsapi
endif
LFLAGSCONS = $(LFLAGS) -Wl,--subsystem,console -mwindows
LFLAGSGUI = $(LFLAGS) -mwindows -lcomctl32 -lcomdlg32

ifeq  ($(MSYSTEM),CLANG64)
STACK= -Wl,/stack:
else
STACK= -Wl,--stack=
endif
STACKBAL = $(STACK)40000000
STACKSC2 = $(STACK)1000000000
NOWHOLEARCH = -Wl,--no-whole-archive

RANDOMSRC  = $(RANDOM)/raninit.c $(SFMT)/SFMT.c
RANDOMHEAD = $(RANDOM)/raninit.h $(RANDOM)/logf_def.h $(SFMT)/SFMT.h $(SFMT)/SFMT-common.h \
	     $(SFMT)/SFMT-params.h $(SFMT)/SFMT-params19937.h $(SFMT)/SFMT-sse2.h

RANLOGFSRC = $(RANDOM)/ranlogftest.c $(RANDOMSRC)

READMOVESRC = readmove.c ugets.c

OPTIMH  = optim.h Verbose.h
READMVIH = readmove.h orderb.h readmvi.h
BALANSH = balans.h Verbose.h $(READMVIH) $(RANDOMHEAD)
COMMONH = common.h inspect.h $(BALANSH)

MVISRC = common.c inspect.c readmvi.c orderb.c

IMPROVE2DEPS = improve2.c $(RANDOMHEAD) $(OPTIMH)

ifdef MAKE_MVER   # Makefile-based multiversioning
IMPROVE2FILES = improve2_avx2.o improve2_avx.o improve2_default.o
else
IMPROVE2FILES = improve2.c
endif

OPTIMFILES0 = optim.c $(IMPROVE2FILES) balans1.c initf.c
OPTIMHEAD = $(BALANSH) optim.h resource.h

LIBS =

# WXFLAG, must be set for all 64-bit compilers
WXFLAG = -D WX_CPU_AMD64
ifeq ($(MSYSTEM),MINGW32)
  WXFLAG =
endif

MINGWPREFIX =
#
### E.g. for cross-compiling from Linux to 64- resp. 32-bit Windows binaries:
# MINGWPREFIX = x86_64-w64-mingw32-
# MINGWPREFIX = i686-w64-mingw32-
#
CC = $(MINGWPREFIX)gcc
WINDRES = $(MINGWPREFIX)windres
ifeq  ($(MSYSTEM),CLANG64)
CC = clang
# CLANG64 strip does not support -p
STRIP = $(MINGWPREFIX)strip
else
STRIP = $(MINGWPREFIX)strip -p
endif

# General rule not normally used by this Windows makefile but may be useful for some testing cases
.c.o:
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $<

# different compilation options (dll/.so options only for testing, not distrib):
# balans: everything in one binary, no dll needed
# balans2: separate main program and dll, with explicit C code to load & use it
# balans3: simplest version, no MSWindows stuff
# balans4: like balans3, but called as subprogram on list of input files
# balans5: optim routines as a .so library instead of dll, loading done by OS
# balans6: like balans2 but without progress window
# balans7: like balans5 but linking to dll via .a import lib (loading via OS)

BASICFILES   = $(MVISRC) $(READMOVESRC) $(GETOPT)
MAINFILES    = balans.c $(BASICFILES)
OPTIMFILES   = $(OPTIMFILES0) $(RANDOMSRC)
WINFILES     = $(MAINFILES) resource.o
DLLFILES     = $(OPTIMFILES) baloptim.def orderb.c dllmain.c
BALANSFILES  = $(WINFILES) $(OPTIMFILES)
BALANS2FILES = $(WINFILES) $(RANDOMSRC)
BALANS3FILES = $(MAINFILES) $(OPTIMFILES)
BALANS4FILES = balans4.c $(BALANS3FILES)
BALANS5FILES = $(BALANS2FILES)
BALANS6FILES = $(MAINFILES) $(RANDOMSRC)
BALANS7FILES = $(BALANS2FILES)
VERNUMFILES  = vernum.c $(BASICFILES) balans1.c sorttables.c $(RANDOMSRC)
SCORE2FILES  = score2.c $(BASICFILES) $(RANDOMSRC)
DEMOFILES    = demo.c $(BASICFILES) resource.o $(RANDOMSRC)

PROGS?=balans3.exe balans.exe vernum.exe score2.exe pairs.exe \
      balans4.exe balans2.exe balans5.exe balans6.exe balans7.exe
XPROGS=demo.exe btest.exe readmovecheck.exe ranlogftest.exe

# Declare the non-exe targets .PHONY to disable built-in rules that would otherwise
# create havoc for all targets FOO with a corresponding FOO.c, e.g. 'balans' & 'vernum'.
.PHONY: all xall $(PROGS:.exe=) $(XPROGS:.exe=) mostlyclean clean strip

all: $(PROGS)

xall: all $(XPROGS)

dist: balans3.exe balans.exe vernum.exe score2.exe pairs.exe

###############
# Makefile-based multiversioning works well for clang as well as gcc (attribute-based doesn't for clang w/ omp).
#   MFLAGS must still contain -DUSE_MVER since we need the multiversioning support in raninit.[ch] too.
#
improve2_avx2.o: $(IMPROVE2DEPS)
	$(CC) $(CPPFLAGS) $(WINFLAGS) $(CFLAGS) $(OMPFLAGS) -DMVER_AVX2 -mavx2 -mfma $(AVX2TUNE) -c improve2.c -o $@

improve2_avx.o: $(IMPROVE2DEPS)
	$(CC) $(CPPFLAGS) $(WINFLAGS) $(CFLAGS) $(OMPFLAGS) -DMVER_AVX -mavx $(AVXTUNE) -c improve2.c -o $@

improve2_default.o: $(IMPROVE2DEPS)
	$(CC) $(CPPFLAGS) $(WINFLAGS) $(CFLAGS) $(OMPFLAGS) -DMVER_DEFAULT -c improve2.c -o $@

###############

resource.o: balans.rc resource.h
	$(WINDRES) $(WXFLAG) balans.rc -o $@

libbaloptim.so: $(DLLFILES) $(OPTIMHEAD) $(MAKEFILE)
	$(CC) $(CPPFLAGS) $(WINFLAGS) $(CFLAGS) $(OMPFLAGS) -shared $(DLLFILES) \
	 $(LFLAGSGUI) -o $@

# Import library for linking to the dll, generated by the --out-implib at DLL build time
libbaloptim.a: baloptim.dll

baloptim.dll: $(DLLFILES) $(OPTIMHEAD) $(MAKEFILE)
	$(CC) $(CPPFLAGS) $(WINFLAGS) $(CFLAGS) $(OMPFLAGS) -DBUILD_DLL -shared $(DLLFILES) \
	-Wl,--out-implib,libbaloptim.a \
	-Wl,--output-def,baloptim.def \
	$(NOWHOLEARCH) $(LFLAGSGUI) -o $@

balans: balans.exe
balans.exe: $(BALANSFILES) $(COMMONH) $(OPTIMHEAD) $(MAKEFILE)
	$(CC) $(CPPFLAGS) $(WINFLAGS) $(CFLAGS) $(OMPFLAGS) $(GETOPTFLAG) $(BALANSFILES) \
	$(STACKBAL) $(LIBS) $(LFLAGSGUI) $(LFLAGSCONS) -o $@

balans2: balans2.exe
balans2.exe: $(BALANS2FILES) $(COMMONH) $(MAKEFILE) baloptim.dll
	$(CC) $(CPPFLAGS) $(WINFLAGS) $(CFLAGS) $(OMPFLAGS) $(GETOPTFLAG) $(DLLFLAGS) $(BALANS2FILES) \
	$(STACKBAL) $(LIBS) $(LFLAGSCONS) -o $@

balans3: balans3.exe
balans3.exe: $(BALANS3FILES) $(COMMONH) optim.h $(MAKEFILE)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(OMPFLAGS) $(GETOPTFLAG) $(BALANS3FILES) \
	$(STACKBAL) -DQPC $(LIBS) $(LFLAGSCONS) -o $@

balans4: balans4.exe
balans4.exe: $(BALANS4FILES) $(COMMONH) optim.h $(MAKEFILE)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DSUBPROGRAM $(OMPFLAGS) $(GETOPTFLAG) $(BALANS4FILES) \
	$(STACKBAL) -DQPC $(LIBS) $(LFLAGSCONS) -o $@

balans5: balans5.exe
balans5.exe: $(BALANS5FILES) $(COMMONH) $(MAKEFILE) libbaloptim.so
	$(CC) $(CPPFLAGS) $(WINFLAGS) $(CFLAGS) $(OMPFLAGS) $(GETOPTFLAG) $(BALANS5FILES) \
	libbaloptim.so \
	$(STACKBAL) $(LIBS) $(LFLAGSCONS) -o $@

balans6: balans6.exe
balans6.exe: $(BALANS3FILES) $(COMMONH) baloptim.dll $(MAKEFILE)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(OMPFLAGS) $(GETOPTFLAG) $(DLLFLAGS) $(BALANS6FILES) \
	$(STACKBAL) -DQPC $(LIBS) $(LFLAGSCONS) -o $@

balans7: balans7.exe
balans7.exe: $(BALANS7FILES) $(COMMONH) $(MAKEFILE) libbaloptim.a
	$(CC) $(CPPFLAGS) $(WINFLAGS) $(CFLAGS) $(OMPFLAGS) $(GETOPTFLAG) $(BALANS7FILES) \
	libbaloptim.a \
	$(STACKBAL) $(LIBS) $(LFLAGSCONS) -o $@

vernum: vernum.exe
vernum.exe: $(VERNUMFILES) $(COMMONH) sorttables.h $(MAKEFILE)
	$(CC) $(CFLAGS) $(GETOPTFLAG) $(VERNUMFILES) $(LIBS) -o $@

score2: score2.exe

score2.exe: $(SCORE2FILES) $(COMMONH) $(MAKEFILE)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GETOPTFLAG) $(SCORE2FILES) \
	$(STACKSC2) $(LIBS) -o $@

pairs: pairs.exe
pairs.exe: $(READMOVESRC) readmove.h FQf.c $(MAKEFILE)
	$(CC) $(CFLAGS) -Dpairs $(READMOVESRC) FQf.c $(LFLAGSCONS) -o $@

demo: demo.exe
demo.exe: $(DEMOFILES) $(COMMONH) $(MAKEFILE) baloptim.dll
	$(CC) $(CFLAGS) $(WINFLAGS) $(OMPFLAGS) -DUSE_DLL $(DEMOFILES) \
	$(LIBS) $(LFLAGSCONS) -o $@

readmovecheck: readmovecheck.exe
readmovecheck.exe: $(READMOVESRC) readmvi.c $(READMVIH) $(MAKEFILE)
	$(CC) $(CFLAGS) -Dstandalone $(READMOVESRC) $(LFLAGSCONS) -o $@

btest: btest.exe
btest.exe: test.c $(READMOVESRC) readmvi.c $(READMVIH) $(MAKEFILE)
	$(CC) $(CFLAGS) test.c $(READMOVESRC) readmvi.c $(LFLAGSCONS) -o $@

ranlogftest: ranlogftest.exe
ranlogftest.exe: $(RANLOGFSRC) $(RANDOMHEAD) $(MAKEFILE)
	$(CC) $(CFLAGS) $(RANLOGFSRC) $(LFLAGSCONS) -o $@

mostlyclean:
	$(RM) *.o klad* *.a *.so *.dll _balans.txt

clean: mostlyclean
	$(RM) $(PROGS) $(XPROGS)

strip:
	$(STRIP) *.exe *.dll *.so
