// Ogg Vorbis I audio decoder
//
// Originally written by Sean Barrett, optimized by Terje Mathisen
//   (C) Copyright 2008-2009 RAD Game Tools, Inc.
//
/*
 * Limitations:
 *
 *   - floor 0 not supported (used in old ogg vorbis files)
 *   - lossless sample-truncation at beginning ignored
 *   - cannot concatenate multiple vorbis streams
 *   - sample positions are 32-bit, limiting seekable 192Khz
 *       files to around 6 hours (Ogg supports 64-bit)
 *
 */

//////////////////////////////////////////////////////////////////////////////
//
//  HEADER BEGINS HERE
//
#define MULTI_LEVEL_TABLES

#ifdef MULTI_LEVEL_TABLES
#define MULTI_LEVEL_BITS 2
#define MULTI_LEVEL_LEN_BITS 1

#define MULTI_LEVEL_LEN_SHIFT (15-MULTI_LEVEL_LEN_BITS)
#define MULTI_LEVEL_TOKEN_MASK ((1<<MULTI_LEVEL_LEN_SHIFT)-1)
#define MULTI_LEVEL_SIZE (1 << MULTI_LEVEL_BITS)
#define MULTI_LEVEL_MASK (MULTI_LEVEL_SIZE-1)
#endif

// This code uses a lot of Microsoft/Intel compiler intrinsics instead of inline SSE asm.
// The result is that it can run unmodified on both 32-bit and 64-bit systems.
// For most of the source code this results in compiled SSE which is very close to hand-written,
// except for one stage (Stage 7) of the IMDCT() function, where the MS 32-bit compiler runs out of
// registers and has to spill/fill to/from ram.

// Place all MS/Intel specific SIMD stuff in a separate file, to simplify porting to VMX/PS3:
#include "radvec4.h"

// To be able to malloc arrays that are SSE-aligned, we need a way to cast (float *) to an integer type,
// add 15, then mask by ~15, before casting it back to (float *).
// intptr_t is the C99 way of doing this, but MS VS2005 does not support it. :-(

// The primary target compilers for this code is 32 and 64-bit Intel and Microsoft.

#ifdef _M_AMD64
// All x64 defines here
  #define intptr_t __int64
#elif defined (_M_IX86)
  // x86 ditto
   #pragma warning(disable: 4311 4312)  // Using a 32-bit int to hold a pointer
   #define intptr_t long
#else
   #define intptr_t long
#endif

#ifdef __cplusplus
extern "C" {
#endif

///////////   THREAD SAFETY

// Individual stb_vorbis* handles are not thread-safe; you cannot decode from
// them from multiple threads at the same time. However, you can have multiple
// stb_vorbis* handles and decode from them independently in multiple thrads.


///////////   MEMORY ALLOCATION

// Mem allocation has been converted to using preallocated buffers with alloca()
// for temp mem... see Vorbis_OpenPushdataPeekahead

typedef unsigned char  uint8;
typedef   signed char   int8;
typedef unsigned short uint16;
typedef   signed short  int16;
typedef unsigned int   uint32;
typedef   signed int    int32;

///////////   FUNCTIONS USEABLE WITH ALL INPUT MODES

typedef struct stb_vorbis stb_vorbis;

typedef struct
{
   unsigned int sample_rate;
   unsigned int bitrate_nominal;
   int channels;

   int max_frame_size;
} stb_vorbis_info;


//! Struct for returning key info to the caller without the open stream heavy lifting.
struct VorbisPeekaheadProps
{
    int m_SampleRate;
    int m_Channels;
    int m_BitrateNominal;
    int m_TotalMemoryNeeded;
    int m_Error;
};


// get general information about the file
extern stb_vorbis_info stb_vorbis_get_info(stb_vorbis *f);

// get the last error detected (clears it, too)
extern int stb_vorbis_get_error(stb_vorbis *f);

// close an ogg vorbis file and free all memory in use
extern void stb_vorbis_close(stb_vorbis *f);

// this function returns the offset (in samples) from the beginning of the
// file that will be returned by the next decode, if it is known, or -1
// otherwise. after a flush_pushdata() call, this may take a while before
// it becomes valid again.
extern int stb_vorbis_get_sample_offset(stb_vorbis *f);

///////////   PUSHDATA API

// this API allows you to get blocks of data from any source and hand
// them to stb_vorbis. you have to buffer them; stb_vorbis will tell
// you how much it used, and you have to give it the rest next time;
// and stb_vorbis may not have enough data to work with and you will
// need to give it the same data again PLUS more. Note that the Vorbis
// specification does not bound the size of an individual frame.

extern stb_vorbis *stb_vorbis_open_pushdata(
         unsigned char *datablock, int datablock_length_in_bytes,
         int *datablock_memory_consumed_in_bytes,
         int *error);
// create a vorbis decoder by passing in the initial data block containing
//    the ogg&vorbis headers (you don't need to do parse them, just provide
//    the first N bytes of the file--you're told if it's not enough, see below)
// on success, returns an stb_vorbis *, does not set error, returns the amount of
//    data parsed/consumed on this call in *datablock_memory_consumed_in_bytes;
// on failure, returns NULL on error and sets *error, does not change *datablock_memory_consumed
// if returns NULL and *error is VORBIS_need_more_data, then the input block was
//       incomplete and you need to pass in a larger block from the start of the file

extern int
stb_vorbis_open_pushdata_begin(
    unsigned char* i_Stream,
    int i_StreamLen,
    VorbisPeekaheadProps* o_Props
    );

extern stb_vorbis* stb_vorbis_open_pushdata_complete(
    unsigned char* i_Stream,
    int i_StreamLen,
    void* i_Buffer,
    unsigned int i_BufferLen
    );

extern int stb_vorbis_init_tables(stb_vorbis *f);
//


extern int stb_vorbis_decode_frame_pushdata(
         stb_vorbis *f, unsigned char *datablock, int datablock_length_in_bytes,
         int *channels,             // place to write number of float * buffers
         float ***output,           // place to write float ** array of float * buffers
         int *samples               // place to write number of output samples
     );
// decode a frame of audio sample data if possible from the passed-in data block

//
// return value: number of bytes we used from datablock
// possible cases:
//     0 bytes used, 0 samples output (need more data)
//     N bytes used, 0 samples output (resynching the stream, keep going)
//     N bytes used, M samples output (one frame of data)
// note that after opening a file, you will ALWAYS get one N-bytes,0-sample
// frame, because Vorbis always "discards" the first frame.
//
// Note that on resynch, stb_vorbis will rarely consume all of the buffer,
// instead only datablock_length_in_bytes-3 or less. This is because it wants
// to avoid missing parts of a page header if they cross a datablock boundary,
// without writing state-machiney code to record a partial detection.
//
// The number of channels returned are stored in *channels (which can be
// NULL--it is always the same as the number of channels reported by
// get_info). *output will contain an array of float* buffers, one per
// channel. In other words, (*output)[0][0] contains the first sample from
// the first channel, and (*output)[1][0] contains the first sample from
// the second channel.

extern int stb_vorbis_flush_pushdata(stb_vorbis *f);
// inform stb_vorbis that your next datablock will not be contiguous with
// previous ones (e.g. you've seeked in the data); future attempts to decode
// frames will cause stb_vorbis to resynchronize (as noted above), and
// once it sees a valid Ogg page (typically 4-8KB, as large as 64KB), it
// will begin decoding the _next_ frame.
//
// if you want to seek using pushdata, you need to seek in your file, then
// call stb_vorbis_flush_pushdata(), then start calling decoding, then once
// decoding is returning you data, call stb_vorbis_get_sample_offset, and
// if you don't like the result, seek your file again and repeat.
//
// returns an error if you have a deferred header read and they were corrupt.

extern int stb_vorbis_flush_pushdata_start_on_page(stb_vorbis *f);
// same as stb_vorbis_flush_pushdata, only it says that the next
//   data we give you will be on a page boundary.



////////   ERROR CODES

enum STBVorbisError
{
   VORBIS__no_error,

   VORBIS_need_more_data=1,             // not a real error

   VORBIS_invalid_api_mixing,           // can't mix API modes
   VORBIS_outofmem,                     // not enough memory
   VORBIS_feature_not_supported,        // uses floor 0
   VORBIS_too_many_channels,            // STB_VORBIS_MAX_CHANNELS is too small
   VORBIS_file_open_failure,            // fopen() failed

   VORBIS_unexpected_eof=10,            // file is truncated?

   // decoding errors (corrupt/invalid stream) -- you probably
   // don't care about the exact details of these

   // vorbis errors:
   VORBIS_invalid_setup=20,
   VORBIS_invalid_stream,

   // ogg errors:
   VORBIS_missing_capture_pattern=30,
   VORBIS_invalid_stream_structure_version,
   VORBIS_continued_packet_flag_invalid,
   VORBIS_incorrect_stream_serial_number,
   VORBIS_invalid_first_page,
   VORBIS_bad_packet_type,
   VORBIS_FORCE_32 = 0x7fffffff
};


#ifdef __cplusplus
}
#endif

//
//  HEADER ENDS HERE
//
//////////////////////////////////////////////////////////////////////////////


// global configuration settings (e.g. set these in the project/makefile),
// or just set them in this file at the top (although ideally the first few
// should be visible when the header file is compiled too, although it's not
// crucial)

// TMA_USE_RECIPROCAL_MUL
//     Replaces as many semi-fixed integer divisions as possible with
//     reciprocal multiplication instead: Does a 32x32->64MUL, then
//     returns the high half of the result. MSVC generates near-optimal
//     code for this construct.
#define TMA_USE_RECIPROCAL_MUL

// TMA_FRACTIONAL_LINE
//     Uses fixed-point 8:24 calculations for the Floor() calculations,
//     instead of a classic Bresenham style test/branch pattern.
//     The y coordinates are limited to 8 bits, they are
//     used as lookup values into a 256-entry decibel table.
//     24-bit fractions are sufficient for a maximal frame of 8K samples
//     without having any overflow problems.
//     Enabling this define gives a 4-5% speedup on my Core 2 Duo laptop
#define TMA_FRACTIONAL_LINE


// STB_VORBIS_MAX_CHANNELS [number]
//     globally define this to the maximum number of channels you need.
//     The spec does not put a restriction on channels except that
//     the count is stored in a byte, so 255 is the hard limit.
//     Reducing this saves about 16 bytes per value, so using 16 saves
//     (255-16)*16 or around 4KB. Plus anything other memory usage
//     I forgot to account for. Can probably go as low as 8 (7.1 audio),
//     6 (5.1 audio), or 2 (stereo only).
#ifndef STB_VORBIS_MAX_CHANNELS
#define STB_VORBIS_MAX_CHANNELS    16  // enough for anyone?
#endif

// STB_VORBIS_PUSHDATA_CRC_COUNT [number]
//     after a flush_pushdata(), stb_vorbis begins scanning for the
//     next valid page, without backtracking. when it finds something
//     that looks like a page, it streams through it and verifies its
//     CRC32. Should that validation fail, it keeps scanning. But it's
//     possible that _while_ streaming through to check the CRC32 of
//     one candidate page, it sees another candidate page. This #define
//     determines how many "overlapping" candidate pages it can search
//     at once. Note that "real" pages are typically ~4KB to ~8KB, whereas
//     garbage pages could be as big as 64KB, but probably average ~16KB.
//     So don't hose ourselves by scanning an apparent 64KB page and
//     missing a ton of real ones in the interim; so minimum of 2
#ifndef STB_VORBIS_PUSHDATA_CRC_COUNT
#define STB_VORBIS_PUSHDATA_CRC_COUNT  4
#endif

// STB_VORBIS_CODEBOOK_FLOATS
//     The vorbis file format encodes VQ codebook floats as ax+b where a and
//     b are floating point per-codebook constants, and x is a 16-bit int.
//     Normally, stb_vorbis leaves them as 16-bit ints and computes ax+b while
//     decoding. It is possible that pre-expanding them to floats is more
//     efficient; but it requires more storage. Due to cacheing, this may
//     not be a win, therefore it is not on by default. But define
//     STB_VORBIS_CODEBOOK_FLOATS to force it to use floats for a possible
//     speed-up.
#define STB_VORBIS_CODEBOOK_FLOATS

// STB_VORBIS_NO_DEFER_FLOOR
//     Normally we only decode the floor without synthesizing the actual
//     full curve. We can instead synthesize the curve immediately. This
//     requires more memory and is very likely slower, so I don't think
//     you'd ever want to do it except for debugging.
// #define STB_VORBIS_NO_DEFER_FLOOR


//////////////////////////////////////////////////////////////////////////////


#include <stdlib.h>
#include <string.h>
#include <math.h>
#if defined(__CELLOS_LV2__) || (defined(HOLLYWOOD_REV) || defined(REVOLUTION)) || defined(__MWERKS__) || defined(__APPLE__) || defined(IS_PSP2)
#   include <alloca.h>
#elif defined(__RADSEKRIT2__)
#   include <stdlib.h> // LLVM apparently puts it here.
#elif !defined(IS_3DS)
#   include <malloc.h>
#endif

#if defined(R5900) || defined(__psp__)

extern "C" F32 AIL_cos( F32 x );
extern "C" F32 AIL_sin( F32 x );
extern "C" F32 AIL_pow( F32 x, F32 y );
extern "C" F32 AIL_floor( F32 x );
extern "C" F32 AIL_log( F32 x );
extern "C" F32 AIL_ldexpf( F32 x, S32 pw2 );

#define sin AIL_sin
#define cos AIL_cos
#define pow AIL_pow
#define floor AIL_floor
#define log AIL_log
#define ldexp AIL_ldexpf
#define exp(x) AIL_pow(2.71828183f, x)
#endif

#ifndef _MSC_VER
#define __forceinline
#endif

#if STB_VORBIS_MAX_CHANNELS > 256
#undef STB_VORBIS_MAX_CHANNELS
#define STB_VORBIS_MAX_CHANNELS 16    // enough for anyone?
#endif

#define MAX_BLOCKSIZE_LOG  13   // from specification
#define MAX_BLOCKSIZE      (1 << MAX_BLOCKSIZE_LOG)

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

//
// Data structures and algorithms modified:
//
// Huffman decoding via recursive data tables instead of binary search
// All sparse Huffman tables are treated as non-sparse, this increases memory usage
// slightly for mostly-empty tables but can save RAM on those with at least 60%+ in use,
// and it is a significant speed win.
//
// Inverse Modified DCT rewritten to allow 4xfloat SSE processing of all stages
// SSE used in many other places, particularly where it can also be used to
// avoid very branchy code.
//


// @NOTE
//
// Some arrays below are tagged "//varies", which means it's actually
// a variable-sized piece of data, but rather than malloc I assume it's
// small enough it's better to just allocate it all together with the
// main thing
//
// Most of the variables are specified with the smallest size I could pack
// them into. It might give better performance to make them all full-sized
// integers. It should be safe to freely rearrange the structures or change
// the sizes larger--nothing relies on silently truncating etc., nor the
// order of variables.

// The Codebook struct is tricky in that the last element is a variable-sized
// array (fast_decode1[]), of which only the entries actually used will be allocated.
// This saves one level of indirection, at a small cost of additional code during setup.

// Layout of fast_decode1[] table entries:
// Each table entry is 32 bits, it can either hold a positive value
// consisting of the token length shifted left 26 bits, plus the
// actual huffman value, or it can be negative.
// A negative value is a biased index into the next level of lookup
// tables, with the bias being 0x80000000, i.e. just the sign bit itself.

typedef struct
{
   uint8 *codeword_lengths;
#ifndef STB_VORBIS_CODEBOOK_FLOATS
   uint16 *multiplicands;
#else
   float  *multiplicands;
#endif
   int dimensions;
#ifdef TMA_USE_RECIPROCAL_MUL
   unsigned dim_reciprocal;
#endif
   int entries;
   int     sorted_entries;

   uint32 lookup_values;
   float  minimum_value;
   float  delta_value;
   uint8  value_bits;
   uint8  lookup_type;
   uint8  sequence_p;
   uint8  ignore_padding;
   int fd;
   int32 fast_decode1[32768];   // Must be last entry, will be only partially allocated!
} Codebook;

typedef struct
{
   uint8 *codeword_lengths;
#ifndef STB_VORBIS_CODEBOOK_FLOATS
   uint16 *multiplicands;
#else
   float  *multiplicands;
#endif
   int dimensions;
#ifdef TMA_USE_RECIPROCAL_MUL
   unsigned dim_reciprocal;
#endif
   int entries;
   int     sorted_entries;

   uint32 lookup_values;
   float  minimum_value;
   float  delta_value;
   uint8  value_bits;
   uint8  lookup_type;
   uint8  sequence_p;
} CodebookLite;


typedef struct
{
   uint8 order;
   uint8 number_of_books;
   uint16 rate;
   uint16 bark_map_size;
   uint8 amplitude_bits;
   uint8 amplitude_offset;
   uint8 book_list[16]; // varies
} Floor0;

typedef struct
{
#ifdef TMA_USE_RECIPROCAL_MUL
   uint32 dx_reciprocal[31*8+2];
#endif
   int values;
   int16 subclass_books[16][8]; // varies
   uint16 Xlist[31*8+2]; // varies
   uint8 partition_class_list[32]; // varies
   uint8 class_dimensions[16]; // varies
   uint8 class_subclasses[16]; // varies
   uint8 class_masterbooks[16]; // varies
   uint8 sorted_order[31*8+2];
   uint8 neighbors[31*8+2][2];
   uint8 partitions;
   uint8 floor1_multiplier;
   uint8 rangebits;
} Floor1;

typedef union
{
   Floor0 floor0;
   Floor1 floor1;
} Floor;

typedef struct
{
   uint8 **classdata;
   uint32 begin, end;
   uint32 part_size;
   uint32 part_size_reciprocal;
   int16 (*residue_books)[8];
   uint8 classifications;
   uint8 classbook;
} Residue;

typedef struct
{
   uint8 magnitude;
   uint8 angle;
   uint8 mux;
} MappingChannel;

typedef struct
{
   MappingChannel *chan;
   uint16 coupling_steps;
   uint8  submaps;
   uint8  submap_floor[15]; // varies
   uint8  submap_residue[15]; // varies
} Mapping;

typedef struct
{
   uint8 blockflag;
   uint8 mapping;
   uint16 windowtype;
   uint16 transformtype;
} Mode;

typedef struct stb_vorbis stb_vorbis;

typedef struct
{
   uint32  goal_crc;    // expected crc if match
   int     bytes_left;  // bytes left in packet
   uint32  crc_so_far;  // running crc
   int     bytes_done;  // bytes processed in _current_ chunk
   uint32  sample_loc;  // granule pos encoded in page
} CRCscan;

// If we have a 64-bit cpu, then we can use a 64-bit type for the huffman bit accumulator
// This reduces the number of refill calls by about 50% or more. The 64-bit accumulator
// code does work in 32-bit mode, but it runs about 3% slower.

#ifdef _M_AMD64
typedef unsigned __int64 acc_t;
#define ACC_LIMIT 56
#define TMA_WIDE_ACCUMULATOR
#else
typedef uint32 acc_t;
#define ACC_LIMIT 24
#undef TMA_WIDE_ACCUMULATOR
#endif

struct stb_vorbis
{
  // user-accessible info
   unsigned int sample_rate;
   unsigned int bitrate_nominal;
   int channels;
#ifdef TMA_USE_RECIPROCAL_MUL
   uint32 chan_reciprocal;
#endif

  // input config

   uint8 *stream;
   uint8 *stream_start;
   uint8 *stream_end;
   int close_on_free;
   uint8  push_mode;
   uint8 ignore_padding0[3];

   // memory management
   void* m_MemoryBuffer;
   unsigned int m_MemoryBufferLen;
   unsigned int m_MemoryOffset;
   unsigned int m_OldStyleMemCount;

  // run-time results
   int eof;
   enum STBVorbisError error;

  // user-useful data

  // header info
   int blocksize_0, blocksize_1;
   int codebook_count;
   Codebook **codebooks;
   int floor_count;
   uint16 floor_types[64]; // varies
   Floor *floor_config;
   int residue_count;
   uint16 residue_types[64]; // varies
   Residue *residue_config;
   int mapping_count;
   Mapping *mapping;
   int mode_count;
   Mode mode_config[64];  // varies

  // decode buffer
   float *channel_buffer_alloc;
   float *channel_buffers[STB_VORBIS_MAX_CHANNELS];
   float *outputs        [STB_VORBIS_MAX_CHANNELS];

   float *previous_window[STB_VORBIS_MAX_CHANNELS];
   int previous_length;

   #ifndef STB_VORBIS_NO_DEFER_FLOOR
   int16 *finalY[STB_VORBIS_MAX_CHANNELS];
   #else
   float *floor_buffers[STB_VORBIS_MAX_CHANNELS];
   #endif

   uint32 current_loc; // sample location of next frame to decode
   int    current_loc_valid;

  // per-blocksize precomputed data

   // twiddle factors
   float *twiddle_buffers[2];       // A, B and C are suballocated here!
   float *A[2],*B[2],*C[2];
   float *window[2];
   uint16 *bit_reverse[2];

  // current page/packet/segment streaming info
   uint32 serial; // stream serial number for verification
   int last_page;
   int segment_count;
   uint8 segments[255];
   uint8 page_flag;
   uint8 bytes_in_seg;
   uint8 first_decode;
   uint8 ignore_padding1[2];
   int next_seg;
   int last_seg;  // flag that we're on the last segment
   int last_seg_which; // what was the segment number of the last seg?
   acc_t acc;
   int valid_bits;
   uint8 *page_end;
   //int packet_bytes;
   int end_seg_with_known_loc;
   uint32 known_loc_for_packet;
   int discard_samples_deferred;
   uint32 samples_output;

  // push mode scanning
   int page_crc_tests; // only in push_mode: number of tests active; -1 if not searching
   CRCscan scan[STB_VORBIS_PUSHDATA_CRC_COUNT];

  // sample-access
   int channel_buffer_start;
   int channel_buffer_end;

   int do_start_decoder;
};

typedef struct stb_vorbis stb_vorbis;
typedef struct stb_vorbis vorb;

static int error(vorb *f, enum STBVorbisError e)
{
   f->error = e;
   if (!f->eof && e != VORBIS_need_more_data) {
      f->error=e; // breakpoint for debugging
   }
   return 0;
}

// these functions are used for allocating temporary memory
// while decoding. if you can afford the stack space, use
// alloca(); otherwise, provide a temp buffer and it will
// allocate out of those.

#define array_size_required(count,size)  (count*(sizeof(void *)+(size)))

#define temp_alloc(f,size)              (alloca(size))
#define temp_free(f,p)                  (0)
#define temp_alloc_save(f)              0
#define temp_alloc_restore(f,p)         {}

#define temp_block_array(f,count,size)  make_block_array(temp_alloc(f,array_size_required(count,size)), count, size)

// given a sufficiently large block of memory, make an array of pointers to subblocks of it
static void *make_block_array(void *mem, int count, int size)
{
   int i;
   void ** p = (void **) mem;
   char *q = (char *) (p + count);
   for (i=0; i < count; ++i) {
      p[i] = q;
      q += size;
   }
   return p;
}


static void *setup_malloc(vorb *f, int sz)
{
   sz = (sz+3) & ~3;

   // Check if we don't have a buffer already.
   if (f->m_MemoryBuffer == 0)
   {
       f->m_OldStyleMemCount += sz;
       return AIL_mem_alloc_lock(sz);
   }

   //
   // Check if we have the remaining space
   //
   if (f->m_MemoryOffset + sz > f->m_MemoryBufferLen)
   {
       // Bad news.
       radassert(0);
       return 0;
   }


   void* Result = (char*)f->m_MemoryBuffer + f->m_MemoryOffset;
   f->m_MemoryOffset += sz;

   return Result;
}

static void *setup_temp_malloc(vorb *f, int sz)
{
    return setup_malloc(f, sz);
}

// All memory is preallocated - free does nothing.
static void setup_free(vorb *f, void *p)
{
    if (f->m_MemoryBuffer == 0)
    {
        AIL_mem_free_lock(p);
    }
}

static void setup_temp_free(vorb *f, void *p, int sz)
{
    setup_free(f, p);
}


#define CRC32_SEED    ~0
#define CRC32_POLY    0x04c11db7   // 0xedb88320

static uint32 crc_table[256];
static void crc32_init(void)
{
   // TMa branchless version
   int i, j;
   for(i=0; i < 256; i++) {
      int s;
      for (s=i<<24, j=0; j < 8; ++j)
         s = (s << 1) ^ ((s >> 31) & CRC32_POLY);
      crc_table[i] = s;
   }
}

static __forceinline uint32 crc32_update(uint32 crc, uint8 byte)
{
   return (crc << 8) ^ crc_table[byte ^ (crc >> 24)];
}

// used inside IMDCT
static unsigned int _bit_reverse(unsigned int n)
{
  n = ((n & 0xAAAAAAAA) >>  1) | ((n & 0x55555555) << 1);
  n = ((n & 0xCCCCCCCC) >>  2) | ((n & 0x33333333) << 2);
  n = ((n & 0xF0F0F0F0) >>  4) | ((n & 0x0F0F0F0F) << 4);
  n = ((n & 0xFF00FF00) >>  8) | ((n & 0x00FF00FF) << 8);
  return (n >> 16) | (n << 16);
}

#define BITREVBITS 8
#define BITREVSIZE (1 << BITREVBITS)
#define BITREVMASK (BITREVSIZE-1)

static unsigned char bitrev_table[BITREVSIZE];

static unsigned int bit_reverse(unsigned int n)
{
    radassert(BITREVBITS == 8);
    unsigned int r =
        (bitrev_table[n & 255] << 24) |
        (bitrev_table[(n >> 8) & 255] << 16) |
        (bitrev_table[(n >> 16) & 255] << 8) |
        (bitrev_table[n >> 24]);

    radassert(r == _bit_reverse(n));
    return r;
}

static void bitrev_init(void)
{
    for (unsigned int n = 0; n < BITREVSIZE; n++) {
        bitrev_table[n] = _bit_reverse(n) >> (32-BITREVBITS);
    }
}

static float square(float x)
{
   return x*x;
}

#undef M_PI

#ifndef M_PI
  #define M_PI  3.14159265358979323846264f  // from CRC
#endif

// code length assigned to a value with no huffman encoding
#define NO_CODE   255

/////////////////////// LEAF SETUP FUNCTIONS //////////////////////////
//
// these functions are only called at setup, and only a few times
// per file

static float float32_unpack(uint32 x)
{
   // from the specification
   uint32 mantissa = x & 0x1fffff;
   uint32 sign = x & 0x80000000;
   uint32 exp = (x & 0x7fe00000) >> 21;
   float res = sign ? -(float)mantissa : (float)mantissa;
   return (float) ldexp(res, exp-788);
}

int32 fd_alloc(Codebook *c); // wii warning
int32 fd_alloc(Codebook *c)
{
    // Allocate a new block of four entries inside the fast_decode1[] array
    int f = c->fd;
    c->fd += 4;
    radassert(c->fd <= 32768); // Array overrun, should be impossible
    c->fast_decode1[f] = 0xFFFFFFFF;
    c->fast_decode1[f+1] = 0xFFFFFFFF;
    c->fast_decode1[f+2] = 0xFFFFFFFF;
    c->fast_decode1[f+3] = 0xFFFFFFFF;
    return f - 0x80000000;
}

int32 fastdecode_index_empty(Codebook* c, uint32 idx); // wii warning
int32 fastdecode_index_empty(Codebook* c, uint32 idx)
{
    return idx <= (uint32)(c->fd - 4) ? c->fast_decode1[idx] == -1 : 1;
}

static void add_entry1(Codebook *c, uint32 huff_code, int symbol, int count, unsigned len)
{
    radassert(len);            // Cannot be zero!
    radassert(symbol < (1 << 26)); // Max 26 bits for symbol

    int32 entry = (len << 26) + symbol;
    // Short symbols end up in the first (static) part of the fast_decode1[] array:
    if (len <= 8) {
        uint32 h_step = 1 << len;
        do {
            c->fast_decode1[huff_code] = entry;
            huff_code += h_step;
        } while (huff_code < 256);
        return;
    }
    // The Huffman code is longer than 8 bits, so we need to traverse
    // a recursive structure:
    uint32 idx = huff_code & 255;
    if (fastdecode_index_empty(c, idx)) { // Empty entry
        c->fast_decode1[idx] = fd_alloc(c);
    }
    int32 *pfd = c->fast_decode1 + (c->fast_decode1[idx] + 0x80000000);
    int l = len-8;
    for (huff_code >>= 8; l > 2; l -= 2, huff_code >>= 2)
    {
        if (fastdecode_index_empty(c, &(pfd[huff_code&3]) - c->fast_decode1))
        {
            pfd[huff_code & 3] = fd_alloc(c);
        }
        pfd = c->fast_decode1 + (pfd[huff_code & 3] + 0x80000000);
    }
    radassert(l <= 2);
    radassert(count < 16384);
    radassert(huff_code < 4);
    uint32 h_step = 1 << l;
    do {
        pfd[huff_code] = entry;
        huff_code += h_step;
    } while (huff_code < 4);
    return;
}

// zlib & jpeg huffman tables assume that the output symbols
// can either be arbitrarily arranged, or have monotonically
// increasing frequencies--they rely on the lengths being sorted;
// this makes for a very simple generation algorithm.
// vorbis allows a huffman table with non-sorted lengths. This
// requires a more sophisticated construction, since symbols in
// order do not map to huffman codes "in order".

static int compute_codewords(Codebook *c, uint8 *len, int n, uint32 *values)
{
   int i,k,m=0;
   uint32 available[32];

   thememset(available, 0, sizeof(available));
   // find the first entry
   for (k=0; k < n; ++k) if (len[k] < NO_CODE) break;
   if (k == n) { radassert(c->sorted_entries == 0); return TRUE; }
   // add to the list
//   add_entry(c, 0, k, m, len[k], values);

   // Initialize internal pointers for allocations within the fast_decode1[] array:
   c->fd = 256;  // After the fixed entries for short tokens!
   // Minus 1 indicates an empty slot

//   thememset(c->fast_decode1, 0xff, sizeof(c->fast_decode1));
   thememset(c->fast_decode1, 0xff, 256 * 4);


   // Insert the first token:
   add_entry1(c, 0, k, m, len[k]);

   m++;
   // add all available leaves
   for (i=1; i <= len[k]; ++i)
      available[i] = 1 << (32-i);
   // note that the above code treats the first case specially,
   // but it's really the same as the following code, so they
   // could probably be combined (except the initial code is 0,
   // and I use 0 in available[] to mean 'empty')
   for (i=k+1; i < n; ++i) {
      uint32 res;
      int z = len[i], y;
      if (z == NO_CODE) continue;
      // find lowest available leaf (should always be earliest,
      // which is what the specification calls for)
      // note that this property, and the fact we can never have
      // more than one free leaf at a given level, isn't totally
      // trivial to prove, but it seems true and the radassert never
      // fires, so!
      while (z > 0 && !available[z]) --z;
      if (z == 0) { radassert(0); return FALSE; }
      res = available[z];
      available[z] = 0;
      add_entry1(c, bit_reverse(res), i, m, len[i]);
      m++;
      // propogate availability up the tree
      if (z != len[i]) {
         for (y=len[i]; y > z; --y) {
            radassert(available[y] == 0);
            available[y] = res + (1 << (32-y));
         }
      }
   }

   return TRUE;
}

// only run while parsing the header (3 times)
static int vorbis_validate(uint8 *data)
{
   static uint8 vorbis[6] = { 'v', 'o', 'r', 'b', 'i', 's' };
   return thememcmp(data, vorbis, 6) == 0;
}

// called from setup only, once per code book
// (formula implied by specification)
static int lookup1_values(int entries, int dim)
{
   int r = (int) floor(exp((float) log((float) entries) / (float)dim));
   if (pow((float) r+1, dim) <= entries) ++r;
   radassert(pow((float) r+1, dim) > entries);
   radassert((int)pow((float) r, dim) <= entries);
   return r;
}

// called twice per file
static void compute_twiddle_factors(int n, float *A, float *B, float *C)
{
   int n4 = n >> 2, n8 = n >> 3;
   int k,k2;

   for (k=k2=0; k < n4; ++k,k2+=2) {
      A[k2  ] = (float)  cos((float)(4.0f*k*M_PI/n));
      A[k2+1] = (float) -sin(4.0f*k*M_PI/n);
      B[k2  ] = (float)  cos((k2+1)*M_PI/n/2) * 0.5f;  // hoist final scale computation
      B[k2+1] = (float)  sin((k2+1)*M_PI/n/2) * 0.5f;  // up into these constants
   }
   for (k=k2=0; k < n8; ++k,k2+=2) {
      C[k2  ] = (float)  cos(2.0f*(k2+1)*M_PI/n);
      C[k2+1] = (float) -sin(2.0f*(k2+1)*M_PI/n);
   }
}

static void compute_window(int n, float *window)
{
   int n2 = n >> 1, i;
   for (i=0; i < n2; ++i)
      window[i] = (float) sin(0.5f * M_PI * square((float) sin((i - 0.f + 0.5f) / n2 * 0.5f * M_PI)));
}
static int ilog(int32 n);

static void compute_bitreverse(int n, uint16 *rev)
{
   int ld = ilog(n) - 1; // ilog is off-by-one from normal definitions
   int i, n8 = n >> 3;
   for (i=0; i < n8; ++i)
      rev[i] = (bit_reverse(i) >> (32-ld+3)) << 2;
}

static int init_blocksize(vorb *f, int b, int n)
{
   int n2 = n >> 1, n4 = n >> 2, n8 = n >> 3;

   f->twiddle_buffers[b] = (float *) setup_malloc(f, sizeof(float) * (n+n2+n4) + 15);
   if (!f->twiddle_buffers[b]) return error(f, VORBIS_outofmem);
   float *t = f->twiddle_buffers[b];
   t = (float *)(((intptr_t) t+15) & ~15);

   f->A[b] = t;
   f->B[b] = t + n2;
   f->C[b] = t + n;
   compute_twiddle_factors(n, f->A[b], f->B[b], f->C[b]);
   f->window[b] = t + n + n4;

   if (!f->window[b]) return error(f, VORBIS_outofmem);
   compute_window(n, f->window[b]);
   f->bit_reverse[b] = (uint16 *) setup_malloc(f, sizeof(uint16) * n8);
   if (!f->bit_reverse[b]) return error(f, VORBIS_outofmem);
   compute_bitreverse(n, f->bit_reverse[b]);
   return TRUE;
}

static void neighbors(uint16 *x, int n, int *plow, int *phigh)
{
   int low = -1;
   int high = 65536;
   int i;
   for (i=0; i < n; ++i) {
      if (x[i] > low  && x[i] < x[n]) { *plow  = i; low = x[i]; }
      if (x[i] < high && x[i] > x[n]) { *phigh = i; high = x[i]; }
   }
}

#ifdef TMA_USE_RECIPROCAL_MUL
 #ifdef TMA_USE_32BIT_RECIPROCALS
uint32 calc_reciprocal(uint32 divisor)
{
    uint32 recip = (uint32) (0x100000000 / divisor);
    if (divisor & (divisor-1)) { // Not a power of two!
        recip++;
    }
    return recip;
}

__forceinline
uint32 use_reciprocal(uint32 n, uint32 recip)
{
    if (recip == 0)
        return n;
    LARGE_INTEGER l;
    l.QuadPart = (__int64) n * recip;
    return l.HighPart;
}
 #else  // Maximum 32-bit intermediate products, with 21-bit reciprocals
uint32 calc_reciprocal(uint32 divisor); // wii warning.
uint32 calc_reciprocal(uint32 divisor)
{
    uint32 recip = (uint32) (0x200000 / divisor);  // 21-bit scale factor
    if (divisor & (divisor-1)) { // Not a power of two!
        recip++;
    }
    return recip;
}

__forceinline uint32 use_reciprocal(uint32 n, uint32 recip); // wii warning
__forceinline
uint32 use_reciprocal(uint32 n, uint32 recip)
{
    uint32 l;
    l = n * recip;
    l >>= 21;
    return l;
}
 #endif
#endif

// this has been repurposed so y is now the original index instead of y
typedef struct
{
   uint16 x,y;
} Point;

//
/////////////////////// END LEAF SETUP FUNCTIONS //////////////////////////

#define USE_MEMORY(z)    TRUE

__forceinline
static uint8 get8(vorb *z)
{
      radassert(z->stream < z->stream_end);
      //if (z->stream >= z->stream_end) { z->eof = TRUE; return 0; }
      return *z->stream++;
}

static uint32 get32(vorb *f)
{
   uint32 x;
   x = get8(f);
   x += get8(f) << 8;
   x += get8(f) << 16;
   x += get8(f) << 24;
   return x;
}

static int getn(vorb *z, uint8 *data, int n)
{
    if (z->stream+n > z->stream_end) { z->eof = 1; return 0; }
    thememcpy(data, z->stream, n);
    z->stream += n;
    return 1;
}

static void skip(vorb *z, int n)
{
   z->stream += n;
   if (z->stream >= z->stream_end) z->eof = 1;
   return;
}

static uint8 ogg_page_header[4] = { 0x4f, 0x67, 0x67, 0x53 };

static int capture_pattern(vorb *f)
{
   if (0x4f != get8(f)) return FALSE;
   if (0x67 != get8(f)) return FALSE;
   if (0x67 != get8(f)) return FALSE;
   if (0x53 != get8(f)) return FALSE;
   return TRUE;
}

#define PAGEFLAG_continued_packet   1
#define PAGEFLAG_first_page         2
#define PAGEFLAG_last_page          4

static int start_page_no_capturepattern(vorb *f)
{
   uint32 loc0,loc1,n;
   int32 i;
   // stream structure version
   if (0 != get8(f)) return error(f, VORBIS_invalid_stream_structure_version);
   // header flag
   f->page_flag = get8(f);
   // absolute granule position
   loc0 = get32(f);
   loc1 = get32(f);
   // @TODO: validate loc0,loc1 as valid positions?
   // stream serial number -- vorbis doesn't interleave, so discard
   get32(f);
   //if (f->serial != get32(f)) return error(f, VORBIS_incorrect_stream_serial_number);
   // page sequence number
   n = get32(f);
   f->last_page = n;
   // CRC32
   get32(f);
   // page_segments
   f->segment_count = get8(f);
   if (f->eof)
      return error(f, VORBIS_unexpected_eof);
   if (!getn(f, f->segments, f->segment_count))
      return error(f, VORBIS_unexpected_eof);
   // assume we _don't_ know any the sample position of any segments
   f->end_seg_with_known_loc = -2;
   if (loc0 != (unsigned)~0 || loc1 != (unsigned)~0) {
      // determine which packet is the last one that will complete
      for (i=f->segment_count-1; i >= 0; --i)
         if (f->segments[i] < 255)
            break;
      // 'i' is now the index of the _last_ segment of a packet that ends
      if (i >= 0) {
         f->end_seg_with_known_loc = i;
         f->known_loc_for_packet   = loc0;
      }
   }
   f->next_seg = 0;
   return TRUE;
}

static int start_page(vorb *f)
{
   if (!capture_pattern(f)) return error(f, VORBIS_missing_capture_pattern);
   return start_page_no_capturepattern(f);
}

static int start_packet(vorb *f)
{
   while (f->next_seg == -1) {
      if (!start_page(f)) return FALSE;
      if (f->page_flag & PAGEFLAG_continued_packet)
         return error(f, VORBIS_continued_packet_flag_invalid);
   }
   f->last_seg = FALSE;
   f->valid_bits = 0;
   f->acc = 0;
   //f->packet_bytes = 0;
   f->bytes_in_seg = 0;
   // f->next_seg is now valid
   return TRUE;
}

static int maybe_start_packet(vorb *f)
{
   if (f->next_seg == -1) {
      int x = get8(f);
      if (f->eof) return FALSE; // EOF at page boundary is not an error!
      if (0x4f != x      ) return error(f, VORBIS_missing_capture_pattern);
      if (0x67 != get8(f)) return error(f, VORBIS_missing_capture_pattern);
      if (0x67 != get8(f)) return error(f, VORBIS_missing_capture_pattern);
      if (0x53 != get8(f)) return error(f, VORBIS_missing_capture_pattern);
      if (!start_page_no_capturepattern(f)) return FALSE;
      if (f->page_flag & PAGEFLAG_continued_packet) {
         // set up enough state that we can read this packet if we want,
         // e.g. during recovery
         f->last_seg = FALSE;
         f->bytes_in_seg = 0;
         return error(f, VORBIS_continued_packet_flag_invalid);
      }
   }
   return start_packet(f);
}

static int next_segment(vorb *f)
{
   int len;
   if (f->last_seg) return 0;
   if (f->next_seg == -1) {
      f->last_seg_which = f->segment_count-1; // in case start_page fails
      if (!start_page(f)) { f->last_seg = 1; return 0; }
      if (!(f->page_flag & PAGEFLAG_continued_packet)) return error(f, VORBIS_continued_packet_flag_invalid);
   }
   len = f->segments[f->next_seg++];
   if (len < 255) {
      f->last_seg = TRUE;
      f->last_seg_which = f->next_seg-1;
   }
   if (f->next_seg >= f->segment_count)
      f->next_seg = -1;
   radassert(f->bytes_in_seg == 0);
   f->bytes_in_seg = len;
   return len;
}

#define EOP    (-1)
#define INVALID_BITS  (-1)

__forceinline
static int get8_packet_raw(vorb *f)
{
    if (!f->bytes_in_seg) {
      if (f->last_seg)
          return EOP;
      else if (!next_segment(f))
          return EOP;
      radassert(f->bytes_in_seg > 0);
    }
    --f->bytes_in_seg;
    //++f->packet_bytes;
    return get8(f);
}

static int get8_packet(vorb *f)
{
   int x = get8_packet_raw(f);
   f->valid_bits = 0;
   f->acc = 0;
   return x;
}

static void flush_packet(vorb *f)
{
   while (get8_packet_raw(f) != EOP)
   { // empty scope for wii warning.
   }
}

// @OPTIMIZE: this is the secondary bit decoder, so it's probably not as important
// as the huffman decoder?
static uint32 get_bits(vorb *f, int n)
{
    uint32 z;

    if (f->valid_bits < 0) return 0;
    if (f->valid_bits < n) {
        if (n > 24) {
            // the accumulator technique below would not work correctly in this case
            z = get_bits(f, 24);
            z |= get_bits(f, n-24) << 24;
            return z;
        }
        if (f->valid_bits == 0) f->acc = 0;
        while (f->valid_bits < n) {
            int z = get8_packet_raw(f);
            if (z == EOP) {
                f->valid_bits = INVALID_BITS;
                return 0;
            }
            f->acc |= ((acc_t) z) << f->valid_bits;
            f->valid_bits += 8;
        }
    }
    if (f->valid_bits < 0) return 0;
    z = (uint32) f->acc & ((1 << n)-1);
    f->acc >>= n;
    f->valid_bits -= n;
        return z;
}

// Never called?
static int32 get_bits_signed(vorb *f, int n)
{
   int32 z = (int32) get_bits(f, n);
   z <<= 32-n;
   z >>= 32-n;
   return z;
}

// @OPTIMIZE: primary accumulator for huffman
// expand the buffer to as many bits as possible without reading off end of packet
// it might be nice to allow f->valid_bits and f->acc to be stored in registers,
// e.g. cache them locally and decode locally
static void _prep_huffman(vorb *f)
{
    radassert((f->valid_bits > 0) || (f->acc == 0));
    do {
        int z;
        z = get8_packet_raw(f);
        if (z == EOP)
            return;

        f->acc |= ((acc_t) z) << f->valid_bits;
        f->valid_bits += 8;
    } while (f->valid_bits <= ACC_LIMIT);
}

#define prep_huffman(f) if (f->valid_bits <= ACC_LIMIT) _prep_huffman(f)

#define DUMP(i, l)

__forceinline int decode_scalar(vorb *f, Codebook *c); // wii warning
__forceinline
int decode_scalar(vorb *f, Codebook *c)
{
    uint32 len;
    acc_t acc;
#ifdef TMA_WIDE_ACCUMULATOR
    if (f->valid_bits < 24)
#else
    if (f->valid_bits < 8)
#endif
        _prep_huffman(f);

    int idx = c->fast_decode1[(uint32) f->acc & 255];
    if (idx < 0) {
#ifndef TMA_WIDE_ACCUMULATOR
        prep_huffman(f);
#endif
        acc = f->acc >> 8;
        do {
            int i = idx + 0x80000000;
            i += (int) acc & 3;
            acc >>= 2;
            idx = c->fast_decode1[i];
        } while (idx < 0);
    }
    len = idx >> 26;
    idx &= (1 << 26)-1;  // 26 bits max
    f->acc >>= len;
#ifndef NO_DECODE_CHECK
    if ((f->valid_bits -= (int) len) < 0) {
        idx = EOP;
        //radassert(f->acc == 0);
        f->valid_bits = 0;
        return -1;
    }
    DUMP(idx, len);
#else
    f->valid_bits -= (int) len;
#endif
    return idx;
}

enum
{
   VORBIS_packet_id = 1,
   VORBIS_packet_comment = 3,
   VORBIS_packet_setup = 5
};

#if (defined(__POWERPC__) && defined(__MACH__)) ||  defined(__CELLOS_LV2__) || (defined(HOLLYWOOD_REV) || defined(REVOLUTION)) ||defined(__MWERKS__)

 static inline S32 __cntlzw( S32 arg )
      {
        U32 val;
        asm (
          "cntlzw %0,%1"
          : "=r" (val)
          : "r" (arg) );
        return val;
      }

static int ilog(int32 n)
{
    unsigned long i;
    i = 32 - __cntlzw( n );
    return i;
}

#elif defined(__psp__)

#define count_leading_zeros(count, x) \
  __asm__ ("clz %0,%1"              \
     : "=r" (count)                   \
     : "r" (x))


static int ilog(S32 n)
{
    unsigned long i;
    count_leading_zeros(i, n);
    i = 32 - i;
    return i;
}

#elif defined(IS_IPHONE) || defined(IS_3DS) || defined(IS_PSP2) || defined(IS_WIIU) || defined(__RADSEKRIT2__) || defined(__RADANDROID__)
// \todo psp2 3ds wiiu ilog

static int ilog(S32 n)
{
    int startn = n;
    int count = 32;
    while (n) { n >>= 1; count--; }

    count = 32 - count;
    return count;
}

#elif defined(R5900)

#define count_leading_zeros(count, x) \
  __asm__ ("plzcw %0,%1"              \
     : "=r" (count)                   \
     : "r" (x))


static int ilog(S32 n)
{
    unsigned long i;
    count_leading_zeros(i, n);
    i = 32 - (i & 0xFF) - 1;
    return i;
}


#elif (defined(_XENON) || (_XBOX_VER == 200))

static int ilog(int32 n)
{
    unsigned long i;
    i = 32 - _CountLeadingZeros( n );
    return i;
}

#else

#if defined( _MSC_VER)
// #include <intrin.h> doesn't work in debug mode, so we'll add these prototypes manually - jm
extern "C" unsigned char _BitScanForward(unsigned long* Index, unsigned long Mask);
extern "C" unsigned char _BitScanReverse(unsigned long* Index, unsigned long Mask);
#endif

#if defined(__INTEL_COMPILER)
// Intel is missing the BitScanReverse() function! :-(
unsigned char _BitScanReverse(unsigned long *i, unsigned long n)
{
    unsigned char result;
    __asm {
        mov ecx,[n]
        mov edx,[i]
        xor eax,eax
        bsr ecx,ecx
        mov [edx],ecx
        setnz al
        mov [result],al
    }
    return result;
}
#endif

#if defined(__WATCOMC__)

unsigned char _BitScanReverse(unsigned long * i, unsigned long n);
#pragma aux _BitScanReverse = "xor eax,eax" "bsr ecx,ecx" "mov [edx],ecx" "setnz al" parm [edx] [ecx] modify [eax];

#endif


// this is a weird definition of log2() for which log2(1) = 1, log2(2) = 2, log2(4) = 3
// as required by the specification.
static int ilog(int32 n)
{
    unsigned long i;
    if (_BitScanReverse(&i, n))
        i++;
    return i;
}

#endif


#define DECODE_RAW(var,f,c)    var = decode_scalar(f,c);
#define DECODE(var,f,c)        var = decode_scalar(f,c);
#define DECODE_VQ(var,f,c)   DECODE_RAW(var,f,c)


// CODEBOOK_ELEMENT_FAST is an optimization for the CODEBOOK_FLOATS case
// where we avoid one addition
#ifndef STB_VORBIS_CODEBOOK_FLOATS
   #define CODEBOOK_ELEMENT(c,off)          (c->multiplicands[off] * c->delta_value + c->minimum_value)
   #define CODEBOOK_ELEMENT_FAST(c,off)     (c->multiplicands[off] * c->delta_value)
   #define CODEBOOK_ELEMENT_BASE(c)         (c->minimum_value)
#else
   #define CODEBOOK_ELEMENT(c,off)          (c->multiplicands[off])
   #define CODEBOOK_ELEMENT_FAST(c,off)     (c->multiplicands[off])
   #define CODEBOOK_ELEMENT_BASE(c)         (0)
#endif

__forceinline
static int codebook_decode_start(vorb *f, Codebook *c, int len)
{
   int z = -1;

   // type 0 is only legal in a scalar context
   if (c->lookup_type == 0)
      error(f, VORBIS_invalid_stream);
   else {
      //DECODE_VQ(z,f,c);
      z = decode_scalar(f,c);
#ifdef NO_DECODE_CHECK
      if (f->valid_bits < 0) {
#else
      if (z == EOP) {  // check for EOP
#endif
         if (!f->bytes_in_seg)
            if (f->last_seg)
               return z;
         error(f, VORBIS_invalid_stream);
      }
   }
   return z;
}

__forceinline
static int codebook_decode(vorb *f, Codebook *c, float *output, int len)
{
   int i,z = codebook_decode_start(f,c,len);
   if (z < 0) return FALSE;
   if (len > c->dimensions) len = c->dimensions;

   z *= c->dimensions;
   if (c->sequence_p) {
      float last = CODEBOOK_ELEMENT_BASE(c);
      for (i=0; i < len; ++i) {
         float val = CODEBOOK_ELEMENT_FAST(c,z+i) + last;
         output[i] += val;
         last = val + c->minimum_value;
      }
   } else {
      float last = CODEBOOK_ELEMENT_BASE(c);
      for (i=0; i < len; ++i) {
         output[i] += CODEBOOK_ELEMENT_FAST(c,z+i) + last;
      }
   }

   return TRUE;
}

static int codebook_decode_step(vorb *f, Codebook *c, float *output, int len, int step)
{
   int i,z = codebook_decode_start(f,c,len);
   float last = CODEBOOK_ELEMENT_BASE(c);
   if (z < 0) return FALSE;
   if (len > c->dimensions) len = c->dimensions;

   z *= c->dimensions;
   for (i=0; i < len; ++i) {
      float val = CODEBOOK_ELEMENT_FAST(c,z+i) + last;
      output[i*step] += val;
      if (c->sequence_p) last = val;
   }

   return TRUE;
}


// This version allows the use of a form of the huffman decoder where buffer overrun is tested
// by the caller, by checking if valid_bits is less than zero. This is normally a very marginal win
// so not enabled by default.
//__forceinline
static int codebook_decode_deinterleave_repeat(vorb *f, Codebook *c, float **outputs, int ch, int *c_inter_p, int *p_inter_p, int len, int total_decode)
{
   int c_inter = *c_inter_p;
   int p_inter = *p_inter_p;
   int i,z, effective = c->dimensions;

   // type 0 is only legal in a scalar context
   if (c->lookup_type == 0)   return error(f, VORBIS_invalid_stream);

   if (c->sequence_p) {
      while (total_decode > 0) {
         float last = CODEBOOK_ELEMENT_BASE(c);
         //DECODE_VQ(z,f,c);
         z = decode_scalar(f,c);
#ifdef NO_DECODE_CHECK
         if (f->valid_bits < 0) {
#else
         if (z == EOP) {  // check for EOP
#endif
            if (!f->bytes_in_seg)
               if (f->last_seg) return FALSE;
            return error(f, VORBIS_invalid_stream);
         }

         z *= effective;
         for (i=0; i < effective; ++i) {
            float val = CODEBOOK_ELEMENT_FAST(c,z+i) + last;
            outputs[c_inter][p_inter] += val;
            if (++c_inter == ch) {
                c_inter = 0;
                ++p_inter;
                if (p_inter >= len) {
                    goto done;
                }
            }
            last = val;
         }
         total_decode -= effective;
      }
   }
   else {
      while (total_decode > 0) {
         float last = CODEBOOK_ELEMENT_BASE(c);
         //DECODE_VQ(z,f,c);
         z = decode_scalar(f,c);
#ifdef NO_DECODE_CHECK
         if (f->valid_bits < 0) {
#else
         if (z == EOP) {  // check for EOP
#endif
            if (!f->bytes_in_seg)
               if (f->last_seg)
                   return FALSE;
            return error(f, VORBIS_invalid_stream);
         }

         z *= effective;
         for (i=0; i < effective; ++i) {
            float val = CODEBOOK_ELEMENT_FAST(c,z+i) + last;
            outputs[c_inter][p_inter] += val;
            if (++c_inter == ch) {
                c_inter = 0;
                ++p_inter;
                if (p_inter >= len) {
                    goto done;
                }
            }
         }
         total_decode -= effective;
      }
   }
done:
   *c_inter_p = c_inter;
   *p_inter_p = p_inter;
   return TRUE;
}


// This form of the multi-channel residue decoder is optimized for interleaved decoding of stereo signals, with
// the deinterleaving handled by a separate stage at the end, using SSE instructions.
static int codebook_decode_nointerleave_repeat(vorb *f, Codebook *c, float *output, int pos, int len, int total_decode)
{
   int i, z, effective = c->dimensions;

   // type 0 is only legal in a scalar context
   if (c->lookup_type == 0)   return error(f, VORBIS_invalid_stream);

   if (c->sequence_p) {
      while (total_decode > 0) {
         float last = CODEBOOK_ELEMENT_BASE(c);
         //DECODE_VQ(z,f,c);
         z = decode_scalar(f,c);
#ifdef NO_DECODE_CHECK
         if (f->valid_bits < 0) {
#else
         if (z == EOP) {  // check for EOP
#endif
            if (!f->bytes_in_seg)
               if (f->last_seg) return FALSE;
            return error(f, VORBIS_invalid_stream);
         }

         z *= effective;
         for (i=0; i < effective; ++i) {
            float val = CODEBOOK_ELEMENT_FAST(c,z+i) + last;
            output[pos++] += val;
            if (pos >= len) goto done;  // Extra space in residue buffers, check can be skipped!
            last = val;
         }
         total_decode -= effective;
      }
   }
   else {
      while (total_decode > 0) {
         float last = CODEBOOK_ELEMENT_BASE(c);
         //DECODE_VQ(z,f,c);
         z = decode_scalar(f,c);
#ifdef NO_DECODE_CHECK
         if (f->valid_bits < 0) {
#else
         if (z == EOP) {  // check for EOP
#endif
            if (!f->bytes_in_seg)
               if (f->last_seg)
                   return FALSE;
            return error(f, VORBIS_invalid_stream);
         }

         z *= effective;
         for (i=0; i < effective; ++i) {
            float val = CODEBOOK_ELEMENT_FAST(c,z+i) + last;
            output[pos++] += val;
            if (pos >= len) goto done;  // If I skip this test, the code runs 2% slower!!!
         }
         total_decode -= effective;
      }
   }
done:
   return TRUE;
}

#ifdef TMA_USE_RECIPROCAL_MUL
static int predict_point_rec(int x, int x0, int x1, uint32 adx_rec, int y0, int y1)
{
   int dy = y1 - y0;
   int adx = x1-x0;
   // The reciprocal mul trick only works for unsigned values!
   int err = abs(dy) * (x - x0);
   // Check that we got the correct reciprocal passed in:
   radassert(adx_rec == calc_reciprocal(adx));
   int off = use_reciprocal(err, adx_rec);
   // Verify the result:
   // (JM: removed check because this shortcut does not always round the same as int division.
   // E.g., if err=37800 and adx=1304, the true value is 28.99+, but off=29.  This happens because 
   // calc_reciprocal(1304)=1609, since 0x200000/1304=1608.2 and the truncated result of 1608
   // is not a power of 2.  Seems to be working otherwise so I'll just yank the radassert...)
   // radassert(off == err / adx);
   dy >>= 31;
   off = (off ^ dy) - dy;
   return y0 + off;
}
#else
static int predict_point(int x, int x0, int x1, int y0, int y1)
{
   int stepy = y1 - y0;
   int stepx = x1 - x0;
   int dx = x-x0;
   // Integer division _must_ truncate!
   int y = y0 + stepy*dx/stepx;
//   radassert(y == _predict_point(x, x0, x1, y0, y1));
   return y;
}
#endif

// the following table is block-copied from the specification
static float inverse_db_table[256] =
{
  1.0649863e-07f, 1.1341951e-07f, 1.2079015e-07f, 1.2863978e-07f,
  1.3699951e-07f, 1.4590251e-07f, 1.5538408e-07f, 1.6548181e-07f,
  1.7623575e-07f, 1.8768855e-07f, 1.9988561e-07f, 2.1287530e-07f,
  2.2670913e-07f, 2.4144197e-07f, 2.5713223e-07f, 2.7384213e-07f,
  2.9163793e-07f, 3.1059021e-07f, 3.3077411e-07f, 3.5226968e-07f,
  3.7516214e-07f, 3.9954229e-07f, 4.2550680e-07f, 4.5315863e-07f,
  4.8260743e-07f, 5.1396998e-07f, 5.4737065e-07f, 5.8294187e-07f,
  6.2082472e-07f, 6.6116941e-07f, 7.0413592e-07f, 7.4989464e-07f,
  7.9862701e-07f, 8.5052630e-07f, 9.0579828e-07f, 9.6466216e-07f,
  1.0273513e-06f, 1.0941144e-06f, 1.1652161e-06f, 1.2409384e-06f,
  1.3215816e-06f, 1.4074654e-06f, 1.4989305e-06f, 1.5963394e-06f,
  1.7000785e-06f, 1.8105592e-06f, 1.9282195e-06f, 2.0535261e-06f,
  2.1869758e-06f, 2.3290978e-06f, 2.4804557e-06f, 2.6416497e-06f,
  2.8133190e-06f, 2.9961443e-06f, 3.1908506e-06f, 3.3982101e-06f,
  3.6190449e-06f, 3.8542308e-06f, 4.1047004e-06f, 4.3714470e-06f,
  4.6555282e-06f, 4.9580707e-06f, 5.2802740e-06f, 5.6234160e-06f,
  5.9888572e-06f, 6.3780469e-06f, 6.7925283e-06f, 7.2339451e-06f,
  7.7040476e-06f, 8.2047000e-06f, 8.7378876e-06f, 9.3057248e-06f,
  9.9104632e-06f, 1.0554501e-05f, 1.1240392e-05f, 1.1970856e-05f,
  1.2748789e-05f, 1.3577278e-05f, 1.4459606e-05f, 1.5399272e-05f,
  1.6400004e-05f, 1.7465768e-05f, 1.8600792e-05f, 1.9809576e-05f,
  2.1096914e-05f, 2.2467911e-05f, 2.3928002e-05f, 2.5482978e-05f,
  2.7139006e-05f, 2.8902651e-05f, 3.0780908e-05f, 3.2781225e-05f,
  3.4911534e-05f, 3.7180282e-05f, 3.9596466e-05f, 4.2169667e-05f,
  4.4910090e-05f, 4.7828601e-05f, 5.0936773e-05f, 5.4246931e-05f,
  5.7772202e-05f, 6.1526565e-05f, 6.5524908e-05f, 6.9783085e-05f,
  7.4317983e-05f, 7.9147585e-05f, 8.4291040e-05f, 8.9768747e-05f,
  9.5602426e-05f, 0.00010181521f, 0.00010843174f, 0.00011547824f,
  0.00012298267f, 0.00013097477f, 0.00013948625f, 0.00014855085f,
  0.00015820453f, 0.00016848555f, 0.00017943469f, 0.00019109536f,
  0.00020351382f, 0.00021673929f, 0.00023082423f, 0.00024582449f,
  0.00026179955f, 0.00027881276f, 0.00029693158f, 0.00031622787f,
  0.00033677814f, 0.00035866388f, 0.00038197188f, 0.00040679456f,
  0.00043323036f, 0.00046138411f, 0.00049136745f, 0.00052329927f,
  0.00055730621f, 0.00059352311f, 0.00063209358f, 0.00067317058f,
  0.00071691700f, 0.00076350630f, 0.00081312324f, 0.00086596457f,
  0.00092223983f, 0.00098217216f, 0.0010459992f,  0.0011139742f,
  0.0011863665f,  0.0012634633f,  0.0013455702f,  0.0014330129f,
  0.0015261382f,  0.0016253153f,  0.0017309374f,  0.0018434235f,
  0.0019632195f,  0.0020908006f,  0.0022266726f,  0.0023713743f,
  0.0025254795f,  0.0026895994f,  0.0028643847f,  0.0030505286f,
  0.0032487691f,  0.0034598925f,  0.0036847358f,  0.0039241906f,
  0.0041792066f,  0.0044507950f,  0.0047400328f,  0.0050480668f,
  0.0053761186f,  0.0057254891f,  0.0060975636f,  0.0064938176f,
  0.0069158225f,  0.0073652516f,  0.0078438871f,  0.0083536271f,
  0.0088964928f,  0.009474637f,   0.010090352f,   0.010746080f,
  0.011444421f,   0.012188144f,   0.012980198f,   0.013823725f,
  0.014722068f,   0.015678791f,   0.016697687f,   0.017782797f,
  0.018938423f,   0.020169149f,   0.021479854f,   0.022875735f,
  0.024362330f,   0.025945531f,   0.027631618f,   0.029427276f,
  0.031339626f,   0.033376252f,   0.035545228f,   0.037855157f,
  0.040315199f,   0.042935108f,   0.045725273f,   0.048696758f,
  0.051861348f,   0.055231591f,   0.058820850f,   0.062643361f,
  0.066714279f,   0.071049749f,   0.075666962f,   0.080584227f,
  0.085821044f,   0.091398179f,   0.097337747f,   0.10366330f,
  0.11039993f,    0.11757434f,    0.12521498f,    0.13335215f,
  0.14201813f,    0.15124727f,    0.16107617f,    0.17154380f,
  0.18269168f,    0.19456402f,    0.20720788f,    0.22067342f,
  0.23501402f,    0.25028656f,    0.26655159f,    0.28387361f,
  0.30232132f,    0.32196786f,    0.34289114f,    0.36517414f,
  0.38890521f,    0.41417847f,    0.44109412f,    0.46975890f,
  0.50028648f,    0.53279791f,    0.56742212f,    0.60429640f,
  0.64356699f,    0.68538959f,    0.72993007f,    0.77736504f,
  0.82788260f,    0.88168307f,    0.9389798f,     1.0f
};


// @OPTIMIZE: if you want to replace this bresenham line-drawing routine,
// note that you must produce bit-identical output to decode correctly;
// this specific sequence of operations is specified in the spec (it's
// drawing integer-quantized frequency-space lines that the encoder
// expects to be exactly the same)
//     ... also, isn't the whole point of Bresenham's algorithm to NOT
// have to divide in the setup? sigh.
//
// TMa: The divide is because we're not really drawing complete lines,
// just a single sample per x value. This means that when dy > dx, y
// will make steps of more than 1. The division is to calculate the
// base step distanse, then sy is the additional distance (+/- 1) to
// be taken each time the error term passes zero.
//
// err is preadjusted so that it will go from >= 0 to < 0 instead of
// testing against "err >= adx" as in the spec.
//
// By carefully controlling the error terms, it is possible to use
// fp operations to directly calculate the y values, with no error
// term branching required, but this requires double precision to
// work across the maximum frame distance (8192 samples)
//
// On my development machine, a Core 2 Duo, this was slower than the
// integer code, even when prescaling the fy value such that the
// byte y value could be extracted directly from byte [5] of the
// double prec variable.
//
#ifndef STB_VORBIS_NO_DEFER_FLOOR
#define LINE_OP(a,b)   a *= b
#else
#define LINE_OP(a,b)   a = b
#endif

// Using fixed-point coordinate calculations saves a test/branch/adjust stage
// inside the inner loop, this alone almost doubles the speed of this function

#ifdef TMA_FRACTIONAL_LINE
static
__forceinline   // Only called from do_floor, nowhere else!
void draw_line(float *output, int x0, int y0, int x1, int y1, int n)
{
   int dy = y1 - y0;
   radassert(x1 <= n);
   int adx = x1 - x0;
   int x=x0,y=y0;
   int err = 0;

   if (dy < 0) {
       uint32 uy = (uint32) (256-y), udy = (uint32) -dy;
       uy <<= 24; udy <<= 24;
       uint32 ubase = udy / adx;
       if (adx & (adx-1))
           ubase++;
       LINE_OP(output[x], inverse_db_table[y]);
       for (++x; x < x1; ++x) {
           uy += ubase;
           y = 256 - (uy >> 24);
           LINE_OP(output[x], inverse_db_table[y]);
       }
   }
   else {
       uint32 uy = (uint32) y, udy = (uint32) dy;
       uy <<= 24; udy <<= 24;
       uint32 ubase = udy / adx;
       if (adx & (adx-1))
           ubase++;
       LINE_OP(output[x], inverse_db_table[y]);
       for (++x; x < x1; ++x) {
           uy += ubase;
           y = uy >> 24;
           LINE_OP(output[x], inverse_db_table[y]);
       }
   }
}
#else
static
__forceinline   // Only called from do_floor, nowhere else!
void draw_line(float *output, int x0, int y0, int x1, int y1, int n)
{
   int dy = y1 - y0;
   // if (x1 > n) x1 = n;
   radassert(x1 <= n);
   int adx = x1 - x0;
   int ady = abs(dy);
   int base;
   int x=x0,y=y0;
   int err = 0;
   int sy;

   // if (dy < 0) sy = -1; else sy = +1;

   // The following division could normally be replaced by a reciprocal division,
   // but only if the two surrounding points both had non-zero offsets from the
   // ideal line: If they happened to land exactly on the line, they would be skipped
   // and the ADX value used below would not be the same as we previously calculated
   // a reciprocal value for during setup.

   base = dy/adx;
   sy = ((dy >> 31) & -2) + 1;  // sy = (dy < 0)? -1 : +1;
   ady -= abs(base) * adx;
   LINE_OP(output[x], inverse_db_table[y]);
   err = adx-1;
   for (++x; x < x1; ++x) {
      y += base;
      if ((err -= ady) < 0) {
         y += sy;
         err += adx;
      }
      LINE_OP(output[x], inverse_db_table[y]);
   }
}
#endif

static int residue_decode(vorb *f, Codebook *book, float *target, int offset, int n, int rtype)
{
   int k;
   if (rtype == 0) {
#ifndef TMA_USE_RECIPROCAL_MUL
      int step = n / book->dimensions;
#else
      int step = use_reciprocal(n, book->dim_reciprocal);
      radassert(step == n / book->dimensions);
#endif
      for (k=0; k < step; ++k)
         if (!codebook_decode_step(f, book, target+offset+k, n-offset-k, step))
            return FALSE;
   } else {
      for (k=0; k < n; ) {
         if (!codebook_decode(f, book, target+offset, n-k))
            return FALSE;
         k += book->dimensions;
         offset += book->dimensions;
      }
   }
   return TRUE;
}

static void decode_residue(vorb *f, float *residue_buffers[], int ch, int n, int rn, uint8 *do_not_decode)
{
   int i,j,pass;
   Residue *r = f->residue_config + rn;
   int rtype = f->residue_types[rn];
   int c = r->classbook;
   int classwords = f->codebooks[c]->dimensions;
   int n_read = r->end - r->begin, part_read;
#ifndef TMA_USE_RECIPROCAL_MUL
   part_read = n_read / r->part_size;
#else
   part_read = use_reciprocal(n_read, r->part_size_reciprocal);
   radassert(part_read == (int) (n_read / r->part_size));
   //uint32 ch_reciprocal = calc_reciprocal(ch);
#endif

   int temp_alloc_point = temp_alloc_save(f);
   #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
   uint8 ***part_classdata = (uint8 ***) temp_block_array(f,f->channels, part_read * sizeof(**part_classdata));
   #else
   int **classifications = (int **) temp_block_array(f,f->channels, part_read * sizeof(**classifications));
   #endif

   if (rtype == 2 && ch != 1) {
      int len = ch * n;
      for (j=0; j < ch; ++j)
         if (!do_not_decode[j])
            break;
      if (j == ch) {
         goto done;
      }

      if (ch == 2 && n <= 1024) // Use local SSE buffer
      {
          VEC4F_STRUCT residue_sse[512];  // Room for 1024 samples from 2 channels
          float *residue = (float *)(void*)residue_sse;

          thememset(residue_sse, 0, sizeof(float)*len);

          for (pass=0; pass < 8; ++pass) {
              int pcount = 0, class_set = 0;
              while (pcount < part_read) {
                  int z = r->begin + pcount*r->part_size;
                  if (pass == 0) {
                      Codebook *c = f->codebooks[r->classbook];
                      int q;
                      //DECODE(q,f,c);
                      q = decode_scalar(f,c);
#ifdef NO_DECODE_CHECK
                      if (f->valid_bits < 0)
#else
                      if (q == EOP)
#endif
                          goto residue_done;
#ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                      part_classdata[0][class_set] = r->classdata[q];
#else
                      for (i=classwords-1; i >= 0; --i) {
                          classifications[0][i+pcount] = q % r->classifications;
                          q /= r->classifications;
                      }
#endif
                  }
                  for (i=0; i < classwords && pcount < part_read; ++i, ++pcount) {
                      int z = r->begin + pcount*r->part_size;
#ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                      int c = part_classdata[0][class_set][i];
#else
                      int c = classifications[0][pcount];
#endif
                      int b = r->residue_books[c][pass];
                      if (b >= 0) {
                          Codebook *book = f->codebooks[b];
                          if (!codebook_decode_nointerleave_repeat(f, book, residue, z, len, r->part_size))
                              goto residue_done;
                      }
                  }
#ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                  ++class_set;
#endif
              }
          }
residue_done:
          // Deinterleave the residue into the individual channel residue buffers:
              VEC4F_STRUCT *l = (VEC4F_STRUCT *)residue_buffers[0], *r = (VEC4F_STRUCT *)residue_buffers[1];
              DECL_SCRATCH;
              for (int i = 0; i < len; i+= 8)
              {
                  DECL_VEC4F(f0);
                  DECL_VEC4F(f1);
                  DECL_VEC4F(o0);
                  DECL_VEC4F(o1);
                  V4LOAD(f0, residue + i);
                  V4LOAD(f1, residue + i + 4);
                  V4PERM0202(o0, f0, f1);
                  V4PERM1313(o1, f0, f1);
                  V4STORE(l, o0);
                  V4STORE(r, o1);
                  l++;
                  r++;
              }
      }
      else
// Fallback code for non-SSE, frame sizes > 2048 samples and/or more than 2 channels
      {
          for (i=0; i < ch; ++i)
              if (!do_not_decode[i])
                  thememset(residue_buffers[i], 0, sizeof(float) * n);

#ifdef TMA_USE_RECIPROCAL_MUL
          uint32 ch_reciprocal = calc_reciprocal(ch);
#endif
          for (pass=0; pass < 8; ++pass) {
              int pcount = 0, class_set = 0;
              while (pcount < part_read) {
                  int z = r->begin + pcount*r->part_size;
                  int c_inter, p_inter;
#ifndef TMA_USE_RECIPROCAL_MUL
                  c_inter = z % ch;
                  p_inter = z / ch;
#else
                  p_inter = use_reciprocal(z, ch_reciprocal);
                  radassert(p_inter == z / ch);
                  c_inter = z - ch*p_inter;
#endif
                  if (pass == 0) {
                      Codebook *c = f->codebooks[r->classbook];
                      int q;
                      DECODE(q,f,c);
                      if (q == EOP) goto done;
#ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                      part_classdata[0][class_set] = r->classdata[q];
#else
                      for (i=classwords-1; i >= 0; --i) {
                          classifications[0][i+pcount] = q % r->classifications;
                          q /= r->classifications;
                      }
#endif
                  }
                  for (i=0; i < classwords && pcount < part_read; ++i, ++pcount) {
                      int z = r->begin + pcount*r->part_size;
#ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                      int c = part_classdata[0][class_set][i];
#else
                      int c = classifications[0][pcount];
#endif
                      int b = r->residue_books[c][pass];
                      if (b >= 0) {
                          Codebook *book = f->codebooks[b];
                          if (!codebook_decode_deinterleave_repeat(f, book, residue_buffers, ch, &c_inter, &p_inter, n, r->part_size))
                              goto done;
                      } else {
                          z += r->part_size;
#ifndef TMA_USE_RECIPROCAL_MUL
                          c_inter = z % ch;
                          p_inter = z / ch;
#else
                          p_inter = use_reciprocal(z, ch_reciprocal);
                          c_inter = z - ch*p_inter;
#endif
                      }
                  }
#ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                  ++class_set;
#endif
              }
          }
      }

      goto done;
   }

   // This was delayed past the previous stereo block:
   for (i=0; i < ch; ++i)
      if (!do_not_decode[i])
         thememset(residue_buffers[i], 0, sizeof(float) * n);

   for (pass=0; pass < 8; ++pass) {
      int pcount = 0, class_set=0;
      while (pcount < part_read) {
         if (pass == 0) {
            for (j=0; j < ch; ++j) {
               if (!do_not_decode[j]) {
                  Codebook *c = f->codebooks[r->classbook];
                  int temp;
                  //DECODE(temp,f,c);
                  temp = decode_scalar(f,c);
#ifdef NO_DECODE_CHECK
                  if (f->valid_bits < 0) goto done;
#else
                  if (temp == EOP) goto done;
#endif
                  #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                  part_classdata[j][class_set] = r->classdata[temp];
                  #else
                  for (i=classwords-1; i >= 0; --i) {
                     classifications[j][i+pcount] = temp % r->classifications;
                     temp /= r->classifications;
                  }
                  #endif
               }
            }
         }
         for (i=0; i < classwords && pcount < part_read; ++i, ++pcount) {
            for (j=0; j < ch; ++j) {
               if (!do_not_decode[j]) {
                  #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
                  int c = part_classdata[j][class_set][i];
                  #else
                  int c = classifications[j][pcount];
                  #endif
                  int b = r->residue_books[c][pass];
                  if (b >= 0) {
                     float *target = residue_buffers[j];
                     int offset = r->begin + pcount * r->part_size;
                     int n = r->part_size;
                     Codebook *book = f->codebooks[b];
                     if (!residue_decode(f, book, target, offset, n, rtype))
                        goto done;
                  }
               }
            }
         }
         #ifndef STB_VORBIS_DIVIDES_IN_RESIDUE
         ++class_set;
         #endif
      }
   }
  done:
   temp_alloc_restore(f,temp_alloc_point);
}


// @OPTIMIZE: This takes n/2 input data and outputs n data. There are
// apparently papers from e.g. 1999 and 2002 for doing fast MDCT, whereas
// this paper is from 1992. I see rumors that there are ways to implement
// it by processing only n/2 items, until the end which would presumably
// double its speed. But this implementation might do that already? If so,
// what the heck are the current papers about?

// the following were split out into separate functions while optimizing;
// they could be pushed back up but eh. __forceinline showed no change;
// they're probably already being inlined.

static void imdct_step3_iter0_loop_sse(int n, float *e, int i_off, int k_off, float *A)
{
   float *ee0 = e + i_off - 3;
   float *ee2 = ee0 + k_off;

   DECL_ZEROES;
   DECL_VEC4F(e0);
   DECL_VEC4F(e2);
   DECL_VEC4F(sum);
   DECL_VEC4F(dif);
   DECL_VEC4F(difr);
   DECL_VEC4F(A80);
   DECL_VEC4F(A91);
   DECL_VEC4F(A01);
   DECL_VEC4F(A89);
   DECL_SCRATCH;

   V4LOAD(A01, zeroes);
   V4LOAD(A89, zeroes);

   SETUP_PMPM;

   int i;

   radassert((n & 3) == 0);
   for (i=(n>>2); i > 0; --i)
   {
      V2LOAD(A01, A01, (A));
      V2LOAD(A89, A89, (A+8));
      V4PERM0000(A80, A89, A01);
      V4PERM1111(A91, A89, A01);

      V4LOAD(e0, ee0);
      V4LOAD(e2, ee2);

      V4SUB(dif, e0, e2);
      V4ADD(sum, e0, e2);
      V4NEG0101(A91, A91);
      V4PERM1032(difr, dif, dif);

      DECL_VEC4F(p1);
      DECL_VEC4F(p2);
      DECL_VEC4F(d);

      V4MUL(p1, dif, A80);
      V4MUL(p2, difr, A91);

      V4ADD(d, p1, p2);

      V4STORE(ee0, sum);
      V4STORE(ee2, d);

      A += 16;
      ee0 -= 4;
      ee2 -= 4;
      V2LOAD(A01, A01, (A));
      V2LOAD(A89, A89, (A+8));
      V4PERM0000(A80, A89, A01);
      V4PERM1111(A91, A89, A01);

      V4LOAD(e0, ee0);
      V4LOAD(e2, ee2);

      V4SUB(dif, e0, e2);
      V4ADD(sum, e0, e2);
      V4NEG0101(A91, A91);
      V4PERM1032(difr, dif, dif);

      V4MUL(p1, dif, A80);
      V4MUL(p2, difr, A91);

      V4ADD(d, p1, p2);

      V4STORE(ee0, sum);
      V4STORE(ee2, d);

      A += 16;
      ee0 -= 4;
      ee2 -= 4;
   }
}

static void imdct_step3_iter0_loop2_sse(int n, float *e, int i_off, int k_off, int pair_off, float *A)
{
   float *ee0 = e + i_off - 3;
   float *ee2 = ee0 + k_off;

   DECL_ZEROES;
   DECL_VEC4F(e0);
   DECL_VEC4F(e2);
   DECL_VEC4F(sum);
   DECL_VEC4F(dif);
   DECL_VEC4F(difr);
   DECL_VEC4F(A80);
   DECL_VEC4F(A91);
   DECL_VEC4F(A01);
   DECL_VEC4F(A89);
   DECL_SCRATCH;


   V4LOAD(A01, zeroes);
   V4LOAD(A89, zeroes);

   SETUP_PMPM

   int i;

   radassert((n & 3) == 0);
   for (i=(n>>2); i > 0; --i)
   {
      V2LOAD(A01, A01, (A));
      V2LOAD(A89, A89, (A+8));
      V4PERM0000(A80, A89, A01);
      V4PERM1111(A91, A89, A01);
      V4NEG0101(A91, A91);

      V4LOAD(e0, ee0);
      V4LOAD(e2, ee2);

      V4SUB(dif, e0, e2);
      V4ADD(sum, e0, e2);
      V4PERM1032(difr, dif, dif);

      DECL_VEC4F(p1);
      DECL_VEC4F(p2);
      DECL_VEC4F(d);

      V4MUL(p1, dif, A80);
      V4MUL(p2, difr, A91);

      V4ADD(d, p1, p2);

      V4STORE(ee0, sum);
      V4STORE(ee2, d);

      /**********************************/

      V4LOAD(e0, ee0+pair_off);
      V4LOAD(e2, ee2+pair_off);

      V4SUB(dif, e0, e2);
      V4ADD(sum, e0, e2);
      V4PERM1032(difr, dif, dif);

      V4MUL(p1, dif, A80);
      V4MUL(p2, difr, A91);

      V4ADD(d, p1, p2);

      V4STORE(ee0+pair_off, sum);
      V4STORE(ee2+pair_off, d);

      /**********************************/

      A += 16;
      ee0 -= 4;
      ee2 -= 4;

      /**********************************/

      V2LOAD(A01, A01, (A));
      V2LOAD(A89, A89, (A+8));
      V4PERM0000(A80, A89, A01);
      V4PERM1111(A91, A89, A01);
      V4NEG0101(A91, A91);

      V4LOAD(e0, ee0);
      V4LOAD(e2, ee2);

      V4SUB(dif, e0, e2);
      V4ADD(sum, e0, e2);
      V4PERM1032(difr, dif, dif);

      V4MUL(p1, dif, A80);
      V4MUL(p2, difr, A91);

      V4ADD(d, p1, p2);

      V4STORE(ee0, sum);
      V4STORE(ee2, d);

      /**********************************/

      V4LOAD(e0, ee0+pair_off);
      V4LOAD(e2, ee2+pair_off);

      V4SUB(dif, e0, e2);
      V4ADD(sum, e0, e2);
      V4PERM1032(difr, dif, dif);

      V4MUL(p1, dif, A80);
      V4MUL(p2, difr, A91);

      V4ADD(d, p1, p2);

      V4STORE(ee0+pair_off, sum);
      V4STORE(ee2+pair_off, d);

      A += 16;
      ee0 -= 4;
      ee2 -= 4;
   }
}

static void imdct_step3_inner_r_loop_sse(int lim, float *e, int d0, int k_off, float *A, int k1)
{
   int i;

   DECL_ZEROES;

   DECL_VEC4F(ee0);
   DECL_VEC4F(ee2);
   DECL_VEC4F(sum);
   DECL_VEC4F(dif);
   DECL_VEC4F(difr);
   DECL_VEC4F(A80);
   DECL_VEC4F(A91);
   DECL_VEC4F(A01);
   DECL_VEC4F(A89);
   DECL_SCRATCH;

   V4LOAD(A01, zeroes);
   V4LOAD(A89, zeroes);

   SETUP_PMPM

   float *e0 = e + d0 - 7;
   float *e2 = e0 + k_off;

   for (i=lim >> 2; i > 0; --i)
   {
      V2LOAD(A01, A01, (A));
      V2LOAD(A89, A89, (A+k1));
      V4PERM0000(A80, A89, A01);
      V4PERM1111(A91, A89, A01);
      V4NEG0101(A91, A91);

      V4LOAD(ee0, e0+4);
      V4LOAD(ee2, e2+4);
      V4SUB(dif, ee0, ee2);
      V4ADD(sum, ee0, ee2);
      V4PERM1032(difr, dif, dif);

      DECL_VEC4F(p1);
      DECL_VEC4F(p2);
      DECL_VEC4F(d);

      V4MUL(p1, dif, A80);
      V4MUL(p2, difr, A91);

      V4ADD(d, p1, p2);

      V4STORE(e0+4, sum);
      V4STORE(e2+4, d);
      A += k1+k1;

      V2LOAD(A01, A01, (A));
      V2LOAD(A89, A89, (A+k1));
      V4PERM0000(A80, A89, A01);
      V4PERM1111(A91, A89, A01);
      V4NEG0101(A91, A91);

      V4LOAD(ee0, e0);
      V4LOAD(ee2, e2);
      V4SUB(dif, ee0, ee2);
      V4ADD(sum, ee0, ee2);
      V4PERM1032(difr, dif, dif);

      V4MUL(p1, dif, A80);
      V4MUL(p2, difr, A91);

      V4ADD(d, p1, p2);

      V4STORE(e0, sum);
      V4STORE(e2, d);
      e0 -= 8;
      e2 -= 8;

      A += k1+k1;
   }
}

static void imdct_step3_inner_r_loop2_sse(int lim, float *e, int d0, int k_off, int pair_off, float *A, int k1)
{
   int i;

   DECL_ZEROES;

   DECL_VEC4F(ee0);
   DECL_VEC4F(ee2);
   DECL_VEC4F(sum);
   DECL_VEC4F(dif);
   DECL_VEC4F(difr);
   DECL_VEC4F(A80);
   DECL_VEC4F(A91);
   DECL_VEC4F(A01);
   DECL_VEC4F(A89);
   DECL_SCRATCH;

   V4LOAD(A01, zeroes);
   V4LOAD(A89, zeroes);

   SETUP_PMPM

   DECL_VEC4F(p1);
   DECL_VEC4F(p2);
   DECL_VEC4F(d);

   float *e0 = e + d0 - 7;
   float *e2 = e0 + k_off;

   for (i=lim >> 2; i > 0; --i)
   {
      V2LOAD(A01, A01, (A));
      V2LOAD(A89, A89, (A+k1));
      V4PERM0000(A80, A89, A01);
      V4PERM1111(A91, A89, A01);
      V4NEG0101(A91, A91);

/*************************************/

      V4LOAD(ee0, e0+4);
      V4LOAD(ee2, e2+4);
      V4SUB(dif, ee0, ee2);
      V4ADD(sum, ee0, ee2);
      V4PERM1032(difr, dif, dif);
      V4MUL(p1, dif, A80);
      V4MUL(p2, difr, A91);

      V4ADD(d, p1, p2);

      V4STORE(e0+4, sum);
      V4STORE(e2+4, d);

/*************************************/

      V4LOAD(ee0, e0+4+pair_off);
      V4LOAD(ee2, e2+4+pair_off);
      V4SUB(dif, ee0, ee2);
      V4ADD(sum, ee0, ee2);
      V4PERM1032(difr, dif, dif);
      V4MUL(p1, dif, A80);
      V4MUL(p2, difr, A91);

      V4ADD(d, p1, p2);

      V4STORE(e0+4+pair_off, sum);
      V4STORE(e2+4+pair_off, d);

/*************************************/

      A += k1+k1;

      V2LOAD(A01, A01, (A));
      V2LOAD(A89, A89, (A+k1));
      V4PERM0000(A80, A89, A01);
      V4PERM1111(A91, A89, A01);
      V4NEG0101(A91, A91);

/*************************************/

      V4LOAD(ee0, e0);
      V4LOAD(ee2, e2);
      V4SUB(dif, ee0, ee2);
      V4ADD(sum, ee0, ee2);
      V4PERM1032(difr, dif, dif);

      V4MUL(p1, dif, A80);
      V4MUL(p2, difr, A91);

      V4ADD(d, p1, p2);

      V4STORE(e0, sum);
      V4STORE(e2, d);

/*************************************/

      V4LOAD(ee0, e0+pair_off);
      V4LOAD(ee2, e2+pair_off);
      V4SUB(dif, ee0, ee2);
      V4ADD(sum, ee0, ee2);
      V4PERM1032(difr, dif, dif);

      V4MUL(p1, dif, A80);
      V4MUL(p2, difr, A91);

      V4ADD(d, p1, p2);

      V4STORE(e0+pair_off, sum);
      V4STORE(e2+pair_off, d);

/*************************************/

      e0 -= 8;
      e2 -= 8;

      A += k1+k1;
   }
}

static void imdct_step3_inner_s_loop_sse(int n, float *e, int i_off, int k_off, float *A, int a_off, int k0)
{
   int i;

   DECL_ZEROES;
   DECL_VEC4F(e0);
   DECL_VEC4F(e2);
   DECL_VEC4F(sum);
   DECL_VEC4F(dif);
   DECL_VEC4F(difr);
   DECL_VEC4F(A2200);
   DECL_VEC4F(A3311);
   DECL_VEC4F(A6644);
   DECL_VEC4F(A7755);
   DECL_VEC4F(A01);
   DECL_VEC4F(A89);
   V4LOAD(A01, zeroes);
   V4LOAD(A89, zeroes);

   DECL_SCRATCH;

   SETUP_PMPM

   V2LOAD(A01, A01, (A));
   V2LOAD(A89, A89, (A+a_off));
   V4PERM0000(A2200, A89, A01);
   V4PERM1111(A3311, A89, A01);
   V4NEG0101(A3311, A3311);

   V2LOAD(A01, A01, (A+a_off*2));
   V2LOAD(A89, A89, (A+a_off*3));
   V4PERM0000(A6644, A89, A01);
   V4PERM1111(A7755, A89, A01);
   V4NEG0101(A7755, A7755);

   float *ee0 = e  +i_off-7;
   float *ee2 = ee0+k_off;

   for (i=n; i > 0; --i)
   {
      V4LOAD(e0, ee0+4);
      V4LOAD(e2, ee2+4);
      V4SUB(dif, e0, e2);
      V4ADD(sum, e0, e2);
      V4PERM1032(difr, dif, dif);

      DECL_VEC4F(p1);
      DECL_VEC4F(p2);
      DECL_VEC4F(d);

      V4MUL(p1, dif, A2200);
      V4MUL(p2, difr, A3311);

      V4ADD(d, p1, p2);

      V4STORE(ee0+4, sum);
      V4STORE(ee2+4, d);

      V4LOAD(e0, ee0);
      V4LOAD(e2, ee2);
      V4SUB(dif, e0, e2);
      V4ADD(sum, e0, e2);
      V4PERM1032(difr, dif, dif);
      V4MUL(p1, dif, A6644);
      V4MUL(p2, difr, A7755);

      V4ADD(d, p1, p2);

      V4STORE(ee0, sum);
      V4STORE(ee2, d);

      ee0 -= k0;
      ee2 -= k0;
   }
}

static __forceinline void iter_54_sse(float *z, VEC4F_STRUCT* e3_str, VEC4F_STRUCT* e7_str)
{
    DECL_VEC4F(e3);
    DECL_VEC4F(e7);
    DECL_SCRATCH;

    V4LOAD(e3, e3_str);
    V4LOAD(e7, e7_str);

    DECL_VEC4F(p);
    DECL_VEC4F(m);

    SETUP_MMPP
    SETUP_PMMP

    V4ADD(p, e3, e7); // z3+z7, z2+z6,z1+z5,z0+z4
    V4SUB(m, e3, e7); // z3-z7, z2-z6,z1-z5,z0-z4

    V4PERM2323(e3, p,p); // z1+z5,z0+z4, z1+z5, z0+z4
    V4PERM0101(e7, p,p); // z3+z7,z2+z6, z3+z7,z2+z6
    V4NEG1100(e7, e7);
    V4ADD(e3, e3, e7);
    V4STORE(z-3, e3);
    V4PERM2323(e3, m,m); // z1-z5,z0-z4, z1-z5,z0-z4
    V4PERM1010(e7, m,m); // z2-z6,z3-z7, z2-z6,z3-z7
    V4NEG0110(e7, e7);
    V4ADD(e3, e3, e7);
    V4STORE(z-7, e3);
}

static void imdct_step3_inner_s_loop_ld654_sse(int n, float *e, int i_off, float *A, int base_n)
{
   int k_off = -8;
   int a_off = base_n >> 3;
   float A2 = A[0+a_off];
   float *z = e + i_off;
   float *base = z - 16 * n;

   SETUP_V4ONE
   SETUP_V4ZERO
   SETUP_MPMP
   SETUP_PMPM

   DECL_VEC4F(A22);
   DECL_SCRATCH;

   DECL_VEC4F(ASRC);
   V1LOAD(ASRC, A+a_off);
   V4PERM0000(A22, ASRC, v4one);

   VEC4F_STRUCT ToPassA, ToPassB;

   while (z > base)
   {
      DECL_VEC4F(e0);
      DECL_VEC4F(e1);
      DECL_VEC4F(e2);
      DECL_VEC4F(e3);
      DECL_VEC4F(dif1);
      DECL_VEC4F(dif0);
      DECL_VEC4F(d01);
      DECL_VEC4F(d45);

      V4LOAD(e3, z-3);
      V4LOAD(e2, z-7);
      V4LOAD(e1, z-11);
      V4LOAD(e0, z-15);

      V4SUB(dif1, e3, e1);

      V4SUB(dif0, e2, e0);
      V4NEG0101(dif0, dif0);

      // temp use d regs for param passing.
      V4ADD(d01, e3, e1);
      V4ADD(d45, e2, e0);
      V4STORE(&ToPassA, d01);
      V4STORE(&ToPassB, d45);
      iter_54_sse(z, &ToPassA, &ToPassB);

      V4PERM1032(d45, dif1, v4zero);
      V4NEG1010(d45, d45);
      V4ADD(dif1, dif1, d45);
      V4MUL(dif1, dif1, A22);

      V4PERM0000(d01, dif0, v4zero);
      V4PERM1132(d45, dif0, dif0);
      V4NEG1010(d01, d01);
      V4ADD(dif0, d45, d01);
      V4MUL(dif0, dif0, A22);

      V4STORE(&ToPassA, dif1);
      V4STORE(&ToPassB, dif0);
      iter_54_sse(z-8, &ToPassA, &ToPassB);
      z -= 16;
   }
}

static void inverse_mdct(float *buffer, int n, vorb *f, int blocktype)
{
   int n2 = n >> 1, n4 = n >> 2, n8 = n >> 3, l;
   int n3_4 = n - n4, ld;
   // @OPTIMIZE: reduce register pressure by using fewer variables?
   int save_point = temp_alloc_save(f);
   float *buf2 = (float *) temp_alloc(f, n2 * sizeof(*buf2)+15);
   buf2 = (float *) (((intptr_t) buf2 + 15) & ~15);
   float *u=NULL,*v=NULL;
   // twiddle factors
   float *A = f->A[blocktype];

   // IMDCT algorithm from "The use of multirate filter banks for coding of high quality digital audio"
   // See notes about bugs in that paper in less-optimal implementation 'inverse_mdct_old' after this function.

   // kernel from paper


   // merged:
   //   copy and reflect spectral data
   //   step 0

   // note that it turns out that the items added together during
   // this step are, in fact, being added to themselves (as reflected
   // by step 0). inexplicable inefficiency! this became obvious
   // once I combined the passes.

   // so there's a missing 'times 2' here (for adding X to itself).
   // this propogates through linearly to the end, where the numbers
   // are 1/2 too small, and need to be compensated for.

   SETUP_MMMM
   SETUP_PMPM
   DECL_SCRATCH;

   {
      float *d0,*d1,*e, *AA0, *AA1, *e_stop;
      d0 = &buf2[n2-4];
      AA0 = A;
      e = buffer;
      e_stop = &buffer[n2];
      d1 = buf2;
      AA1 = &A[n2-4];

      do {
         DECL_VEC4F(e0);
         DECL_VEC4F(e4);
         DECL_VEC4F(e4400);
         DECL_VEC4F(e6622);
         DECL_VEC4F(e3377);
         DECL_VEC4F(e1155);
         DECL_VEC4F(A0);
         DECL_VEC4F(A0r);
         DECL_VEC4F(A1);
         DECL_VEC4F(A1r);

         V4LOAD(A0, AA0);
         V4LOAD(A1, AA1);
         V4PERM2301(A0r, A0,A0);
         V4PERM2301(A1r, A1,A1);

         DECL_VEC4F(A0p);
         DECL_VEC4F(A1p);

         V4PERM3210(A0p, A0,A0);
         V4PERM3210(A1p, A1,A1);

         V4NEG0101(A0r, A0r);
         V4NEG0101(A1r, A1r);

         V4LOAD(e0, e);
         V4LOAD(e4, e+4);
         V4PERM0000(e4400, e4, e0);
         V4PERM2222(e6622, e4, e0);
         V4PERM3333(e3377, e0, e4);
         V4PERM1111(e1155, e0, e4);

         DECL_VEC4F(temp0);
         DECL_VEC4F(temp1);
         V4MUL(temp0, e4400, A0p);
         V4MUL(temp1, e6622, A0r);
         V4ADD(temp0, temp0, temp1);

         V4STORE(d0, temp0);
         d0 -= 4;
         AA0 += 4;

         V4MUL(temp0, e3377, A1p);
         V4MUL(temp1, e1155, A1r);
         V4ADD(e0, temp0, temp1);
         V4NEG1111(e0, e0);
         V4STORE(d1, e0);
         d1 += 4;
         AA1 -= 4;
         e += 8;
      } while (e != e_stop);
   }
   // now we use symbolic names for these, so that we can
   // possibly swap their meaning as we change which operations
   // are in place


   u = buffer;
   v = buf2;

   // step 2    (paper output is w, now u)
   // this could be in place, but the data ends up in the wrong
   // place... _somebody_'s got to swap it, so this is nominated
   {
      float *AA = &A[n2-8];
      float *d0,*d1, *e0, *e1;

      e0 = &v[n4];
      e1 = &v[0];

      d0 = &u[n4];
      d1 = &u[0];
      while (AA >= A)
      {
         //VEC4F ee0, ee1, dif, difr, A4400, A5511;
         DECL_VEC4F(ee0);
         DECL_VEC4F(ee1);
         DECL_VEC4F(dif);
         DECL_VEC4F(difr);
         DECL_VEC4F(A4400);
         DECL_VEC4F(A5511);

         V4LOAD(ee0, AA);
         V4LOAD(ee1, AA+4);
         V4PERM1111(A5511, ee1, ee0);
         V4PERM0000(A4400, ee1, ee0);
         V4NEG0101(A5511, A5511);

         V4LOAD(ee0, e0);
         V4LOAD(ee1, e1);
         V4SUB(dif, ee0, ee1);

         DECL_VEC4F(temp0);
         V4ADD(temp0, ee0, ee1);
         V4STORE(d0, temp0);
         V4PERM1032(difr, dif, dif);

         V4MUL(temp0, dif, A4400);
         V4MUL(ee0, difr, A5511);
         V4ADD(temp0, temp0, ee0);
         V4STORE(d1, temp0);

         AA -= 8;
         d0 += 4;
         d1 += 4;
         e0 += 4;
         e1 += 4;
      }
   }


   // step 3
   ld = ilog(n) - 1; // ilog is off-by-one from normal definitions

   // optimized step 3:

   // the original step3 loop can be nested r inside s or s inside r;
   // it's written originally as s inside r, but this is dumb when r
   // iterates many times, and s few. So I have two copies of it and
   // switch between them halfway.

   // TMa: Calculating the twiddle factors is nearly as costly as the
   // actual Butterfly operation itself, so it makes sense to run two
   // iterations in parallel, sharing the cost of the twiddle calculation!

   // this is iteration 0 of step 3
   imdct_step3_iter0_loop2_sse(n >> 4, u, n2-1, -(n >> 3), -n4, A);


   // this is iteration 1 of step 3
   // Paired iterations here as well:
   imdct_step3_inner_r_loop2_sse(n >> 5, u, n2-1 - n8*0, -(n >> 4), -n8, A, 16);
   imdct_step3_inner_r_loop2_sse(n >> 5, u, n2-1 - n8*2, -(n >> 4), -n8, A, 16);
   l=2;

   for (; l < (ld-3)>>1; ++l) {
      int k0 = n >> (l+2), k0_2 = k0>>1;
      int lim = 1 << (l+1);
      int i;
      // and pairs here:
      for (i=0; i < lim; i+=2)
         imdct_step3_inner_r_loop2_sse(n >> (l+4), u, n2-1 - k0*i, -k0_2, -k0, A, 1 << (l+3));
   }

   for (; l < ld-6; ++l) {
      int k0 = n >> (l+2), k1 = 1 << (l+3), k0_2 = k0>>1;
      int rlim = n >> (l+6), r;
      int lim = 1 << (l+1);
      int i_off;
      float *A0 = A;
      i_off = n2-1;
      for (r=rlim; r > 0; --r) {
         imdct_step3_inner_s_loop_sse(lim, u, i_off, -k0_2, A0, k1, k0);
         A0 += k1*4;
         i_off -= 8;
      }
   }

   // iterations with count:
   //   ld=6,5,4 all interleaved together
   //       the big win comes from getting rid of needless flops
   //         due to the constants on pass 5 & 4 being all 1 and 0;
   //       combining them to be simultaneous to improve cache made little difference
   imdct_step3_inner_s_loop_ld654_sse(n >> 5, u, n2-1, A, n);

   // output is u

   // step 4, 5, and 6
   // cannot be in-place because of step 5
   {
      uint16 *bitrev = f->bit_reverse[blocktype];
      // weirdly, I'd have thought reading sequentially and writing
      // erratically would have been better than vice-versa, but in
      // fact that's not what my testing showed. (That is, with
      // j = bitreverse(i), do you read i and write j, or read j and write i.)

      float *d0 = &v[n4-4];
      float *d1 = &v[n2-4];
      while (d0 >= v)
      {
         int ka, kb;
         ka = bitrev[0];
         kb = bitrev[1];

         DECL_VEC4F(a);
         DECL_VEC4F(b);
         DECL_VEC4F(t);

         V4LOAD(a, u+ka);
         V4LOAD(b, u+kb);

         V4PERM1010(t, b,a);
         V4STORE(d1, t);
         V4PERM3232(t, b,a);
         V4STORE(d0, t);

         d0 -= 4;
         d1 -= 4;
         bitrev += 2;
      }
   }
   // (paper output is u, now v)


   // data must be in buf2
   radassert(v == buf2);

   // step 7   (paper output is v, now v)
   // this is now in place
   {
      float *C = f->C[blocktype];
      float *d, *e;

      d = v;
      e = v + n2 - 4;
      while (d < e)
      {
         DECL_VEC4F(d0);
         DECL_VEC4F(e0);
         DECL_VEC4F(a);
         DECL_VEC4F(ar);
         DECL_VEC4F(b2367);
         DECL_VEC4F(b0145);
         DECL_VEC4F(C1133);
         DECL_VEC4F(C0022);

         V4LOAD(e0, C);
         V4PERM0022(C0022, e0,e0);
         V4PERM1133(C1133, e0,e0);
         V4NEG0101(C0022, C0022);

         V4LOAD(e0, e);
         V4LOAD(d0, d);
         V4PERM2301(a, e0,e0);
         V4NEG0101(e0, a);

         V4SUB(a, d0, e0);
         V4ADD(b2367, d0, e0);
         V4PERM1032(ar, a,a);

         V4MUL(a, a, C1133);
         V4MUL(ar, ar, C0022);
         V4ADD(b0145, a, ar);

         V4ADD(a, b2367, b0145);
         V4STORE(d, a);

         V4SUB(e0, b2367, b0145);
         V4NEG0101(a, e0);
         V4PERM2301(e0,a, a);
         V4STORE(e, e0);
         C += 4;
         d += 4;
         e -= 4;
      }
   }
   // data must be in buf2

   // step 8+decode   (paper output is X, now buffer)
   // this generates pairs of data a la 8 and pushes them directly through
   // the decode kernel (pushing rather than pulling) to avoid having
   // to make another pass later

   // this cannot POSSIBLY be in place, so we refer to the buffers directly

   {
      float *d0,*d1,*d2,*d3;

      float *B = f->B[blocktype] + n2 - 8;
      float *e = buf2 + n2 - 8;
      d0 = &buffer[0];
      d1 = &buffer[n2-4];
      d2 = &buffer[n2];
      d3 = &buffer[n-4];
      while (e >= v)
      {
         DECL_VEC4F(p3175);
         DECL_VEC4F(p2064);
         DECL_VEC4F(e0);
         DECL_VEC4F(e4);
         DECL_VEC4F(e6420);
         DECL_VEC4F(e7531);
         DECL_VEC4F(B7531);
         DECL_VEC4F(B6420);

         V4LOAD(e0, B);
         V4LOAD(e4, B+4);
         V4PERM3131(B7531, e4,e0);
         V4PERM2020(B6420, e4,e0);

         V4LOAD(e0, e);
         V4LOAD(e4, e+4);
         V4PERM3131(e7531, e4,e0);
         V4PERM2020(e6420, e4,e0);

         V4MUL(e0, e6420, B7531);
         V4MUL(e4, e7531, B6420);
         V4SUB(p3175, e0, e4);

         V4MUL(e0, e6420, B6420);
         V4MUL(e4, e7531, B7531);
         V4ADD(p2064, e0, e4);

         V4NEG1111(p2064, p2064);
         V4STORE(d0, p3175);
         V4NEG1111(e0, p3175);
         V4PERM3210(p3175, e0, e0);

         V4STORE(d1, p3175);
         V4STORE(d2, p2064);

         V4PERM3210(e0, p2064,p2064);
         V4STORE(d3, e0);

         B -= 8;
         e -= 8;
         d0 += 4;
         d2 += 4;
         d1 -= 4;
         d3 -= 4;
      }
   }

   temp_alloc_restore(f,save_point);
}


static float *get_window(vorb *f, int len)
{
   len <<= 1;
   if (len == f->blocksize_0) return f->window[0];
   if (len == f->blocksize_1) return f->window[1];
   radassert(0);
   return NULL;
}

#ifndef STB_VORBIS_NO_DEFER_FLOOR
typedef int16 YTYPE;
#else
typedef int YTYPE;
#endif
static int do_floor(vorb *f, Mapping *map, int i, int n, float *target, YTYPE *finalY, uint8 *step2_flag)
{
   int n2 = n >> 1;
   int s = map->chan[i].mux, floor;
   floor = map->submap_floor[s];
   if (f->floor_types[floor] == 0) {
      return error(f, VORBIS_invalid_stream);
   } else {
      Floor1 *g = &f->floor_config[floor].floor1;
      int j,q;
      int lx = 0, ly = finalY[0] * g->floor1_multiplier;

      for (q=1; q < g->values; ++q) {
         j = g->sorted_order[q];
         #ifndef STB_VORBIS_NO_DEFER_FLOOR
         if (finalY[j] >= 0)
         #else
         if (step2_flag[j])
         #endif
         {
            int hy = finalY[j] * g->floor1_multiplier;
            int hx = g->Xlist[j];
            draw_line(target, lx,ly, hx,hy, n2);
            lx = hx, ly = hy;
         }
      }
      if (lx < n2)
         // optimization of: draw_line(target, lx,ly, n,ly, n2);
         for (j=lx; j < n2; ++j)
            LINE_OP(target[j], inverse_db_table[ly]);
   }
   return TRUE;
}

static int vorbis_decode_packet(vorb *f, int *len, int *p_left, int *p_right)
{
   Mode *m;
   Mapping *map;
   int i,j,k,n,prev,next,n2;
   int zero_channel[256];
   int really_zero_channel[256];
   int window_center, left_start, left_end, right_start, right_end, left_n, right_n;

   f->channel_buffer_start = f->channel_buffer_end = 0;

   if (f->eof) return FALSE;
   if (!maybe_start_packet(f))
      return FALSE;
   // check packet type
   if (get_bits(f,1) != 0)
   {
      return error(f,VORBIS_bad_packet_type);
   }

   i = get_bits(f, ilog(f->mode_count-1));
   if (i == EOP) return FALSE;
   if (i >= f->mode_count) return FALSE;
   m = f->mode_config + i;
   if (m->blockflag) {
      n = f->blocksize_1;
      prev = get_bits(f,1);
      next = get_bits(f,1);
   } else {
      prev = next = 0;
      n = f->blocksize_0;
   }


// WINDOWING
   window_center = n >> 1;
   if (m->blockflag && !prev) {
      left_start = (n - f->blocksize_0) >> 2;
      left_end   = (n + f->blocksize_0) >> 2;
   } else {
      left_start = 0;
      left_end   = window_center;
   }
   left_n = left_end - left_start;
   if (m->blockflag && !next) {
      right_start = (n*3 - f->blocksize_0) >> 2;
      right_end   = (n*3 + f->blocksize_0) >> 2;
   } else {
      right_start = window_center;
      right_end   = n;
   }
   right_n = right_end - right_start;
   *p_right = right_start;
   *p_left = left_start;

   map = &f->mapping[m->mapping];

// FLOORS
   n2 = n >> 1;

   for (i=0; i < f->channels; ++i) {
      int s = map->chan[i].mux, floor;
      zero_channel[i] = FALSE;
      floor = map->submap_floor[s];
      if (f->floor_types[floor] == 0) {
         return error(f, VORBIS_invalid_stream);
      } else {
         Floor1 *g = &f->floor_config[floor].floor1;
         if (get_bits(f, 1)) {
            short *finalY;
            uint8 step2_flag[256];
            static int range_list[4] = { 256, 128, 86, 64 };
            int range = range_list[g->floor1_multiplier-1];
            int offset = 2;
            finalY = f->finalY[i];
            finalY[0] = get_bits(f, ilog(range)-1);
            finalY[1] = get_bits(f, ilog(range)-1);
            for (j=0; j < g->partitions; ++j) {
               int pclass = g->partition_class_list[j];
               int cdim = g->class_dimensions[pclass];
               int cbits = g->class_subclasses[pclass];
               int csub = (1 << cbits)-1;
               int cval = 0;
               if (cbits) {
                  Codebook *c = f->codebooks[g->class_masterbooks[pclass]];
                  //DECODE(cval,f,c);
                  cval = decode_scalar(f,c);
               }
               for (k=0; k < cdim; ++k) {
                  int book = g->subclass_books[pclass][cval & csub];
                  cval = cval >> cbits;
                  int temp = 0;
                  if (book >= 0) {
                     Codebook *c = f->codebooks[book];
                     //DECODE(temp,f,c);
                     temp = decode_scalar(f,c);
                  }
                  finalY[offset++] = temp;
               }
            }
            if (f->valid_bits == INVALID_BITS) goto error; // behavior according to spec
            step2_flag[0] = step2_flag[1] = 1;
            for (j=2; j < g->values; ++j) {
               int low, high, pred, highroom, lowroom, val;
               low = g->neighbors[j][0];
               high = g->neighbors[j][1];
#ifdef TMA_USE_RECIPROCAL_MUL
               pred = predict_point_rec(g->Xlist[j], g->Xlist[low], g->Xlist[high], g->dx_reciprocal[j], finalY[low], finalY[high]);
#else
               pred = predict_point(g->Xlist[j], g->Xlist[low], g->Xlist[high], finalY[low], finalY[high]);
#endif

               step2_flag[j] = 0;
               val = finalY[j];
               if (val) {
                  step2_flag[low] = step2_flag[high] = step2_flag[j] = 1;

                  highroom = range - pred;
                  lowroom = pred;
                  if (highroom > lowroom) {
                     if (val >= lowroom*2) {
                        pred = val;
                        //pred += val - lowroom;
                     }
                     else {
                        int v = 0 - (val & 1);
                        val >>= 1;
                        pred += (val ^ v);
                     }
                  }
                  else {
                     if (val >= highroom*2) {
                        pred -= val - highroom + 1;
                     }
                     else {
                        int v = 0 - (val & 1);
                        val >>= 1;
                        pred += (val ^ v);
                     }
                  }
               }
               finalY[j] = pred;
            }
#ifdef STB_VORBIS_NO_DEFER_FLOOR
            do_floor(f, map, i, n, f->floor_buffers[i], finalY, step2_flag);
#else
            // defer final floor computation until _after_ residue
            for (j=0; j < g->values; ++j) {
               if (!step2_flag[j])
                  finalY[j] = -1;
            }
#endif
         } else {
           error:
            zero_channel[i] = TRUE;
         }
         // So we just defer everything else to later

         // at this point we've decoded the floor into buffer
      }
   }
   // at this point we've decoded all floors

   // re-enable coupled channels if necessary
   thememcpy(really_zero_channel, zero_channel, sizeof(really_zero_channel[0]) * f->channels);
   for (i=0; i < map->coupling_steps; ++i)
      if (!zero_channel[map->chan[i].magnitude] || !zero_channel[map->chan[i].angle]) {
         zero_channel[map->chan[i].magnitude] = zero_channel[map->chan[i].angle] = FALSE;
      }

// RESIDUE DECODE
   for (i=0; i < map->submaps; ++i) {
      float *residue_buffers[STB_VORBIS_MAX_CHANNELS];
      int r,t;
      uint8 do_not_decode[256];
      int ch = 0;
      for (j=0; j < f->channels; ++j) {
         if (map->chan[j].mux == i) {
            if (zero_channel[j]) {
               do_not_decode[ch] = TRUE;
               residue_buffers[ch] = NULL;
            } else {
               do_not_decode[ch] = FALSE;
               residue_buffers[ch] = f->channel_buffers[j];
            }
            ++ch;
         }
      }
      r = map->submap_residue[i];
      t = f->residue_types[r];
      decode_residue(f, residue_buffers, ch, n2, r, do_not_decode);
   }

// INVERSE COUPLING
   for (i = map->coupling_steps-1; i >= 0; --i) {
      int n2 = n >> 1;
      float *m = f->channel_buffers[map->chan[i].magnitude];
      float *a = f->channel_buffers[map->chan[i].angle    ];

#if defined(RADVEC_SUPPORTS_VECTOR_COMPARE) // SSE replacement for a very branchy C algorithm
      for (j=0; j < n2; j+=4)
      {
          DECL_VEC4F(aa);
          DECL_VEC4F(mm);
          DECL_VEC4F(ag);
          DECL_VEC4F(mg);

          V4LOAD(aa, a+j);
          V4LOAD(mm, m+j);
          ag = (VEC4F_STRUCT)V4CMPGT(aa, mmmm);
          mg = (VEC4F_STRUCT)V4CMPGT(mm, mmmm);
          // Swap sign of A if (M > 0):
          aa = V4XOR(aa, V4AND(mg, mmmm));
          DECL_VEC4F(new_a);
          DECL_VEC4F(new_m);
          V4ADD(new_a, mm, V4AND(ag, aa));
          V4SUB(new_m, mm, V4ANDNOT(ag, aa));
          V4STORE(a+j, new_a);
          V4STORE(m+j, new_m);
      }
#else
      for (j=0; j < n2; ++j) {
         float a2,m2;
         if (m[j] > 0)
            if (a[j] > 0)
               m2 = m[j], a2 = m[j] - a[j];
            else
               a2 = m[j], m2 = m[j] + a[j];
         else
            if (a[j] > 0)
               m2 = m[j], a2 = m[j] + a[j];
            else
               a2 = m[j], m2 = m[j] - a[j];
         m[j] = m2;
         a[j] = a2;
      }
#endif
   }

   // finish decoding the floors
#ifndef STB_VORBIS_NO_DEFER_FLOOR
   for (i=0; i < f->channels; ++i) {
      if (really_zero_channel[i]) {
         thememset(f->channel_buffers[i], 0, sizeof(*f->channel_buffers[i]) * n2);
      } else {
         do_floor(f, map, i, n, f->channel_buffers[i], f->finalY[i], NULL);
      }
   }
#else
   for (i=0; i < f->channels; ++i) {
      if (really_zero_channel[i]) {
         thememset(f->channel_buffers[i], 0, sizeof(*f->channel_buffers[i]) * n2);
      } else {
         for (j=0; j < n2; ++j)
            f->channel_buffers[i][j] *= f->floor_buffers[i][j];
      }
   }
#endif


// INVERSE MDCT
   for (i=0; i < f->channels; ++i)
      inverse_mdct(f->channel_buffers[i], n, f, m->blockflag);

   // this shouldn't be necessary, unless we exited on an error
   // and want to flush to get to the next packet
   flush_packet(f);

   if (f->first_decode) {
      // assume we start so first non-discarded sample is sample 0
      // this isn't to spec, but spec would require us to read ahead
      // and decode the size of all current frames--could be done,
      // but presumably it's not a commonly used feature
      f->current_loc = -n2; // start of first frame is positioned for discard
      // we might have to discard samples "from" the next frame too,
      // if we're lapping a large block then a small at the start?
      f->discard_samples_deferred = n - right_end;
      f->current_loc_valid = TRUE;
      f->first_decode = FALSE;
   } else if (f->discard_samples_deferred) {
      left_start += f->discard_samples_deferred;
      *p_left = left_start;
      f->discard_samples_deferred = 0;
   } else if (f->previous_length == 0 && f->current_loc_valid) {
      // we're recovering from a seek... that means we're going to discard
      // the samples from this packet even though we know our position from
      // the last page header, so we need to update the position based on
      // the discarded samples here
      // but wait, the code below is going to add this in itself even
      // on a discard, so we don't need to do it here...
   }

   // check if we have ogg information about the sample # for this packet
   if (f->last_seg_which == f->end_seg_with_known_loc) {
      // if we have a valid current loc, and this is final:
      if (f->current_loc_valid && (f->page_flag & PAGEFLAG_last_page)) {
         uint32 current_end = f->known_loc_for_packet - (n-right_end);
         // then let's infer the size of the (probably) short final frame
         if (current_end < f->current_loc + right_end) {
            if (current_end < f->current_loc) {
               // negative truncation, that's impossible!
               *len = 0;
            } else {
               *len = current_end - f->current_loc;
            }
            *len += left_start;
            f->current_loc += *len;
            return TRUE;
         }
      }
      // otherwise, just set our sample loc
      // guess that the ogg granule pos refers to the _middle_ of the
      // last frame?
      // set f->current_loc to the position of left_start
      f->current_loc = f->known_loc_for_packet - (n2-left_start);
      f->current_loc_valid = TRUE;
   }
   if (f->current_loc_valid)
      f->current_loc += (right_start - left_start);

   *len = right_end;  // ignore samples after the window goes to 0
   return TRUE;
}

static int vorbis_finish_frame(stb_vorbis *f, int len, int left, int right)
{
   int prev,i,j;
   // we use right&left (the start of the right- and left-window sin()-regions)
   // to determine how much to return, rather than inferring from the rules
   // (same result, clearer code); 'left' indicates where our sin() window
   // starts, therefore where the previous window's right edge starts, and
   // therefore where to start mixing from the previous buffer. 'right'
   // indicates where our sin() ending-window starts, therefore that's where
   // we start saving, and where our returned-data ends.

   DECL_SCRATCH;


   // mixin from previous window
   if (f->previous_length) {
      int i,j, n = f->previous_length;
      float *w = get_window(f, n);
      for (i=0; i < f->channels; ++i) {
          // Use SSE code, requires 4-float alignment!
         radassert((left & 3) == 0);
         radassert((n & 7) == 0);

         float *pw = f->previous_window[i], *cb = &f->channel_buffers[i][left];
         for (j=0; j < n; j+=4)
         {
             DECL_VEC4F(t0);
             DECL_VEC4F(t1);
             DECL_VEC4F(w0);
             DECL_VEC4F(w1);

             V4LOAD(t0, &cb[j]);
             V4LOAD(w0, &w[j]);
             V4LOAD(t1, &w[n-4-j]);
             V4PERM3210(w1, t1, t1);
             V4LOAD(t1, &pw[j]);

             V4MUL(t0, t0,w0);
             V4MUL(t1, t1,w1);
             V4ADD(t0, t0,t1);
             V4STORE(&cb[j], t0);
         }
      }
   }

   prev = f->previous_length;

   // last half of this data becomes previous window
   f->previous_length = len - right;

   // @OPTIMIZE: could avoid this copy by double-buffering the
   // output (flipping previous_window with channel_buffers), but
   // then previous_window would have to be 2x as large, and
   // channel_buffers couldn't be temp mem (although they're NOT
   // currently temp mem, they could be (unless we want to level
   // performance by spreading out the computation))
   for (i=0; i < f->channels; ++i) {
      float *pw = f->previous_window[i], *cb = &f->channel_buffers[i][right];
      for (j=0; right+j < len; j+=16)
      {
          DECL_VEC4F(t0);
          DECL_VEC4F(t1);
          DECL_VEC4F(t2);
          DECL_VEC4F(t3);

          V4LOAD(t0, cb+j);
          V4LOAD(t1, cb+j+4);
          V4LOAD(t2, cb+j+8);
          V4LOAD(t3, cb+j+12);
          V4STORE(pw+j, t0);
          V4STORE(pw+j+4, t1);
          V4STORE(pw+j+8, t2);
          V4STORE(pw+j+12, t3);
      }
   }

   if (!prev)
      // there was no previous packet, so this data isn't valid...
      // this isn't entirely true, only the would-have-overlapped data
      // isn't valid, but this seems to be what the spec requires
      return 0;

   // truncate a short frame
   if (len < right) right = len;

   f->samples_output += right-left;

   return right - left;
}

static void vorbis_pump(stb_vorbis *f)
{
   int len, right, left;
   if (vorbis_decode_packet(f, &len, &left, &right))
      vorbis_finish_frame(f, len, left, right);
}

static int is_whole_packet_present(stb_vorbis *f, int end_page)
{
   // make sure that we have the packet available before continuing...
   // this requires a full ogg parse, but we know we can fetch from f->stream

   // instead of coding this out explicitly, we could save the current read state,
   // read the next packet with get8() until end-of-packet, check f->eof, then
   // reset the state? but that would be slower, esp. since we'd have over 256 bytes
   // of state to restore (primarily the page segment table)

   int s = f->next_seg, first = TRUE;
   uint8 *p = f->stream;

   if (s != -1) { // if we're not starting the packet with a 'continue on next page' flag
      for (; s < f->segment_count; ++s) {
         p += f->segments[s];
         if (f->segments[s] < 255)               // stop at first short segment
            break;
      }
      // either this continues, or it ends it...
      if (end_page)
         if (s < f->segment_count-1)             return error(f, VORBIS_invalid_stream);
      if (s == f->segment_count)
         s = -1; // set 'crosses page' flag
      if (p > f->stream_end)                     return error(f, VORBIS_need_more_data);
      first = FALSE;
   }
   for (; s == -1;) {
      uint8 *q;
      int n;

      // check that we have the page header ready
      if (p + 26 >= f->stream_end)               return error(f, VORBIS_need_more_data);
//      if (p + 26 > f->stream_end)               return error(f, VORBIS_need_more_data);
      // validate the page
      if (thememcmp(p, ogg_page_header, 4))         return error(f, VORBIS_invalid_stream);
      if (p[4] != 0)                             return error(f, VORBIS_invalid_stream);
      if (first) { // the first segment must NOT have 'continued_packet', later ones MUST
         if (f->previous_length)
            if ((p[5] & PAGEFLAG_continued_packet))  return error(f, VORBIS_invalid_stream);
         // if no previous length, we're resynching, so we can come in on a continued-packet,
         // which we'll just drop
      } else {
         if (!(p[5] & PAGEFLAG_continued_packet)) return error(f, VORBIS_invalid_stream);
      }
      n = p[26]; // segment counts
      q = p+27;  // q points to segment table
      p = q + n; // advance past header
      // make sure we've read the segment table
      if (p >= f->stream_end)                    return error(f, VORBIS_need_more_data);
//      if (p > f->stream_end)                    return error(f, VORBIS_need_more_data);
      for (s=0; s < n; ++s) {
         p += q[s];
         if (q[s] < 255)
            break;
      }
      if (end_page)
         if (s < n-1)                           return error(f, VORBIS_invalid_stream);
      if (s == n)
         s = -1; // set 'crosses page' flag
      if (p > f->stream_end)                     return error(f, VORBIS_need_more_data);
      first = FALSE;
   }
   return TRUE;
}

static int start_decoder(vorb *f)
{
   uint8 header[6], x,y;
   int len,i,j,k, max_submaps = 0;
   int longest_floorlist=0;

   f->do_start_decoder = 0;

   // first page, first packet

    if (!start_page(f))                              return FALSE;
   // validate page flag
   if (!(f->page_flag & PAGEFLAG_first_page))       return error(f, VORBIS_invalid_first_page);
   if (f->page_flag & PAGEFLAG_last_page)           return error(f, VORBIS_invalid_first_page);
   if (f->page_flag & PAGEFLAG_continued_packet)    return error(f, VORBIS_invalid_first_page);
   // check for expected packet length
   if (f->segment_count != 1)                       return error(f, VORBIS_invalid_first_page);
   if (f->segments[0] != 30)                        return error(f, VORBIS_invalid_first_page);
   // read packet
   // check packet header
   if (get8(f) != VORBIS_packet_id)                 return error(f, VORBIS_invalid_first_page);
   if (!getn(f, header, 6))                         return error(f, VORBIS_unexpected_eof);
   if (!vorbis_validate(header))                    return error(f, VORBIS_invalid_first_page);
   // vorbis_version
   if (get32(f) != 0)                               return error(f, VORBIS_invalid_first_page);
   f->channels = get8(f); if (!f->channels)         return error(f, VORBIS_invalid_first_page);
#ifdef TMA_USE_RECIPROCAL_MUL
   f->chan_reciprocal = calc_reciprocal(f->channels);
#endif
   if (f->channels > STB_VORBIS_MAX_CHANNELS)       return error(f, VORBIS_too_many_channels);
   f->sample_rate = get32(f); if (!f->sample_rate)  return error(f, VORBIS_invalid_first_page);
   get32(f);                      // bitrate_maximum
   f->bitrate_nominal = get32(f); // bitrate_nominal
   get32(f);                      // bitrate_minimum
   x = get8(f);
   { int log0,log1;
   log0 = x & 15;
   log1 = x >> 4;
   f->blocksize_0 = 1 << log0;
   f->blocksize_1 = 1 << log1;
   if (log0 < 6 || log0 > 13)                       return error(f, VORBIS_invalid_setup);
   if (log1 < 6 || log1 > 13)                       return error(f, VORBIS_invalid_setup);
   if (log0 > log1)                                 return error(f, VORBIS_invalid_setup);
   }

   // framing_flag
   x = get8(f);
   if (!(x & 1))                                    return error(f, VORBIS_invalid_first_page);

   // second packet!
   if (!start_page(f))                              return FALSE;

   if (!start_packet(f))                            return FALSE;
   do {
      len = next_segment(f);
      skip(f, len);
      f->bytes_in_seg = 0;
   } while (len);

   // third packet!
   if (!start_packet(f))                            return FALSE;

   if (!is_whole_packet_present(f, TRUE))
   {
       // convert error in ogg header to write type
       if (f->error == VORBIS_invalid_stream)
            f->error = VORBIS_invalid_setup;
       return FALSE;
   }

   crc32_init(); // always init it, to avoid multithread race conditions

   bitrev_init();

   if (get8_packet(f) != VORBIS_packet_setup)       return error(f, VORBIS_invalid_setup);
   for (i=0; i < 6; ++i) header[i] = get8_packet(f);
   if (!vorbis_validate(header))                    return error(f, VORBIS_invalid_setup);

   // codebooks
   f->codebook_count = get_bits(f,8) + 1;
   f->codebooks = (Codebook **) setup_malloc(f, sizeof(*f->codebooks) * f->codebook_count);
   if (f->codebooks == NULL)                        return error(f, VORBIS_outofmem);
   thememset(f->codebooks, 0, sizeof(*f->codebooks) * f->codebook_count);

   //
   // If we don't have a setup buffer, we have to allocate into a temporary giganto buffer.
   //
   Codebook* pSetupCodebook = 0;
   if (f->m_MemoryBuffer == 0)
   {
       pSetupCodebook = (Codebook*)themalloc(sizeof(Codebook));
   }
   CodebookLite LiteBook;

   for (i=0; i < f->codebook_count; ++i)
   {
      uint32 *values;
      int ordered; //, sorted_count;
      int total=0;
      uint8 *lengths;

      CodebookLite* c = &LiteBook;
      memset(c, 0, sizeof(CodebookLite));

      x = get_bits(f, 8);
      if (x != 0x42)
      {
          thefree(pSetupCodebook);
          return error(f, VORBIS_invalid_setup);
      }

      x = get_bits(f, 8);
      if (x != 0x43)
      {
          thefree(pSetupCodebook);
          return error(f, VORBIS_invalid_setup);
      }
      x = get_bits(f, 8);
      if (x != 0x56)
      {
          thefree(pSetupCodebook);
          return error(f, VORBIS_invalid_setup);
      }
      x = get_bits(f, 8);
      c->dimensions = (get_bits(f, 8)<<8) + x;
      c->dim_reciprocal = calc_reciprocal(c->dimensions);
      x = get_bits(f, 8);
      y = get_bits(f, 8);
      c->entries = (get_bits(f, 8)<<16) + (y<<8) + x;
      ordered = get_bits(f,1);
      int sparse = ordered ? 0 : get_bits(f,1);

      lengths = (uint8 *) setup_temp_malloc(f, c->entries);

      if (!lengths)
      {
          thefree(pSetupCodebook);
          return error(f, VORBIS_outofmem);
      }

      if (ordered) {
         int current_entry = 0;
         int current_length = get_bits(f,5) + 1;
         while (current_entry < c->entries) {
            int limit = c->entries - current_entry;
            int n = get_bits(f, ilog(limit));
            if (current_entry + n > (int) c->entries)
            {
                thefree(pSetupCodebook);
                setup_temp_free(f, lengths, c->entries);
                return error(f, VORBIS_invalid_setup);
            }
            thememset(lengths + current_entry, current_length, n);

            current_entry += n;
            ++current_length;
         }
      } else {
         for (j=0; j < c->entries; ++j) {
            int present = sparse ? get_bits(f,1) : 1;
            if (present) {
               lengths[j] = get_bits(f, 5) + 1;
               ++total;
            } else {
               lengths[j] = NO_CODE;
            }
         }
      }
      values = NULL;


      c->lookup_type = get_bits(f, 4);
      if (c->lookup_type > 2)
      {
          thefree(pSetupCodebook);
          return error(f, VORBIS_invalid_setup);
      }
      if (c->lookup_type > 0) {
         uint16 *mults;
         c->minimum_value = float32_unpack(get_bits(f, 32));
         c->delta_value = float32_unpack(get_bits(f, 32));
         c->value_bits = get_bits(f, 4)+1;
         c->sequence_p = get_bits(f,1);
         if (c->lookup_type == 1) {
            c->lookup_values = lookup1_values(c->entries, c->dimensions);
         } else {
            c->lookup_values = c->entries * c->dimensions;
         }
         mults = (uint16 *) setup_temp_malloc(f, sizeof(mults[0]) * c->lookup_values);
         if (mults == NULL)
         {
             thefree(pSetupCodebook);
             return error(f, VORBIS_outofmem);
         }
         for (j=0; j < (int) c->lookup_values; ++j) {
            int q = get_bits(f, c->value_bits);
            if (q == EOP) { setup_temp_free(f,mults,sizeof(mults[0])*c->lookup_values); thefree(pSetupCodebook); return error(f, VORBIS_invalid_setup); }
            mults[j] = q;
         }

         if (c->lookup_type == 1) {
            int len; //, sparse = c->sparse;
            // pre-expand the lookup1-style multiplicands, to avoid a divide in the inner loop
//               if (c->sorted_entries == 0) goto skip;
               c->multiplicands = (float *) setup_malloc(f, sizeof(c->multiplicands[0]) * c->entries        * c->dimensions);

            if (c->multiplicands == NULL) { setup_temp_free(f,mults,sizeof(mults[0])*c->lookup_values); thefree(pSetupCodebook); return error(f, VORBIS_outofmem); }
            len = c->entries;
            for (j=0; j < len; ++j) {
               int z = j, div=1;
               for (k=0; k < c->dimensions; ++k) {
                  int off = (z / div) % c->lookup_values;
                  c->multiplicands[j*c->dimensions + k] =
                         #ifndef STB_VORBIS_CODEBOOK_FLOATS
                            mults[off];
                         #else
                            mults[off]*c->delta_value + c->minimum_value;
                            // in this case (and this case only) we could pre-expand c->sequence_p,
                            // and throw away the decode logic for it; have to ALSO do
                            // it in the case below, but it can only be done if
                            //    STB_VORBIS_CODEBOOK_FLOATS
                         #endif
                  div *= c->lookup_values;
               }
            }
            setup_temp_free(f, mults,sizeof(mults[0])*c->lookup_values);
            c->lookup_type = 2;
         }
         else
         {
            #ifndef STB_VORBIS_CODEBOOK_FLOATS
            c->multiplicands = (uint16 *) setup_malloc(f, sizeof(c->multiplicands[0]) * c->lookup_values);
            thememcpy(c->multiplicands, mults, sizeof(c->multiplicands[0]) * c->lookup_values);
            #else
            c->multiplicands = (float *) setup_malloc(f, sizeof(c->multiplicands[0]) * c->lookup_values);
            for (j=0; j < (int) c->lookup_values; ++j)
               c->multiplicands[j] = mults[j] * c->delta_value + c->minimum_value;
            setup_temp_free(f, mults,sizeof(mults[0])*c->lookup_values);
            #endif
         }
//        skip:;

         #ifdef STB_VORBIS_CODEBOOK_FLOATS
         if (c->lookup_type == 2 && c->sequence_p) {
            for (j=1; j < (int) c->lookup_values; ++j)
               c->multiplicands[j] = c->multiplicands[j-1];
            c->sequence_p = 0;
         }
         #endif
      }

      //
      // Put off computing the codewords until we can use the memory buffer as a workspace.
      //
      Codebook* pWorkingSpace = pSetupCodebook;
      if (f->m_MemoryBuffer)
      {
          pWorkingSpace = (Codebook*)((char*)f->m_MemoryBuffer + f->m_MemoryOffset);
      }

      // Copy the setup from above into the new codebook.
      thememcpy(pWorkingSpace, c, sizeof(CodebookLite));

      // Compute the codewords inplace in the buffer.
      if (!compute_codewords(pWorkingSpace, lengths, c->entries, values))
      {
          setup_temp_free(f, lengths, c->entries);
          thefree(pSetupCodebook);
         return error(f, VORBIS_invalid_setup);
      }

      setup_temp_free(f, lengths, c->entries);


      // Figure how much space in the buffer we actually used.
      size_t codebook_size = (char *) &pWorkingSpace->fast_decode1[pWorkingSpace->fd] - (char *) pWorkingSpace;

      // If we are in a buffer, call malloc, which will just increment the pointer over where we already are.
      // Otherwise, call malloc and copy the working data into this new pointer.
      f->codebooks[i] = (Codebook *) setup_malloc(f, (int) codebook_size);
      if (f->codebooks[i] == NULL)
      {
          thefree(pSetupCodebook);
          return error(f, VORBIS_outofmem);
      }

      if (f->m_MemoryBuffer == 0)
      {
          thememcpy(f->codebooks[i], pSetupCodebook, codebook_size);
      }

   }

   thefree(pSetupCodebook);

   // time domain transfers (notused)
   x = get_bits(f, 6) + 1;
   for (i=0; i < x; ++i) {
      uint32 z = get_bits(f, 16);
      if (z != 0) return error(f, VORBIS_invalid_setup);
   }

   // Floors
   f->floor_count = get_bits(f, 6)+1;
   f->floor_config = (Floor *)  setup_malloc(f, f->floor_count * sizeof(*f->floor_config));
   for (i=0; i < f->floor_count; ++i) {
      f->floor_types[i] = get_bits(f, 16);
      if (f->floor_types[i] > 1) return error(f, VORBIS_invalid_setup);
      if (f->floor_types[i] == 0) {
         Floor0 *g = &f->floor_config[i].floor0;
         g->order = get_bits(f,8);
         g->rate = get_bits(f,16);
         g->bark_map_size = get_bits(f,16);
         g->amplitude_bits = get_bits(f,6);
         g->amplitude_offset = get_bits(f,8);
         g->number_of_books = get_bits(f,4) + 1;
         for (j=0; j < g->number_of_books; ++j)
            g->book_list[j] = get_bits(f,8);
         return error(f, VORBIS_feature_not_supported);
      } else {
         Point p[31*8+2];
         Floor1 *g = &f->floor_config[i].floor1;
         int max_class = -1;
         g->partitions = get_bits(f, 5);
         for (j=0; j < g->partitions; ++j) {
            g->partition_class_list[j] = get_bits(f, 4);
            if (g->partition_class_list[j] > max_class)
               max_class = g->partition_class_list[j];
         }
         for (j=0; j <= max_class; ++j) {
            g->class_dimensions[j] = get_bits(f, 3)+1;
            g->class_subclasses[j] = get_bits(f, 2);
            if (g->class_subclasses[j]) {
               g->class_masterbooks[j] = get_bits(f, 8);
               if (g->class_masterbooks[j] >= f->codebook_count) return error(f, VORBIS_invalid_setup);
            }
            for (k=0; k < 1 << g->class_subclasses[j]; ++k) {
               g->subclass_books[j][k] = get_bits(f,8)-1;
               if (g->subclass_books[j][k] >= f->codebook_count) return error(f, VORBIS_invalid_setup);
            }
         }
         g->floor1_multiplier = get_bits(f,2)+1;
         g->rangebits = get_bits(f,4);
         g->Xlist[0] = 0;
         g->Xlist[1] = 1 << g->rangebits;
         g->values = 2;
         for (j=0; j < g->partitions; ++j) {
            int c = g->partition_class_list[j];
            for (k=0; k < g->class_dimensions[c]; ++k) {
               g->Xlist[g->values] = get_bits(f, g->rangebits);
               ++g->values;
            }
         }
         // precompute the sorting
         for (j=0; j < g->values; ++j) {
            p[j].x = g->Xlist[j];
            p[j].y = j;
         }

         #define SORTINDEX Point*
         #define SORTFIRSTINDEX p
         #define SORTLENGTH g->values
         #define SORTSWAP(index1, index2) SORTSWAPDEFAULT(index1, index2, Point)
         #define SORTSWAP3( ind1, ind2, ind3 ) SORTSWAP3DEFAULT( ind1, ind2, ind3, Point)
         #define SORTKEYDECLARE() uint16 keyX;
         #define SORTKEYSET(index) keyX = index->x

         #define SORTKEYISBEFORE( key_ind, ind ) keyX < (*ind).x
         #define SORTKEYISAFTER( key_ind, ind ) keyX > (*ind).x

         #include "rrsort.inl"

/*         uint16 cur = 0;
         for (int test = 0; test < g->values; test++)
         {
             radassert (p[test].x >= cur);
             cur = p[test].x;
         }
*/

         //qsort(p, g->values, sizeof(p[0]), point_compare);
         for (j=0; j < g->values; ++j)
            g->sorted_order[j] = (uint8) p[j].y;
         // precompute the neighbors
         for (j=2; j < g->values; ++j)
         {
            int low=0,hi=0; // compiler warning.
            neighbors(g->Xlist, j, &low,&hi);
            g->neighbors[j][0] = low;
            g->neighbors[j][1] = hi;
#ifdef TMA_USE_RECIPROCAL_MUL
            g->dx_reciprocal[j] = calc_reciprocal(g->Xlist[hi]-g->Xlist[low]);
#endif
         }

         if (g->values > longest_floorlist)
            longest_floorlist = g->values;
      }
   }

   // Residue
   f->residue_count = get_bits(f, 6)+1;
   f->residue_config = (Residue *) setup_malloc(f, f->residue_count * sizeof(*f->residue_config));
   for (i=0; i < f->residue_count; ++i) {
      uint8 residue_cascade[64];
      Residue *r = f->residue_config+i;
      f->residue_types[i] = get_bits(f, 16);
      if (f->residue_types[i] > 2) return error(f, VORBIS_invalid_setup);
      r->begin = get_bits(f, 24);
      r->end = get_bits(f, 24);
      r->part_size = get_bits(f,24)+1;
#ifdef TMA_USE_RECIPROCAL_MUL
      r->part_size_reciprocal = calc_reciprocal(r->part_size);
#endif
      r->classifications = get_bits(f,6)+1;
      r->classbook = get_bits(f,8);
      for (j=0; j < r->classifications; ++j) {
         uint8 high_bits=0;
         uint8 low_bits=get_bits(f,3);
         if (get_bits(f,1))
            high_bits = get_bits(f,5);
         residue_cascade[j] = high_bits*8 + low_bits;
      }
      r->residue_books = (short (*)[8]) setup_malloc(f, sizeof(r->residue_books[0]) * r->classifications);
      for (j=0; j < r->classifications; ++j) {
         for (k=0; k < 8; ++k) {
            if (residue_cascade[j] & (1 << k)) {
               r->residue_books[j][k] = get_bits(f, 8);
               if (r->residue_books[j][k] >= f->codebook_count) return error(f, VORBIS_invalid_setup);
            } else {
               r->residue_books[j][k] = -1;
            }
         }
      }
      // precompute the classifications[] array to avoid inner-loop mod/divide
      // call it 'classdata' since we already have r->classifications
      r->classdata = (uint8 **) setup_malloc(f, sizeof(*r->classdata) * f->codebooks[r->classbook]->entries);
      if (!r->classdata) return error(f, VORBIS_outofmem);
      thememset(r->classdata, 0, sizeof(*r->classdata) * f->codebooks[r->classbook]->entries);
      for (j=0; j < f->codebooks[r->classbook]->entries; ++j) {
         int classwords = f->codebooks[r->classbook]->dimensions;
         int temp = j;
         r->classdata[j] = (uint8 *) setup_malloc(f, sizeof(r->classdata[j][0]) * classwords);
         for (k=classwords-1; k >= 0; --k) {
            r->classdata[j][k] = temp % r->classifications;
            temp /= r->classifications;
         }
      }
   }

   // Mappings
   f->mapping_count = get_bits(f,6)+1;
   f->mapping = (Mapping *) setup_malloc(f, f->mapping_count * sizeof(*f->mapping));
   for (i=0; i < f->mapping_count; ++i) {
      Mapping *m = f->mapping + i;
      int mapping_type = get_bits(f,16);
      if (mapping_type != 0) return error(f, VORBIS_invalid_setup);
      m->chan = (MappingChannel *) setup_malloc(f, f->channels * sizeof(*m->chan));
      if (get_bits(f,1))
         m->submaps = get_bits(f,4);
      else
         m->submaps = 1;
      if (m->submaps > max_submaps)
         max_submaps = m->submaps;
      if (get_bits(f,1)) {
         m->coupling_steps = get_bits(f,8)+1;
         for (k=0; k < m->coupling_steps; ++k) {
            m->chan[k].magnitude = get_bits(f, ilog(f->channels)-1);
            m->chan[k].angle = get_bits(f, ilog(f->channels)-1);
            if (m->chan[k].magnitude >= f->channels)        return error(f, VORBIS_invalid_setup);
            if (m->chan[k].angle     >= f->channels)        return error(f, VORBIS_invalid_setup);
            if (m->chan[k].magnitude == m->chan[k].angle)   return error(f, VORBIS_invalid_setup);
         }
      } else
         m->coupling_steps = 0;

      // reserved field
      if (get_bits(f,2)) return error(f, VORBIS_invalid_setup);
      if (m->submaps > 1) {
         for (j=0; j < f->channels; ++j) {
            m->chan[j].mux = get_bits(f, 4);
            if (m->chan[j].mux >= m->submaps)                return error(f, VORBIS_invalid_setup);
         }
      } else
         // @SPECIFICATION: this case is missing from the spec
         for (j=0; j < f->channels; ++j)
            m->chan[j].mux = 0;

      for (j=0; j < m->submaps; ++j) {
         get_bits(f,8); // discard
         m->submap_floor[j] = get_bits(f,8);
         m->submap_residue[j] = get_bits(f,8);
         if (m->submap_floor[j] >= f->floor_count)      return error(f, VORBIS_invalid_setup);
         if (m->submap_residue[j] >= f->residue_count)  return error(f, VORBIS_invalid_setup);
      }
   }

   // Modes
   f->mode_count = get_bits(f, 6)+1;
   for (i=0; i < f->mode_count; ++i) {
      Mode *m = f->mode_config+i;
      m->blockflag = get_bits(f,1);
      m->windowtype = get_bits(f,16);
      m->transformtype = get_bits(f,16);
      m->mapping = get_bits(f,8);
      if (m->windowtype != 0)                 return error(f, VORBIS_invalid_setup);
      if (m->transformtype != 0)              return error(f, VORBIS_invalid_setup);
      if (m->mapping >= f->mapping_count)     return error(f, VORBIS_invalid_setup);
   }

   flush_packet(f);

   f->previous_length = 0;

#define CHANNEL_SKEW (2*64)   // Try to avoid having all buffers in the same cache set!

   // Allocate enough ram for all the buffers, then SSE-align them:
   int size_pr_channel = (sizeof(float) * f->blocksize_1 +          // SSE-multiple
                          sizeof(float) * f->blocksize_1/2 +        // SSE-multiple
                          (sizeof(int16) * longest_floorlist + 15) & ~15) +
                          CHANNEL_SKEW * 3;

   f->channel_buffer_alloc = (float *) setup_malloc(f, f->channels * size_pr_channel + 15);
   char *ca = (char *) ((((intptr_t) f->channel_buffer_alloc)+15)& ~15);

   for (i=0; i < f->channels; ++i) {
      f->channel_buffers[i] = (float *) ca;
      ca += sizeof(float) * f->blocksize_1 + CHANNEL_SKEW;
   }
   for (i=0; i < f->channels; ++i) {
      f->previous_window[i] = (float *) ca;
      ca += sizeof(float) * f->blocksize_1/2 + CHANNEL_SKEW;
   }
   for (i=0; i < f->channels; ++i) {
      f->finalY[i]          = (int16 *) ca;
      ca += ((sizeof(int16) * longest_floorlist + 15) & ~15) + CHANNEL_SKEW;
   }

   #ifdef STB_VORBIS_NO_DEFER_FLOOR
   for (i=0; i < f->channels; ++i) {
      f->floor_buffers[i]   = (float *) setup_malloc(f, sizeof(float) * f->blocksize_1/2);
   }
   #endif


   if (!init_blocksize(f, 0, f->blocksize_0)) return FALSE;
   if (!init_blocksize(f, 1, f->blocksize_1)) return FALSE;


   f->first_decode = TRUE;

   return TRUE;
}

static void vorbis_deinit(stb_vorbis *p)
{
   int i,j;
   for (i=0; i < p->residue_count; ++i) {
      Residue *r = p->residue_config+i;
      if (r->classdata) {
         for (j=0; j < p->codebooks[r->classbook]->entries; ++j)
            setup_free(p, r->classdata[j]);
         setup_free(p, r->classdata);
      }
      setup_free(p, r->residue_books);
   }

   if (p->codebooks) {
      for (i=0; i < p->codebook_count; ++i) {
         Codebook *c = p->codebooks[i];
//         setup_free(p, c->codeword_lengths);
         setup_free(p, c->multiplicands);
         setup_free(p, c);
      }
      setup_free(p, p->codebooks);
   }
   setup_free(p, p->bit_reverse[0]);
   setup_free(p, p->bit_reverse[1]);
   setup_free(p, p->floor_config);
   setup_free(p, p->residue_config);
   for (i=0; i < p->mapping_count; ++i)
      setup_free(p, p->mapping[i].chan);
   setup_free(p, p->mapping);

   // Free the SSE-aligned common channel buffer
   setup_free(p, p->channel_buffer_alloc);
   #ifdef STB_VORBIS_NO_DEFER_FLOOR
   for (i=0; i < p->channels; ++i) {
      setup_free(p, p->floor_buffers[i]);
   }
   #endif
   for (i=0; i < 2; ++i) {
      setup_free(p, p->twiddle_buffers[i]);
   }

}

void stb_vorbis_close(stb_vorbis *p)
{
   if (p == NULL) return;
   vorbis_deinit(p);

   // We can no longer call setup_free since all of the setup memory is
   // cleaned up in deinit. As a result, p is now invalid...
   //setup_free(p,p);
}

static void vorbis_init(stb_vorbis *p)
{
   thememset(p, 0, sizeof(*p)); // NULL out all malloc'd pointers to start
   p->eof = 0;
   p->error = VORBIS__no_error;
   p->stream = NULL;
   p->codebooks = NULL;
   p->close_on_free = FALSE;
   p->page_crc_tests = -1;
}

int stb_vorbis_get_sample_offset(stb_vorbis *f)
{
   if (f->current_loc_valid)
      return f->current_loc;
   else
      return -1;
}

stb_vorbis_info stb_vorbis_get_info(stb_vorbis *f)
{
   stb_vorbis_info d;
   d.channels = f->channels;
   d.sample_rate = f->sample_rate;
   d.bitrate_nominal = f->bitrate_nominal;
   d.max_frame_size = f->blocksize_1 >> 1;
   return d;
}

int stb_vorbis_get_error(stb_vorbis *f)
{
   int e = f->error;
   f->error = VORBIS__no_error;
   return e;
}

static stb_vorbis * vorbis_alloc(stb_vorbis *f)
{
   stb_vorbis *p = (stb_vorbis *) setup_malloc(f, sizeof(*p));
   return p;
}

int stb_vorbis_init_tables(stb_vorbis *f)
{
   //
   // If we started with a deferred open, we might not have started the decoder.
   //
   // NOTE!  Subtle: we assume that the buffer you originally open the vorbis
   //        file with is still valid! (otherwise, we don't have the data to
   //        decoder the initial tables).  Little scary.
   //

   if (f->do_start_decoder)
   {
       if (!start_decoder(f))
       {
           return 0;
       }
   }
   return 1;
}

int stb_vorbis_flush_pushdata(stb_vorbis *f)
{
   f->previous_length = 0;
   f->page_crc_tests  = 0;
   f->discard_samples_deferred = 0;
   f->current_loc_valid = FALSE;
   f->first_decode = FALSE;
   f->samples_output = 0;
   f->channel_buffer_start = 0;
   f->channel_buffer_end = 0;

   return 1;
}

int stb_vorbis_flush_pushdata_start_on_page(stb_vorbis *f)
{
   f->first_decode = TRUE;

   f->page_crc_tests = -1; // drop out of page scan mode
   f->previous_length = 0; // decode-but-don't-output one frame
   f->next_seg = -1;       // start a new page
   f->current_loc_valid = FALSE;

   return 1;
}



static int vorbis_search_for_page_pushdata(vorb *f, uint8 *data, int data_len)
{
   int i,n;
   for (i=0; i < f->page_crc_tests; ++i)
      f->scan[i].bytes_done = 0;

   // if we have room for more scans, search for them first, because
   // they may cause us to stop early if their header is incomplete
   if (f->page_crc_tests < STB_VORBIS_PUSHDATA_CRC_COUNT) {
      if (data_len < 4) return 0;
      data_len -= 3; // need to look for 4-byte sequence, so don't miss
                     // one that straddles a boundary
      for (i=0; i < data_len; ++i) {
         if (data[i] == 0x4f) {
            if (0==thememcmp(data+i, ogg_page_header, 4)) {
               int j,len;
               uint32 crc;
               // make sure we have the whole page header
               if (i+26 >= data_len || i+27+data[i+26] >= data_len) {
                  // only read up to this page start, so hopefully we'll
                  // have the whole page header start next time
                  data_len = i;
                  break;
               }
               // ok, we have it all; compute the length of the page
               len = 27 + data[i+26];
               for (j=0; j < data[i+26]; ++j)
                  len += data[i+27+j];
               // scan everything up to the embedded crc (which we must 0)
               crc = 0;
               for (j=0; j < 22; ++j)
                  crc = crc32_update(crc, data[i+j]);
               // now process 4 0-bytes
               for (   ; j < 26; ++j)
                  crc = crc32_update(crc, 0);
               // len is the total number of bytes we need to scan
               n = f->page_crc_tests++;
               f->scan[n].bytes_left = len-j;
               f->scan[n].crc_so_far = crc;
               f->scan[n].goal_crc = data[i+22] + (data[i+23] << 8) + (data[i+24]<<16) + (data[i+25]<<24);
               // if the last frame on a page is continued to the next, then
               // we can't recover the sample_loc immediately
               if (data[i+27+data[i+26]-1] == 255)
                  f->scan[n].sample_loc = ~0;
               else
                  f->scan[n].sample_loc = data[i+6] + (data[i+7] << 8) + (data[i+ 8]<<16) + (data[i+ 9]<<24);
               f->scan[n].bytes_done = i+j;
               if (f->page_crc_tests == STB_VORBIS_PUSHDATA_CRC_COUNT)
                  break;
               // keep going if we still have room for more
            }
         }
      }
   }

   for (i=0; i < f->page_crc_tests;) {
      uint32 crc;
      int j;
      int n = f->scan[i].bytes_done;
      int m = f->scan[i].bytes_left;
      if (m > data_len - n) m = data_len - n;
      // m is the bytes to scan in the current chunk
      crc = f->scan[i].crc_so_far;
      for (j=0; j < m; ++j)
         crc = crc32_update(crc, data[n+j]);
      f->scan[i].bytes_left -= m;
      f->scan[i].crc_so_far = crc;
      if (f->scan[i].bytes_left == 0) {
         // does it match?
         if (f->scan[i].crc_so_far == f->scan[i].goal_crc) {
            // Houston, we have page
            data_len = n+m; // consumption amount is wherever that scan ended
            f->page_crc_tests = -1; // drop out of page scan mode
            f->previous_length = 0; // decode-but-don't-output one frame
            f->next_seg = -1;       // start a new page
            f->current_loc = f->scan[i].sample_loc; // set the current sample location
                                    // to the amount we'd have decoded had we decoded this page
            f->current_loc_valid = f->current_loc != (unsigned)~0;
            return data_len;
         }
         // delete entry
         f->scan[i] = f->scan[--f->page_crc_tests];
      } else {
         ++i;
      }
   }

   return data_len;
}

// return value: number of bytes we used
int stb_vorbis_decode_frame_pushdata(
         stb_vorbis *f,                 // the file we're decoding
         uint8 *data, int data_len,     // the memory available for decoding
         int *channels,                 // place to write number of float * buffers
         float ***output,               // place to write float ** array of float * buffers
         int *samples                   // place to write number of output samples
     )
{
   int i;
   int len,right,left;

   f->stream     = data;
   f->stream_end = data + data_len;
   f->error      = VORBIS__no_error;

   if (f->page_crc_tests >= 0) {
      *samples = 0;
      return vorbis_search_for_page_pushdata(f, data, data_len);
   }


   // check that we have the entire packet in memory
   if (!is_whole_packet_present(f, FALSE)) {
      *samples = 0;
      return 0;
   }


   if (!vorbis_decode_packet(f, &len, &left, &right)) {
      // save the actual error we encountered
      enum STBVorbisError error = f->error;
      if (error == VORBIS_bad_packet_type) {
         // flush and resynch
         f->error = VORBIS__no_error;
         while (get8_packet(f) != EOP)
            if (f->eof) break;
         *samples = 0;
         return (int) (f->stream - data);
      }
      if (error == VORBIS_continued_packet_flag_invalid) {
         if (f->previous_length == 0) {
            // we may be resynching, in which case it's ok to hit one
            // of these; just discard the packet
            f->error = VORBIS__no_error;
            while (get8_packet(f) != EOP)
               if (f->eof) break;
            *samples = 0;
            return (int) (f->stream - data);
         }
      }
      // if we get an error while parsing, what to do?
      // well, it DEFINITELY won't work to continue from where we are!
      stb_vorbis_flush_pushdata(f);
      // restore the error that actually made us bail
      f->error = error;
      *samples = 0;
      return 1;
   }


   // success!
   len = vorbis_finish_frame(f, len, left, right);
   for (i=0; i < f->channels; ++i)
      f->outputs[i] = f->channel_buffers[i] + left;

   if (channels) *channels = f->channels;
   *samples = len;
   *output = f->outputs;
   return (int) (f->stream - data);
}

stb_vorbis *stb_vorbis_open_pushdata(
         unsigned char *data, int data_len, // the memory available for decoding
         int *data_used,              // only defined if result is not NULL
         int *error)
{

   stb_vorbis *f, p;
   vorbis_init(&p);
   p.stream     = data;
   p.stream_end = data + data_len;
   p.stream_start = data;
   p.push_mode  = TRUE;

   if (!start_decoder(&p)) {
      if (p.eof)
         *error = VORBIS_need_more_data;
      else
         *error = p.error;
      return NULL;
   }

   f = vorbis_alloc(&p);
   if (f)
   {
       thememcpy(f, &p, sizeof(stb_vorbis));
      *data_used = (int) (f->stream - data);
      *error = 0;
      return f;
   } else {
      vorbis_deinit(&p);
      return NULL;
   }
}

static int Vorbis_GetRequiredMemory(vorb *f, int* o_SetupTempMemNeeded, int* o_SetupMemNeeded);

//-----------------------------------------------------------------------------
/*!
    Opens an ogg stream to get initial memory requirements and stream sound info.
*/
//-----------------------------------------------------------------------------
int
stb_vorbis_open_pushdata_begin(
    unsigned char* i_Stream,
    int i_StreamLen,
    VorbisPeekaheadProps* o_Props
    )
{
    o_Props->m_Error = 0;

    //
    // Init a pushahead, but only to get key information.
    //
    stb_vorbis Vorbis;
    vorbis_init(&Vorbis);
    Vorbis.stream = i_Stream;
    Vorbis.stream_end = i_Stream + i_StreamLen;
    Vorbis.stream_start = i_Stream;
    Vorbis.push_mode = 1;

    int SetupMemNeeded, TempSetupMemNeeded;
    if (Vorbis_GetRequiredMemory(&Vorbis, &TempSetupMemNeeded, &SetupMemNeeded) == 0)
    {
        if (Vorbis.eof) o_Props->m_Error = VORBIS_need_more_data;
        else o_Props->m_Error = Vorbis.error;
        return 0;
    }

    o_Props->m_TotalMemoryNeeded = SetupMemNeeded + TempSetupMemNeeded;
    o_Props->m_Channels = Vorbis.channels;
    o_Props->m_SampleRate = Vorbis.sample_rate;
    o_Props->m_BitrateNominal = Vorbis.bitrate_nominal;

    return 1;
}

//-----------------------------------------------------------------------------
/*!
    This provides the interface to the vorbis system that allows for a preallocated
    buffer. Use the memory information provided via Vorbis_OpenPushdataPeekahead
    to allocate the buffer. If you do not wish to preallocate, just use the standard
    stb_vorbis_open_pushdata() call.

    This function pushes the decoder initialization off to the first decode_frame
    call.
*/
//-----------------------------------------------------------------------------
stb_vorbis*
stb_vorbis_open_pushdata_complete(
    unsigned char* i_Stream,
    int i_StreamLen,
    void* i_Buffer,
    unsigned int i_BufferLen
    )
{
    stb_vorbis *f, p;
    vorbis_init(&p);
    p.stream     = i_Stream;
    p.stream_end = i_Stream + i_StreamLen;
    p.stream_start = i_Stream;
    p.push_mode  = TRUE;
    p.m_MemoryBuffer = i_Buffer;
    p.m_MemoryOffset = 0;
    p.m_MemoryBufferLen = i_BufferLen;
    p.do_start_decoder = 1;

    // start_decoder happens in the first decode call.

    // get a pointer to it and return.
    f = vorbis_alloc(&p);
    if (f)
    {
        thememcpy(f, &p, sizeof(stb_vorbis));
    }
    return f;
}


#define ADDMEM(x) {SetupMemNeeded += (x + 3) & ~3; }
#define ADDTEMPMEM(x) {SetupTempMemNeeded += (x + 3) & ~3; }

//-----------------------------------------------------------------------------
/*!
    Vorbis_GetRequiredMemory

    This function duplicates a minimum required subset of start_decoder for the
    purpose of determining how much memory start_decoder will require.

    \return 1 on success.
*/
//-----------------------------------------------------------------------------
static int Vorbis_GetRequiredMemory(vorb *f, int* o_SetupTempMemNeeded, int* o_SetupMemNeeded)
{
    U32 SetupMemNeeded = 0;
    U32 SetupTempMemNeeded = 0;

   uint8 header[6], x,y;
   int len,i,j,k, max_submaps = 0;
   int longest_floorlist=0;

   // first page, first packet

    if (!start_page(f))                              return FALSE;
   // validate page flag
   if (!(f->page_flag & PAGEFLAG_first_page))       return error(f, VORBIS_invalid_first_page);
   if (f->page_flag & PAGEFLAG_last_page)           return error(f, VORBIS_invalid_first_page);
   if (f->page_flag & PAGEFLAG_continued_packet)    return error(f, VORBIS_invalid_first_page);
   // check for expected packet length
   if (f->segment_count != 1)                       return error(f, VORBIS_invalid_first_page);
   if (f->segments[0] != 30)                        return error(f, VORBIS_invalid_first_page);
   // read packet
   // check packet header
   if (get8(f) != VORBIS_packet_id)                 return error(f, VORBIS_invalid_first_page);
   if (!getn(f, header, 6))                         return error(f, VORBIS_unexpected_eof);
   if (!vorbis_validate(header))                    return error(f, VORBIS_invalid_first_page);
   // vorbis_version
   if (get32(f) != 0)                               return error(f, VORBIS_invalid_first_page);
   f->channels = get8(f); if (!f->channels)         return error(f, VORBIS_invalid_first_page);
   if (f->channels > STB_VORBIS_MAX_CHANNELS)       return error(f, VORBIS_too_many_channels);
   f->sample_rate = get32(f); if (!f->sample_rate)  return error(f, VORBIS_invalid_first_page);
   get32(f);                      // bitrate_maximum
   f->bitrate_nominal = get32(f); // bitrate_nominal
   get32(f);                      // bitrate_minimum
   x = get8(f);
   { int log0,log1;
   log0 = x & 15;
   log1 = x >> 4;
   f->blocksize_0 = 1 << log0;
   f->blocksize_1 = 1 << log1;
   if (log0 < 6 || log0 > 13)                       return error(f, VORBIS_invalid_setup);
   if (log1 < 6 || log1 > 13)                       return error(f, VORBIS_invalid_setup);
   if (log0 > log1)                                 return error(f, VORBIS_invalid_setup);
   }

   // framing_flag
   x = get8(f);
   if (!(x & 1))                                    return error(f, VORBIS_invalid_first_page);

   // second packet!
   if (!start_page(f))                              return FALSE;

   if (!start_packet(f))                            return FALSE;
   do {
      len = next_segment(f);
      skip(f, len);
      f->bytes_in_seg = 0;
   } while (len);

   // third packet!
   if (!start_packet(f))                            return FALSE;

   if (!is_whole_packet_present(f, TRUE))
   {
       // convert error in ogg header to write type
       if (f->error == VORBIS_invalid_stream)
            f->error = VORBIS_invalid_setup;
       return FALSE;
   }

   if (get8_packet(f) != VORBIS_packet_setup)       return error(f, VORBIS_invalid_setup);
   for (i=0; i < 6; ++i) header[i] = get8_packet(f);
   if (!vorbis_validate(header))                    return error(f, VORBIS_invalid_setup);


   // codebooks
   f->codebook_count = get_bits(f,8) + 1;
   ADDMEM(sizeof(*f->codebooks) * f->codebook_count);
   S32 CodebookEntryCount[256];
   S32 CodebookDimensions[256];

   for (i=0; i < f->codebook_count; ++i)
    {
        int ordered; //, sorted_count;
        int total=0;
//
        x = get_bits(f, 8);
        if (x != 0x42) return error(f, VORBIS_invalid_setup);

        x = get_bits(f, 8);
        if (x != 0x43) return error(f, VORBIS_invalid_setup);

        x = get_bits(f, 8);
        if (x != 0x56) return error(f, VORBIS_invalid_setup);

        x = get_bits(f, 8);
        U32 dimensions = (get_bits(f, 8)<<8) + x;
        x = get_bits(f, 8);
        y = get_bits(f, 8);
        int32 entries = (get_bits(f, 8)<<16) + (y<<8) + x;
        ordered = get_bits(f,1);
        int sparse = ordered ? 0 : get_bits(f,1);

        ADDTEMPMEM(entries);
        CodebookEntryCount[i] = entries;
        CodebookDimensions[i] = dimensions;

        S32 TreeEntryCount = 0;
        { // begin codeword length
            S32 FirstEntryIndex = 0;
            U32 Available[32];
            thememset(Available, 0, sizeof(Available));
            S32 FoundFirstEntry = 0;

            if (ordered)
            {
                int current_entry = 0;
                int current_length = get_bits(f,5) + 1;
                while (current_entry < entries)
                {
                    int limit = entries - current_entry;
                    int n = get_bits(f, ilog(limit));
                    if (current_entry + n > (int) entries)
                    {
                        return error(f, VORBIS_invalid_setup);
                    }

                    if (n != NO_CODE)
                    {
                        //
                        // Handle state setup for the tree construction - but only if we ever get a non NO_CODE entry.
                        //
                        if (FoundFirstEntry == 0)
                        {
                            FoundFirstEntry = 1;
                            FirstEntryIndex = current_entry;
                            TreeEntryCount = 256; // Base table size.

                            // Setup Available array.
                            for (int i=1; i <= current_length; ++i)
                            {
                                Available[i] = 1 << (32-i);
                                if (i > 8 && (i & 1)) // every time we split an odd node, we're adding 4 entries
                                {
                                    TreeEntryCount += 4;
                                }
                            }

                            // This seems (?) to take up the first entry? So increment.
                            current_entry++;
                            n--;
                        }

                        // Check for tree propogation...
                        for (int LengthIndex = 0; LengthIndex < n; LengthIndex++)
                        {
                            current_entry++;

                            uint32 res;
                            int z = current_length, y;

                            while (z > 0 && !Available[z]) --z;
                            if (z == 0) return error(f, VORBIS_invalid_setup);

                            res = Available[z];
                            Available[z] = 0;
                            // propogate availability up the tree
                            if (z != current_length)
                            {
                                for (y=current_length; y > z; --y)
                                {
                                    if (y > 8 && (y & 1)) TreeEntryCount += 4;
                                    radassert(Available[y] == 0);
                                    Available[y] = res + (1 << (32-y));
                                }
                            }
                        }
                    } // end if we didn't get NO_CODE.
                    else
                    {
                        // Still increment the entry count.
                        current_entry += n;
                    }

                    ++current_length;
                } // while we are decompressing the RLE huffcode lengths.
            } // end if ordered huffcodes.
            else
            {
                for (j=0; j < entries; ++j)
                {
                    int present = sparse ? get_bits(f,1) : 1;
                    if (present)
                    {
                        int length = get_bits(f, 5) + 1;
                        ++total;

                        //
                        // Handle state setup for the tree construction - but only if we ever get a non NO_CODE entry.
                        //
                        if (FoundFirstEntry == 0)
                        {
                            FoundFirstEntry = 1;
                            FirstEntryIndex = j;
                            TreeEntryCount = 256; // Base table size.

                            // Setup Available array.
                            for (int i=1; i <= length; ++i)
                            {
                                Available[i] = 1 << (32-i);
                                if (i > 8 && (i & 1)) // every time we split an odd node, we're adding 4 entries
                                {
                                    TreeEntryCount += 4;
                                }
                            }
                        }
                        else
                        {
                            // Check for tree propogation...
                            uint32 res;
                            int z = length, y;

                            while (z > 0 && !Available[z]) --z;
                            if (z == 0) return error(f, VORBIS_invalid_setup);

                            res = Available[z];
                            Available[z] = 0;
                            // propogate availability up the tree
                            if (z != length)
                            {
                                for (y=length; y > z; --y)
                                {
                                    if (y > 8 && (y & 1)) TreeEntryCount += 4;
                                    radassert(Available[y] == 0);
                                    Available[y] = res + (1 << (32-y));
                                }
                            }
                        }
                    }
                }
            } // end if not ordered.
        } // end codeword length


      C8 lookup_type = get_bits(f, 4);
      if (lookup_type > 2) return error(f, VORBIS_invalid_setup);

      if (lookup_type > 0)
      {
         get_bits(f, 32);
         get_bits(f, 32);
         U32 value_bits = get_bits(f, 4)+1;
         get_bits(f,1);
         U32 lookup_values;
         if (lookup_type == 1)
         {
            lookup_values = lookup1_values(entries, dimensions); // slow!! transendentals.
         }
         else
         {
            lookup_values = entries * dimensions;
         }

         ADDTEMPMEM(sizeof(uint16) * lookup_values);

         for (j=0; j < (int) lookup_values; ++j)
         {
            int q = get_bits(f, value_bits);
            if (q == EOP) return error(f, VORBIS_invalid_setup);
         }


         if (lookup_type == 1)
         {
#ifndef STB_VORBIS_CODEBOOK_FLOATS
            ADDMEM(sizeof(uint16) * entries * dimensions);
#else
            ADDMEM(sizeof(float) * entries * dimensions);
#endif
            lookup_type = 2;
         }
         else
         {
#ifndef STB_VORBIS_CODEBOOK_FLOATS
             ADDMEM(sizeof(uint16) * lookup_values);
#else
             ADDMEM(sizeof(float) * lookup_values);
#endif
         }
      }

      // Allocate a real codebook entry, with just enough room for the # of entries actually used:
      Codebook* pSetupCodebook = 0;
      S32 codebook_size = (char *) &pSetupCodebook->fast_decode1[TreeEntryCount] - (char *) pSetupCodebook;
      ADDMEM(codebook_size);
   }

   // time domain transfers (notused)
   x = get_bits(f, 6) + 1;
   for (i=0; i < x; ++i) {
      uint32 z = get_bits(f, 16);
      if (z != 0) return error(f, VORBIS_invalid_setup);
   }

   // Floors
   S32 floor_count = get_bits(f, 6)+1;
   ADDMEM(floor_count * sizeof(Floor));
   f->floor_count = floor_count;

   for (i=0; i < floor_count; ++i)
   {
      U32 floor_types = get_bits(f, 16);
      if (floor_types > 1) return error(f, VORBIS_invalid_setup);
      if (floor_types == 0)
      {
         return error(f, VORBIS_feature_not_supported);
      }
      else
      {
         Floor1 localcopy;
         Floor1* g = &localcopy;

         g->partitions = get_bits(f, 5);
         S32 max_class = -1;
         for (j=0; j < g->partitions; ++j) {
            g->partition_class_list[j] = get_bits(f, 4);
            if (g->partition_class_list[j] > max_class)
               max_class = g->partition_class_list[j];
         }

         for (j=0; j <= max_class; ++j)
         {
            g->class_dimensions[j] = get_bits(f, 3)+1;
            g->class_subclasses[j] = get_bits(f, 2);
            if (g->class_subclasses[j]) {
               g->class_masterbooks[j] = get_bits(f, 8);
               if (g->class_masterbooks[j] >= f->codebook_count) return error(f, VORBIS_invalid_setup);
            }
            for (k=0; k < 1 << g->class_subclasses[j]; ++k) {
               g->subclass_books[j][k] = get_bits(f,8)-1;
               if (g->subclass_books[j][k] >= f->codebook_count) return error(f, VORBIS_invalid_setup);
            }
         }
         g->floor1_multiplier = get_bits(f,2)+1;
         g->rangebits = get_bits(f,4);
         g->Xlist[0] = 0;
         g->Xlist[1] = 1 << g->rangebits;
         g->values = 2;
         for (j=0; j < g->partitions; ++j) {
            int c = g->partition_class_list[j];
            for (k=0; k < g->class_dimensions[c]; ++k) {
               g->Xlist[g->values] = get_bits(f, g->rangebits);
               ++g->values;
            }
         }
         if (g->values > longest_floorlist)
            longest_floorlist = g->values;
      }
   }


   // Residue
   U32 residue_count = get_bits(f, 6)+1;
   ADDMEM(residue_count * sizeof(Residue));
   f->residue_count = residue_count;

   for (i=0; i < f->residue_count; ++i)
   {
      uint8 residue_cascade[64];
      Residue localcopy;
      Residue *r = &localcopy;

      f->residue_types[i] = get_bits(f, 16);
      if (f->residue_types[i] > 2) return error(f, VORBIS_invalid_setup);
      r->begin = get_bits(f, 24);
      r->end = get_bits(f, 24);
      r->part_size = get_bits(f,24)+1;
#ifdef TMA_USE_RECIPROCAL_MUL
      r->part_size_reciprocal = calc_reciprocal(r->part_size);
#endif
      r->classifications = get_bits(f,6)+1;
      r->classbook = get_bits(f,8);
      for (j=0; j < r->classifications; ++j) {
         uint8 high_bits=0;
         uint8 low_bits=get_bits(f,3);
         if (get_bits(f,1))
            high_bits = get_bits(f,5);
         residue_cascade[j] = high_bits*8 + low_bits;
      }

      ADDMEM(sizeof(r->residue_books[0]) * r->classifications);

      for (j=0; j < r->classifications; ++j)
      {
         for (k=0; k < 8; ++k)
         {
            if (residue_cascade[j] & (1 << k))
            {
               S32 book = get_bits(f, 8);
               if (book >= f->codebook_count) return error(f, VORBIS_invalid_setup);
            }
         }
      }

      // precompute the classifications[] array to avoid inner-loop mod/divide
      // call it 'classdata' since we already have r->classifications
      ADDMEM(sizeof(*r->classdata) * CodebookEntryCount[r->classbook]);


      for (j=0; j < CodebookEntryCount[r->classbook]; ++j)
      {
         int classwords = CodebookDimensions[r->classbook];
         ADDMEM(sizeof(r->classdata[j][0]) * classwords);
      }
   }

   // Mappings
   f->mapping_count = get_bits(f,6)+1;

   ADDMEM(f->mapping_count * sizeof(*f->mapping));
   for (i=0; i < f->mapping_count; ++i)
   {
       Mapping localcopy;
      Mapping *m = &localcopy;
      int mapping_type = get_bits(f,16);
      if (mapping_type != 0) return error(f, VORBIS_invalid_setup);
      ADDMEM(f->channels * sizeof(*m->chan));

      if (get_bits(f,1))
         m->submaps = get_bits(f,4);
      else
         m->submaps = 1;

      if (m->submaps > max_submaps)
         max_submaps = m->submaps;

      if (get_bits(f,1))
      {
         m->coupling_steps = get_bits(f,8)+1;
         for (k=0; k < m->coupling_steps; ++k)
         {
            S32 magnitude = get_bits(f, ilog(f->channels)-1);
            S32 angle = get_bits(f, ilog(f->channels)-1);
            if (magnitude >= f->channels)        return error(f, VORBIS_invalid_setup);
            if (angle     >= f->channels)        return error(f, VORBIS_invalid_setup);
            if (magnitude == angle)   return error(f, VORBIS_invalid_setup);
         }
      } else
         m->coupling_steps = 0;

      // reserved field
      if (get_bits(f,2)) return error(f, VORBIS_invalid_setup);
      if (m->submaps > 1) {
         for (j=0; j < f->channels; ++j) {
            S32 mux = get_bits(f, 4);
            if (mux >= m->submaps)                return error(f, VORBIS_invalid_setup);
         }
      }

      for (j=0; j < m->submaps; ++j) {
         get_bits(f,8); // discard
         m->submap_floor[j] = get_bits(f,8);
         m->submap_residue[j] = get_bits(f,8);
         if (m->submap_floor[j] >= f->floor_count)      return error(f, VORBIS_invalid_setup);
         if (m->submap_residue[j] >= f->residue_count)  return error(f, VORBIS_invalid_setup);
      }
   }

   // Modes
   f->mode_count = get_bits(f, 6)+1;
   for (i=0; i < f->mode_count; ++i) {
      Mode *m = f->mode_config+i;
      m->blockflag = get_bits(f,1);
      m->windowtype = get_bits(f,16);
      m->transformtype = get_bits(f,16);
      m->mapping = get_bits(f,8);
      if (m->windowtype != 0)                 return error(f, VORBIS_invalid_setup);
      if (m->transformtype != 0)              return error(f, VORBIS_invalid_setup);
      if (m->mapping >= f->mapping_count)     return error(f, VORBIS_invalid_setup);
   }

   flush_packet(f);

   f->previous_length = 0;

#define CHANNEL_SKEW (2*64)   // Try to avoid having all buffers in the same cache set!

   // Allocate enough ram for all the buffers, then SSE-align them:
   int size_pr_channel = (sizeof(float) * f->blocksize_1 +          // SSE-multiple
                          sizeof(float) * f->blocksize_1/2 +        // SSE-multiple
                          (sizeof(int16) * longest_floorlist + 15) & ~15) +
                          CHANNEL_SKEW * 3;

   ADDMEM(f->channels * size_pr_channel + 15);
   {
       int n2 = f->blocksize_0 >> 1, n4 = f->blocksize_0 >> 2, n8 = f->blocksize_0 >> 3;
       ADDMEM(sizeof(float) * (f->blocksize_0+n2+n4) + 15);
       ADDMEM(sizeof(uint16) * n8);
   }
   {
       int n2 = f->blocksize_1 >> 1, n4 = f->blocksize_1 >> 2, n8 = f->blocksize_1 >> 3;
       ADDMEM(sizeof(float) * (f->blocksize_1+n2+n4) + 15);
       ADDMEM(sizeof(uint16) * n8);
   }


   //
   // Add the mem required for the vorbis, used in open_pushdata.
   //
   ADDMEM(sizeof(stb_vorbis));

   *o_SetupMemNeeded = SetupMemNeeded;
   *o_SetupTempMemNeeded = SetupTempMemNeeded;
   return 1;
}

