#ifdef DO_SCALARS
#undef DO_SCALARS
#endif

#ifndef AUDIOFLOAT
#define AUDIOFLOAT F32
#endif

#ifdef IS_XENON
#define __RADXENON__
#endif

#ifdef IS_PS3
#define __RADPS3__
#endif

#ifdef IS_PS2
#define __RADPS2__
#endif

#ifdef IS_X86
#define __RADX86__
#endif

#ifdef IS_32
#define __RAD32__
#endif

#ifdef IS_WIN64
#define __RAD64__
#endif

#ifdef IS_WII
#define __RADPPC__
#endif

#ifdef IS_PSP
#define __RADPSP__
#endif

#ifdef IS_WIIU
#define __RADWIIU__
#endif

#ifndef RADRESTRICT
#define RADRESTRICT MSSRESTRICT
#endif

#if defined(CLAMP_FORCE_LE) && !defined(STORE_LE_SWAP16) // we might inherit this from mss.h
  #if defined(__RADLITTLEENDIAN__)
    #define STORE_LE_SWAP16( ptr, val )  { *((U16*)(ptr))=(U16)(val); }
  #elif defined(__RADWII__)
    #define STORE_LE_SWAP16( ptr, val )  __sthbrx( (U16)(val), (S16 *)ptr, 0 )
  #elif defined(__RADWIIU__)
    #define STORE_LE_SWAP16( ptr, val ) (*(__bytereversed short *)(ptr) = (val))
  #elif defined(__RADMAC__)
    static __inline__ void st_le16(S16 *addr, const U16 val)
    {
        __asm__ __volatile__ ("sthbrx %1,0,%2" : "=m" (*addr) : "r" (val), "r" (addr));
    }
    #define STORE_LE_SWAP16(ptr, val) st_le16(ptr, val)
  #endif
#endif



#if defined(__RADNDS__) && !defined(__GCC__)
static void ClampToS16( S16* dest,
                         AUDIOSAMPLE* inp,
                         AUDIOTABLESAMPLE scale,
                         U32 num )
{
  register AUDIOSAMPLE * f;
  register S16 * c;
  register U32 tmp;

  f = (AUDIOSAMPLE*) inp;
  c = (S16*) dest;

  if ( ((U32)c ) & 2 )
  {
    S32 v = (S32)( AudioSampleToInt( AudioTableSampleMul( *f, scale ) ) );
    *c++ = ( v >= 32767 ) ? (S16)32767 : ( (v <= -32768 ) ? (S16)-32768 : (S16)v );
    f++;
    --num;
  }

  tmp = num >> 1;

  if ( tmp )
  {
    register S32 v1, v2, tmp2, v32k, vand;
 
    v32k = 32767;
    vand = 65535;
    num -= tmp * 2;
    f -= 1;
    c -= 2;
    __asm
    {
     loop:
      ldr v1, [f,#4]!
      ldr v2, [f,#4]!
      smull tmp2, v1, v1, scale
      smull tmp2, v2, v2, scale

      mov v1, v1, ASR #2
      mov v2, v2, ASR #2

      cmp v1, v32k
      movgt v1, v32k
      cmp v2, v32k
      movgt v2, v32k

      cmn v1, v32k
      rsblt v1, v32k, #0 
      cmn v2, v32k
      rsblt v2, v32k, #0 

      and v1, v1, vand
      orr v1, v1, v2, LSL #16

      str v1, [c, #4]!
      subs tmp, tmp, #1
      bne loop
    }
    f += 1;
    c -= 2;
  }
  if ( num ) 
  {
    S32 v = (S32)( AudioSampleToInt( AudioTableSampleMul( *f, scale ) ) );
    *c++ = ( v >= 32767 ) ? (S16)32767 : ( (v <= -32768 ) ? (S16)-32768 : (S16)v );
    f++;
  }
}

static void ClampToS16AndInterleave( S16* dest,
                                     AUDIOSAMPLE* inp0,
                                     AUDIOSAMPLE* inp1,
                                     AUDIOTABLESAMPLE scale,
                                     U32 num )
{
  register AUDIOSAMPLE * f;
  register S16 * c;

  f = (AUDIOSAMPLE*) inp0;
  c = (S16*) dest;

  {
    register S32 v1,v2, tmp2, v32k,vand, diff;

    v32k = 32767;
    vand = 65535;
    f -= 1;
    c -= 2;
    diff = ( (char*) inp1 ) - ( (char*) inp0 );
    __asm
    {
     loop:
      ldr v1, [f,#4]!
      ldr v2, [f,diff]
      smull tmp2, v1, v1, scale
      smull tmp2, v2, v2, scale

      mov v1, v1, ASR #2
      mov v2, v2, ASR #2
      cmp v1, v32k
      movgt v1, v32k
      cmp v2, v32k
      movgt v2, v32k
      
      cmn v1, v32k
      rsblt v1, v32k, #0 
      cmn v2, v32k
      rsblt v2, v32k, #0 

      and v1, v1, vand
      orr v1, v1, v2, LSL #16

      str v1, [c, #4]!
      subs num, num, #1
      bne loop
    }
    f += 1;
    c -= 2;
  }
}

#elif defined(__RADFIXEDPOINT__) || ( defined(__RADNDS__) && defined(__GCC__) )

static void ClampToS16( S16* dest,
                        AUDIOSAMPLE* inp,
                        AUDIOTABLESAMPLE scale,
                        U32 num )
{
  AUDIOSAMPLE * f;
  S16 * c;
  U32 i;

  f = (AUDIOSAMPLE*) inp;
  c = (S16*) dest;
  for (i = num; i; i--) {
    S32 v = (S32)( AudioSampleToInt( AudioTableSampleMul( *f, scale ) ) );
    *c++ = ( v >= 32767 ) ? (S16)32767 : ( (v <= -32768 ) ? (S16)-32768 : (S16)v );
    f++;
  }
}

static void ClampToS16AndInterleave( S16* dest,
                                     AUDIOSAMPLE* inp0,
                                     AUDIOSAMPLE* inp1,
                                     AUDIOTABLESAMPLE scale,
                                     U32 num )
{
  AUDIOSAMPLE * f;
  S16 * c;
  U32 i;
  SINTa diff = ( (char*) inp1 ) - ( (char*) inp0 );

  f = (AUDIOSAMPLE*) inp0;
  c = (S16*) dest;
  for ( i = num ; i-- ; )
  {
    S32 v;
    v = (S32)( AudioSampleToInt( AudioTableSampleMul( f[ 0 ], scale ) ) );
    *c++ = ( v >= 32767 ) ? (S16) 32767 : ( ( v <= -32768 ) ? (S16) -32768 : (S16) v );
    v = (S32)( AudioSampleToInt( AudioTableSampleMul( ((AUDIOSAMPLE*)( ((char*)f) + diff ))[ 0 ], scale ) ) );
    *c++ = ( v >= 32767 ) ? (S16) 32767 : ( ( v <= -32768 ) ? (S16) -32768 : (S16) v );
    ++f;
  }
}

#elif defined(__RADX86__) && defined(__RAD32__) && !defined(__RADWINEXT__) && !defined(__RADDOS__) && defined(_MSC_VER) && !defined(__RADXENON__)

// for sse intrinsics.
#if defined(_MSC_VER) && !defined(__RADXENON__) && !defined(__RADXBOX__)
#include <emmintrin.h>
#include <mmintrin.h>
#endif

#ifdef __RADXBOX__
#include <mmintrin.h>
#endif

#if defined( __RADXBOX__ )

// xbox uses normal x86
#define ClampToS16 ClampToS16_x86
#define ClampToS16AndInterleave ClampToS16AndInterleave_x86

#elif defined( __RAD64__ )

// 64 uses sse2
#define ClampToS16 ClampToS16_sse2
#define ClampToS16AndInterleave ClampToS16AndInterleave_sse2

#else

#ifndef CLAMP_SSE2_CHECK
#define CLAMP_SSE2_CHECK CPU_can_use(CPU_SSE2)
#endif

#define DO_SCALARS
#define ClampToS16 ClampToS16_dispatch
#define ClampToS16AndInterleave ClampToS16AndInterleave_dispatch

static void ClampToS16AndInterleave_x86(S16* dest, F32* inp0, F32* inp1, F32 scale, U32 num);
static void ClampToS16AndInterleave_sse2(S16* dest, F32* inp0, F32* inp1, F32 scale, U32 num);
static void ClampToS16AndInterleave_dispatch(S16* dest, F32* inp0, F32* inp1, F32 scale, U32 num)
{
    if (CLAMP_SSE2_CHECK)
    {
        ClampToS16AndInterleave_sse2(dest, inp0, inp1, scale, num);
        return;
    }
    ClampToS16AndInterleave_x86(dest, inp0, inp1, scale, num);
}

static void ClampToS16_x86(S16* dest, F32* inp, F32 scale, U32 num);
static void ClampToS16_sse2(S16* dest, F32* inp, F32 scale, U32 num);
static void ClampToS16_dispatch(S16* dest, F32* inp, F32 scale, U32 num)
{
    if (CLAMP_SSE2_CHECK)
    {
        ClampToS16_sse2(dest, inp, scale, num);
        return;
    }
    ClampToS16_x86(dest, inp, scale, num);
}

#endif

// x64 can't build inline asm
#ifndef __RAD64__
static void ClampToS16_x86( S16* dest,
                            F32* inp,
                            F32 scale,
                            U32 num )
{
  U32 buf1,buf2;
  __asm {
    push ecx
    mov ecx,[num]
    fld dword ptr [scale]
    mov esi,[inp]
    shr ecx,1
    mov edi,[dest]
  loop1:
    fld dword ptr [esi]
    fld dword ptr [esi+4]
    fmul ST,ST(2)
    fxch ST(1)
    fmul ST,ST(2)
    fistp dword ptr [buf1]
    fistp dword ptr [buf2]
    add esi,8
    mov eax,[buf1]
    mov edx,[buf2]
    cmp eax,32767
    jg over1
    cmp eax,-32768
    jl under1
    mov [edi],ax
    cmp edx,32767
    jg over2
    cmp edx,-32768
    jl under2
    mov [edi+2],dx
    add edi,4
    dec ecx
    jnz loop1
    jmp done

   over1:
    mov word ptr [edi],32767
    cmp edx,32767
    jg over2
    cmp edx,-32768
    jl under2
    mov [edi+2],dx
    add edi,4
    dec ecx
    jnz loop1
    jmp done

   under1:
    mov word ptr [edi],-32768
    cmp edx,32767
    jg over2
    cmp edx,-32768
    jl under2
    mov [edi+2],dx
    add edi,4
    dec ecx
    jnz loop1
    jmp done

   over2:
    mov word ptr [edi+2],32767
    add edi,4
    dec ecx
    jnz loop1
    jmp done

   under2:
    mov word ptr [edi+2],-32768
    add edi,4
    dec ecx
    jnz loop1

   done:
    pop ecx
    fistp dword ptr [buf1]
  }
}

static void ClampToS16AndInterleave_x86( S16* dest,
                                         F32* inp0,
                                         F32* inp1,
                                         F32 scale,
                                         U32 num )
{
  U32 buf1,buf2;
  __asm {
    push ecx
    push ebx
    mov ecx,[num]
    fld dword ptr [scale]
    mov esi,[inp0]
    mov ebx,[inp1]
    sub ebx,esi
    mov edi,[dest]
  loop1:
    fld dword ptr [esi]
    fld dword ptr [esi+ebx]
    fmul ST,ST(2)
    fxch ST(1)
    fmul ST,ST(2)
    fistp dword ptr [buf1]
    fistp dword ptr [buf2]
    add esi,4
    mov eax,[buf1]
    mov edx,[buf2]
    cmp eax,32767
    jg over1
    cmp eax,-32768
    jl under1
    mov [edi],ax
    cmp edx,32767
    jg over2
    cmp edx,-32768
    jl under2
    mov [edi+2],dx
    add edi,4
    dec ecx
    jnz loop1
    jmp done

   over1:
    mov word ptr [edi],32767
    cmp edx,32767
    jg over2
    cmp edx,-32768
    jl under2
    mov [edi+2],dx
    add edi,4
    dec ecx
    jnz loop1
    jmp done

   under1:
    mov word ptr [edi],-32768
    cmp edx,32767
    jg over2
    cmp edx,-32768
    jl under2
    mov [edi+2],dx
    add edi,4
    dec ecx
    jnz loop1
    jmp done

   over2:
    mov word ptr [edi+2],32767
    add edi,4
    dec ecx
    jnz loop1
    jmp done

   under2:
    mov word ptr [edi+2],-32768
    add edi,4
    dec ecx
    jnz loop1

   done:
    pop ebx
    pop ecx
    fistp dword ptr [buf1]
  }
}

#endif

#ifndef __RADXBOX__
static void ClampToS16_sse2( S16* dest,
                             F32* inp,
                             F32 scale,
                             U32 num )
{
    AUDIOFLOAT * RADRESTRICT f;
    S16* c;
    U32 i;

    __m128 MultVec = _mm_set_ps(scale, scale, scale, scale);

    radassert(!(num%8));
    //radassert(!((UINTa)dest & 0xF));
    //radassert(!((UINTa)inp & 0xF));

    f=inp;
    c = (S16*)dest;
    for (i = (num/8); i; i--)
    {
#ifdef CLAMP_SUPPORT_UNALIGNED_SOURCE
        __m128 LoadVector = _mm_loadu_ps(f);
        __m128 LoadVector2 = _mm_loadu_ps(f + 4);
#else
        __m128 LoadVector = _mm_load_ps(f);
        __m128 LoadVector2 = _mm_load_ps(f + 4);
#endif
        __m128i LoadVectorInt;
        __m128i LoadVector2Int;

        LoadVector = _mm_mul_ps(LoadVector, MultVec);
        LoadVector2 = _mm_mul_ps(LoadVector2, MultVec);

        LoadVectorInt = _mm_cvtps_epi32(LoadVector);
        LoadVector2Int = _mm_cvtps_epi32(LoadVector2);

        LoadVectorInt = _mm_packs_epi32(LoadVectorInt, LoadVector2Int);

        _mm_store_si128((__m128i*)c, LoadVectorInt);

        f+=8;
        c+=8;
    }
}

static void ClampToS16AndInterleave_sse2( S16* dest,
                                          F32* inp0,
                                          F32* inp1,
                                          F32 scale,
                                          U32 num )
{
    AUDIOFLOAT * RADRESTRICT f;
    S32* c;
    U32 i;
    SINTa diff = ( (char*) inp1 ) - ( (char*) inp0 );

    __m128 MultVec = _mm_set_ps(scale, scale, scale, scale);

    radassert(!(num%4));
    //radassert(!((UINTa)dest & 0xF));
    //radassert(!((UINTa)inp0 & 0xF));

    f=inp0;
    c = (S32*)dest;
    for (i = (num/4); i; i--)
    {
#ifdef CLAMP_SUPPORT_UNALIGNED_SOURCE
        __m128 LoadVector = _mm_loadu_ps(f);
        __m128 LoadVector2 = _mm_loadu_ps( (F32*)(((char*)f) + diff ) );
#else
        __m128 LoadVector = _mm_load_ps(f);
        __m128 LoadVector2 = _mm_load_ps( (F32*)(((char*)f) + diff ) );
#endif
        __m128i LoadVectorInt;
        __m128i LoadVector2Int;
        __m128i PackedOne;

        // [ x, y, z, w ]
        LoadVector = _mm_mul_ps(LoadVector, MultVec);
        LoadVector2 = _mm_mul_ps(LoadVector2, MultVec);

        // [ x, y, z, w ]
        LoadVectorInt = _mm_cvtps_epi32(LoadVector);
        LoadVector2Int = _mm_cvtps_epi32(LoadVector2);

        // [ x1, y1, z1, w1, x2, y2, z2, w2 ]
        PackedOne = _mm_packs_epi32(LoadVectorInt, LoadVector2Int);

        // [ x2, y2, z2, w2, x1, y1, z1, w1 ]
        LoadVector2Int = _mm_packs_epi32(LoadVector2Int, LoadVectorInt);

        // [ x1, x2, y1, y2, z1, z2, w1, w2 ]
        LoadVectorInt = _mm_unpacklo_epi16(PackedOne, LoadVector2Int);
        
        _mm_store_si128((__m128i*)c, LoadVectorInt);

        f+=4;
        c+=4;
    }

}

#endif

#elif defined(__RADPPC__) && defined(__MWERKS__) && (__MWERKS__ >= 0x2301)
static void ClampToS16( S16* dest,
                        F32* inp,
                        F32 scale,
                        U32 num )
{
    AUDIOFLOAT* f;
    S16* c;
    U32 quads = num >> 2;
    U32 remn = num & 3;

    f = inp;
    c = ((S16*)dest) - 1;

    while (quads)
    {
        register int v1 = (int) (f[0] * (AUDIOFLOAT)scale);
        register int v2 = (int) (f[1] * (AUDIOFLOAT)scale);
        register int v3 = (int) (f[2] * (AUDIOFLOAT)scale);
        register int v4 = (int) (f[3] * (AUDIOFLOAT)scale);

        if (v1 > 32767) v1 = 32767;
        else if (v1 < -32768) v1 = -32768;
        if (v2 > 32767) v2 = 32767;
        else if (v2 < -32768) v2 = -32768;
        if (v3 > 32767) v3 = 32767;
        else if (v3 < -32768) v3 = -32768;
        if (v4 > 32767) v4 = 32767;
        else if (v4 < -32768) v4 = -32768;

#ifdef CLAMP_FORCE_LE
        c++;
        STORE_LE_SWAP16(c, v1);
        c++;
        STORE_LE_SWAP16(c, v2);
        c++;
        STORE_LE_SWAP16(c, v3);
        c++;
        STORE_LE_SWAP16(c, v4);
#else
        *++c = v1;
        *++c = v2;
        *++c = v3;
        *++c = v4;
#endif
        f += 4;
        quads -- ;
    }

    while (remn)
    {
        register int v = (int) (*f * (AUDIOFLOAT)scale);
        if (v > 32767) v = 32767;
        else if (v < -32768) v = -32768;

#ifdef CLAMP_FORCE_LE
        c++;
        STORE_LE_SWAP16(c, v);
#else
        *++c = v;
#endif

        f++;
        remn--;
    }
}

static void ClampToS16AndInterleave( S16* dest,
                                     F32* inp0,
                                     F32* inp1,
                                     F32 scale,
                                     U32 num )
{
    AUDIOFLOAT* f;
    S16* c;
    U32 doubles = num >> 1;
    U32 singles = num & 1;
    register SINTa diff = ( (char*) inp1 ) - ( (char*) inp0 );

    f = inp0;
    c = ( (S16*) dest ) - 1;

    while (doubles)
    {
        register int vv1 = (int) ( ((F32*)f)[ 0 ] * (AUDIOFLOAT) scale );
        register int vv2 = (int) ( ((F32*)(((char*)f ) + diff ))[ 0 ] * (AUDIOFLOAT) scale );
        register int vv3 = (int) ( ((F32*)f)[ 1 ] * (AUDIOFLOAT) scale );
        register int vv4 = (int) ( ((F32*)(((char*)f ) + diff ))[ 1 ] * (AUDIOFLOAT) scale );

        if (vv1 > 32767) vv1 = 32767;
        else if (vv1 < -32768) vv1 = -32768;
        if (vv2 > 32767) vv2 = 32767;
        else if (vv2 < -32768) vv2 = -32768;
        if (vv3 > 32767) vv3 = 32767;
        else if (vv3 < -32768) vv3 = -32768;
        if (vv4 > 32767) vv4 = 32767;
        else if (vv4 < -32768) vv4 = -32768;

#ifdef CLAMP_FORCE_LE
        c++;
        STORE_LE_SWAP16(c, vv1);
        c++;
        STORE_LE_SWAP16(c, vv2);
        c++;
        STORE_LE_SWAP16(c, vv3);
        c++;
        STORE_LE_SWAP16(c, vv4);
#else
        *++c = vv1;
        *++c = vv2;
        *++c = vv3;
        *++c = vv4;
#endif
        
        f+=2;
        doubles--;
    }

    if (singles)
    {
        register int vv1 = (int) ( ((F32*)f)[ 0 ] * (AUDIOFLOAT) scale );
        register int vv2 = (int) ( ((F32*)(((char*)f ) + diff ))[ 0 ] * (AUDIOFLOAT) scale );

        if (vv1 > 32767) vv1 = 32767;
        else if (vv1 < -32768) vv1 = -32768;
        if (vv2 > 32767) vv2 = 32767;
        else if (vv2 < -32768) vv2 = -32768;

#ifdef CLAMP_FORCE_LE
        c++;
        STORE_LE_SWAP16(c, vv1);
        c++;
        STORE_LE_SWAP16(c, vv2);
#else
        *++c = vv1;
        *++c = vv2;
#endif
        f++;
    }
}

#elif defined( __RADPS2__ )

#include <eekernel.h>

#ifndef U128
#define U128 u_long128
#endif

typedef union ftu_clamp
{
    float f[4];
    U128 b;
} ftu_clamp;

#define InterleaveDecl \
    register U128 LeftVec, RightVec, LeftEEVec, RightEEVec; \
    register U128 lowest = 0xFFFF8000FFFF8000FFFF8000FFFF8000LL; \
    register U128 highest = 0x00007FFF00007FFF00007FFF00007FFFLL; \
    register U128 Low, High;

#define InterleaveImplement \
    asm volatile ("vmul %0, %1, %2"      : "=j" (LeftVec) : "j" (LeftVec), "j" (scaleVec)); \
    asm volatile ("vmul %0, %1, %2"      : "=j" (RightVec) : "j" (RightVec), "j" (scaleVec)); \
    asm volatile ("vftoi0 %0, %1"        : "=j" (LeftVec) : "j" (LeftVec)); \
    asm volatile ("vftoi0 %0, %1"        : "=j" (RightVec) : "j" (RightVec)); \
    asm volatile ("qmfc2 %0, %1"         : "=r" (LeftEEVec) : "j" (LeftVec) ); \
    asm volatile ("qmfc2 %0, %1"         : "=r" (RightEEVec) : "j" (RightVec)); \
    asm volatile ("pminw %0, %1, %2"     : "=r" (LeftEEVec) : "r" (LeftEEVec), "r" (highest)); \
    asm volatile ("pmaxw %0, %1, %2"     : "=r" (LeftEEVec) : "r" (LeftEEVec), "r" (lowest)); \
    asm volatile ("pminw %0, %1, %2"     : "=r" (RightEEVec) : "r" (RightEEVec), "r" (highest)); \
    asm volatile ("pmaxw %0, %1, %2"     : "=r" (RightEEVec) : "r" (RightEEVec), "r" (lowest)); \
    asm volatile ("ppach %0, $0, %1"     : "=r" (LeftEEVec) : "r" (LeftEEVec)); \
    asm volatile ("ppach %0, $0, %1"     : "=r" (RightEEVec) : "r" (RightEEVec)); \
    asm volatile ("pextlh %0, %1, %2"     : "=r" (LeftEEVec) : "r" (RightEEVec), "r" (LeftEEVec)); \
    asm volatile ("sq %0, 0(%1)"         : : "r" (LeftEEVec), "r" (c) : "memory" ); \
    c += 2; \
    f += 4; \
    f2 += 4;

#define LoadUnaligned(outeereg, f) \
    asm volatile ("ldr %0, 0(%1)"       : "=r" (Low) : "r" (f)); \
    asm volatile ("ldl %0, 7(%1)"       : "=r" (Low) : "r" (f), "r" (Low) ); \
    asm volatile ("ldr %0, 8(%1)"       : "=r" (High) : "r" (f) ); \
    asm volatile ("ldl %0, 15(%1)"       : "=r" (High) : "r" (f), "r" (High) ); \
    asm volatile ("pcpyld %0, %1, %2"   : "=r" (High) : "r" (High), "r" (Low) ); \
    asm volatile ("qmtc2 %1, %0"        : "=j" (outeereg) : "r" (High) ); \

#define LoadAligned8(outeereg, f) \
    asm volatile ("ld %0, 0(%1)"       : "=r" (Low) : "r" (f) ); \
    asm volatile ("ld %0, 8(%1)"       : "=r" (High) : "r" (f) ); \
    asm volatile ("pcpyld %0, %1, %2"   : "=r" (High) : "r" (High), "r" (Low) ); \
    asm volatile ("qmtc2 %1, %0"        : "=j" (outeereg) : "r" (High) );

#define LoadAligned16(outeereg, f) \
    asm volatile ("lqc2 %0, 0(%1)"       : "=j" (outeereg) : "r" (f) );

#define MonoDecl \
    register U128 LeftVec, Left2Vec, LeftEEVec, Left2EEVec; \
    register U128 lowest = 0xFFFF8000FFFF8000FFFF8000FFFF8000LL; \
    register U128 highest = 0x00007FFF00007FFF00007FFF00007FFFLL; \
    register U128 Low, High;

#define MonoImplement \
    asm volatile ("vmul %0, %1, %2"      : "=j" (LeftVec) : "j" (LeftVec), "j" (scaleVec)); \
    asm volatile ("vmul %0, %1, %2"      : "=j" (Left2Vec) : "j" (Left2Vec), "j" (scaleVec)); \
    asm volatile ("vftoi0 %0, %1"        : "=j" (LeftVec) : "j" (LeftVec)); \
    asm volatile ("vftoi0 %0, %1"        : "=j" (Left2Vec) : "j" (Left2Vec)); \
    asm volatile ("qmfc2 %0, %1"         : "=r" (LeftEEVec) : "j" (LeftVec) ); \
    asm volatile ("qmfc2 %0, %1"         : "=r" (Left2EEVec) : "j" (Left2Vec)); \
    asm volatile ("pminw %0, %1, %2"     : "=r" (LeftEEVec) : "r" (LeftEEVec), "r" (highest)); \
    asm volatile ("pmaxw %0, %1, %2"     : "=r" (LeftEEVec) : "r" (LeftEEVec), "r" (lowest)); \
    asm volatile ("pminw %0, %1, %2"     : "=r" (Left2EEVec) : "r" (Left2EEVec), "r" (highest)); \
    asm volatile ("pmaxw %0, %1, %2"     : "=r" (Left2EEVec) : "r" (Left2EEVec), "r" (lowest)); \
    asm volatile ("ppach %0, $0, %1"     : "=r" (LeftEEVec) : "r" (LeftEEVec)); \
    asm volatile ("ppach %0, $0, %1"     : "=r" (Left2EEVec) : "r" (Left2EEVec)); \
    asm volatile ("pcpyld %0, %1, %2"     : "=r" (LeftEEVec) : "r" (Left2EEVec), "r" (LeftEEVec)); \
    asm volatile ("sq %0, 0(%1)"         : : "r" (LeftEEVec), "r" (c) : "memory" ); \
    c += 2; \
    f += 8;

static void ClampToS16( S16* dest,
                        F32* inp,
                        F32 scale,
                        U32 num )
{
    register const F32 * f;
    register U64 * c;
    register U32 i;

    static RAD_ALIGN(ftu_clamp, scaleSrc, 16);
    register U128 scaleVec;
   
    scaleSrc.f[0]=scale;
    scaleSrc.f[1]=scale;
    scaleSrc.f[2]=scale;
    scaleSrc.f[3]=scale;
    scaleVec = scaleSrc.b;

    f = inp;
    c = (U64*) dest;

#ifdef CLAMP_SUPPORT_UNALIGNED_SOURCE
    if ( (UINTa)inp & 0x7 )
    {
        // unaligned
        for ( i = num ; i; i -= 8 )
        {
            MonoDecl;
            LoadUnaligned(LeftVec, f);
            LoadUnaligned(Left2Vec, f + 4);
            MonoImplement;
        }
    }
    else if ( (UINTa)inp & 0xF)
    {
        // 8 byte aligned
        for ( i = num ; i; i -= 8 )
        {
            MonoDecl;
            LoadAligned8(LeftVec, f);
            LoadAligned8(Left2Vec, f + 4);
            MonoImplement;
        }
    }
    else
    {
#endif
        // 16 byte aligned.
        for ( i = num ; i; i -= 8 )
        {
            MonoDecl;
            LoadAligned16(LeftVec, f);
            LoadAligned16(Left2Vec, f + 4);
            MonoImplement;
        }
#ifdef CLAMP_SUPPORT_UNALIGNED_SOURCE
    }
#endif
}

static void ClampToS16AndInterleave( S16* dest,
                                     F32* inp0,
                                     F32* inp1,
                                     F32 scale,
                                     U32 num )
{
    register const F32 * f;
    register const F32 * f2;
    register U64 * c;
    register U32 i;

    static RAD_ALIGN(ftu_clamp, scaleSrc, 16);
    register U128 scaleVec;
   
    scaleSrc.f[0]=scale;
    scaleSrc.f[1]=scale;
    scaleSrc.f[2]=scale;
    scaleSrc.f[3]=scale;
    scaleVec = scaleSrc.b;

    f = inp0;
    f2 = inp1;
    c = (U64*) dest;

#ifdef CLAMP_SUPPORT_UNALIGNED_SOURCE
    if ( (UINTa)inp0 & 0x7 )
    {
        radassert( ((UINTa)inp1 & 0x7) == ((UINTa)inp0 & 0x7) );
        // First source is completely unaligned.
        for ( i = num ; i; i -= 4 )
        {
            InterleaveDecl;
            LoadUnaligned(LeftVec, f);
            LoadUnaligned(RightVec, f2);
            InterleaveImplement;
        }
    }
    else if ( (UINTa)inp0 & 0xF )
    {
        radassert( ((UINTa)inp1 & 0xf) == ((UINTa)inp0 & 0xF) );
        // First source is 8 aligned.
        for ( i = num ; i; i -= 4 )
        {
            InterleaveDecl;
            LoadAligned8(LeftVec, f);
            LoadAligned8(RightVec, f2);
            InterleaveImplement;
        }
    }
    else
    {
#endif
        // inp1 is 16 byte aligned. (Both)
        for ( i = num ; i; i -= 4 )
        {
            InterleaveDecl;
            LoadAligned16(LeftVec, f);
            LoadAligned16(RightVec, f2);
            InterleaveImplement;
        }
#ifdef CLAMP_SUPPORT_UNALIGNED_SOURCE
    }
#endif
}

#elif defined( __RADSPU__)

static void ClampToS16( S16* dest,
                        F32* inp,
                        F32 scale,
                        U32 num )
{
  vector float * RADRESTRICT f4;
  vector signed short * RADRESTRICT c4;
  U32 i;
  vector float scale4;

  scale4 = spu_splats(scale);

  f4=(vector float*)inp;
  c4=(vector signed short*)dest;
  for (i = num/8; i; i--) 
  {
    vector float s41;
    vector signed int i41;
    vector float s42;
    vector signed int i42;

    vector signed short h4;

    s41 = *f4++;
    s42 = *f4++;
    s41 = s41 * scale4;
    s42 = s42 * scale4;
    i41 = spu_convts( s41, 16);
    i42 = spu_convts( s42, 16);

#ifdef CLAMP_FORCE_LE
    h4 = (vec_short8)spu_shuffle(i41, i42, ((vec_uchar16){ 1,  0,  5,  4,  9,  8, 13, 12,
                                                          17, 16, 21, 20, 25, 24, 29, 28}));
#else
    h4 = (vec_short8)spu_shuffle(i41, i42, ((vec_uchar16){ 0,  1,  4,  5,  8,  9, 12, 13,
                                                          16, 17, 20, 21, 24, 25, 28, 29}));
#endif

     *c4++ = h4;
   }
}

static void ClampToS16AndInterleave( S16* dest,
                                     F32* inp0,
                                     F32* inp1,
                                     F32 scale,
                                     U32 num )
{
  vector float * RADRESTRICT f41;
  vector float * RADRESTRICT f42;
  vector signed short * RADRESTRICT c4;
  U32 i;
  vector float scale4;

  scale4 = spu_splats(scale);

  f41=(vector float*)inp0;
  f42=(vector float*)inp1;
  c4=(vector signed short*)dest;
  for (i = num/4; i; i--) 
  {
    vector float s41;
    vector signed int i41;
    vector float s42;
    vector signed int i42;

    vector signed short h4;

    s41 = *f41++;
    s42 = *f42++;
    s41 = s41 * scale4;
    s42 = s42 * scale4;
    i41 = spu_convts( s41, 16);
    i42 = spu_convts( s42, 16);

#ifdef CLAMP_FORCE_LE
    h4 = (vec_short8)spu_shuffle(i41, i42, ((vec_uchar16){ 1,  0, 17, 16,  5,  4, 21, 20,
                                                           9,  8, 25, 24, 13, 12, 29, 28}));
#else
    h4 = (vec_short8)spu_shuffle(i41, i42, ((vec_uchar16){ 0,  1, 16, 17,  4,  5, 20, 21,
                                                           8,  9, 24, 25, 12, 13, 28, 29}));
#endif

    *c4++ = h4;
   }

}

#elif defined(__RADPS3__)

static void ClampToS16( S16* dest,
                        F32* inp,
                        F32 scale,
                        U32 num )
{
    AUDIOFLOAT * RADRESTRICT f;
    S16* c;
    U32 i;

    vector float MultVec = {scale, scale, scale, scale};
    vector float ZeroVector = {0};

#ifdef CLAMP_FORCE_LE
    const vector unsigned int LEPermVector = {0x01000302, 0x05040706, 0x09080b0a, 0x0d0c0f0e};
#endif

    radassert(!(num%8));
    radassert(!((unsigned int)dest & 0xF));

#ifdef CLAMP_SUPPORT_UNALIGNED_SOURCE
    if ((UINTa)inp & 0xF)
    {
        // Unaligned load.
        f=inp;
        c = (S16*)dest;
        for (i = (num/8); i; i--)
        {
            vector float LoadVector_a = vec_lvlx(0, f);
            vector float LoadVector_b = vec_lvrx(16, f);
            vector float LoadVector2_a = vec_lvlx(16, f);
            vector float LoadVector2_b = vec_lvrx(32, f);
            vector float LoadVector = vec_or(LoadVector_a, LoadVector_b);
            vector float LoadVector2 = vec_or(LoadVector2_a, LoadVector2_b);

            LoadVector = vec_madd(LoadVector, MultVec, ZeroVector);
            LoadVector2 = vec_madd(LoadVector2, MultVec, ZeroVector);

            LoadVector2 = (vector float)vec_cts(LoadVector2, 0);
            LoadVector = (vector float)vec_cts(LoadVector, 0);

            LoadVector = (vector float)vec_packs((vector int)LoadVector, (vector int)LoadVector2);

#ifdef CLAMP_FORCE_LE
            // Permute to little endian.
            LoadVector = vec_perm(LoadVector, LoadVector, (vector unsigned char)LEPermVector);
#endif
                
            vec_st(LoadVector, 0, (vector float*)c);

            f+=8;
            c+=8;
        }
    }
    else
    {
#endif
        // Aligned load.
        radassert(!((unsigned int)inp & 0xF));

        f=inp;
        c = (S16*)dest;
        for (i = (num/8); i; i--)
        {
            vector float LoadVector = vec_ld(0, f);
            vector float LoadVector2 = vec_ld(16, f);

            LoadVector = vec_madd(LoadVector, MultVec, ZeroVector);
            LoadVector2 = vec_madd(LoadVector2, MultVec, ZeroVector);

            LoadVector2 = (vector float)vec_cts(LoadVector2, 0);
            LoadVector = (vector float)vec_cts(LoadVector, 0);

            LoadVector = (vector float)vec_packs((vector int)LoadVector, (vector int)LoadVector2);

#ifdef CLAMP_FORCE_LE
            // Permute to little endian.
            LoadVector = vec_perm(LoadVector, LoadVector, (vector unsigned char)LEPermVector);
#endif
                
            vec_st(LoadVector, 0, (vector float*)c);

            f+=8;
            c+=8;
        }
#ifdef CLAMP_SUPPORT_UNALIGNED_SOURCE
    }
#endif
}

static void ClampToS16AndInterleave( S16* dest,
                                     F32* inp0,
                                     F32* inp1,
                                     F32 scale,
                                     U32 num )
{
    AUDIOFLOAT * RADRESTRICT f;
    AUDIOFLOAT * RADRESTRICT f2;
    S32* c;
    U32 i;

    vector float MultVec = {scale, scale, scale, scale};
    vector float ZeroVector = {0};
#ifdef CLAMP_FORCE_LE
    const vector unsigned char PermConst = {
        1, 0,
        9, 8,
        3, 2,
        11, 10,
        5, 4,
        13, 12,
        7, 6,
        15, 14};
#else
    const vector unsigned char PermConst = {
        0, 1,
        8, 9,
        2, 3,
        10, 11,
        4, 5,
        12, 13,
        6, 7,
        14, 15};
#endif
    radassert(!(num%4));
    radassert(!((unsigned int)dest & 0xF));

#ifdef CLAMP_SUPPORT_UNALIGNED_SOURCE
    if (((UINTa)inp0 & 0xF) || ((UINTa)inp1 & 0xF))
    {
        // Unaligned source.
        f=inp0;
        f2=inp1;
        c = (S32*)dest;
        for (i = (num/4); i; i--)
        {
            vector float LoadVector_a = vec_lvlx(0, f);
            vector float LoadVector_b = vec_lvrx(16, f);
            vector float LoadVector2_a = vec_lvlx(0, f2);
            vector float LoadVector2_b = vec_lvrx(16, f2);
            vector float LoadVector = vec_or(LoadVector_a, LoadVector_b);
            vector float LoadVector2 = vec_or(LoadVector2_a, LoadVector2_b);

            LoadVector = vec_madd(LoadVector, MultVec, ZeroVector);
            LoadVector2 = vec_madd(LoadVector2, MultVec, ZeroVector);

            LoadVector2 = (vector float)vec_cts(LoadVector2, 0);
            LoadVector = (vector float)vec_cts(LoadVector, 0);

            LoadVector = (vector float)vec_packs((vector int)LoadVector, (vector int)LoadVector2);
            LoadVector = vec_perm(LoadVector, LoadVector, PermConst);

            vec_st(LoadVector, 0, (vector float*)c);

            f+=4;
            f2+=4;
            c+=4;
        }
    }
    else
    {
#endif
        radassert(!((UINTa)inp0 & 0xF));
        radassert(!((UINTa)inp1 & 0xF));

        f=inp0;
        f2=inp1;
        c = (S32*)dest;
        for (i = (num/4); i; i--)
        {
            vector float LoadVector = vec_ld(0, f);
            vector float LoadVector2 = vec_ld(0, f2);

            LoadVector = vec_madd(LoadVector, MultVec, ZeroVector);
            LoadVector2 = vec_madd(LoadVector2, MultVec, ZeroVector);

            LoadVector2 = (vector float)vec_cts(LoadVector2, 0);
            LoadVector = (vector float)vec_cts(LoadVector, 0);

            LoadVector = (vector float)vec_packs((vector int)LoadVector, (vector int)LoadVector2);
            LoadVector = vec_perm(LoadVector, LoadVector, PermConst);

            vec_st(LoadVector, 0, (vector float*)c);

            f+=4;
            f2+=4;
            c+=4;
        }
#ifdef CLAMP_SUPPORT_UNALIGNED_SOURCE
    }
#endif
}
#elif defined(__RADPSP__)


static void ClampToS16( S16* dest,
                        F32* inp,
                        F32 scale,
                        U32 num )
{
    AUDIOFLOAT * RADRESTRICT f;
    register S16* c;
    U32 i;

    radassert(!(num%8));
    radassert(!((unsigned int)dest & 0xF));

    // the ftoi divides by 65536, so multiply by it here.
    float kay = 65536.0f * scale;
    __asm__ ("lv.s s200, 0+%0\n" : : "m" (kay));

#ifdef CLAMP_SUPPORT_UNALIGNED_SOURCE
    switch ((U32)inp % 16)
    {
    case 0:
        {
#endif
            radassert(!((unsigned int)inp & 0xF));
            // aligned
            f=inp;
            c = dest;
            for (i = (num/8); i; i--)
            {
                __asm__ (
                    "lv.q c000, 0(%1)\n"
                    "lv.q c010, 16(%1)\n"

                    // Scale
                    "vscl.q c000, c000, s200\n"
                    "vscl.q c010, c010, s200\n"

                    // Convert to int
                    "vf2iz.q c000, c000, 0\n"
                    "vf2iz.q c010, c010, 0\n"

                    // Convert to short, and move into a line.
                    "vi2s.q c000, c000\n"
                    "vi2s.q c002, c010\n"

                    "sv.q c000, %0\n"
                    :
                    : "m" (*c), "r" (f)
                    );

                f+=8;
                c+=8;
            }

#ifdef CLAMP_SUPPORT_UNALIGNED_SOURCE
            break;
        }
    case 4:
        {
            f = (F32*)(((U32)inp) & ~0xf);
            c = dest;
            for (i = (num/8); i; i--)
            {
                __asm__ (
                    "lv.q c000, 0(%1)\n"
                    "lv.q c010, 16(%1)\n"
                    "lv.q c020, 32(%1)\n"
                    "vmov.t c000, c001\n"
                    "vmov.s s003, s010\n"
                    "vmov.t c010, c011\n"
                    "vmov.s s013, s020\n"

                    // Scale
                    "vscl.q c000, c000, s200\n"
                    "vscl.q c010, c010, s200\n"

                    // Convert to int
                    "vf2iz.q c000, c000, 0\n"
                    "vf2iz.q c010, c010, 0\n"

                    // Convert to short, and move into a line.
                    "vi2s.q c000, c000\n"
                    "vi2s.q c002, c010\n"

                    "sv.q c000, %0\n"
                    :
                    : "m" (*c), "r" (f)
                    );

                f+=8;
                c+=8;
            }
            break;
        }
    case 8:
        {
            f = (F32*)(((U32)inp) & ~0xf);
            c = dest;
            for (i = (num/8); i; i--)
            {
                __asm__ (
                    "lv.q c000, 0(%1)\n"
                    "lv.q c010, 16(%1)\n"
                    "lv.q c020, 32(%1)\n"
                    "vmov.p c000, c002\n"
                    "vmov.p c002, c010\n"
                    "vmov.p c010, c012\n"
                    "vmov.p c012, c020\n"

                    // Scale
                    "vscl.q c000, c000, s200\n"
                    "vscl.q c010, c010, s200\n"

                    // Convert to int
                    "vf2iz.q c000, c000, 0\n"
                    "vf2iz.q c010, c010, 0\n"

                    // Convert to short, and move into a line.
                    "vi2s.q c000, c000\n"
                    "vi2s.q c002, c010\n"

                    "sv.q c000, %0\n"
                    :
                    : "m" (*c), "r" (f)
                    );

                f+=8;
                c+=8;
            }
            break;
        }
    case 12:
        {
            f = (F32*)(((U32)inp) & ~0xf);
            c = dest;
            for (i = (num/8); i; i--)
            {
                __asm__ (
                    "lv.q c000, 0(%1)\n"
                    "lv.q c010, 16(%1)\n"
                    "lv.q c020, 32(%1)\n"
                    "vmov.s s000, s003\n"
                    "vmov.t c001, c010\n"
                    "vmov.s s010, s013\n"
                    "vmov.t c011, c020\n"

                    // Scale
                    "vscl.q c000, c000, s200\n"
                    "vscl.q c010, c010, s200\n"

                    // Convert to int
                    "vf2iz.q c000, c000, 0\n"
                    "vf2iz.q c010, c010, 0\n"

                    // Convert to short, and move into a line.
                    "vi2s.q c000, c000\n"
                    "vi2s.q c002, c010\n"

                    "sv.q c000, %0\n"
                    :
                    : "m" (*c), "r" (f)
                    );

                f+=8;
                c+=8;
            }
            break;
        }
    } // end switch for alignment.
#endif // clamp unaligned
}

static void ClampToS16AndInterleave( S16* dest,
                                     F32* inp0,
                                     F32* inp1,
                                     F32 scale,
                                     U32 num )
{
    register AUDIOFLOAT * RADRESTRICT f;
    register AUDIOFLOAT * RADRESTRICT f2;
    register S16* c = dest;
    U32 i;

    radassert(!(num%4));
    radassert(!((unsigned int)dest & 0xF));


    // The float to int conversion divides by 65536, so premultiply.
    float kay = 65536.0f * scale;
    __asm__ ("lv.s s201, 0 +%0\n" : : "m" (kay));

#ifdef CLAMP_SUPPORT_UNALIGNED_SOURCE
    switch (((U32)inp0) % 16)
    {
    case 0:
        {
#endif
            radassert(!((UINTa)inp0 & 0xF));
            radassert(!((UINTa)inp1 & 0xF));
            f=inp0;
            f2=inp1;
            c = dest;
            for (i = (num/4); i; i--)
            {
                // Load
                __asm__ (
                    "lv.q c000, 0(%1)\n"
                    "lv.q c010, 0(%2)\n"

                    // Interleave
                    "vmov.p c100, r000\n"
                    "vmov.p c102, r001\n"
                    "vmov.p c110, r002\n"
                    "vmov.p c112, r003\n"

                    // Scale
                    "vscl.q c000, c100, s201\n"
                    "vscl.q c010, c110, s201\n"

                    // Convert to int
                    "vf2iz.q c000, c000, 0\n"
                    "vf2iz.q c010, c010, 0\n"

                    // Convert to short, and move into a line.
                    "vi2s.q c000, c000\n"
                    "vi2s.q c002, c010\n"

                    "sv.q c000, %0\n"
                    : 
                    : "m"(*c), "r" (f), "r" (f2)
                    );

                f += 4;
                f2 += 4;
                c += 8;
            }

#ifdef CLAMP_SUPPORT_UNALIGNED_SOURCE
            break;
        }
    case 4:
        {
            f=(F32*)(((U32)inp0) & ~0xF);
            f2=(F32*)(((U32)inp1) & ~0xF);
            c = dest;
            for (i = (num/4); i; i--)
            {
                // Load
                __asm__ (
                    "lv.q c000, 0(%1)\n"
                    "lv.q c010, 16(%1)\n"
                    "lv.q c020, 0(%2)\n"
                    "lv.q c030, 16(%2)\n"
                    "vmov.t c000, c001\n"
                    "vmov.s s003, s010\n"
                    "vmov.t c010, c021\n"
                    "vmov.s s013, s030\n"

                    // Interleave
                    "vmov.p c100, r000\n"
                    "vmov.p c102, r001\n"
                    "vmov.p c110, r002\n"
                    "vmov.p c112, r003\n"

                    // Scale
                    "vscl.q c000, c100, s201\n"
                    "vscl.q c010, c110, s201\n"

                    // Convert to int
                    "vf2iz.q c000, c000, 0\n"
                    "vf2iz.q c010, c010, 0\n"

                    // Convert to short, and move into a line.
                    "vi2s.q c000, c000\n"
                    "vi2s.q c002, c010\n"

                    "sv.q c000, %0\n"
                    : 
                    : "m"(*c), "r" (f), "r" (f2)
                    );

                f += 4;
                f2 += 4;
                c += 8;
            }
            break;
        }
    case 8:
        {
            f=(F32*)(((U32)inp0) & ~0xF);
            f2=(F32*)(((U32)inp1) & ~0xF);
            c = dest;
            for (i = (num/4); i; i--)
            {
                // Load
                __asm__ (
                    "lv.q c000, 0(%1)\n"
                    "lv.q c010, 16(%1)\n"
                    "lv.q c020, 0(%2)\n"
                    "lv.q c030, 16(%2)\n"
                    "vmov.p c000, c002\n"
                    "vmov.p c002, c010\n"
                    "vmov.p c010, c022\n"
                    "vmov.p c012, c030\n"

                    // Interleave
                    "vmov.p c100, r000\n"
                    "vmov.p c102, r001\n"
                    "vmov.p c110, r002\n"
                    "vmov.p c112, r003\n"

                    // Scale
                    "vscl.q c000, c100, s201\n"
                    "vscl.q c010, c110, s201\n"

                    // Convert to int
                    "vf2iz.q c000, c000, 0\n"
                    "vf2iz.q c010, c010, 0\n"

                    // Convert to short, and move into a line.
                    "vi2s.q c000, c000\n"
                    "vi2s.q c002, c010\n"

                    "sv.q c000, %0\n"
                    : 
                    : "m"(*c), "r" (f), "r" (f2)
                    );

                f += 4;
                f2 += 4;
                c += 8;
            }
            break;
        }
    case 12:
        {
            f=(F32*)(((U32)inp0) & ~0xF);
            f2=(F32*)(((U32)inp1) & ~0xF);
            c = dest;
            for (i = (num/4); i; i--)
            {
                // Load
                __asm__ (
                    "lv.q c000, 0(%1)\n"
                    "lv.q c010, 16(%1)\n"
                    "lv.q c020, 0(%2)\n"
                    "lv.q c030, 16(%2)\n"
                    "vmov.s s000, s003\n"
                    "vmov.t c001, c010\n"
                    "vmov.s s010, s023\n"
                    "vmov.t c011, c030\n"

                    // Interleave
                    "vmov.p c100, r000\n"
                    "vmov.p c102, r001\n"
                    "vmov.p c110, r002\n"
                    "vmov.p c112, r003\n"

                    // Scale
                    "vscl.q c000, c100, s201\n"
                    "vscl.q c010, c110, s201\n"

                    // Convert to int
                    "vf2iz.q c000, c000, 0\n"
                    "vf2iz.q c010, c010, 0\n"

                    // Convert to short, and move into a line.
                    "vi2s.q c000, c000\n"
                    "vi2s.q c002, c010\n"

                    "sv.q c000, %0\n"
                    : 
                    : "m"(*c), "r" (f), "r" (f2)
                    );

                f += 4;
                f2 += 4;
                c += 8;
            }
            break;
        }
    } // end switch alignment
#endif
}

#elif defined(__RADXENON__)

#define radfsel(dest,cond,cmp1,cmp2) dest = (F32)__fsel(cond,cmp1,cmp2)

static void ClampToS16( S16* dest,
                        F32* inp,
                        F32 scale,
                        U32 num )
{
    AUDIOFLOAT * RADRESTRICT f;
    S16* c;
    U32 i;

    __vector4 MultVec = {scale, scale, scale, scale};
    __vector4 ZeroVector = {0};

#ifdef CLAMP_FORCE_LE
    static RAD_ALIGN(U32, LEPermSource[4], 16) = {0x01000302, 0x05040706, 0x09080b0a, 0x0d0c0f0e};
    __vector4 LEPermVector = __lvx(LEPermSource, 0);
#endif

    radassert(!(num%8));
    radassert(!((unsigned int)dest & 0xF)); // only check alignment if there is work to be done.

#ifdef CLAMP_SUPPORT_UNALIGNED_SOURCE
    if ((unsigned int)inp & 0xF)
    {
        // Unaligned copy.
        f=inp;
        c = (S16*)dest;
        for (i = (num/8); i; i--)
        {
            __vector4 LoadVector_a = __lvlx(f, 0);
            __vector4 LoadVector_b = __lvrx(f, 16);
            __vector4 LoadVector2_a = __lvlx(f, 16);
            __vector4 LoadVector2_b = __lvrx(f, 32);
            __vector4 LoadVector = __vor(LoadVector_a, LoadVector_b);
            __vector4 LoadVector2 = __vor(LoadVector2_a, LoadVector2_b);

            LoadVector = __vmaddfp(LoadVector, MultVec, ZeroVector);
            LoadVector2 = __vmaddfp(LoadVector2, MultVec, ZeroVector);

            LoadVector2 = __vctsxs(LoadVector2, 0);
            LoadVector = __vctsxs(LoadVector, 0);

            LoadVector = __vpkswss(LoadVector, LoadVector2);

#ifdef CLAMP_FORCE_LE
            // Permute to little endian.
            LoadVector = __vperm(LoadVector, LoadVector, LEPermVector);
#endif

            __stvx(LoadVector, c, 0);

            f+=8;
            c+=8;
        }
    }
    else
    {
#endif
        // Aligned copy.
        //radassert(!((unsigned int)inp & 0xF));
        f=inp;
        c = (S16*)dest;
        for (i = (num/8); i; i--)
        {
            __vector4 LoadVector = __lvx(f, 0);
            __vector4 LoadVector2 = __lvx(f, 16);

            LoadVector = __vmaddfp(LoadVector, MultVec, ZeroVector);
            LoadVector2 = __vmaddfp(LoadVector2, MultVec, ZeroVector);

            LoadVector2 = __vctsxs(LoadVector2, 0);
            LoadVector = __vctsxs(LoadVector, 0);

            LoadVector = __vpkswss(LoadVector, LoadVector2);

    #ifdef CLAMP_FORCE_LE
            // Permute to little endian.
            LoadVector = __vperm(LoadVector, LoadVector, LEPermVector);
    #endif

            __stvx(LoadVector, c, 0);

            f+=8;
            c+=8;
        }
#ifdef CLAMP_SUPPORT_UNALIGNED_SOURCE
    } // end aligned.
#endif
}

static void ClampToS16AndInterleave( S16* dest,
                                     F32* inp0,
                                     F32* inp1,
                                     F32 scale,
                                     U32 num )
{
    AUDIOFLOAT * RADRESTRICT f;
    AUDIOFLOAT * RADRESTRICT f2;
    S32 * RADRESTRICT c;
    U32 i;

    __vector4 MultVec = {scale, scale, scale, scale};
    __vector4 ZeroVector = {0};

#ifdef CLAMP_FORCE_LE
    const int __declspec(align(16)) PermConst[4] = { 
        (1 << 24) + (0 << 16) + (9 << 8) + (8),
        (3 << 24) + (2 << 16) + (11 << 8) + 10,
        (5 << 24) + (4 << 16) + (13 << 8) + 12,
        (7 << 24) + (6 << 16) + (15 << 8) + 14};
#else
    const int __declspec(align(16)) PermConst[4] = { 
        (0 << 24) + (1 << 16) + (8 << 8) + (9),
        (2 << 24) + (3 << 16) + (10 << 8) + 11,
        (4 << 24) + (5 << 16) + (12 << 8) + 13,
        (6 << 24) + (7 << 16) + (14 << 8) + 15};
#endif

    radassert(!(num%4));
    radassert(!((unsigned int)dest & 0xF));

#ifdef CLAMP_SUPPORT_UNALIGNED_SOURCE
    if (((unsigned int)inp0 & 0xF) || ((unsigned int)inp1 & 0xF))
    {
        // Unaligned
        f=inp0;
        f2=inp1;
        c = (S32*)dest;
        for (i = (num/4); i; i--)
        {
            __vector4 LoadVector_a = __lvlx(f, 0);
            __vector4 LoadVector_b = __lvrx(f, 16);
            __vector4 LoadVector2_a = __lvlx(f2, 0);
            __vector4 LoadVector2_b = __lvrx(f2, 16);
            __vector4 LoadVector = __vor(LoadVector_a, LoadVector_b);
            __vector4 LoadVector2 = __vor(LoadVector2_a, LoadVector2_b);

            LoadVector = __vmaddfp(LoadVector, MultVec, ZeroVector);
            LoadVector2 = __vmaddfp(LoadVector2, MultVec, ZeroVector);

            LoadVector2 = __vctsxs(LoadVector2, 0);
            LoadVector = __vctsxs(LoadVector, 0);

            LoadVector = __vpkswss(LoadVector, LoadVector2);
            LoadVector = __vperm(LoadVector, LoadVector, *(__vector4*)PermConst);

            __stvx(LoadVector, c, 0);

            f+=4;
            f2+=4;
            c+=4;
        }
    }
    else
    {
#endif
        //radassert(!((unsigned int)inp0 & 0xF));
        //radassert(!((unsigned int)inp1 & 0xF));

        f=inp0;
        f2=inp1;
        c = (S32*)dest;
        for (i = (num/4); i; i--)
        {
            __vector4 LoadVector = __lvx(f, 0);
            __vector4 LoadVector2 = __lvx(f2, 0);

            LoadVector = __vmaddfp(LoadVector, MultVec, ZeroVector);
            LoadVector2 = __vmaddfp(LoadVector2, MultVec, ZeroVector);

            LoadVector2 = __vctsxs(LoadVector2, 0);
            LoadVector = __vctsxs(LoadVector, 0);

            LoadVector = __vpkswss(LoadVector, LoadVector2);
            LoadVector = __vperm(LoadVector, LoadVector, *(__vector4*)PermConst);

            __stvx(LoadVector, c, 0);

            f+=4;
            f2+=4;
            c+=4;
        }
#ifdef CLAMP_SUPPORT_UNALIGNED_SOURCE
    } // end if aligned.
#endif
}

#else

// Use the c versions if we have no speedy version.
#define ClampToS16AndInterleave ClampToS16AndInterleave_c
#define ClampToS16 ClampToS16_c
#define DO_SCALARS

#endif

#ifdef DO_SCALARS

static void ClampToS16_c( S16* dest,
                          F32* inp,
                          F32 scale,
                          U32 num )
{
  AUDIOFLOAT* f;
  S16* c;
  U32 i;

  f=inp;
  c=(S16*)dest;
  for (i = num; i; i--) {
    S32 v=(S32)(*f * ((AUDIOFLOAT)scale));
    S16 v16 = (v>=32767)?(S16)32767:((v<=-32768)?(S16)-32768:(S16)v);
#ifdef CLAMP_FORCE_LE
    STORE_LE_SWAP16(c, v16);
#else
    *c = v16;
#endif
    c++;
    f++;
  }
}

static void ClampToS16AndInterleave_c( S16* dest,
                                       F32* inp0,
                                       F32* inp1,
                                       F32 scale,
                                       U32 num )
{
  AUDIOFLOAT * f;
  AUDIOFLOAT * f2;

  S16 * c;
  U32 i;

  f = inp0;
  f2 = inp1;
  c = (S16*) dest;
  for ( i = num ; i-- ; )
  {
    S32 v;
    S16 v16;

    v = (S32) ( f[ 0 ] * scale );
    v16 = ( v >= 32767 ) ? (S16) 32767 : ( ( v <= -32768 ) ? (S16) -32768 : (S16) v );
#ifdef CLAMP_FORCE_LE
    STORE_LE_SWAP16(c, v16);
#else
    *c = v16;
#endif
    c++;
    v = (S32) ( f2[ 0 ] * scale );
    v16 = ( v >= 32767 ) ? (S16) 32767 : ( ( v <= -32768 ) ? (S16) -32768 : (S16) v );
#ifdef CLAMP_FORCE_LE
    STORE_LE_SWAP16(c, v16);
#else
    *c = v16;
#endif
    c++;
    ++f;
    ++f2;
  }
}

#endif
