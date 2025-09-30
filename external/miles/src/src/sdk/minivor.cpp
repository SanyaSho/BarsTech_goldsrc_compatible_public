//############################################################################
//#                                                                          #
//# ASI interface to inline Ogg Vorbis decoders from stb/tma                 #
//#                                                                          #
//############################################################################

#if defined(_MSC_VER)
#pragma warning(disable:4244)    // conversion loses precision (e.g., uint32 -> uint8)
#pragma warning(disable:4505)    // unref'd local fn ignored
#pragma warning(disable:4245)    // sign/unsign mismatch
#pragma warning(disable:4189)    // local init'd but unref'd
//#pragma warning(disable:4063)
#pragma warning(disable:4127)
#endif

#ifdef __WATCOMC__
#pragma warning 389 9
#pragma warning 555 9
#pragma warning 368 9
#pragma warning 367 9
#pragma warning 604 9
#pragma warning 594 9
#pragma warning 13 9
#endif

#if defined(_XENON) || (defined(_XBOX_VER) && (_XBOX_VER == 200) )
#include <xtl.h> 
#endif

#include "mss.h"
#include "imssapi.h"

#ifdef _DEBUG
#define radassert(x) if (!(x)) MSSBreakPoint();
#else
#define radassert(x)
#endif

#ifndef assert
#define assert radassert
#endif

#define USE_OGG_PEEKAHEAD 1


#define themalloc AIL_mem_alloc_lock //todo - use passed in values...
#define thefree AIL_mem_free_lock
#define thememset AIL_memset
#define thememcpy AIL_memcpy
#define thememcmp AIL_memcmp

#include "vorbis.inl"

#define CLAMP_SSE2_CHECK (AIL_MMX_available() & 0x8)
#ifndef CLAMP_FORCE_LE
    #define CLAMP_FORCE_LE
#endif
#define CLAMP_SUPPORT_UNALIGNED_SOURCE
#include "clamps16.inl" // shared ftoi routines.

static const int OUTPUT_RESERVOIR_BYTES   = 65536;     // was 32768, but 6-channel files that yield 2944 16-bit samples have been encountered...
static const int HEADER_FETCH_BYTES       = 8192;     // Streams will fail if this exceeds AIL_MAX_FILE_HEADER_SIZE
static const int N_INPUT_FRAGMENTS        = 1;                                   
static const int DEFAULT_FETCH_BYTES      = 1024;     // Typical of max frame size for most files
static const int MAX_INPUT_FRAGMENT_BYTES = 65536;      // Needs to be >= 2x DEFAULT_FETCH_BYTES

//
// Table of destination (WDM) channel indexes for each source (AC3) channel,
// for one to six-channel formats
//
// This converts Vorbis's AC3-style channel order to the standard 
// WAVEFORMATEXTENSIBLE order used by MSS
//
// For >6-channel formats, channel order is application-defined, per
// Vorbis I spec
//

static const S32 AC3_to_WDM[6][6] = { { 0, 0, 0, 0, 0, 0 },     // mono
                                      { 0, 1, 0, 0, 0, 0 },     // stereo
                                      { 0, 2, 1, 0, 0, 0 },     // 1D surround 
                                      { 0, 1, 2, 3, 0, 0 },     // quadraphonic
                                      { 0, 2, 1, 3, 4, 0 },     // 5.0
                                      { 0, 2, 1, 4, 5, 3 } };   // 5.1

//
// Table of WAVEFORMATEXTENSIBLE;.dwChannelMask-equivalent channel masks corresponding
// to one to six-channel formats
//

static const S32 WDM_masks[6] = { (1 << MSS_SPEAKER_FRONT_LEFT),

                                  (1 << MSS_SPEAKER_FRONT_LEFT)    |
                                  (1 << MSS_SPEAKER_FRONT_RIGHT),

                                  (1 << MSS_SPEAKER_FRONT_LEFT)    |
                                  (1 << MSS_SPEAKER_FRONT_RIGHT)   |
                                  (1 << MSS_SPEAKER_BACK_CENTER),

                                  (1 << MSS_SPEAKER_FRONT_LEFT)    |
                                  (1 << MSS_SPEAKER_FRONT_RIGHT)   |
                                  (1 << MSS_SPEAKER_BACK_LEFT)     |
                                  (1 << MSS_SPEAKER_BACK_RIGHT),

                                  (1 << MSS_SPEAKER_FRONT_LEFT)    |
                                  (1 << MSS_SPEAKER_FRONT_CENTER)  |
                                  (1 << MSS_SPEAKER_FRONT_RIGHT)   |
                                  (1 << MSS_SPEAKER_BACK_LEFT)     |
                                  (1 << MSS_SPEAKER_BACK_RIGHT),

                                  (1 << MSS_SPEAKER_FRONT_LEFT)    |
                                  (1 << MSS_SPEAKER_FRONT_RIGHT)   |
                                  (1 << MSS_SPEAKER_FRONT_CENTER)  |
                                  (1 << MSS_SPEAKER_LOW_FREQUENCY) |
                                  (1 << MSS_SPEAKER_BACK_LEFT)     |
                                  (1 << MSS_SPEAKER_BACK_RIGHT) };

//
// Property tokens
//

enum PROPERTY
{
   //        
   // Additional decoder props (beyond standard RIB PROVIDER_ properties)
   //

   IN_FTYPES,               // STRING supported input file types
   IN_ED_FTYPES,            // STRING input file types supported in editor, if present
   OUT_FTYPES,              // STRING supported output file types
   FRAME_SIZE,              // S32 worst-case frame buffer size

   //
   // Generic ASI stream props
   //

   INPUT_BIT_RATE,          // S32 input bit rate
   INPUT_SAMPLE_RATE,       // S32 sample rate of input data
   INPUT_BITS,              // S32 bit width per sample of input data
   INPUT_CHANNELS,          // S32 # of channels in input data

   OUTPUT_BIT_RATE,         // S32 output bit rate
   OUTPUT_SAMPLE_RATE,      // S32 output sample rate
   OUTPUT_BITS,             // S32 bit width per sample of output data
   OUTPUT_CHANNELS,         // S32 # of channels in output data
   OUTPUT_CHANNEL_MASK,     // U32 channel mask, compatible with WAVEFORMATEXTENSIBLE.dwChannelMask

   OUTPUT_RESERVOIR,        // S32 bytes decoded but not yet requested from ASI_stream_process
   POSITION,                // S32 bytes processed so far
   PERCENT_DONE,            // % percent done
   MIN_INPUT_BLOCK_SIZE,    // S32 minimum block size for input

   //
   // Stream properties
   //

   REQUESTED_RATE,          // S32 requested rate for output data
   REQUESTED_BITS,          // S32 requested bit width for output data
   REQUESTED_CHANS,         // S32 requested # of channels for output data
   STREAM_SEEK_POS,         // S32 S->pos value after a stream seek (e.g., any change in HSAMPLE position other than normal incrementing during playback)

   // decoder props where the index is the value
   PROF_INDEX=0x1003       // profile index: 0x1001=bink,0x1002=mp3,0x1003=ogg, 
};

typedef struct STREAMCONTEXT
{
   //
   // STB_VORBIS context and related state
   //

   stb_vorbis      *VORB;

   //! Allocated buffer for vorbis usage.
   void* m_pBuffer;

   S32   decoded_channels;
   S32   decoded_samples;
   F32 **decoded_output;
   
   S32   output_bytes_available;
   S32   next_output_byte;

   //
   // Ring of input fragments
   //

   U8   *input_frame;
   S32   frame_bytes;

   S32   head_offset;         // Offset where input data will be loaded next
   S32   tail_offset;         // Offset in input_frame of first byte unconsumed by decoder

   S32   need_to_init_tables;
   S32   need_to_flush;

} 
STREAMCONTEXT;

#include "asyncasi.inl"

DXDEF S32 AILEXPORT RIB_MAIN_NAME(OggDec)( HPROVIDER provider_handle, U32 up_down, RIB_ALLOC_PROVIDER_HANDLE_TYPE * rib_alloc, RIB_REGISTER_INTERFACE_TYPE * rib_reg, RIB_UNREGISTER_INTERFACE_TYPE * rib_unreg )
{
   const RIB_INTERFACE_ENTRY ASI1[] =
      {
      REG_FN(PROVIDER_property),
      REG_PR("Name",                 PROVIDER_NAME,    (RIB_DATA_SUBTYPE) (RIB_READONLY|RIB_STRING)),
      REG_PR("Version",              PROVIDER_VERSION, (RIB_DATA_SUBTYPE) (RIB_READONLY|RIB_HEX)),
      REG_PR("Input file types",     IN_FTYPES,        (RIB_DATA_SUBTYPE) (RIB_READONLY|RIB_STRING)),
      };

   const RIB_INTERFACE_ENTRY ASI2[] =
      {
      REG_PR("Editor-supported input file types",  IN_ED_FTYPES, (RIB_DATA_SUBTYPE) (RIB_READONLY|RIB_STRING)),
      REG_PR("Output file types",                  OUT_FTYPES,   (RIB_DATA_SUBTYPE) (RIB_READONLY|RIB_STRING)),
      REG_PR("Maximum frame size",                 FRAME_SIZE,   (RIB_DATA_SUBTYPE) (RIB_READONLY|RIB_DEC)),
      REG_PR("Profile index",                      PROF_INDEX,   (RIB_DATA_SUBTYPE) (RIB_READONLY|RIB_DEC)),
      };

   const RIB_INTERFACE_ENTRY ASI3[] =
      {
      REG_FN(ASI_startup),
      REG_FN(ASI_error),
      REG_FN(ASI_shutdown),
      };

   const RIB_INTERFACE_ENTRY ASISTR1[] =
      {
      REG_FN(ASI_stream_open),
      REG_FN(ASI_stream_process),
      REG_FN(ASI_stream_property),
      };

   const RIB_INTERFACE_ENTRY ASISTR2[] =
      {
      REG_FN(ASI_stream_seek),
      REG_FN(ASI_stream_close),
      REG_PR("Input bit rate",           INPUT_BIT_RATE,       (RIB_DATA_SUBTYPE) (RIB_READONLY|RIB_DEC)),
      REG_PR("Input sample rate",        INPUT_SAMPLE_RATE,    (RIB_DATA_SUBTYPE) (RIB_READONLY|RIB_DEC)),
      };

   const RIB_INTERFACE_ENTRY ASISTR3[] =
      {
      REG_PR("Input sample width",       INPUT_BITS,           (RIB_DATA_SUBTYPE) (RIB_READONLY|RIB_DEC)),
      REG_PR("Input channels",           INPUT_CHANNELS,       (RIB_DATA_SUBTYPE) (RIB_READONLY|RIB_DEC)),
      REG_PR("Output bit rate",          OUTPUT_BIT_RATE,      (RIB_DATA_SUBTYPE) (RIB_READONLY|RIB_DEC)),
      REG_PR("Output sample rate",       OUTPUT_SAMPLE_RATE,   (RIB_DATA_SUBTYPE) (RIB_READONLY|RIB_DEC)),
      };

   const RIB_INTERFACE_ENTRY ASISTR4[] =
      {
      REG_PR("Output sample width",      OUTPUT_BITS,          (RIB_DATA_SUBTYPE) (RIB_READONLY|RIB_DEC)),
      REG_PR("Output channels",          OUTPUT_CHANNELS,      (RIB_DATA_SUBTYPE) (RIB_READONLY|RIB_DEC)),
      REG_PR("Output reservoir",         OUTPUT_RESERVOIR,     (RIB_DATA_SUBTYPE) (RIB_READONLY|RIB_DEC)),
      REG_PR("Position",                 POSITION,             (RIB_DATA_SUBTYPE) (RIB_READONLY|RIB_DEC)),
      };

   const RIB_INTERFACE_ENTRY ASISTR5[] =
      {
      REG_PR("Percent done",             PERCENT_DONE,         (RIB_DATA_SUBTYPE) (RIB_READONLY|RIB_PERCENT)),
      REG_PR("Minimum input block size", MIN_INPUT_BLOCK_SIZE, (RIB_DATA_SUBTYPE) (RIB_READONLY|RIB_DEC)),
      };

   const RIB_INTERFACE_ENTRY ASISTR6[] =
      {
      REG_PR("Requested sample rate",    REQUESTED_RATE,       RIB_DEC),
      REG_PR("Requested bit width",      REQUESTED_BITS,       RIB_DEC),
      REG_PR("Requested # of channels",  REQUESTED_CHANS,      RIB_DEC),
      REG_PR("Stream seek position",     STREAM_SEEK_POS,      RIB_DEC),
      };

   if (up_down)
     {
         RIB_registerP(provider_handle, "ASI codec", ASI1);
         RIB_registerP(provider_handle, "ASI codec", ASI2);
         RIB_registerP(provider_handle, "ASI codec", ASI3);

         RIB_registerP(provider_handle, "ASI stream", ASISTR1);
         RIB_registerP(provider_handle, "ASI stream", ASISTR2);
         RIB_registerP(provider_handle, "ASI stream", ASISTR3);
         RIB_registerP(provider_handle, "ASI stream", ASISTR4);
         RIB_registerP(provider_handle, "ASI stream", ASISTR5);
         RIB_registerP(provider_handle, "ASI stream", ASISTR6);
      }
    else
      {
         RIB_unregister_allP(provider_handle);
      }

   return TRUE;
}

//############################################################################
//#                                                                          #
//# Retrieve a standard RIB provider property by index                       #
//#                                                                          #
//############################################################################

static S32 AILCALL PROVIDER_property(HPROPERTY index, void * before_value, void const * new_value, void * after_value)
{
   switch ((SINTa) index)
      {
      case PROVIDER_NAME:
        if (before_value)
        {
          *(char const **)before_value = "MSS Ogg Vorbis Audio Decoder";
          return 1;
        }
        break;
        
      case PROVIDER_VERSION:
        if (before_value) 
        {
          *(S32*)before_value = 0x130;
          return 1;
        }
        break;

      case IN_FTYPES:
      case IN_ED_FTYPES:
        if (before_value)
        {
          *(char const **)before_value =  "Ogg Vorbis files\0*.OGG\0";
          return 1;
        }
        break;
        
      case OUT_FTYPES:
        if (before_value)
        {
          *(char const **)before_value = "Raw PCM files\0*.RAW\0";
          return 1;
        }
        break;
        
      case FRAME_SIZE:
        if (before_value)
        {
          *(S32*)before_value = DEFAULT_FETCH_BYTES;
          return 1;
        }
        break;
      
      default:
         break;
      }

   return 0;
}

//############################################################################
//#                                                                          #
//# Retrieve an ASI stream property value by index                           #
//#                                                                          #
//############################################################################

static S32     AILCALL ASI_stream_property (HASISTREAM stream, //)
                                            HPROPERTY  property,
                                            void * before_value,
                                            void const* new_value,
                                            void * after_value)
{
   struct ASISTREAM *STR = (ASISTREAM *) stream;

   switch ((PROPERTY) property)
      {
      //
      // Properties
      //

      case INPUT_BIT_RATE:
        if (before_value) 
        {
          *(S32 *) before_value = STR->bitrate_nominal;
          return 1;
        }
        break;

      case INPUT_SAMPLE_RATE:
        if (before_value) 
        {
          *(S32*)before_value = STR->sample_rate;
          return 1;
        }
        break;

      case INPUT_BITS:
        if (before_value) 
        {
          *(S32*)before_value = 1;
          return 1;
        }
        break;

      case INPUT_CHANNELS:
        if (before_value) 
        {
          *(S32*)before_value = STR->nch;
          return 1;
        }
        break;

      case OUTPUT_BIT_RATE:   
        if (before_value) 
        {
          *(S32*)before_value = STR->sample_rate * 16 * STR->nch;
          return 1;
        }
        break;

      case OUTPUT_SAMPLE_RATE: 
        if (before_value) 
        {
          *(S32*)before_value = STR->sample_rate;
          return 1;
        }
        break;

      case OUTPUT_BITS: 
        if (before_value) 
        {
          *(S32*)before_value = 16;
          return 1;
        }
        break;

      case OUTPUT_CHANNELS:
        if (before_value) 
        {
          *(S32*)before_value = STR->nch;
          return 1;
        }
        break;

      case OUTPUT_CHANNEL_MASK:
         if (before_value)
            {
            *(U32 *) before_value = (STR->nch > 6) ? (1 << STR->nch)-1 : WDM_masks[STR->nch-1];
            return 1;
            }
         break;

      case OUTPUT_RESERVOIR: 
        if (before_value) 
        {
        *(S32*)before_value = STR->output_reservoir_level + MINI_output_bytes_available(STR);
          return 1;
        }
        break;

      case POSITION:   
        if (before_value) 
        {
          *(S32*)before_value = STR->current_offset;
          return 1;
        }
        break;

      case MIN_INPUT_BLOCK_SIZE: 
        if (before_value) 
        {
          *(S32*)before_value = DEFAULT_FETCH_BYTES;
          return 1;
        }
        break;

      case PERCENT_DONE:
        if (before_value) 
        {
         if ((STR->current_offset == 0) || (STR->data_chunk_len == 0))
         {
            *(F32*)before_value = 0.0f;
         }
         else
         {
            *(F32*)before_value = F32(STR->current_offset) * 100.0F / F32(STR->data_chunk_len);
         }
         return 1;
        }
        break;

      case REQUESTED_RATE:
        if (before_value) 
        {
          *(S32*)before_value = STR->requested_rate;
        }
        if (new_value)
        {
          STR->requested_rate  = *(S32*)new_value;
        }
        if (after_value) 
        {
          *(S32*)after_value = STR->requested_rate;
        }
        return 1;

      case REQUESTED_BITS:
        if (before_value) 
        {
          *(S32*)before_value = STR->requested_bits;
        }
        if (new_value)
        {
          STR->requested_bits  = *(S32*)new_value;
        }
        if (after_value) 
        {
          *(S32*)after_value = STR->requested_bits;
        }
        return 1;
  
      case REQUESTED_CHANS:
        if (before_value) 
        {
          *(S32*)before_value = STR->requested_chans;
        }
        if (new_value)
        {
          STR->requested_chans  = *(S32*)new_value;
        }
        if (after_value) 
        {
          *(S32*)after_value = STR->requested_chans;
        }
        return 1;

      case STREAM_SEEK_POS:
        if (before_value) 
        {
          *(S32*)before_value = STR->current_offset;
        }
        if (new_value)
        {
          STR->current_offset = STR->seek_param  = *(S32*) new_value;
        }
        if (after_value) 
        {
          *(S32*)after_value = STR->current_offset;
        }
        return 1;


      default: break;
      }

   return 0;
}

//############################################################################
//#                                                                          #
//# Identify a valid file header, returning pointer to format descriptor     #
//# if one was found                                                         #
//#                                                                          #
//############################################################################

static void *MINI_identify_file_header (U8  *address, //)  
                                        S32 *total_len)
{
   //
   // Vorbis doesn't need us to detect seeks to the beginning of the file
   //

   return NULL;
}

//############################################################################
//#                                                                          #
//# Dump playback state to debug console                                     #
//#                                                                          #
//############################################################################

static void MINI_dump_playback_state(ASISTREAM *STR)
{
}

//############################################################################
//#                                                                          #
//# Main CPU: validate the stream and create its decoder context             #
//#                                                                          #
//############################################################################

static S32 MINI_stream_open(ASISTREAM *STR,
                            S32        total_size)
{
   //
   // Allocate input buffer
   //
   STR->C->m_pBuffer = 0;
   STR->C->tail_offset = 0;
   STR->C->head_offset = 0;
   STR->C->frame_bytes = MAX_INPUT_FRAGMENT_BYTES * N_INPUT_FRAGMENTS;

   STR->input_frame = (U8 *) AIL_mem_alloc_lock(STR->C->frame_bytes);

   if (STR->input_frame == NULL)
      {
      strcpy(ASI_error_text,"ASI_stream_open() failed -- out of memory");
      return FALSE;
      }  

   //
   // Fetch initial input fragment's worth of bytes
   //

   S32 initial_fragment_bytes = STR->fetch_CB(STR->user,
                                              STR->input_frame,
                                              (total_size-1) < HEADER_FETCH_BYTES ? (total_size-1) : HEADER_FETCH_BYTES,
                                              0);

   if (initial_fragment_bytes == 0)
      {
      strcpy(ASI_error_text,"ASI_stream_open() failed -- truncated file");
      return FALSE;
      }

   //
   // Pass in the initial data block containing the Ogg and Vorbis headers
   //

   S32 error = 0;
   S32 bytes_used = 0;

#if USE_OGG_PEEKAHEAD
   VorbisPeekaheadProps props;
   AIL_memset(&props, 0, sizeof(props));

   int Succeeded = stb_vorbis_open_pushdata_begin(
       STR->input_frame,
       initial_fragment_bytes,
       &props);

   STR->sample_rate = props.m_SampleRate;
   STR->nch = props.m_Channels;
   STR->bitrate_nominal = props.m_BitrateNominal;

   if (Succeeded)
   {
       //
       // Complete opening...
       //
       STR->C->m_pBuffer = AIL_mem_alloc_lock(props.m_TotalMemoryNeeded);
       if (STR->C->m_pBuffer == 0)
       {
           strcpy(ASI_error_text, "ASI_stream_open() failed -- out of memory");
           return FALSE;
       }

       STR->C->VORB = stb_vorbis_open_pushdata_complete(STR->input_frame, initial_fragment_bytes, STR->C->m_pBuffer, props.m_TotalMemoryNeeded);
    
       STR->C->need_to_init_tables = 1;
   }
#else
   STR->C->VORB = stb_vorbis_open_pushdata(STR->input_frame, 
                                           initial_fragment_bytes,
                                          &bytes_used,
                                          &error);

   stb_vorbis_info info = stb_vorbis_get_info(STR->C->VORB);

   STR->sample_rate     = info.sample_rate;
   STR->nch             = info.channels;
   STR->bitrate_nominal = info.bitrate_nominal;

#endif

   if (STR->C->VORB == NULL)
      {
      strcpy(ASI_error_text,"ASI_stream_open() failed -- could not initialize Vorbis decoder");
      return FALSE;
      }   

   if (!STR->bitrate_nominal)
      {
      STR->bitrate_nominal = 320000;   // Some Ogg streams may not specify their rate, but we have to report a valid rate, so make something up...
      }

#if 0
   debug_printf("      Sample rate: %d Hz\n",STR->C->INFO.sample_rate);
   debug_printf("    # of channels: %d\n",STR->C->INFO.channels);
   debug_printf("        Setup mem: %d bytes\n",STR->C->INFO.setup_memory_required);
   debug_printf("   Setup temp mem: %d bytes\n",STR->C->INFO.setup_temp_memory_required);
   debug_printf("         Temp mem: %d bytes\n",STR->C->INFO.temp_memory_required);
   debug_printf("   Max frame size: %d bytes\n",STR->C->INFO.max_frame_size);
   debug_printf("   Initial offset: %d bytes\n",bytes_used);
#endif

   //
   // Current offset is specified relative to beginning of file
   //

   STR->C->head_offset = initial_fragment_bytes;
   STR->C->tail_offset = USE_OGG_PEEKAHEAD ? 0 : bytes_used;
   STR->seek_param     = -1;
   STR->current_offset = initial_fragment_bytes;

   return TRUE;
}

//############################################################################
//#                                                                          #
//# Main CPU: validate the stream and create its decoder context             #
//#                                                                          #
//############################################################################

static void MINI_stream_close(ASISTREAM *STR)
{
    if (STR->input_frame)
    {
        AIL_mem_free_lock(STR->input_frame);
    }


   if (STR->C->VORB != NULL)
      {
      stb_vorbis_close(STR->C->VORB);
      STR->C->VORB = NULL;
      }

    if (STR->C->m_pBuffer)
    {
        AIL_mem_free_lock(STR->C->m_pBuffer);
    }
}

//############################################################################
//#                                                                          #
//# Main CPU: do any housekeeping needed before servicing the stream         #
//#                                                                          #
//############################################################################

static void MINI_begin_stream_process(ASISTREAM *STR)
{
}

//############################################################################
//#                                                                          #
//# Main CPU: do any housekeeping needed after servicing the stream          #
//#                                                                          #
//############################################################################

static void MINI_end_stream_process(ASISTREAM *STR)
{
}

//############################################################################
//#                                                                          #
//# Main CPU: request lock resource needed to check input status and submit  #
//#           new source data                                                #
//#                                                                          #
//############################################################################

static S32 MINI_acquire_input_lock(ASISTREAM *STR)
{
   return TRUE;
}

//############################################################################
//#                                                                          #
//# Main CPU: release input lock resource                                    #
//#                                                                          #
//############################################################################

static void MINI_release_input_lock(ASISTREAM *STR)
{
}

//############################################################################
//#                                                                          #
//# Main CPU: request lock resource needed to check output status and fetch  #
//#           returned data (if allowed)                                     #
//#                                                                          #
//# Synchronous decoders can issue data-fetch callbacks here and save the    #
//# resulting output data to be returned in MINI_copy_LE16_output_data()     #
//#                                                                          #
//############################################################################

static S32 MINI_acquire_output_lock(ASISTREAM *STR,
                                    S32        allow_data_fetch)
{
   if ( STR->C->need_to_init_tables )
   {
     STR->C->need_to_init_tables = 0;
     stb_vorbis_init_tables( STR->C->VORB );  // on error??
   }

   if ( STR->C->need_to_flush )
   {
     STR->C->need_to_flush = 0;
     stb_vorbis_flush_pushdata_start_on_page(STR->C->VORB);   
   }

   //
   // Assume no more output data
   //

   STR->C->output_bytes_available = 0;
   STR->C->next_output_byte       = 0;

   //
   // Process data, fetching more from callback until an output frame 
   // becomes available
   //

   for (;;)
      {
      S32 bytes_consumed   = 0;
      STR->C->decoded_samples = 0;
   
      bytes_consumed = stb_vorbis_decode_frame_pushdata(STR->C->VORB,
                                                       &STR->input_frame[STR->C->tail_offset],
                                                        STR->C->head_offset - STR->C->tail_offset,
                                                       &STR->C->decoded_channels,
                                                       &STR->C->decoded_output,
                                                       &STR->C->decoded_samples);

      //
      // See if there was a page-alignment or other corruption error
      // (typically will happen after a random seek)
      //

      if ((bytes_consumed == 0) && 
          (STR->C->decoded_samples == 0) && 
          (STR->C->VORB->error == VORBIS_invalid_stream))
         {
         stb_vorbis_flush_pushdata(STR->C->VORB);   // force page seek and retry
         continue;
         }

      STR->C->tail_offset += bytes_consumed;

      if (STR->C->decoded_samples > 0)
         {
         //
         // One frame of data was returned
         //

         STR->C->output_bytes_available = STR->C->decoded_samples * STR->bytes_per_sample;

         return TRUE;
         }
      else
         {
         //
         // We must need more data -- fetch up to DEFAULT_FETCH_BYTES more and store it 
         // at the head pointer, then try the decode operation again
         //
         // The caller can set allow_data_fetch to FALSE to return only frames that can be
         // decoded from source data already passed to the Vorbis decoder.  Since no further
         // frames were decodable above, we must return without altering the decoder state 
         // in any way if allow_data_fetch is FALSE
         //

         if (!allow_data_fetch)
            {
            return TRUE;
            }

         //
         // First, make room in the buffer if the head pointer can't be advanced that far
         //
         // TODO: This memmove() could go away if the STB API could take a DirectX-style ring-
         // buffer pointer pair
         //

         if (STR->C->head_offset + DEFAULT_FETCH_BYTES > STR->C->frame_bytes)
            {
            radassert(STR->C->tail_offset >= DEFAULT_FETCH_BYTES);

            memmove(&STR->input_frame[STR->C->tail_offset - DEFAULT_FETCH_BYTES],
                    &STR->input_frame[STR->C->tail_offset],
                     STR->C->head_offset - STR->C->tail_offset);

            STR->C->head_offset -= DEFAULT_FETCH_BYTES;
            STR->C->tail_offset -= DEFAULT_FETCH_BYTES;
            }

         S32 result = STR->fetch_CB(STR->user,
                                   &STR->input_frame[STR->C->head_offset],
                                    DEFAULT_FETCH_BYTES,
                                    STR->seek_param);

         STR->current_offset += result;
         STR->seek_param = -1;

         STR->C->head_offset += result;

         if (!result && bytes_consumed == 0)
            {
            //
            // We weren't able to fetch any more data, and couldn't process a frame;
            // return 0 bytes output available
            // 

            return TRUE;
            }
         }
      }

//   return TRUE;
}

//############################################################################
//#                                                                          #
//# Main CPU: release output lock resource                                   #
//#                                                                          #
//############################################################################

static void MINI_release_output_lock(ASISTREAM *STR)
{
}

//############################################################################
//#                                                                          #
//# Main CPU: return TRUE if the driver wants a chance to be fed input       #
//# data before handling any output requests                                 #
//#                                                                          #
//# A driver can return FALSE here if it wants to request its own data       #
//# synchronously in MINI_acquire_output_lock() (Ogg), or if it can tell     #
//# that the read will go past the end of the source data (XMA).  If the     #
//# driver returns FALSE here, MINI_ready_for_more_input_data() and          #
//# MINI_submit_input_data() will not be called.                             #
//#                                                                          #
//# (This must be callable without taking the input or output lock)          #
//#                                                                          #
//############################################################################

static S32 MINI_input_data_OK(ASISTREAM *STR)
{
   return FALSE;
}

//############################################################################
//#                                                                          #
//# Main CPU: returns number of bytes that can be accepted by the decoder,   #
//# or 0 if not ready                                                        #
//#                                                                          #
//############################################################################

static S32 MINI_input_bytes_requested(ASISTREAM *STR)
{
   return 0;
}

//############################################################################
//#                                                                          #
//# Main CPU: Submit next block of input data to decoder                     #
//#                                                                          #
//############################################################################

static void MINI_submit_input_data (ASISTREAM *STR, 
                                    void      *src,
                                    S32        bytes)
{
}

//############################################################################
//#                                                                          #
//# Main CPU: Return # of output bytes available from codec, or zero         #
//# if no output data is currently available                                 #
//#                                                                          #
//############################################################################

static S32 MINI_output_bytes_available(ASISTREAM *STR)
{
   return STR->C->output_bytes_available;
}

//############################################################################
//#                                                                          #
//# Main CPU: Return TRUE if codec is not actively working on input data     #
//# in the background                                                        #
//#                                                                          #
//# (This must be callable without taking the input or output lock)          #
//#                                                                          #
//############################################################################

static S32 MINI_playback_is_idle(ASISTREAM *STR)
{
   return TRUE;
}

#ifndef FAST_SCALED_FLOAT_TO_INT

#ifdef IS_BE
   #define FAST_ENDIAN     1
#else
   #define FAST_ENDIAN     0
#endif

   #define MAGIC(SHIFT) (1.5 * (1 << 26) * (1 << (26-SHIFT)) + 0.5/(1 << SHIFT))
   #define FAST_SCALED_FLOAT_TO_INT(x,s) ((temp = (x) + MAGIC(s)), ((int *)&temp)[FAST_ENDIAN])
#endif

//############################################################################
//#                                                                          #
//# Main CPU: Copy specified # of bytes of output data to caller's buffer,   #
//# returning # of bytes actually written                                    #
//#                                                                          #
//# Multi-byte sample data is always written in LE S16 format                #
//#                                                                          #
//# The return value will always equal n_bytes unless you ask for more data  #
//# than MINI_output_bytes_available() says is available                     #
//#                                                                          #
//############################################################################

static S32 MINI_copy_LE16_output_data      (ASISTREAM *STR,  //)
                                            S32        n_bytes,
                                            void      *dest)
{
    if (n_bytes > STR->C->output_bytes_available)
    {
        n_bytes = STR->C->output_bytes_available;
    }

    S32 n_samples    = n_bytes                  / STR->bytes_per_sample;
    S32 first_sample = STR->C->next_output_byte / STR->bytes_per_sample;
    S32 end_sample   = first_sample + n_samples;

    //
    // If we are mono or stereo, we can float->int convert straight in to
    // the output buffer.
    //
    if (STR->nch == 1)
    {
        //
        // Convert the first samples until we have an aligned destination.
        //
        S16* pDest = (S16*)dest;
        U32 DestRemainder = (UINTa)pDest - ((UINTa)pDest & ~0xF);

        for (; DestRemainder && DestRemainder < 16 && (first_sample < end_sample); first_sample++)
        {
           
            S32 v = (S32)(STR->C->decoded_output[0][first_sample] * 32767.f);
            v = ( v >= 32767 ) ? (S16) 32767 : ( ( v <= -32768 ) ? (S16) -32768 : (S16) v );    
            STORE_LE_SWAP16(pDest,v);

            pDest++;
            DestRemainder+=2;
        }

        // Force the remaining count to be %8 = 0.
        U32 Num = end_sample - first_sample;
        Num &= ~7;
        
        // Mass convert into an aligned buffer the count that is %8=0.
        if (Num)
        {
            ClampToS16(pDest, &STR->C->decoded_output[0][first_sample], 32767.0f, Num);
        }

        // Convert the tail that wasn't aligned.
        S16 *D = pDest + Num;
        for (S32 s=first_sample + Num; s < end_sample; s++)
        {
            S32 v = (S32)(STR->C->decoded_output[0][s] * 32767.f);
            v = ( v >= 32767 ) ? (S16) 32767 : ( ( v <= -32768 ) ? (S16) -32768 : (S16) v );    
            STORE_LE_SWAP16(D,v);

            D++;
        }
    }
    else if (STR->nch == 2)
    {
        //
        // Convert until we have an aligned destination
        //
        S16* pDest = (S16*)dest;
        U32 DestRemainder = (UINTa)pDest - ((UINTa)pDest & ~0xF);


        for (; DestRemainder && DestRemainder < 16 && (first_sample < end_sample); first_sample++)
        {
            S32 v = (S32)(STR->C->decoded_output[0][first_sample] * 32767.f);
            v = ( v >= 32767 ) ? (S16) 32767 : ( ( v <= -32768 ) ? (S16) -32768 : (S16) v );    
            STORE_LE_SWAP16(pDest,v);
            pDest++;

            v = (S32)(STR->C->decoded_output[1][first_sample] * 32767.f);
            v = ( v >= 32767 ) ? (S16) 32767 : ( ( v <= -32768 ) ? (S16) -32768 : (S16) v );    
            STORE_LE_SWAP16(pDest,v);
            pDest++;

            DestRemainder += 4;
        }

        // Force the remaining count to be %4 = 0.
        U32 Num = end_sample - first_sample;
        Num &= ~3;

        // Mass convert into an aligned buffer the count that is %4=0.
        if (Num)
        {
            {
                // Fast ftoi with interleave.
                ClampToS16AndInterleave(
                    (S16*)pDest, 
                    &STR->C->decoded_output[0][first_sample],
                    &STR->C->decoded_output[1][first_sample],
                    32767.0f,
                    Num); 
            }
        }

        // Slow path for non standard cases.
        for (S32 ch = 0; ch < 2; ch++)
        {
            S16 *D = pDest + 2*Num + ch;

            for (S32 s=first_sample + Num; s < end_sample; s++)
            {
                S32 v = (S32)(STR->C->decoded_output[ch][s] * 32767.f);
                v = ( v >= 32767 ) ? (S16) 32767 : ( ( v <= -32768 ) ? (S16) -32768 : (S16) v );    
                STORE_LE_SWAP16(D,v);

                D += 2;
            }
        }

    }
    else
    {
        // Slow path for non standard cases.
        for (S32 ch = 0; ch < STR->nch; ch++)
        {
            S16 *D = ((S16 *) dest) + ((STR->nch > 6) ? ch : AC3_to_WDM[STR->nch-1][ch]);

            for (S32 s=first_sample; s < end_sample; s++)
            {
                // PS2 and PS3 have issues with the fast converter.
                // PS3 has alias issues, PS2 has upconvert to double.
                // disable the fast conversion for windows as it relies on the FPU state, which may
                // be changed by d3d
#if !defined(IS_PS3) && !defined(IS_PS2) && !defined(IS_PSP) && !defined(IS_WIN32API) && !defined(IS_IPHONE)
                F64 temp;
                S32 v = FAST_SCALED_FLOAT_TO_INT(STR->C->decoded_output[ch][s], 15);

                if ((U32) (v + 32768) > 65535)
                {
                    v = v < 0 ? -32768 : 32767;
                }
#else
                S32 v = (S32)(STR->C->decoded_output[ch][s] * 32767.f);
                v = ( v >= 32767 ) ? (S16) 32767 : ( ( v <= -32768 ) ? (S16) -32768 : (S16) v );
#endif
                STORE_LE_SWAP16(D,v);

                D += STR->nch;
            }
        }        
    } // end if > 2 channels.

   STR->C->output_bytes_available -= n_bytes;
   STR->C->next_output_byte       += n_bytes;

   return n_bytes;
}

//############################################################################
//#                                                                          #
//# Clear any internal buffers and prepare to continue playing at a new      #
//# data offset                                                              #
//#                                                                          #
//# stream_offset will be -1 (or a valid data offset) for arbitrary seeks,   #
//# or -2 for loops where the decoder may prefer to keep overlap-window data #
//# intact                                                                   #
//#                                                                          #
//# If a valid offset is available, it will be provided separately via a     #
//# ASI_stream_property(STREAM_SEEK_POS) call                                #
//#                                                                          #
//############################################################################

static void MINI_preseek(ASISTREAM *STR, //)
                         S32        stream_offset)
{
   STR->C->need_to_flush = 1;

   //
   // For now, we'll assume we should dump all pushed data during any seek operation
   //

   STR->C->head_offset = STR->C->tail_offset = 0;
}
