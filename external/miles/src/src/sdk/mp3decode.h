//############################################################################
//##                                                                        ##
//##  Miles Sound System                                                    ##
//##                                                                        ##
//##  MPAUDIO.H: Private header file for MPEG decoder                       ##
//##                                                                        ##
//##  Version 1.00 of 6-Apr-98: Initial                                     ##
//##                                                                        ##
//##  Author: John Miles                                                    ##
//##                                                                        ##
//############################################################################
//##                                                                        ##
//##  Contact RAD Game Tools at 425-893-4300 for technical support.         ##
//##                                                                        ##
//############################################################################

#include "mss.h"
#include "imssapi.h"
#include "mp3_direct.h"

#define SBLIMIT 32                // 32 subbands
#define SSLIMIT 18                // 18 frequency lines/subband

#define HEAD_BUFSIZE   64         // Header buffer = 64 bytes
#define STREAM_BUFSIZE 4096       // Stream buffer = 2 x 2K blocks
#define FRAME_BUFSIZE  9600       // about 4 frames

#define MPG_MD_STEREO       0
#define MPG_MD_JOINT_STEREO 1
#define MPG_MD_DUAL_CHANNEL 2
#define MPG_MD_MONO         3

#define MPG_MD_LR_LR        0
#define MPG_MD_LR_I         1
#define MPG_MD_MS_LR        2
#define MPG_MD_MS_I         3

#ifndef PI
#define PI 3.14159265358979f
#endif

extern S32 ASI_started;

extern C8 ASI_error_text[256];

struct ASISTREAM;

typedef S32 (*FRAMEFN)(struct ASISTREAM *STR);
typedef S32 (*FINFOFN)(struct ASISTREAM *STR);

//
// Huffman source table representation
//

struct HTABLE
{
   U8 x;
   U8 y;
   U8 len;
   U8 padding;
   C8 *string;
};

//
// Huffman tree node
//
// Root node is index 0; any child pointer to node 0 indicates that
// this is a terminal node with valid x,y members
//

struct HUFFNODE
{
   S16 child[2];
   S16 x;
   S16 y;
};

//
// Structure for layer-3 scale factors, from ISO reference source
//

typedef struct _III_scalefac_t
{
   S32 l[23];            // [cb]
   S32 s[3][13];         // [window][cb]
} III_scalefac_t[2];       // [ch]

//
// Stream structure
//

typedef struct ASISTREAM
{
   // the alignment of these variables is very specific to cause "u" to be aligned
   //   in both 32-bit and 64-bit.  Also, the top part of this structure is duplicated
   //   in the spu code in wavefile.cpp, so that the spu weirdness doesn't creep all
   //   over the code.

   //
   // Output frame buffer
   //
   // MPEG streams are decoded one frame at a time, and returned to the
   // application on demand
   //

#ifndef IS_DOS
   S64 emitted_sample_count;        // # of mono or stereo 16-bit PCM samples returned to caller
#else
   S32 emitted_sample_count;
#endif
   U8  *frame_buffer;
   S16 *samples;                // points into frame_buffer
   S32 frame_size;                  // Size of output frame in bytes
   S32 output_cursor;               // Next location from which to copy output data

   //
   // Layer-dependent function pointers
   //

   FRAMEFN frame_function;
   FINFOFN frame_info_function;

// =================
//#ifdef IS_PS2
   F32 dmaCacheLinePadPreUS[16];    // 1 cache line to avoid problems with DMA/cache interactions
//#endif

   //
   // Persistent storage for POLY
   //

   F32 u[2][2][17][16];

   //
   // Persistent storage for IMDCT -- must be cleared to zero at seek time
   //

   F32 s[2][32][18];

#ifdef IS_PS2
   F32 dmaCacheLinePadPostUS[16];   // 1 cache line to avoid problems with DMA/cache interactions
#endif
// =================


   S32 u_start[2];
   S32 u_div  [2];

   //
   // Stream creation parameters
   //

   UINTa         user;
   VOIDFUNC * fetch_CB;
   MSS_FREE_TYPE  * free_CB; 
   U32           total_size;

   //
   // Preferences (not currently supported)
   //

   S32 requested_rate;
   S32 requested_bits;
   S32 requested_chans;

   //
   // Other stream source parms
   //

   U32 current_offset;

   S32 write_cursor;                // Byte offset for incoming data

   S32 apos;                        // Current bit-fetch position in audio buffer

   U8  header_buffer[HEAD_BUFSIZE]; // Raw header and side info for current frame
                                    // (Actual size of side info data depends on MPEG layer #)
   S32 hpos;                        // Current bit position in header/side buffer

   S32 header_size;                 // 4 or 6 bytes, depending on CRC
   S32 side_info_size;              // Valid for layer 3 side info only

   //
   // Header information for current frame
   //

   S32 MPEG1;                       // 1=MPEG1, 0=MPEG2 or MPEG2.5
   S32 MPEG25;                      // 1=MPEG2.5, 0=MPEG1 or MPEG2
   S32 layer;
   S32 protection_bit;
   S32 bitrate_index;
   S32 sampling_frequency;
   S32 padding_bit;
   S32 private_bit;
   S32 mode;
   S32 mode_extension;
   S32 copyright;
   S32 original;
   S32 emphasis;

   //
   // Copy of certain fields from first header encountered in stream,
   // used for validation of subsequent headers
   //

   S32 check_valid;
   S32 check_MPEG1;
   S32 check_MPEG25;
   S32 check_layer;
   S32 check_protection_bit;
   S32 check_sampling_frequency;
   S32 check_mode;
   S32 check_copyright;
   S32 check_original;

   //
   // Side information for current frame
   //

   S32 main_data_begin;
   S32 scfsi                 [2][32];
   S32 part2_3_length        [2][2];
   S32 big_values            [2][2];
   S32 global_gain           [2][2];
   S32 scalefac_compress     [2][2];
   S32 window_switching_flag [2][2];
   S32 block_type            [2][2];
   S32 mixed_block_flag      [2][2];
   S32 table_select          [2][2][3];
   S32 subblock_gain         [2][2][3];
   S32 region0_count         [2][2];
   S32 region1_count         [2][2];
   S32 preflag               [2][2];
   S32 scalefac_scale        [2][2];
   S32 count1table_select    [2][2];
   S32 allocation            [2][32];

   //
   // Layer 3 scale factors for long and short blocks
   //

   III_scalefac_t scalefac[2];      // [granule]

   //
   // MPEG2-specific data
   //

   S32 intensity_scale;
   S32 pos_limit[32];

   //
   // Miscellaneous data for current frame, derived by seek_frame()
   //

   S32 bit_rate;
   S32 sample_rate;
   S32 average_frame_size;
   S32 data_size;
   S32 nch;
   S32 ngr;

   S32 zero_part[2];
   S32 zero_count[2];

   //
   // Miscellaneous stream decoder data
   //

   S32 seek_param;

   //
   // Input stream buffer
   //
   // Buffer is split into two halves to ensure that previous buffer is
   // always available for audio data retrieval.  As soon as a read-request
   // would cause a buffer overflow, the second half of the buffer is copied
   // to the first half to free up sufficient space for the incoming data.
   //

   U8  audio_buffer[STREAM_BUFSIZE];

} ASISTREAM;

//
// General prototypes
//

S32 fetch_audio_data(ASISTREAM *STR);
S32 seek_frame(ASISTREAM *STR, S32 offset);

//
// Layer-specific prototypes
//

S32 L3_init( MSS_ALLOC_TYPE * palloc, MSS_FREE_TYPE * pfree );
S32 L3_destroy(void);

#ifdef __cplusplus
extern "C"
#endif
S32 L3_frame(ASISTREAM *STR);

S32 L3_frame_info(ASISTREAM *STR);

//
// Define to control whether the dewindow is vectorized.
//
#if defined(IS_PSP) || defined(IS_XENON) || ( defined(IS_PS3) && !defined(IS_SPU) ) || (defined(IS_MAC) && defined(IS_PPC)) || defined(__RADSEKRIT2__)
#define VECTOR_DEWINDOW 1
#else
#define VECTOR_DEWINDOW 0
#endif

#if VECTOR_DEWINDOW
    #ifdef __cplusplus
extern "C"
{
    #endif
extern void AILCALL Vector_dewindow_and_write(F32 *u,
                                           F32 *dewindow,
                                           S32      start,
                                           S16 *samples,
                                           S32      output_step,
                                           S32      div);
    #ifdef __cplusplus
}
    #endif

#endif

//
// Math prototypes
//

#ifdef __cplusplus
extern "C"
{
#endif

// declare scalar versions of the math kernels...
extern void AILCALL Scalar_poly_filter(const F32 *src,
                                    const F32 *b,
                                    S32            phase,
                                    F32       *out1,
                                    F32       *out2);

extern void AILCALL Scalar_dewindow_and_write(F32 *u,
                                           F32 *dewindow,
                                           S32      start,
                                           S16 *samples,
                                           S32      output_step,
                                           S32      div);

extern void AILCALL Scalar_IMDCT_3x12    (F32 *in,
                                       S32      sb,
                                       F32 *result,
                                       F32 *save);

extern  void AILCALL Scalar_IMDCT_1x36    (register F32 *input,
                                        register S32      sb,
                                        register F32 *result,
                                        register F32 *last,
                                        register F32 *window);

#if defined(IS_X86)

#ifdef DPMI
#define X86CALL __cdecl
#else
#define X86CALL __stdcall
#endif


extern void X86CALL x86_poly_filter(const F32 *src,
                                    const F32 *b,
                                    S32            phase,
                                    F32       *out1,
                                    F32       *out2);

extern void X86CALL x86_dewindow_and_write(F32 *u,
                                           F32 const *dewindow,
                                           S32      start,
                                           S16 *samples,
                                           S32      output_step,
                                           S32      div);

extern void X86CALL AMD_poly_filter(const F32 *src,
                                    const F32 *b,
                                    S32            phase,
                                    F32       *out1,
                                    F32       *out2);

extern void X86CALL AMD_dewindow_and_write(F32 *u,
                                           F32 const *dewindow,
                                           S32      start,
                                           S16 *samples,
                                           S32      output_step,
                                           S32      div);

extern void X86CALL SSE_dewindow_and_write(F32 *u,
                                           F32 const *dewindow,
                                           S32      start,
                                           S16 *samples,
                                           S32      output_step,
                                           S32      div);

extern void X86CALL x86_IMDCT_3x12        (F32 *in,
                                           S32      sb,
                                           F32 *result,
                                           F32 *save);

extern void X86CALL AMD_IMDCT_3x12        (F32 *in,
                                           S32      sb,
                                           F32 *result,
                                           F32 *save);

extern void X86CALL x86_IMDCT_1x36        (F32 *in,
                                           S32      sb,
                                           F32 *result,
                                           F32 *save,
                                           F32 *window);

extern void X86CALL AMD_IMDCT_1x36        (F32 *in,
                                           S32      sb,
                                           F32 *result,
                                           F32 *save,
                                           F32 *window);
#else
#ifdef IS_PS2

extern void PS2_poly_filter(const F32 *src,
                          const F32 *b,
                          S32            phase,
                          F32       *out1,
                          F32       *out2);

extern void PS2_dewindow_and_write(F32 *u,
                                   const F32 *dewindow,
                                   S32      start,
                                   S16 *samples,
                                   S32      output_step,
                                   S32      div);

extern void PS2_IMDCT_3x12(F32 *in,
                         S32      sb,
                         F32 *result,
                         F32 *save);

extern void PS2_IMDCT_1x36(F32 *in,
                         S32      sb,
                         F32 *result,
                         F32 *save,
                         F32 *window);

#else

extern void AILCALL PPC_poly_filter(const F32 *src,
                                    const F32 *b,
                                    S32            phase,
                                    F32       *out1,
                                    F32       *out2);

extern void AILCALL PPC_dewindow_and_write(F32 *u,
                                           F32 *dewindow,
                                           S32      start,
                                           S16 *samples,
                                           S32      output_step,
                                           S32      div);

extern void AILCALL PPC_IMDCT_3x12    (F32 *in,
                                       S32      sb,
                                       F32 *result,
                                       F32 *save);

extern  void AILCALL PPC_IMDCT_1x36    (register F32 *input,
                                        register S32      sb,
                                        register F32 *result,
                                        register F32 *last,
                                        register F32 *window);

#endif
#endif

#ifdef __cplusplus
}
#endif

//############################################################################
//#                                                                          #
//# Macros to acquire bitfield data of length n from header/side or audio    #
//# buffers, n <= 24                                                         #
//#                                                                          #
//# Bit position 0 is MSB of byte 0                                          #
//#                                                                          #
//# Request for 0 bits is considered valid, returning 0                      #
//#                                                                          #
//############################################################################

#define H(n) (n ? read_bits(STR->header_buffer, &STR->hpos, (n)) : 0)
#define A(n) (n ? read_bits(STR->audio_buffer,  &STR->apos, (n)) : 0)
#define V8() (view_bits8(STR->audio_buffer,  STR->apos))

#ifdef IS_LE

RADINLINE U32 read_bits(U8 *data, S32 *bitpos, S32 n)
{
   U32 val;

#if defined(IS_WIN32) && !defined(IS_WIN64)

   _asm
      {
      mov edx,bitpos
      mov ebx,data

      mov ecx,[edx]
      mov eax,ecx

      and ecx,7
      shr eax,3

      mov eax,[ebx][eax]
      bswap eax

      shl eax,cl

      mov ecx,n
      add [edx],ecx

      mov ecx,32
      sub ecx,n
      shr eax,cl
      mov val,eax
      }

#else

   S32     b = *bitpos;
   U8 *p = &data[b >> 3];

   val = ((U32)(p[3]))        +
        (((U32)(p[2])) << 8)  +
        (((U32)(p[1])) << 16) +
        (((U32)(p[0])) << 24);

   val <<= b & 7;
   val >>= 32 - n;

   *bitpos = b + n;

#endif

   return val;
}

RADINLINE U32 view_bits8(U8 *data, S32 bitpos)
{
   U32 val;

#if defined(IS_WIN32) && !defined(IS_WIN64)

   _asm
      {
      mov ecx,[bitpos]
      mov ebx,[data]
      mov eax,ecx

      and ecx,7
      shr eax,3

      mov ax,[ebx][eax]

      rol ax,cl

      movzx eax,al
      mov val,eax
      }

#else

   U8 *p = &data[bitpos >> 3];

   val = ((U32)(p[1])) + (((U32)(p[0])) << 8);

   val <<= (bitpos & 7) + 16;
   val >>= 24;

#endif

   return val;
}

#else

static __inline U32 read_bits(U8 *data, S32 *bitpos, S32 n)
{
   U32 val;
   S32     b = *bitpos;

   val = *(U32*)&data[b>>3];

   val <<= b & 7;
   val >>= 32 - n;

   *bitpos = b + n;

   return val;
}

static __inline U32 view_bits8(U8 *data, S32 bitpos)
{
   U32 val;

   U8 *p = &data[bitpos >> 3];

   val = *(U32*)&data[bitpos>>3];

   val <<= bitpos & 7;
   val >>= 32 - 8;

   return val;
}

#endif


