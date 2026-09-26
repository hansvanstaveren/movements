/* WARNING: The FMA-based logf is horribly slow unless hw support available, e.g. via -march=skylake */

#include <math.h>

#ifndef USE_FMA_LOGF
#define LOGF(x) logf(x)
#else
#define LOGF(x) logf_fma(x)

#include <stdint.h>

/* natural log on [0x1.f7a5ecp-127, 0x1.fffffep127]. Maximum relative error 9.4529e-5 */
/* From https://stackoverflow.com/questions/39821367/very-fast-logarithm-natural-log-function-in-c
 * with the float<->int conversions implemented via type punning (ok without -fno-strict-aliasing in gcc).
 *
 * Mostly useful for compilation with clang, MinGW without (a recent) glibc and/or for accelerator hw
 * (e.g. nVidia GPU) because a similarly fast vectorisable/vectorised version isn't easily available
 * in those cases.
 * When using gcc 9.1.1 with glibc 2.29 on Skylake (tested on Intel Core i5-E8400, 6 cores), the default
 * logf(3) is vectorised and actually even faster than the below version for single-threaded use;
 * but for 6-threaded OpenMP use, gcc generates the fastest result by using the below inlined version.
 */
static inline float logf_fma (float a)
{
    union { int32_t i; float f; } uni;
    float m, r, s, t, i, f;
    int32_t e;

    uni.f = a;
    uni.i -= (e = (uni.i - 0x3f2aaaab) & 0xff800000);
    m = uni.f;
    i = (float)e * 1.19209290e-7f; // 0x1.0p-23
    /* m in [2/3, 4/3] */
    f = m - 1.0f;
    s = f * f;
    /* Compute log1p(f) for f in [-1/3, 1/3] */
    r = fmaf (0.230836749f, f, -0.279208571f); // 0x1.d8c0f0p-3, -0x1.1de8dap-2
    t = fmaf (0.331826031f, f, -0.498910338f); // 0x1.53ca34p-2, -0x1.fee25ap-2
    r = fmaf (r, s, t);
    r = fmaf (r, s, f);
    r = fmaf (i, 0.693147182f, r); // 0x1.62e430p-1 // log(2)
    return r;
}
#endif /* Use FMA-based approximate logf */
