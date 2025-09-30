//############################################################################
//##                                                                        ##
//##  MATH_A.C                                                              ##
//##                                                                        ##
//##  Miles Sound System                                                    ##
//##                                                                        ##
//##  IMDCT and polyphase-filter support routines converted from PC Asm     ##
//##                                                                        ##
//##  Version 1.00 of 2-Mar-99: Initial, derived from algorithms by         ##
//##                            Byeong Gi Lee, Jeff Tsay, Francois          ##
//##                            Raymond-Boyer, K. Konstantinides, Mikko     ##
//##                            Tommila, et al.                             ##
//##  Version 1.01 of 2-Jul-01: Converted to PPC assembly                   ##
//##                                                                        ##
//##  Author: Jeff Roberts                                                  ##
//##                                                                        ##
//############################################################################
//##                                                                        ##
//##  Copyright (C) RAD Game Tools, Inc.                                    ##
//##                                                                        ##
//##  Contact RAD Game Tools at 425-893-4300 for technical support.         ##
//##                                                                        ##
//############################################################################

#include "mss.h"
#include "imssapi.h"
#include "mp3decode.h"

#define ASMSTACKSIZE 384

static F32 MP3Constants[] = {

 #define fconst( val ) val##f,

  #define sin30 0
  #define cos30 1

  #define SQRT3 2
  #define SQRT2D2 3
  #define SQRT6PSQRT2D2 4
  #define SQRT6MSQRT2D2 5

  #define dct3t4 6
  #define dct3t5 7
  #define dct3t6 8
  #define dct3t7 9
  #define dct3t8 10
  #define dct3t9 11
  #define dct3t10 12
  #define dct3t11 13
  #define dct3t12 14
  #define dct3t13 15
  #define dct3t14 16
  #define dct3t15 17
  #define dct3t17 18
  #define dct3t18 19
  #define dct3t19 20
  #define dct3t20 21
  #define dct3t21 22
  #define dct3t22 23

  #define dct9t0 24
  #define dct9t1 25
  #define dct9t2 26
  #define dct9t10 27
  #define dct9t11 28
  #define dct9t12 29

  #define dct36t0 30
  #define dct36t1 31
  #define dct36t2 32

  #define dct36t4 33
  #define dct36t5 34
  #define dct36t6 35
  #define dct36t7 36
  #define dct36t8 37
  #define dct36t9 38
  #define dct36t10 39
  #define dct36t11 40
  #define dct36t12 41
  #define dct36t13 42
  #define dct36t14 43
  #define dct36t15 44
  #define dct36t16 45
  #define dct36t17 46
  #define dct36t19 47
  #define dct36t20 48
  #define dct36t21 49
  #define dct36t22 50
  #define dct36t23 51
  #define dct36t25 52
  #define dct36t26 53

  fconst( 0.500000000 ) // sin(pi/6)
  fconst( 0.866025403 ) // cos(pi/6)

  fconst( 1.732050808 )
  fconst( 0.707106781 )
  fconst( 1.931851653 )
  fconst( 0.517638090 ) 

  fconst( 0.504314480 )
  fconst( 0.541196100 )
  fconst( 0.630236207 )
  fconst( 0.821339815 )
  fconst( 1.306562965 )
  fconst( 3.830648788 )
  fconst( -0.793353340 )
  fconst( -0.608761429 )
  fconst( -0.923879532 )
  fconst( -0.382683432 )
  fconst( -0.991444861 )
  fconst( -0.130526192 )
  fconst( 0.382683432 )
  fconst( 0.608761429 )
  fconst( -0.793353340 )
  fconst( -0.923879532 )
  fconst( -0.991444861 )
  fconst( 0.130526192 )

  fconst( 1.8793852415718 )
  fconst( 1.532088886238 )
  fconst( 0.34729635533386 )
  fconst( 1.9696155060244 )
  fconst( 1.2855752193731 )
  fconst( 0.68404028665134 )

  fconst( 0.501909918 )
  fconst( -0.250238171 )
  fconst( -5.731396405 )

  fconst( -0.25215724 )
  fconst( -1.915324394 )
  fconst( 0.551688959 )
  fconst( -0.256069878 )
  fconst( -1.155056579 )
  fconst( 0.610387294 )
  fconst( -0.262132281 )
  fconst( -0.831377381 )
  fconst( 0.871723397 )
  fconst( -0.281845486 )
  fconst( -0.541420142 )
  fconst( 1.183100792 )
  fconst( -0.296422261 )
  fconst( -0.465289749 )
  fconst( -0.315118103 )
  fconst( -0.410669907 )
  fconst( 5.736856623 )
  fconst( -0.339085426 )
  fconst( -0.370046808 )
  fconst( -0.541196100 )
  fconst( -1.306562965 )

};

//#############################################################################
//##                                                                         ##
//## 1x36 IMDCT for window types 0, 1, and 3 (long windows)                  ##
//##                                                                         ##
//#############################################################################
extern "C"
void 
AILCALL 
Scalar_IMDCT_1x36(
    register F32 *input2,
    register S32  sb2,
    register F32 *result2,
    register F32 *save2,
    register F32 *window2
    )
{
    register F32* srcp = input2;
    register S32 sbp = sb2;
    register F32* lastp = save2;
    register F32* windp = window2;
    register F32* destp = result2;

    float fpregs[32];

#define float_load(reg, ofs, ptr) fpregs[reg] = ptr[ofs/4]
#define float_add(dst, a, b) fpregs[dst] = fpregs[a] + fpregs[b]
#define float_store(reg, ofs, ptr) ptr[ofs/4] = fpregs[reg]
#define float_mul(dst, a, b) fpregs[dst] = fpregs[a] * fpregs[b]
#define float_sub(dst, a, b) fpregs[dst] = fpregs[a] - fpregs[b]
#define float_madd(dst, a, c, b) fpregs[dst] = (fpregs[a] * fpregs[c]) + fpregs[b]
#define float_msub(dst, a, c, b) fpregs[dst] = (fpregs[a] * fpregs[c]) - fpregs[b]
#define float_neg(dst, a) fpregs[dst] = -fpregs[a]

    float_load(18, (0*4), (srcp));
    float_load(1, (1*4), (srcp));
    float_load(2, (2*4), (srcp));
    float_load(3, (3*4), (srcp));
    float_load(4, (4*4), (srcp));
    float_load(5, (5*4), (srcp));
    float_load(6, (6*4), (srcp));
    float_load(7, (7*4), (srcp));
    float_load(8, (8*4), (srcp));
    float_load(9, (9*4), (srcp));
    float_load(10, (10*4), (srcp));
    float_load(11, (11*4), (srcp));
    float_load(12, (12*4), (srcp));
    float_load(13, (13*4), (srcp));
    float_load(14, (14*4), (srcp));
    float_load(15, (15*4), (srcp));
    float_load(16, (16*4), (srcp));
    float_load(17, (17*4), (srcp));

    float_add(17, 17, 16);
    float_add(30, 16, 15);
    float_add(15, 15, 14);
    float_add(14, 14, 13);
    float_add(13, 13, 12);
    float_add(28, 12, 11);
    float_add(11, 11, 10);
    float_add(10, 10, 9);
    float_add(9, 9, 8);
    float_add(16, 8, 7);
    float_add(7, 7, 6);
    float_add(6, 6, 5);
    float_add(5, 5, 4);
    float_add(24, 4, 3);
    float_add(3, 3, 2);
    float_add(26, 2, 1);
    float_add(19, 1, 18);

    float_add(31, 17, 15);
    float_add(15, 15, 13);
    float_add(29, 13, 11);
    float_add(11, 11, 9);
    float_add(17, 9, 7);
    float_add(7, 7, 5);
    float_add(25, 5, 3);
    float_add(27, 3, 19);
  
    float_store(6, (0*4), (srcp));
    float_store(7, (1*4), (srcp));
    float_store(10, (2*4), (srcp));
    float_store(11, (3*4), (srcp));
    float_store(14, (4*4), (srcp));
    float_store(15, (5*4), (srcp));

    //
    // do the five pointer
    //

    float_load(10, 4*dct9t0, MP3Constants);
    float_load(11, 4*dct9t1, MP3Constants);
    float_load(12, 4*dct9t2, MP3Constants);
    float_load(15, 4*SQRT2D2, MP3Constants);

    float_add(22, 18, 18);
    float_add(23, 19, 19);
    float_add(20, 28, 22);
    float_add(21, 29, 23);

    float_mul(0, 16, 11);
    float_mul(1, 17, 11);
    float_madd(0, 24, 10, 0);
    float_madd(1, 25, 10, 1);
    float_madd(13, 30, 12, 20);
    float_madd(14, 31, 12, 21);
    float_add(0, 0, 13);
    float_add(1, 1, 14);

    float_add(2, 22, 24);
    float_add(3, 23, 25);
    float_add(13, 28, 30);
    float_add(14, 29, 31);
    float_sub(2, 2, 16);
    float_sub(3, 3, 17);
    float_sub(2, 2, 13);
    float_sub(3, 3, 14);
    float_sub(2, 2, 28);
    float_sub(3, 3, 29);

    float_mul(4, 16, 10);
    float_mul(5, 17, 10);
    float_madd(4, 24, 12, 4);
    float_madd(5, 25, 12, 5);
    float_madd(13, 30, 11, 20);
    float_madd(14, 31, 11, 21);
    float_sub(4, 13, 4);
    float_sub(5, 14, 5);

    float_mul(6, 30, 10);
    float_mul(7, 31, 10);
    float_madd(6, 24, 11, 6);
    float_madd(7, 25, 11, 7);
    float_madd(13, 16, 12, 20);
    float_madd(14, 17, 12, 21);
    float_sub(6, 13, 6);
    float_sub(7, 14, 7);

    float_sub(9, 19, 25);
    float_sub(8, 18, 24);
    float_sub(14, 17, 29);
    float_sub(13, 16, 28);
    float_add(9, 9, 31);
    float_add(8, 8, 30);
    float_add(9, 9, 14);
    float_add(8, 8, 13);
    float_mul(9, 9, 15);

    //
    // do four points
    //

    float_load(18, SQRT3 * 4, MP3Constants);
    float_load(24, (0*4), (srcp));
    float_load(25, (1*4), (srcp));
    float_load(28, (2*4), (srcp));
    float_load(29, (3*4), (srcp));
    float_load(30, (4*4), (srcp));
    float_load(31, (5*4), (srcp));
    float_mul(24, 24, 18);
    float_mul(25, 25, 18);

    float_load(21, dct9t10*4, MP3Constants);
    float_load(22, dct9t11*4, MP3Constants);
    float_load(23, dct9t12*4, MP3Constants);

    float_mul(19, 28, 22);
    float_mul(20, 29, 22);
    float_madd(10, 26, 21, 24);
    float_madd(11, 27, 21, 25);
    float_madd(19, 30, 23, 19);
    float_madd(20, 31, 23, 20);
    float_add(10, 10, 19);
    float_add(11, 11, 20);

    float_sub(12, 26, 28);
    float_sub(13, 27, 29);
    float_sub(12, 12, 30);
    float_sub(13, 13, 31);
    float_mul(12, 12, 18);
    float_mul(13, 13, 18);

    float_mul(19, 28, 23);
    float_mul(20, 29, 23);
    float_msub(14, 26, 22, 24);
    float_msub(15, 27, 22, 25);
    float_msub(19, 30, 21, 19);
    float_msub(20, 31, 21, 20);
    float_add(14, 14, 19);
    float_add(15, 15, 20);

    float_mul(19, 30, 22);
    float_mul(20, 31, 22);
    float_msub(16, 26, 23, 24);
    float_msub(17, 27, 23, 25);
    float_msub(19, 28, 21, 19);
    float_msub(20, 29, 21, 20);
    float_add(16, 16, 19);
    float_add(17, 17, 20);

  // twiddle into same registers but all mixed up
  // 0=0, 1=17, 2=1, 3=16, 4=2, 5=15, 6=3, 7=14, 8=4, 9=13
  // 10=8, 11=9, 12=7, 13=10, 14=6, 15=11, 16=5, 17=12

    #define twiddle( f51, f52, f41, f42, scale1, scale2, lscale1, lscale2, hscale1, hscale2 ) ;\
        float_load(26, 4*scale1, MP3Constants)          ;\
        float_load(27, 4*scale2, MP3Constants)          ;\
        float_load(28, 4*lscale1, MP3Constants)         ;\
        float_load(29, 4*lscale2, MP3Constants)         ;\
        float_load(30, 4*hscale1, MP3Constants)         ;\
        float_load(31, 4*hscale2, MP3Constants)         ;\
        float_add(18, f51, f41      );\
        float_add(19, f52, f42      );\
        float_sub(20, f51, f41      );\
        float_sub(21, f52, f42      );\
        float_mul(19, 19, 26            );\
        float_mul(21, 21, 27            );\
        float_add(f51, 18, 19         );\
        float_sub(f52, 18, 19         );\
        float_add(f41, 20, 21         );\
        float_sub(f42, 20, 21         );\
        float_mul(f51, f51, 28      );\
        float_mul(f52, f52, 30      );\
        float_mul(f41, f41, 29      );\
        float_mul(f42, f42, 31      );

    twiddle( 0, 1, 10, 11, dct36t0, dct36t21, dct36t1, dct36t22, dct36t2, dct36t23 )
    twiddle( 2, 3, 12, 13, SQRT6MSQRT2D2, SQRT6PSQRT2D2, dct36t4, dct36t19, dct36t5, dct36t20 )
    twiddle( 4, 5, 14, 15, dct36t6, dct36t15, dct36t7, dct36t16, dct36t8, dct36t17 )
    twiddle( 6, 7, 16, 17, dct36t9, dct36t12, dct36t10, dct36t13, dct36t11, dct36t14 )

    float_load(20, 4*dct36t25, MP3Constants);
    float_load(21, 4*dct36t26, MP3Constants);
    float_add(18, 8, 9);
    float_sub(19, 8, 9);
    float_mul(8, 18, 20);
    float_mul(9, 19, 21);

    if ((sbp & 1) == 0) goto __even_window;

    // 0 to 3
    float_load(18, (0*4), (windp));
    float_load(19, (1*4), (windp));
    float_load(20, (2*4), (windp));
    float_load(21, (3*4), (windp));
    float_neg(18, 18);
    float_neg(20, 20);
    float_load(22, (0*4), (lastp));
    float_load(23, (1*4), (lastp));
    float_load(24, (2*4), (lastp));
    float_load(25, (3*4), (lastp));
    float_madd(18, 18, 11, 22);
    float_msub(19, 19, 13, 23);
    float_madd(20, 20, 15, 24);
    float_msub(21, 21, 17, 25);
    float_store(18, (0*4), (destp));
    float_store(19, (1*4), (destp));
    float_store(20, (2*4), (destp));
    float_store(21, (3*4), (destp));

    // 4 to 8
    float_load(18, (4*4), (windp));
    float_load(19, (5*4), (windp));
    float_load(20, (6*4), (windp));
    float_load(21, (7*4), (windp));
    float_load(26, (8*4), (windp));
    float_neg(18, 18);
    float_neg(20, 20);
    float_neg(26, 26);
    float_load(22, (4*4), (lastp));
    float_load(23, (5*4), (lastp));
    float_load(24, (6*4), (lastp));
    float_load(25, (7*4), (lastp));
    float_load(27, (8*4), (lastp));
    float_madd(18, 18, 9, 22);
    float_msub(19, 19, 7, 23);
    float_madd(20, 20, 5, 24);
    float_msub(21, 21, 3, 25);
    float_madd(26, 26, 1, 27);
    float_store(18, (4*4), (destp));
    float_store(19, (5*4), (destp));
    float_store(20, (6*4), (destp));
    float_store(21, (7*4), (destp));
    float_store(26, (8*4), (destp));

    // 9 to 12
    float_load(18, (9*4), (windp));
    float_load(19, (10*4), (windp));
    float_load(20, (11*4), (windp));
    float_load(21, (12*4), (windp));
    float_load(22, (9*4), (lastp));
    float_load(23, (10*4), (lastp));
    float_load(24, (11*4), (lastp));
    float_load(25, (12*4), (lastp));
    float_madd(18, 18, 1, 22);
    float_madd(19, 19, 3, 23);
    float_madd(20, 20, 5, 24);
    float_madd(21, 21, 7, 25);
    float_neg(18, 18);
    float_neg(20, 20);
    float_store(18, (9*4), (destp));
    float_store(19, (10*4), (destp));
    float_store(20, (11*4), (destp));
    float_store(21, (12*4), (destp));

    // 13 to 17
    float_load(18, (13*4), (windp));
    float_load(19, (14*4), (windp));
    float_load(20, (15*4), (windp));
    float_load(21, (16*4), (windp));
    float_load(26, (17*4), (windp));
    float_load(22, (13*4), (lastp));
    float_load(23, (14*4), (lastp));
    float_load(24, (15*4), (lastp));
    float_load(25, (16*4), (lastp));
    float_load(27, (17*4), (lastp));
    float_madd(18, 18, 9, 22);
    float_madd(19, 19, 17, 23);
    float_madd(20, 20, 15, 24);
    float_madd(21, 21, 13, 25);
    float_madd(26, 26, 11, 27);
    float_neg(18, 18);
    float_neg(20, 20);
    float_neg(26, 26);
    float_store(18, (13*4), (destp));
    float_store(19, (14*4), (destp));
    float_store(20, (15*4), (destp));
    float_store(21, (16*4), (destp));
    float_store(26, (17*4), (destp));

    goto __save;

__even_window:

    // 0 to 3
    float_load(18, (0*4), (windp));
    float_load(19, (1*4), (windp));
    float_load(20, (2*4), (windp));
    float_load(21, (3*4), (windp));
    float_neg(18, 18);
    float_neg(19, 19);
    float_neg(20, 20);
    float_neg(21, 21);
    float_load(22, (0*4), (lastp));
    float_load(23, (1*4), (lastp));
    float_load(24, (2*4), (lastp));
    float_load(25, (3*4), (lastp));
    float_madd(18, 18, 11, 22);
    float_madd(19, 19, 13, 23);
    float_madd(20, 20, 15, 24);
    float_madd(21, 21, 17, 25);
    float_store(18, (0*4), (destp));
    float_store(19, (1*4), (destp));
    float_store(20, (2*4), (destp));
    float_store(21, (3*4), (destp));

    // 4 to 8
    float_load(18, (4*4), (windp));
    float_load(19, (5*4), (windp));
    float_load(20, (6*4), (windp));
    float_load(21, (7*4), (windp));
    float_load(26, (8*4), (windp));
    float_neg(18, 18);
    float_neg(19, 19);
    float_neg(20, 20);
    float_neg(21, 21);
    float_neg(26, 26);
    float_load(22, (4*4), (lastp));
    float_load(23, (5*4), (lastp));
    float_load(24, (6*4), (lastp));
    float_load(25, (7*4), (lastp));
    float_load(27, (8*4), (lastp));
    float_madd(18, 18, 9, 22);
    float_madd(19, 19, 7, 23);
    float_madd(20, 20, 5, 24);
    float_madd(21, 21, 3, 25);
    float_madd(26, 26, 1, 27);
    float_store(18, (4*4), (destp));
    float_store(19, (5*4), (destp));
    float_store(20, (6*4), (destp));
    float_store(21, (7*4), (destp));
    float_store(26, (8*4), (destp));

    // 9 to 12
    float_load(18, (9*4), (windp));
    float_load(19, (10*4), (windp));
    float_load(20, (11*4), (windp));
    float_load(21, (12*4), (windp));
    float_load(22, (9*4), (lastp));
    float_load(23, (10*4), (lastp));
    float_load(24, (11*4), (lastp));
    float_load(25, (12*4), (lastp));
    float_madd(18, 18, 1, 22);
    float_madd(19, 19, 3, 23);
    float_madd(20, 20, 5, 24);
    float_madd(21, 21, 7, 25);
    float_store(18, (9*4), (destp));
    float_store(19, (10*4), (destp));
    float_store(20, (11*4), (destp));
    float_store(21, (12*4), (destp));

    // 13 to 17
    float_load(18, (13*4), (windp));
    float_load(19, (14*4), (windp));
    float_load(20, (15*4), (windp));
    float_load(21, (16*4), (windp));
    float_load(26, (17*4), (windp));
    float_load(22, (13*4), (lastp));
    float_load(23, (14*4), (lastp));
    float_load(24, (15*4), (lastp));
    float_load(25, (16*4), (lastp));
    float_load(27, (17*4), (lastp));
    float_madd(18, 18, 9, 22);
    float_madd(19, 19, 17, 23);
    float_madd(20, 20, 15, 24);
    float_madd(21, 21, 13, 25);
    float_madd(26, 26, 11, 27);
    float_store(18, (13*4), (destp));
    float_store(19, (14*4), (destp));
    float_store(20, (15*4), (destp));
    float_store(21, (16*4), (destp));
    float_store(26, (17*4), (destp));

__save:
    // save into previous
    float_load(18, (18*4), (windp));
    float_load(19, (19*4), (windp));
    float_load(20, (20*4), (windp));
    float_load(21, (21*4), (windp));
    float_load(22, (22*4), (windp));
    float_load(23, (23*4), (windp));
    float_load(24, (24*4), (windp));
    float_load(25, (25*4), (windp));
    float_load(26, (26*4), (windp));
    float_mul(18, 18, 10);
    float_mul(19, 19, 12);
    float_mul(20, 20, 14);
    float_mul(21, 21, 16);
    float_mul(22, 22, 8);
    float_mul(23, 23, 6);
    float_mul(24, 24, 4);
    float_mul(25, 25, 2);
    float_mul(26, 26, 0);
    float_store(18, (0*4), (lastp));
    float_store(19, (1*4), (lastp));
    float_store(20, (2*4), (lastp));
    float_store(21, (3*4), (lastp));
    float_store(22, (4*4), (lastp));
    float_store(23, (5*4), (lastp));
    float_store(24, (6*4), (lastp));
    float_store(25, (7*4), (lastp));
    float_store(26, (8*4), (lastp));

    float_load(18, (27*4), (windp));
    float_load(19, (28*4), (windp));
    float_load(20, (29*4), (windp));
    float_load(21, (30*4), (windp));
    float_load(22, (31*4), (windp));
    float_load(23, (32*4), (windp));
    float_load(24, (33*4), (windp));
    float_load(25, (34*4), (windp));
    float_load(26, (35*4), (windp));
    float_mul(18, 18, 0);
    float_mul(19, 19, 2);
    float_mul(20, 20, 4);
    float_mul(21, 21, 6);
    float_mul(22, 22, 8);
    float_mul(23, 23, 16);
    float_mul(24, 24, 14);
    float_mul(25, 25, 12);
    float_mul(26, 26, 10);
    float_store(18, (9*4), (lastp));
    float_store(19, (10*4), (lastp));
    float_store(20, (11*4), (lastp));
    float_store(21, (12*4), (lastp));
    float_store(22, (13*4), (lastp));
    float_store(23, (14*4), (lastp));
    float_store(24, (15*4), (lastp));
    float_store(25, (16*4), (lastp));
    float_store(26, (17*4), (lastp));  
}

//#############################################################################
//##                                                                         ##
//## 3x12 IMDCT for window type 2 (short windows)                            ##
//##                                                                         ##
//#############################################################################

                //
                //t1 = in2 * cos(pi/6)
                //t2 = in1 * sin(pi/6)
                //t3 = in3 + t2;
                //out1 = in3 - in1;
                //out2 = t3 + t1;
                //out3 = t3 - t1;
                //

#define IDCT_3( in1, in2, in3, reg1, reg2, reg3, scale1, scale2 ) ;\
    float_madd(reg3, in1, scale1, in3                  );\
    float_mul(in2, in2, scale2                         );\
    float_sub(reg1, in3, in1                           );\
    float_add(reg2, reg3, in2                          );\
    float_sub(reg3, reg3, in2                          );

#define blend_four_alt( save_off, dest_off ) ;\
    float_load(0, ((save_off+0)*4), (save)          );\
    float_load(1, ((save_off+1)*4), (save)          );\
    float_load(2, ((save_off+2)*4), (save)          );\
    float_load(3, ((save_off+3)*4), (save)          );\
    float_load(4, ((dest_off+0)*4), (destp)         );\
    float_load(5, ((dest_off+1)*4), (destp)         );\
    float_load(6, ((dest_off+2)*4), (destp)         );\
    float_load(7, ((dest_off+3)*4), (destp)         );\
    float_neg(5, 5                            );\
    float_neg(7, 7                            );\
    float_add(4, 4, 0                      );\
    float_sub(5, 5, 1                      );\
    float_add(6, 6, 2                      );\
    float_sub(7, 7, 3                      );\
    float_store(4, ((save_off+0)*4), (result)       );\
    float_store(5, ((save_off+1)*4), (result)       );\
    float_store(6, ((save_off+2)*4), (result)       );\
    float_store(7, ((save_off+3)*4), (result)       );

#define blend_four( save_off, dest_off )     ;\
    float_load(0, ((save_off+0)*4), (save)          );\
    float_load(1, ((save_off+1)*4), (save)          );\
    float_load(2, ((save_off+2)*4), (save)          );\
    float_load(3, ((save_off+3)*4), (save)          );\
    float_load(4, ((dest_off+0)*4), (destp)         );\
    float_load(5, ((dest_off+1)*4), (destp)         );\
    float_load(6, ((dest_off+2)*4), (destp)         );\
    float_load(7, ((dest_off+3)*4), (destp)         );\
    float_add(4, 4, 0                      );\
    float_add(5, 5, 1                      );\
    float_add(6, 6, 2                      );\
    float_add(7, 7, 3                      );\
    float_store(4, ((save_off+0)*4), (result)       );\
    float_store(5, ((save_off+1)*4), (result)       );\
    float_store(6, ((save_off+2)*4), (result)       );\
    float_store(7, ((save_off+3)*4), (result)       );

#define CASCADE_ADD_6( a, f0, f1, f2, f3, f4, f5 )     ;\
    float_load(f0, (((a)+0)*4), (srcp)   );\
    float_load(f1, (((a)+1)*4), (srcp)   );\
    float_load(f2, (((a)+2)*4), (srcp)   );\
    float_load(f3, (((a)+3)*4), (srcp)   );\
    float_load(f4, (((a)+4)*4), (srcp)   );\
    float_load(f5, (((a)+5)*4), (srcp)   );\
    float_add(f5,f5,f4              );\
    float_add(f4,f4,f3              );\
    float_add(f3,f3,f2              );\
    float_add(f2,f2,f1              );\
    float_add(f1,f1,f0              );\
    float_add(f5,f5,f3              );\
    float_add(f3,f3,f1              );

extern "C"
void AILCALL Scalar_IMDCT_3x12    (register F32 *in2,        // r3
                                register S32      sb2,        // r4
                                register F32 *resultp2,    // r5
                                register F32 *savep2)     // r6
{

    F32* srcp = in2;
    S32 sbp = sb2;
    F32* result = resultp2;
    F32* save = savep2;

    char Stack[ASMSTACKSIZE];
    float fpregs[32];

    F32* destp = (F32*)(Stack + (ASMSTACKSIZE/2));
    F32* temp = srcp + 18;

    destp[0] = 0.0f;
    destp[1] = 0.0f;
    destp[2] = 0.0f;
    destp[3] = 0.0f;
    destp[4] = 0.0f;
    destp[5] = 0.0f;


    float_load(30, sin30*4, MP3Constants);                
    float_load(31, cos30*4, MP3Constants);               
                     
    float_load(26, SQRT6PSQRT2D2*4, MP3Constants);
    float_load(27, SQRT2D2*4, MP3Constants);
    float_load(28, SQRT6MSQRT2D2*4, MP3Constants);
                     
    float_load(15, dct3t4*4, MP3Constants);
    float_load(22, dct3t5*4, MP3Constants);
    float_load(23, dct3t6*4, MP3Constants);
    float_load(24, dct3t7*4, MP3Constants);
    float_load(25, dct3t8*4, MP3Constants);
    float_load(29, dct3t9*4, MP3Constants);
                     
    float_load(16, dct3t17*4, MP3Constants);
    float_load(17, dct3t18*4, MP3Constants);
    float_load(18, dct3t19*4, MP3Constants);
    float_load(19, dct3t20*4, MP3Constants);
    float_load(20, dct3t21*4, MP3Constants);
    float_load(21, dct3t22*4, MP3Constants);

__for_DCT:            
  
    CASCADE_ADD_6( 0, 6, 7, 8, 9, 10, 11 )

    IDCT_3( 10, 8, 6, 1, 0, 2, 30, 31 )
    IDCT_3( 11, 9, 7, 4, 5, 3, 30, 31 )

    // scale
    float_mul(3, 3, 26);
    float_mul(4, 4, 27);
    float_mul(5, 5, 28);

    // butterfly
    float_sub(6, 0, 5);
    float_add(0, 0, 5);
    float_sub(7, 1, 4);
    float_add(1, 1, 4);
    float_sub(8, 2, 3);
    float_add(2, 2, 3);

    // swizzle
    float_mul(0, 0, 15);
    float_mul(1, 1, 22);
    float_mul(2, 2, 23);
    float_mul(3, 8, 24);
    float_mul(4, 7, 25);
    float_mul(5, 6, 29);

    // multiply and accumulate into the first six
    float_load(6, (0*4), (destp));;
    float_load(7, (1*4), (destp));;
    float_load(8, (2*4), (destp));;
    float_load(9, (3*4), (destp));;
    float_load(10, (4*4), (destp));;
    float_load(11, (5*4), (destp));;

    float_madd(7, 4, 16, 7);
    float_madd(8, 5, 17, 8);
    float_madd(9, 5, 18, 9);
    float_madd(10, 4, 19, 10);
    float_madd(11, 3, 20, 11);
    float_madd(6, 3, 21, 6);

    float_store(6, (0*4), (destp));
    float_store(7, (1*4), (destp));
    float_store(8, (2*4), (destp));
    float_store(9, (3*4), (destp));
    float_store(10, (4*4), (destp));
    float_store(11, (5*4), (destp));

    // do the direct copy into the second six
    float_load(10, dct3t14*4, MP3Constants);
    float_load(8,  dct3t12*4, MP3Constants);
    float_load(6,  dct3t10*4, MP3Constants);
    float_load(7,  dct3t11*4, MP3Constants);
    float_load(9,  dct3t13*4, MP3Constants);
    float_load(11, dct3t15*4, MP3Constants);
    float_mul(10, 2, 10);
    float_mul(8, 1, 8);
    float_mul(6, 0, 6);
    float_mul(7, 0, 7);
    float_mul(9, 1, 9);
    float_mul(11, 2, 11);
    float_store(10, (6*4), (destp));
    float_store(8, (7*4), (destp));
    float_store(6, (8*4), (destp));
    float_store(7, (9*4), (destp));
    float_store(9, (10*4), (destp));
    float_store(11, (11*4), (destp));

    // keep looping...
    srcp = srcp + 6;
    destp = destp + 6;
    if (srcp != temp) goto __for_DCT;

    destp = (F32*)(Stack + (ASMSTACKSIZE/2));

    if ((sbp & 1) == 0) goto overlap;

    float_load(0, (0*4), (save));
    float_load(1, (1*4), (save));
    float_load(2, (2*4), (save));
    float_load(3, (3*4), (save));
    float_load(4, (4*4), (save));
    float_load(5, (5*4), (save));
    float_neg(1, 1);
    float_neg(3, 3);
    float_neg(5, 5);
    float_store(0, (0*4), (result));
    float_store(1, (1*4), (result));
    float_store(2, (2*4), (result));
    float_store(3, (3*4), (result));
    float_store(4, (4*4), (result));
    float_store(5, (5*4), (result));

    blend_four_alt( 6, 0 )
    blend_four_alt( 10, 4 )
    blend_four_alt( 14, 8 )

    goto overlap_done;

 overlap:
    float_load(0, (0*4), (save));
    float_load(1, (1*4), (save));
    float_load(2, (2*4), (save));
    float_load(3, (3*4), (save));
    float_load(4, (4*4), (save));
    float_load(5, (5*4), (save));
    float_store(0, (0*4), (result));
    float_store(1, (1*4), (result));
    float_store(2, (2*4), (result));
    float_store(3, (3*4), (result));
    float_store(4, (4*4), (result));
    float_store(5, (5*4), (result));

    blend_four( 6, 0 )
    blend_four( 10, 4 )
    blend_four( 14, 8 )

 overlap_done:
    float_load(0, (12 + 0)*4, destp);
    float_load(1, (12 + 1)*4, destp);
    float_load(2, (12 + 2)*4, destp);
    float_load(3, (12 + 3)*4, destp);
    float_load(4, (12 + 4)*4, destp);
    float_load(5, (12 + 5)*4, destp);

    float_load(6, (12 + 6)*4, destp);
    float_load(7, (12 + 7)*4, destp);
    float_load(8, (12 + 8)*4, destp);
    float_load(9, (12 + 9)*4, destp);
    float_load(10, (12 + 10)*4, destp);
    float_load(11, (12 + 11)*4, destp);

    float_store(0, (0)*4, save);
    float_store(1, (1)*4, save);
    float_store(2, (2)*4, save);
    float_store(3, (3)*4, save);
    float_store(4, (4)*4, save);
    float_store(5, (5)*4, save);

    float_store(6, (6)*4, save);
    float_store(7, (7)*4, save);
    float_store(8, (8)*4, save);
    float_store(9, (9)*4, save);
    float_store(10, (10)*4, save);
    float_store(11, (11)*4, save);

    save[12] = 0.0f;
    save[13] = 0.0f;
    save[14] = 0.0f;
    save[15] = 0.0f;
    save[16] = 0.0f;
    save[17] = 0.0f;
}

//#############################################################################
//##                                                                         ##
//## IDCT reordering and butterflies for polyphase filter                    ##
//##                                                                         ##
//#############################################################################

                //
                //B = (A - C) * D; d1 = A + C;
                //F = (E - G) * H; d2 = E + G;
                //

#define REORD_PAIR( in, out, out_ofs, dest1, A, B, C, D, dest2, E, F, G, H ) ;\
  float_load(16, ((A)*18*4), (in))                                        ;\
  float_load(17, ((E)*18*4), (in))                                        ;\
  float_load(18, ((C)*18*4), (in))                                        ;\
  float_load(19, ((G)*18*4), (in))                                        ;\
  float_load(20, ((D)*4), (barrayp))                                      ;\
  float_load(21, ((H)*4), (barrayp))                                      ;\
                                                                  ;\
  float_add(dest1, 16, 18                                     );\
  float_add(dest2, 17, 19                                     );\
  float_sub(16, 16, 18                                          );\
  float_sub(17, 17, 19                                          );\
  float_mul(16, 16, 20                                          );\
  float_mul(17, 17, 21                                          );\
                                                                  ;\
  float_store(16, ((((B)-16))*4+out_ofs), (out))                          ;\
  float_store(17, ((((F)-16)*4)+out_ofs), (out))                          ;


#define REORD_PAIR_TO_R( in, in_ofs, dest1, A, B, C, D, dest2, E, F, G, H ) ;\
  float_load(B, ((((A)-16)*4)+in_ofs), (in)                              );\
  float_load(F, ((((E)-16)*4)+in_ofs), (in)                              );\
  float_load(16, ((((C)-16)*4)+in_ofs), (in)                               );\
  float_load(17, ((((G)-16)*4)+in_ofs), (in)                               );\
  float_load(18, ((D)*4), (barrayp)                                        );\
  float_load(19, ((H)*4), (barrayp)                                        );\
  float_add(dest1, B, 16                                      );\
  float_add(dest2, F, 17                                      );\
  float_sub(B, B, 16                                          );\
  float_sub(F, F, 17                                          );\
  float_mul(B, B, 18                                          );\
  float_mul(F, F, 19                                          );\

#define REORD_PAIR_R( A, B, scale1, A2, B2, scale2 ) ;\
  float_sub(24, A, B                           );\
  float_sub(25, A2, B2                         );\
  float_add(A, A, B                          );\
  float_add(A2, A2, B2                       );\
  float_mul(B, 24, scale1                          );\
  float_mul(B2, 25, scale2                         );


extern "C"
void AILCALL Scalar_poly_filter(register const F32 *lpsrc2,   //r3
                             register const F32 *barray2,  //r4
                             register S32            phase2,   //r5
                             register F32       *lpout12,  //r6
                             register F32       *lpout22)  //r7
{

    const F32* srcp = lpsrc2;
    const F32* barrayp = barray2;
    S32 phasep = phase2;
    F32* lpout1p = lpout12;
    F32* lpout2p = lpout22;

  #define temp1_ofs  (ASMSTACKSIZE/2)

    char Stack[ASMSTACKSIZE];
    char* pStack = Stack;
    float fpregs[32];
    const F32* temp;
    F32* temp1_base;

    phasep = phasep << 2;
    temp = srcp + phasep/4;
    temp1_base = (F32*)pStack;

    REORD_PAIR( temp, temp1_base, temp1_ofs, 0,0,16,31,1,    1,1,17,30,3 )
    REORD_PAIR( temp, temp1_base, temp1_ofs, 3,2,19,29,5,    2,3,18,28,7 )
    REORD_PAIR( temp, temp1_base, temp1_ofs, 6,4,22,27,9,    7,5,23,26,11 )
    REORD_PAIR( temp, temp1_base, temp1_ofs, 5,6,21,25,13,   4,7,20,24,15 )
    REORD_PAIR( temp, temp1_base, temp1_ofs, 12,8,28,23,17,  13,9,29,22,19 )
    REORD_PAIR( temp, temp1_base, temp1_ofs, 15,10,31,21,21, 14,11,30,20,23 )
    REORD_PAIR( temp, temp1_base, temp1_ofs, 10,12,26,19,25, 11,13,27,18,27 )
    REORD_PAIR( temp, temp1_base, temp1_ofs, 9,14,25,17,29,  8,15,24,16,31 )

    float_load(16, (2*4), (barrayp));
    float_load(17, (6*4), (barrayp));
    float_load(18, (14*4), (barrayp));
    float_load(19, (10*4), (barrayp));
    float_load(20, (30*4), (barrayp));
    float_load(21, (26*4), (barrayp));
    float_load(22, (18*4), (barrayp));
    float_load(23, (22*4), (barrayp));
    REORD_PAIR_R( 0,8,16,    1,9,17 )
    REORD_PAIR_R( 2,10,18,  3,11,19 )
    REORD_PAIR_R( 4,12,20,  5,13,21 )
    REORD_PAIR_R( 6,14,22,  7,15,23 )

    float_load(16, (4*4), (barrayp));
    float_load(17, (12*4), (barrayp));
    float_load(18, (28*4), (barrayp));
    float_load(19, (20*4), (barrayp));
    REORD_PAIR_R( 0,4,16,      1,5,17 )
    REORD_PAIR_R( 2,6,18,      3,7,19 )
    REORD_PAIR_R( 8,12,16,    9,13,17 )
    REORD_PAIR_R( 10,14,18,  11,15,19 )

    float_load(16, (8*4), (barrayp));
    float_load(17, (24*4), (barrayp));
    REORD_PAIR_R( 0,2,16,     1,3,17 )
    REORD_PAIR_R( 4,6,16,     5,7,17 )
    REORD_PAIR_R( 8,10,16,   9,11,17 )
    REORD_PAIR_R( 12,14,16, 13,15,17 )

    float_neg(16, 0);
    float_load(17, (16*4), (barrayp));
    float_sub(16, 16, 1);
    float_sub(1, 0, 1);
    float_add(0, 16, 16);
    float_mul(1, 1, 17);

    float_sub(16, 2, 3);
    float_add(2, 2, 3);
    float_sub(18, 4, 5);
    float_msub(3, 16, 17, 2);

    float_add(4, 4, 5);
    float_sub(16, 6, 7);
    float_madd(5, 18, 17, 4);

    float_add(6, 6, 7);
    float_msub(7, 16, 17, 5);

    float_sub(16, 8, 9);
    float_add(8, 8, 9);
    float_mul(9, 16, 17);

    float_sub(16, 10, 11);
    float_add(18, 8, 9);
    float_add(10, 10, 11);
    float_madd(11, 16, 17, 18);

    float_sub(16, 12, 13);
    float_add(12, 12, 13);
    float_sub(19, 14, 15);
    float_sub(18, 12, 18);
    float_add(14, 14, 15);
    float_add(20, 8, 10);
    float_madd(13, 16, 17, 18);

    float_msub(15, 19, 17, 11);
    float_sub(14, 14, 20);

    float_sub(19, 11, 13);
    float_add(18, 8, 9);
    float_sub(17, 9, 14);
    float_sub(16, 5, 6);
    float_sub(19, 19, 18);
    float_sub(18, 13, 10);
    float_store(1, (0*16*4), (lpout1p));
    float_store(17, (2*16*4), (lpout1p));
    float_store(16, (4*16*4), (lpout1p));
    float_store(18, (6*16*4), (lpout1p));

    float_store(3, (8*16*4), (lpout1p));
    float_store(19, (10*16*4), (lpout1p));
    float_store(7, (12*16*4), (lpout1p));
    float_store(15, (14*16*4), (lpout1p));

    float_neg(16, 1);
    float_neg(17, 14);
    float_sub(19, 4, 6);
    float_sub(20, 8, 12);
    float_sub(21, 12, 8);
    float_store(16, (0*16*4), (lpout2p));
    float_store(17, (2*16*4), (lpout2p));

    float_neg(16, 8);
    float_neg(17, 4);
    float_sub(21, 21, 10);
    float_neg(18, 2);
    float_store(19, (4*16*4), (lpout2p));
    float_store(21, (6*16*4), (lpout2p));
    float_store(18, (8*16*4), (lpout2p));
    float_store(20, (10*16*4), (lpout2p));
    float_store(17, (12*16*4), (lpout2p));
    float_store(16, (14*16*4), (lpout2p));
    float_store(0, (16*16*4), (lpout2p));

    REORD_PAIR_TO_R( temp1_base, temp1_ofs, 0,16,8,24,2,   1,17,9,25,6 )
    REORD_PAIR_TO_R( temp1_base, temp1_ofs, 2,18,10,26,14, 3,19,11,27,10 )
    REORD_PAIR_TO_R( temp1_base, temp1_ofs, 4,20,12,28,30, 5,21,13,29,26 )
    REORD_PAIR_TO_R( temp1_base, temp1_ofs, 6,22,14,30,18, 7,23,15,31,22 )

    float_load(16, (4*4), (barrayp));
    float_load(17, (12*4), (barrayp));
    float_load(18, (28*4), (barrayp));
    float_load(19, (20*4), (barrayp));
    REORD_PAIR_R( 0,4,16,      1,5,17 )
    REORD_PAIR_R( 2,6,18,      3,7,19 )
    REORD_PAIR_R( 8,12,16,    9,13,17 )
    REORD_PAIR_R( 10,14,18,  11,15,19 )

    float_load(16, (8*4), (barrayp));
    float_load(17, (24*4), (barrayp));
    REORD_PAIR_R( 0,2,16,     1,3,17 )
    REORD_PAIR_R( 4,6,16,     5,7,17 )
    REORD_PAIR_R( 8,10,16,   9,11,17 )
    REORD_PAIR_R( 12,14,16, 13,15,17 )

    float_load(17, (16*4), (barrayp));
    float_sub(16, 0, 1);
    float_add(0, 0, 1);
    float_mul(1, 16, 17);

    float_sub(16, 2, 3);
    float_add(2, 2, 3);
    float_mul(3, 16, 17);

    float_add(16, 4, 5);
    float_sub(5, 4, 5);
    float_add(4, 16, 0);
    float_madd(5, 5, 17, 1);

    float_sub(16, 6, 7);
    float_add(6, 6, 7);
    float_madd(7, 16, 17, 0);
    float_add(6, 6, 0);
    float_add(7, 7, 1);
    float_add(6, 6, 2);
    float_add(7, 7, 3);

    float_sub(16, 8, 9);
    float_add(8, 8, 9);
    float_mul(9, 16, 17);

    float_sub(16, 10, 11);
    float_add(10, 10, 11);
    float_madd(11, 16, 17, 9);
    float_add(10, 10, 8);
    float_add(11, 11, 8);

    float_add(16, 12, 13);
    float_sub(13, 12, 13);
    float_sub(12, 16, 4);
    float_msub(13, 13, 17, 5);
    float_add(13, 13, 12);

    float_add(16, 14, 15);
    float_sub(15, 14, 15);
    float_sub(14, 16, 6);
    float_msub(15, 15, 17, 7);

    float_neg(16, 14);
    float_add(18, 2, 4);
    float_sub(17, 10, 6);
    float_sub(19, 12, 2);
    float_sub(18, 18, 10);
    float_store(16, (1*16*4), (lpout2p));
    float_store(17, (3*16*4), (lpout2p));
    float_store(18, (5*16*4), (lpout2p));
    float_store(19, (7*16*4), (lpout2p));

    float_neg(16, 12);
    float_sub(17, 8, 4);
    float_sub(18, 0, 8);
    float_neg(19, 0);
    float_store(16, (9*16*4), (lpout2p));
    float_store(17, (11*16*4), (lpout2p));
    float_store(18, (13*16*4), (lpout2p));
    float_store(19, (15*16*4), (lpout2p));

    float_sub(16, 13, 2);
    float_sub(17, 4, 9);
    float_sub(18, 5, 10);
    float_add(17, 17, 2);
    float_sub(19, 9, 6);
    float_add(17, 17, 18);
    float_sub(18, 10, 1);
    float_sub(20, 1, 14);
    float_add(18, 18, 19);
    float_store(20, (1*16*4), (lpout1p));
    float_store(18, (3*16*4), (lpout1p));
    float_store(17, (5*16*4), (lpout1p));
    float_store(16, (7*16*4), (lpout1p));

    float_sub(18, 11, 3);
    float_sub(19, 3, 13);
    float_sub(18, 18, 4);
    float_sub(17, 7, 11);
    float_sub(18, 18, 5);
    float_store(19, (9*16*4), (lpout1p));
    float_store(18, (11*16*4), (lpout1p));
    float_store(17, (13*16*4), (lpout1p));
    float_store(15, (15*16*4), (lpout1p));
}

//#############################################################################
//##                                                                         ##
//## Apply inverse window and write sample data                              ##
//##                                                                         ##
//#############################################################################

#define MAC_INIT_PAIR( f1, f2, f3, f4 ) ;\
    float_load(0, (f1+0), (srcp)                 );\
    float_load(1, (f3+0), (srcp)                 );\
    float_load(2, (f2+0), (windp)                );\
    float_load(3, (f4+0), (windp)                );\
    float_mul(0, 0, 2                   );\
    float_mul(1, 1, 3                   );

#define MAC_INIT_PAIR_LOOP( f1, f2, f3, f4, srcd, wind ) ;\
    float_load(0, (f1+0), (srcp)                 );\
    float_load(1, (f3+0), (srcp)                 );\
    float_load(10, ((f1+0)+(srcd)), (srcp)       );\
    float_load(11, ((f3+0)+(srcd)), (srcp)       );\
    float_load(2, (f2+0), (windp)                );\
    float_load(3, (f4+0), (windp)                );\
    float_load(4, ((f2+0)+(wind)), (windp)       );\
    float_load(5, ((f4+0)+(wind)), (windp)       );\
    float_mul(0, 0, 2                   );\
    float_mul(1, 1, 3                   );\
    float_mul(10, 10, 4                 );\
    float_mul(11, 11, 5                 );


#define MAC_PAIR( f1, f2, f3, f4 ) ;\
    float_load(2, (f1+0), (srcp)            );\
    float_load(3, (f3+0), (srcp)            );\
    float_load(4, (f2+0), (windp)           );\
    float_load(5, (f4+0), (windp)           );\
    float_madd(0, 2, 4, 0        );\
    float_madd(1, 3, 5, 1        );\

#define MAC_PAIR_LOOP( f1, f2, f3, f4, srcd, wind ) ;\
    float_load(2, (f1+0), (srcp)            );\
    float_load(3, (f3+0), (srcp)            );\
    float_load(6, ((f1+0)+(srcd)), (srcp)   );\
    float_load(7, ((f3+0)+(srcd)), (srcp)   );\
    float_load(4, (f2+0), (windp)           );\
    float_load(5, (f4+0), (windp)           );\
    float_load(8, ((f2+0)+(wind)), (windp)  );\
    float_load(9, ((f4+0)+(wind)), (windp)  );\
    float_madd(0, 2, 4, 0        );\
    float_madd(1, 3, 5, 1        );\
    float_madd(10, 6, 8, 10      );\
    float_madd(11, 7, 9, 11      );


#define clip( v1 )         \
  v1 = v1 - 32767       ; \
  tmp1 = v1 >> 31      ; \
  v1 = v1 & tmp1        ; \
  v1 = v1 + 65535     ; \
  tmp1 = v1 >> 31      ; \
  v1 = v1 & (~tmp1)       ; \
  v1 = v1 - 32768       ; \
  
#define clip2( v1, v2, ln )    \
  v1 = v1 - 32767       ; \
  v2 = v2 - 32767       ; \
  tmp1 = v1 >> 31      ; \
  tmp2 = v2 >> 31      ; \
  v1 = v1 & tmp1        ; \
  v2 = v2 & tmp2        ; \
  v1 = v1 + 65535     ; \
  v2 = v2 + 65535     ; \
  tmp1 = v1 >> 31      ; \
  tmp2 = v2 >> 31      ; \
  v1 = v1 & (~tmp1)       ; \
  v2 = v2 & (~tmp2)       ; \
  v1 = v1 - 32768       ; \
  v2 = v2 - 32768       ; \

#define MAC_END_SUM_LOOP        ;\
  float_add(0, 0, 1)           ;\
  float_add(10, 10, 11)        ;\
  v1 = (S32)fpregs[0];          \
  v2 = (S32)fpregs[10];         \
  clip2( v1, v2, 1 )     ;\
  temp1 = destp + stepr       ;\
  STORE_LE_SWAP16(destp, v1);    \
  STORE_LE_SWAP16(temp1, v2);   \
  destp = temp1 + stepr       ;

#define MAC_END_DIF_LOOP        ;\
  float_sub(0, 0, 1)           ;\
  float_sub(10, 10, 11)        ;\
  v1 = (S32)fpregs[0];              \
  v2 = (S32)fpregs[10];             \
  clip2( v1, v2, 2 )     ;\
  temp1 = destp + stepr       ;\
  STORE_LE_SWAP16(destp, v1); \
  STORE_LE_SWAP16(temp1, v2); \
  destp = temp1 + stepr       ;

#define MAC_END_SUM             ;\
  float_add(0, 0, 1)           ;\
  v1 = (S32)fpregs[0];          \
  clip( v1 )                 ;\
  STORE_LE_SWAP16(destp, v1);       \
  destp = destp + stepr       ;

#define MAC_END_DIF             ;\
  float_sub(0, 0, 1)           ;\
  v1 = (S32)fpregs[0];          \
  clip( v1 )                 ;\
  STORE_LE_SWAP16(destp, v1);   \
  destp = destp + stepr       ;

extern "C"
void AILCALL Scalar_dewindow_and_write(F32 *u2,           //r3
                                    F32 *dewindow2,    //r4
                                    S32      start2,       //r5
                                    S16 *samples2,     //r6
                                    S32      output_step2, //r7
                                    S32      div2)         //r8
{
    char* destp = (char*)samples2;
    S32 stepr = output_step2;
    S32 divr = div2;

    S32 v1, v2;
    S32 tmp1, tmp2;
    S32 startr = start2 << 2;
    F32* windp = dewindow2;
    F32* srcp;
    F32* endp;
    char* temp1;

    float fpregs[32];

    windp = windp + 16;
    windp = windp - startr/4;

    startr = startr + startr;
    startr = startr - 48*4;

    //
    //First 16 samples
    //
    srcp = u2;
    endp = srcp + 16*16;

__for_loops:
    MAC_INIT_PAIR_LOOP( 0,0,4,4, 16*4, 32*4 )

    MAC_PAIR_LOOP( 8, 8, (8+4), (8+4), 16*4, 32*4 )
    MAC_PAIR_LOOP( 16,16,(16+4),(16+4), 16*4, 32*4 )
    MAC_PAIR_LOOP( 24,24,(24+4),(24+4), 16*4, 32*4 )
    MAC_PAIR_LOOP( 32,32,(32+4),(32+4), 16*4, 32*4 )
    MAC_PAIR_LOOP( 40,40,(40+4),(40+4), 16*4, 32*4 )
    MAC_PAIR_LOOP( 48,48,(48+4),(48+4), 16*4, 32*4 )
    MAC_PAIR_LOOP( 56,56,(56+4),(56+4), 16*4, 32*4 )

    windp += 32*2;
    srcp += 16*2;

    MAC_END_SUM_LOOP

    if (srcp != endp) goto __for_loops;

    if ( (divr & 1) == 0) goto __even_segment;

    //
    //Odd segment, 17th sample
    //

    MAC_INIT_PAIR( 0,0,8,8 )

    MAC_PAIR( 16, 16, (16+8), (16+8) )
    MAC_PAIR( 32, 32, (32+8), (32+8) )
    MAC_PAIR( 48, 48, (48+8), (48+8) )

    MAC_END_SUM

    windp += startr / 4;

    //
    //Odd segment, 14 samples
    //

    endp = srcp - 14*16;
    
__odd_loops:
    srcp -= 16;

    MAC_INIT_PAIR_LOOP( 4,((15*4)-4),0,(15*4), -16*4, -32*4 )

    MAC_PAIR_LOOP( (8+4), ((15*4)-8-4), 8, ((15*4)-8), -16*4, -32*4 )
    MAC_PAIR_LOOP( (16+4),((15*4)-16-4),16,((15*4)-16), -16*4, -32*4 )
    MAC_PAIR_LOOP( (24+4),((15*4)-24-4),24,((15*4)-24), -16*4, -32*4 )
    MAC_PAIR_LOOP( (32+4),((15*4)-32-4),32,((15*4)-32), -16*4, -32*4 )
    MAC_PAIR_LOOP( (40+4),((15*4)-40-4),40,((15*4)-40), -16*4, -32*4 )
    MAC_PAIR_LOOP( (48+4),((15*4)-48-4),48,((15*4)-48), -16*4, -32*4 )
    MAC_PAIR_LOOP( (56+4),((15*4)-56-4),56,((15*4)-56), -16*4, -32*4 )

    windp -= 32*2;
    srcp -= 16;

    MAC_END_DIF_LOOP

    if (srcp != endp) goto __odd_loops;

    //
    //Odd segment, last sample
    //

    srcp -= 16;

    MAC_INIT_PAIR( 4,((15*4)-4),0,(15*4) )

    MAC_PAIR( (8+4), ((15*4)-8-4), 8, ((15*4)-8) )
    MAC_PAIR( (16+4),((15*4)-16-4),16,((15*4)-16) )
    MAC_PAIR( (24+4),((15*4)-24-4),24,((15*4)-24) )
    MAC_PAIR( (32+4),((15*4)-32-4),32,((15*4)-32) )
    MAC_PAIR( (40+4),((15*4)-40-4),40,((15*4)-40) )
    MAC_PAIR( (48+4),((15*4)-48-4),48,((15*4)-48) )
    MAC_PAIR( (56+4),((15*4)-56-4),56,((15*4)-56) )

    MAC_END_DIF

    goto __exit;

    //
    //Even segment, 17th sample
    //

__even_segment:
    MAC_INIT_PAIR( 4,4,12,12 )

    MAC_PAIR( (16+4), (16+4), (16+12), (16+12) )
    MAC_PAIR( (32+4), (32+4), (32+12), (32+12) )
    MAC_PAIR( (48+4), (48+4), (48+12), (48+12) )
    MAC_END_SUM

    windp += startr / 4;

    //
    //Even segment, 14 samples
    //

    endp = srcp - 14*16;

__even_loops:
    srcp -= 16;

    MAC_INIT_PAIR_LOOP( 0,(15*4),4,((15*4)-4), -16*4, -32*4 )

    MAC_PAIR_LOOP( 8, ((15*4)-8), (8+4), ((15*4)-8-4), -16*4, -32*4 )
    MAC_PAIR_LOOP( 16,((15*4)-16),(16+4),((15*4)-16-4), -16*4, -32*4 )
    MAC_PAIR_LOOP( 24,((15*4)-24),(24+4),((15*4)-24-4), -16*4, -32*4 )
    MAC_PAIR_LOOP( 32,((15*4)-32),(32+4),((15*4)-32-4), -16*4, -32*4 )
    MAC_PAIR_LOOP( 40,((15*4)-40),(40+4),((15*4)-40-4), -16*4, -32*4 )
    MAC_PAIR_LOOP( 48,((15*4)-48),(48+4),((15*4)-48-4), -16*4, -32*4 )
    MAC_PAIR_LOOP( 56,((15*4)-56),(56+4),((15*4)-56-4), -16*4, -32*4 )

    windp -= 32*2;
    srcp -= 16;

    MAC_END_DIF_LOOP

    if (srcp != endp) goto __even_loops;

    //
    //Even segment, last sample
    //

    srcp -= 16;

    MAC_INIT_PAIR( 0,(15*4),4,((15*4)-4) )

    MAC_PAIR( 8, ((15*4)-8), (8+4), ((15*4)-8-4) )
    MAC_PAIR( 16,((15*4)-16),(16+4),((15*4)-16-4) )
    MAC_PAIR( 24,((15*4)-24),(24+4),((15*4)-24-4) )
    MAC_PAIR( 32,((15*4)-32),(32+4),((15*4)-32-4) )
    MAC_PAIR( 40,((15*4)-40),(40+4),((15*4)-40-4) )
    MAC_PAIR( 48,((15*4)-48),(48+4),((15*4)-48-4) )
    MAC_PAIR( 56,((15*4)-56),(56+4),((15*4)-56-4) )

    MAC_END_DIF

__exit:
    return;
}

#if VECTOR_DEWINDOW

#ifdef RAD_IGGY
#include "../../shared/radvec4.h"
#else
#include "radvec4.h"
#endif

#define ALIGN_WINDP \
    switch ((windp - curwindpbase) % 4) \
    {\
    case 0: break; \
    case 1: { UINTa Offset = windp - curwindpbase; windp = s_DewindowOff12 + Offset; curwindpbase = s_DewindowOff12; break; }\
    case 2: { UINTa Offset = windp - curwindpbase; windp = s_DewindowOff8 + Offset; curwindpbase = s_DewindowOff8; break; }\
    case 3: { UINTa Offset = windp - curwindpbase; windp = s_DewindowOff4 + Offset; curwindpbase = s_DewindowOff4; break; }\
    }

extern  F32* s_DewindowOff4;
extern  F32* s_DewindowOff8;
extern  F32* s_DewindowOff12;

// First loop #defines

#define UNROLL_FIRST_LOOP(acc) \
    V4LOAD(srcvec, (srcp + 0) );\
    V4LOAD(winvec, (windp + 0) );\
    V4MUL(srcvec, srcvec, winvec);\
    V4ADD(acc, acc, srcvec);\
    V4LOAD(srcvec, (srcp + 4) );\
    V4LOAD(winvec, (windp + 4) );\
    V4MUL(srcvec, srcvec, winvec);\
    V4ADD(acc, acc, srcvec);\
    V4LOAD(srcvec, (srcp + 8) );\
    V4LOAD(winvec, (windp + 8) );\
    V4MUL(srcvec, srcvec, winvec);\
    V4ADD(acc, acc, srcvec);\
    V4LOAD(srcvec, (srcp + 12) );\
    V4LOAD(winvec, (windp + 12) );\
    V4MUL(srcvec, srcvec, winvec);\
    V4ADD(acc, acc, srcvec);

#ifdef RADVEC_SUPPORTS_VECTOR_CONVERT
#define STORE_FIRST_LOOP(acc) \
    {\
        DECL_VEC4F(perm)\
        V4PERM1032(perm, acc, acc);\
        V4ADD(acc, acc, perm);\
        V4PERM3210(perm, acc, acc);\
        V4ADD(acc, acc, perm);\
    }
#else
#define STORE_FIRST_LOOP(acc) \
    { \
        S32 v1, tmp1; \
        RAD_ALIGN(float, st[4], 16); \
        V4STORE(st, acc); \
        st[0] += st[1];\
        st[0] += st[2];\
        v1 = (S32)(st[0] + st[3]);\
        clip(v1);\
        STORE_LE_SWAP16(destp, v1);\
        destp = destp + stepr;\
    }
#endif

// Odd loop defines

#define UNROLL_ODD_LOOP(acc) \
        V4LOAD(srcvec, (srcp + 0) ); \
        V4LOAD(temp, (windp + 12) ); \
        V4PERM3210(winvec, temp, temp);\
        V4MUL(srcvec, srcvec, winvec);\
        V4ADD(acc, acc, srcvec);\
        V4LOAD(srcvec, (srcp + 4) );\
        V4LOAD(temp, (windp + 8) );\
        V4PERM3210(winvec, temp, temp);\
        V4MUL(srcvec, srcvec, winvec);\
        V4ADD(acc, acc, srcvec);\
        V4LOAD(srcvec, (srcp + 8) );\
        V4LOAD(temp, (windp + 4) );\
        V4PERM3210(winvec, temp, temp);\
        V4MUL(srcvec, srcvec, winvec);\
        V4ADD(acc, acc, srcvec);\
        V4LOAD(srcvec, (srcp + 12) );\
        V4LOAD(temp, (windp + 0) );\
        V4PERM3210(winvec, temp, temp);\
        V4MUL(srcvec, srcvec, winvec);\
        V4ADD(acc, acc, srcvec);

#ifdef RADVEC_SUPPORTS_VECTOR_CONVERT
#define STORE_ODD_LOOP(acc)\
        {\
            DECL_VEC4F(perm)\
            V4PERM1032(perm, acc, acc);\
            V4SUB(acc, perm, acc);\
            V4PERM2301(perm, acc, acc);\
            V4ADD(acc, perm, acc);\
        }
#else
#define STORE_ODD_LOOP(acc)\
        {\
            S32 v1, tmp1;\
            RAD_ALIGN(float, st[4], 16);\
            V4STORE(st, acc);\
            v1 = (S32)(-st[0] + st[1] - st[2] + st[3]);\
            clip(v1);\
            STORE_LE_SWAP16(destp, v1);\
            destp = destp + stepr;\
        }
#endif

// Even loop defines
#ifdef RADVEC_SUPPORTS_VECTOR_CONVERT
#define STORE_EVEN_LOOP(acc) \
        {\
            DECL_VEC4F(perm)\
            V4PERM1032(perm, acc, acc);\
            V4SUB(acc, acc, perm);\
            V4PERM2301(perm, acc, acc);\
            V4ADD(acc, perm, acc);\
        }
#else
#define STORE_EVEN_LOOP(acc) \
        {\
            S32 v1, tmp1;\
            RAD_ALIGN(float, st[4], 16);\
            V4STORE(st, acc);\
            v1 = (S32)(st[0] - st[1] + st[2] - st[3]);\
            clip(v1);\
            STORE_LE_SWAP16(destp, v1);\
            destp = destp + stepr;\
        }
#endif

extern "C"
void AILCALL Vector_dewindow_and_write(F32 *u2,           //r3
                                    F32 *dewindow2,    //r4
                                    S32      start2,       //r5
                                    S16 *samples2,     //r6
                                    S32      output_step2, //r7
                                    S32      div2)         //r8
{
    char* destp = (char*)samples2;
    S32 stepr = output_step2;
    S32 divr = div2;

    DECL_SCRATCH

    S32 startr = start2 << 2;
    F32* srcp;
    F32* endp;
    F32* curwindpbase = dewindow2;

    //
    // Setup the aligned dewindow
    //

    F32* windp = dewindow2;

    windp = windp + 16;
    windp = windp - startr/4;

    startr = startr + startr;
    startr = startr - 48*4;

    //
    //First 16 samples
    //
    srcp = u2;
    endp = srcp + 16*16;
    ALIGN_WINDP

    //
    // Need a stack var to stuff the data in to when accumulating the samples into
    // one vector.
    //
    typedef union
    {
        U16 u[8];
        F32 f[4];
    } now_couple;

#ifdef RADVEC_SUPPORTS_VECTOR_CONVERT
    RAD_ALIGN(now_couple, target, 16);
#endif
    //
    // In order to merge, we need to be able to effectivly XOR out
    // components and add.
    //
    RAD_ALIGN(now_couple, mult1, 16);
    mult1.f[0] = 1;
    mult1.f[1] = 0;
    mult1.f[2] = 1;
    mult1.f[3] = 0;
    DECL_VEC4F(vMult1);
    V4LOAD(vMult1, mult1.f);
    mult1.f[1] = 1;
    mult1.f[0] = 0;
    mult1.f[3] = 1;
    mult1.f[2] = 0;
    DECL_VEC4F(vMult2);
    V4LOAD(vMult2, mult1.f);

    SETUP_V4ZERO

    {
        DECL_VEC4F(acc1)
        DECL_VEC4F(acc2)
        DECL_VEC4F(acc3)
        DECL_VEC4F(acc4)

        do // for each sample.
        {
            DECL_VEC4F(srcvec)
            DECL_VEC4F(winvec)
            V4ASSIGN(acc1, v4zero);
            V4ASSIGN(acc2, v4zero);
            V4ASSIGN(acc3, v4zero);
            V4ASSIGN(acc4, v4zero);

            // First Unroll
            UNROLL_FIRST_LOOP(acc1);
            srcp += 16; 
            windp += 32;
            STORE_FIRST_LOOP(acc1);

            // Second Unroll
            UNROLL_FIRST_LOOP(acc2);
            srcp += 16;
            windp += 32;
            STORE_FIRST_LOOP(acc2);

            // Third Unroll
            UNROLL_FIRST_LOOP(acc3);
            srcp += 16;
            windp += 32;
            STORE_FIRST_LOOP(acc3);

            // Fourth Unroll
            UNROLL_FIRST_LOOP(acc4);
            srcp += 16;
            windp += 32;
            STORE_FIRST_LOOP(acc4);

#ifdef RADVEC_SUPPORTS_VECTOR_CONVERT
            {
                VEC4I intver;
                DECL_VEC4F(perm);

                // Do final accumulation
                V4PERM0022(perm, acc1, acc3);
                V4PERM1133(acc1, acc2, acc4);
                V4MUL(perm, perm, vMult1);
                V4MUL(acc1, acc1, vMult2);
                V4ADD(perm, perm, acc1);

                // Convert and pack.
                intver = V4FTOI(perm);
                intver = V4ITO8I(intver, intver);
                V8STOREI(target.f, intver);

                // Results in u[0, 1, 2, 3].
                STORE_LE_SWAP16(destp, target.u[0]);
                destp = destp + stepr;
                STORE_LE_SWAP16(destp, target.u[1]);
                destp = destp + stepr;
                STORE_LE_SWAP16(destp, target.u[2]);
                destp = destp + stepr;
                STORE_LE_SWAP16(destp, target.u[3]);
                destp = destp + stepr;
            }
#endif
        } while (srcp != endp);
    }

    if ((divr & 1) == 0) goto __even_segment;

    //
    //Odd segment, 17th sample
    //
    {
        DECL_VEC4F(acc)

        DECL_VEC4F(srcvec)
        DECL_VEC4F(winvec)
        V4ASSIGN(acc, v4zero);

        UNROLL_FIRST_LOOP(acc); // happens to be the same as the first loop.

        // Only store the 0 and 2 for this sample.
#ifdef RADVEC_SUPPORTS_VECTOR_CONVERT
        {
            // Add the components
            DECL_VEC4F(perm)
            VEC4I intver;

            V4PERM2020(perm, acc, acc);
            V4ADD(acc, acc, perm);
            intver = V4FTOI(acc);
            intver = V4ITO8I(intver, intver);
            V8STOREI(target.f, intver);
            STORE_LE_SWAP16(destp, target.u[0]);
            destp = destp + stepr;
        }
#else
        {
            S32 v1, tmp1;
            RAD_ALIGN(float, st[4], 16);

            V4STORE(st, acc);
            st[0] += st[2];
            v1 = (S32)(st[0]);

            clip(v1);
            STORE_LE_SWAP16(destp, v1);
            destp = destp + stepr;
        }
#endif

    }

    //
    //Odd segment, 17th sample
    //

    windp = windp + startr/4;
    ALIGN_WINDP

    //
    //Odd segment, 12 samples
    //

    endp = srcp - 12*16;

    do
    {
        DECL_VEC4F(acc1)
        DECL_VEC4F(acc2)
        DECL_VEC4F(acc3)
        DECL_VEC4F(acc4)

        DECL_VEC4F(srcvec)
        DECL_VEC4F(winvec)
        DECL_VEC4F(temp)
        V4ASSIGN(acc1, v4zero);
        V4ASSIGN(acc2, v4zero);
        V4ASSIGN(acc3, v4zero);
        V4ASSIGN(acc4, v4zero);

        // Unroll #1
        srcp -= 16;
        UNROLL_ODD_LOOP(acc1);
        windp -= 32;
        STORE_ODD_LOOP(acc1);

        // Unroll #2
        srcp -= 16;
        UNROLL_ODD_LOOP(acc2);
        windp -= 32;
        STORE_ODD_LOOP(acc2);

        // Unroll #3
        srcp -= 16;
        UNROLL_ODD_LOOP(acc3);
        windp -= 32;
        STORE_ODD_LOOP(acc3);

        // Unroll #4
        srcp -= 16;
        UNROLL_ODD_LOOP(acc4);
        windp -= 32;
        STORE_ODD_LOOP(acc4);

#ifdef RADVEC_SUPPORTS_VECTOR_CONVERT
        {
            DECL_VEC4F(perm);
            VEC4I intver;

            // Do final accumulation
            V4PERM0022(perm, acc1, acc3);
            V4PERM0022(acc1, acc2, acc4);
            V4MUL(perm, perm, vMult1);
            V4MUL(acc1, acc1, vMult2);
            V4ADD(perm, perm, acc1);

            // Convert and pack.
            intver = V4FTOI(perm);
            intver = V4ITO8I(intver, intver);
            V8STOREI(target.f, intver);

            // Results in u[0, 1, 2, 3].
            STORE_LE_SWAP16(destp, target.u[0]);
            destp = destp + stepr;
            STORE_LE_SWAP16(destp, target.u[1]);
            destp = destp + stepr;
            STORE_LE_SWAP16(destp, target.u[2]);
            destp = destp + stepr;
            STORE_LE_SWAP16(destp, target.u[3]);
            destp = destp + stepr;
        }
#endif


    } while (srcp != endp);

    //
    // Finish the 12th -15th samples
    // We unroll this just like the one above, except only store 3 results.
    //
    {
        DECL_VEC4F(acc1)
        DECL_VEC4F(acc2)
        DECL_VEC4F(acc3)
        DECL_VEC4F(acc4)

        DECL_VEC4F(srcvec)
        DECL_VEC4F(winvec)
        DECL_VEC4F(temp)
        V4ASSIGN(acc1, v4zero);
        V4ASSIGN(acc2, v4zero);
        V4ASSIGN(acc3, v4zero);
        V4ASSIGN(acc4, v4zero);

        // Unroll #1
        srcp -= 16;
        UNROLL_ODD_LOOP(acc1);
        windp -= 32;
        STORE_ODD_LOOP(acc1);

        // Unroll #2
        srcp -= 16;
        UNROLL_ODD_LOOP(acc2);
        windp -= 32;
        STORE_ODD_LOOP(acc2);

        // Unroll #3
        srcp -= 16;
        UNROLL_ODD_LOOP(acc3);
        windp -= 32;
        STORE_ODD_LOOP(acc3);

#ifdef RADVEC_SUPPORTS_VECTOR_CONVERT
        {
            DECL_VEC4F(perm);
            VEC4I intver;

            // Do final accumulation
            V4PERM0022(perm, acc1, acc3);
            V4PERM0022(acc1, acc2, acc4);
            V4MUL(perm, perm, vMult1);
            V4MUL(acc1, acc1, vMult2);
            V4ADD(perm, perm, acc1);

            // Convert and pack.
            intver = V4FTOI(perm);
            intver = V4ITO8I(intver, intver);
            V8STOREI(target.f, intver);

            // Results in u[0, 1, 2, 3]. - only store 3 results
            STORE_LE_SWAP16(destp, target.u[0]);
            destp = destp + stepr;
            STORE_LE_SWAP16(destp, target.u[1]);
            destp = destp + stepr;
            STORE_LE_SWAP16(destp, target.u[2]);
            destp = destp + stepr;
        }
#endif
    }

    goto __exit;

    //
    //Even segment, 17th sample
    //

__even_segment:
    {

        SETUP_V4ZERO
        DECL_VEC4F(acc)

        DECL_VEC4F(srcvec)
        DECL_VEC4F(winvec)
        V4ASSIGN(acc, v4zero);

        UNROLL_FIRST_LOOP(acc); // happens to be the same as the first.

        // Only store the 1 and 3 for this sample.
#ifdef RADVEC_SUPPORTS_VECTOR_CONVERT
        {
            // Add the components
            DECL_VEC4F(perm)
            VEC4I intver;

            V4PERM2323(perm, acc, acc);
            V4ADD(acc, acc, perm);
            intver = V4FTOI(acc);
            intver = V4ITO8I(intver, intver);
            V8STOREI(target.f, intver);
            STORE_LE_SWAP16(destp, target.u[1]);
            destp = destp + stepr;
        }
#else
        {
            S32 v1, tmp1;
            RAD_ALIGN(float, st[4], 16);

            V4STORE(st, acc);
            st[1] += st[3];
            v1 = (S32)(st[1]);

            clip(v1);
            STORE_LE_SWAP16(destp, v1);
            destp = destp + stepr;
        }
#endif
    }

    windp = windp + startr/4;
    ALIGN_WINDP

    //
    //Even segment, 12 samples
    //
    endp = srcp - 12*16;

    do
    {
        DECL_VEC4F(acc1)
        DECL_VEC4F(acc2)
        DECL_VEC4F(acc3)
        DECL_VEC4F(acc4)

        DECL_VEC4F(srcvec)
        DECL_VEC4F(winvec)
        DECL_VEC4F(temp)
        V4ASSIGN(acc1, v4zero);
        V4ASSIGN(acc2, v4zero);
        V4ASSIGN(acc3, v4zero);
        V4ASSIGN(acc4, v4zero);

        //
        // The even loop uses the same math as the odd, just different storage.
        //

        // Unroll 1
        srcp -= 16;
        UNROLL_ODD_LOOP(acc1);
        windp -= 32;
        STORE_EVEN_LOOP(acc1);

        // Unroll 2
        srcp -= 16;
        UNROLL_ODD_LOOP(acc2);
        windp -= 32;
        STORE_EVEN_LOOP(acc2);

        // Unroll 3
        srcp -= 16;
        UNROLL_ODD_LOOP(acc3);
        windp -= 32;
        STORE_EVEN_LOOP(acc3);

        // Unroll 4
        srcp -= 16;
        UNROLL_ODD_LOOP(acc4);
        windp -= 32;
        STORE_EVEN_LOOP(acc4);

#ifdef RADVEC_SUPPORTS_VECTOR_CONVERT
        {
            DECL_VEC4F(perm);
            VEC4I intver;

            // Do final accumulation
            V4PERM0022(perm, acc1, acc3);
            V4PERM0022(acc1, acc2, acc4);
            V4MUL(perm, perm, vMult1);
            V4MUL(acc1, acc1, vMult2);
            V4ADD(perm, perm, acc1);

            // Convert and pack.
            intver = V4FTOI(perm);
            intver = V4ITO8I(intver, intver);
            V8STOREI(target.f, intver);

            // Results in u[0, 1, 2, 3].
            STORE_LE_SWAP16(destp, target.u[0]);
            destp = destp + stepr;
            STORE_LE_SWAP16(destp, target.u[1]);
            destp = destp + stepr;
            STORE_LE_SWAP16(destp, target.u[2]);
            destp = destp + stepr;
            STORE_LE_SWAP16(destp, target.u[3]);
            destp = destp + stepr;
        }
#endif
    } while (srcp != endp);

    // 3 more samples, unrolled and we only save the first 3 out of 4
    {
        DECL_VEC4F(acc1)
        DECL_VEC4F(acc2)
        DECL_VEC4F(acc3)

        DECL_VEC4F(srcvec)
        DECL_VEC4F(winvec)
        DECL_VEC4F(temp)
        V4ASSIGN(acc1, v4zero);
        V4ASSIGN(acc2, v4zero);
        V4ASSIGN(acc3, v4zero);

        //
        // The even loop uses the same math as the odd, just different storage.
        //

        // Unroll 1
        srcp -= 16;
        UNROLL_ODD_LOOP(acc1);
        windp -= 32;
        STORE_EVEN_LOOP(acc1);

        // Unroll 2
        srcp -= 16;
        UNROLL_ODD_LOOP(acc2);
        windp -= 32;
        STORE_EVEN_LOOP(acc2);

        // Unroll 3
        srcp -= 16;
        UNROLL_ODD_LOOP(acc3);
        windp -= 32;
        STORE_EVEN_LOOP(acc3);

#ifdef RADVEC_SUPPORTS_VECTOR_CONVERT
        {
            DECL_VEC4F(perm);
            VEC4I intver;

            // Do final accumulation
            V4PERM0022(perm, acc1, acc3);
            V4PERM0022(acc1, acc2, acc2);
            V4MUL(perm, perm, vMult1);
            V4MUL(acc1, acc1, vMult2);
            V4ADD(perm, perm, acc1);

            // Convert and pack.
            intver = V4FTOI(perm);
            intver = V4ITO8I(intver, intver);
            V8STOREI(target.f, intver);

            // Results in u[0, 1, 2, 3].
            STORE_LE_SWAP16(destp, target.u[0]);
            destp = destp + stepr;
            STORE_LE_SWAP16(destp, target.u[1]);
            destp = destp + stepr;
            STORE_LE_SWAP16(destp, target.u[2]);
            destp = destp + stepr;
        }
#endif
    }

__exit:
    return ;
}

#endif // VECTOR_DEWINDOW

static char marker[]={ 
  "\r\n\r\nMiles Sound System\r\n\r\n"
  "Copyright (C) 1991-2009 RAD Game Tools, Inc.\r\n\r\n"
};

