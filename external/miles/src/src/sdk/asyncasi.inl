//############################################################################
//##                                                                        ##
//##  Miles Sound System                                                    ##
//##                                                                        ##
//##  ASYNCASI.INL: ASI module for asynchronous decoders (e.g., XMA)        ##
//##                                                                        ##
//##  64-bit protected-mode source compatible with MSVC                     ##
//##                                                                        ##
//##  Version 1.00 of 25-Apr-07: Initial, derived from XMAHAL.CPP           ##
//##                                                                        ##
//##  Author: John Miles                                                    ##
//##                                                                        ##
//############################################################################
//##                                                                        ##
//##  Copyright (C) RAD Game Tools, Inc.                                    ##
//##                                                                        ##
//##  Contact RAD Game Tools at 425-893-4300 for technical support.         ##
//##                                                                        ##
//############################################################################

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

#ifndef radassert
#ifdef _DEBUG
#define radassert(x) if (!(x)) MSSBreakPoint();
#else
#define radassert(x)
#endif
#endif

static S32 ASI_started = 0;

static C8 ASI_error_text[256];

#define DEBUG_FILE  0  // Nonzero to record 16-bit raw file
#define DEBUG_TRACE 0  // Nonzero to enable debugging with optional deferred printfs to avoid interfering with hardware timing

#if DEBUG_TRACE 

static C8 debug_text[65536];

#define TPRINTF if (GetAsyncKeyState(VK_SHIFT) & 0x8000) debugger.dprintf

struct DBG
{
   S32 immed;

   DBG()
      {
#if DEBUG_FILE
      _unlink("d:\\test.raw");
#endif
      debug_text[0] = 0;
      immed = 1;
      }

   void dprintf(C8 const *fmt,
               ...)
      {
      C8 buffer[512];

      va_list ap;

      va_start(ap, 
               fmt);

      wvsprintf(buffer,    // _vsnprintf for 360
                fmt,
                ap);

      va_end(ap);

      if (immed)
         debug_printf("%s",buffer);
      else
         strcat(debug_text, buffer);
      }

   void dump(void)
      {
      for (U32 i=0; i < strlen(debug_text); i++)
         {
         debug_printf("%c",debug_text[i]);
         }

      debug_text[0] = 0;
      }
}
debugger;

#else
#define TPRINTF(...)
#endif

//
// Stream structure
//

typedef struct ASISTREAM
{
   //
   // Opaque (to this module) format-specific context 
   //

   struct STREAMCONTEXT *C;

   //
   // Stream creation parameters
   //

   UINTa         user;
   AILASIFETCHCB fetch_CB;
   MSS_FREE_TYPE * free_CB;

   S32           beginning_of_data_chunk;
   S32           data_chunk_len;
   S32           total_len;

   //
   // Stream properties
   //

   S32 sample_rate;
   S32 nch;
   S32 bytes_per_sample;
   S32 bitrate_nominal;

   //
   // Miscellaneous stream decoder data
   //

   S32  current_offset;    // Relative to beginning_of_data_chunk!
   S32  seek_param;

   //
   // Preferences (not currently supported)
   //

   S32 requested_rate;
   S32 requested_bits;
   S32 requested_chans;

   //
   // Ring of 2K input fragments
   //

   U8   *input_frame;
   U8   *input_fragment;

   //
   // Output buffer reservoir used for any data that's available during a given call to
   // ASI_stream_process() beyond what the caller has asked for
   //
   // This helps us keep the codec's output buffer free to avoid stalls, while also
   // avoiding the need to copy all decoded data through an intermediate buffer
   //

   U8 output_reservoir[OUTPUT_RESERVOIR_BYTES];
   S32 output_reservoir_cursor;
   S32 output_reservoir_level;

} 
ASISTREAM;

static ASIRESULT      AILCALL ASI_stream_close(ASISTREAM *STR);

//
// Minidriver prototypes
//

struct ASISTREAM;

static S32     AILCALL PROVIDER_property   (HPROPERTY        index, 
                                            void *       before_value, 
                                            void const * new_value, 
                                            void *       after_value);

static S32     AILCALL ASI_stream_property (HASISTREAM stream, 
                                            HPROPERTY  property,
                                            void * before_value,
                                            void const* new_value,
                                            void * after_value);

static void *MINI_identify_file_header     (U8  *address, 
                                            S32 *total_len = NULL);

static void MINI_dump_playback_state       (ASISTREAM *STR);

static S32 MINI_stream_open                (ASISTREAM *STR,
                                            S32        total_size);

static void MINI_stream_close              (ASISTREAM *STR);

static void MINI_begin_stream_process      (ASISTREAM *STR);

static void MINI_end_stream_process        (ASISTREAM *STR);

static S32 MINI_acquire_input_lock         (ASISTREAM *STR);

static void MINI_release_input_lock        (ASISTREAM *STR);

static S32 MINI_acquire_output_lock        (ASISTREAM *STR, 
                                            S32        allow_data_fetch);

static void MINI_release_output_lock       (ASISTREAM *STR);

static S32 MINI_input_bytes_requested      (ASISTREAM *STR);

static S32 MINI_input_data_OK              (ASISTREAM *STR);

static void MINI_submit_input_data         (ASISTREAM *STR, 
                                            void      *src,
                                            S32        bytes);

static S32 MINI_output_bytes_available     (ASISTREAM *STR);

static S32 MINI_playback_is_idle           (ASISTREAM *STR);

static S32 MINI_copy_LE16_output_data      (ASISTREAM *STR, 
                                            S32        n_bytes,
                                            void      *dest);

static void MINI_preseek                   (ASISTREAM *STR, 
                                            S32        stream_offset);

//
// Copy 16-bit data with byte swaps
//

#ifdef IS_LE

#define AIL_memcpy_DEST_LE16 AIL_memcpy

#else

static void AIL_memcpy_DEST_LE16(void *dest, //)
                                 void *src,
                                 S32   cnt)
{
   S16 *S = (S16 *) src;
   S16 *D = (S16 *) dest;
   S32 n = cnt >> 1;

   for (S32 i=0; i < n; i++)
      {
      STORE_LE_SWAP16(D, *S);
      D++;
      S++;
      }
}

#endif

//############################################################################
//#                                                                          #
//# Return ASI decoder error text                                            #
//#                                                                          #
//############################################################################

static C8 *       AILCALL ASI_error       (void)
{
   if (ASI_error_text[0]==0)
      {
      return NULL;
      }

   return ASI_error_text;
}


//############################################################################
//#                                                                          #
//# Initialize ASI stream decoder                                            #
//#                                                                          #
//############################################################################

static ASIRESULT AILCALL ASI_startup     (void)
{
   if (ASI_started++)
      {
      strcpy(ASI_error_text,"Already started");
      return ASI_ALREADY_STARTED;
      }

   //
   // Init static properties
   //

   ASI_error_text[0] = 0;

   //
   // Init codec-specific data
   //

   return ASI_NOERR;
}

//############################################################################
//#                                                                          #
//# Shut down ASI stream decoder                                             #
//#                                                                          #
//############################################################################

static ASIRESULT      AILCALL ASI_shutdown    (void)
{
   if (!ASI_started)
      {
      strcpy(ASI_error_text,"Not initialized");
      return ASI_NOT_INIT;
      }

   --ASI_started;

   //
   // Destroy codec-specific data
   //

   return ASI_NOERR;
}

//############################################################################
//#                                                                          #
//# Open a stream, returning handle to stream                                #
//#                                                                          #
//############################################################################

static HASISTREAM   AILCALL ASI_stream_open (MSS_ALLOC_TYPE * palloc,
                                             MSS_FREE_TYPE  * pfree,
                                             UINTa         user,  //)
                                             AILASIFETCHCB fetch_CB,
                                             U32           total_size)
{
   //
   // Allocate ASISTREAM descriptor
   //

   ASISTREAM *STR = (ASISTREAM *) palloc(sizeof(ASISTREAM),user,__FILE__,__LINE__);

   if (STR == NULL)
      {
      strcpy(ASI_error_text,"ASI_stream_open() failed -- out of memory");
      return 0;
      }

   AIL_memset(STR, 0, sizeof(ASISTREAM));

   //
   // Init props
   //

   STR->requested_rate = -1;
   STR->requested_bits = -1;
   STR->requested_chans = -1;

   STR->user     = user;
   STR->fetch_CB = fetch_CB;
   STR->free_CB  = pfree;

   //
   // Allocate driver-specific context structure
   //

   STR->C = (STREAMCONTEXT *) palloc(sizeof(STREAMCONTEXT),user,__FILE__,__LINE__);

   if (STR->C == NULL)
      {
      strcpy(ASI_error_text,"ASI_stream_open() failed -- out of memory");
      return 0;
      }

   if (!MINI_stream_open(STR, total_size))
      {
      ASI_stream_close(STR);
      return 0;
      }

   STR->bytes_per_sample = STR->nch * sizeof(S16);

   //
   // Return descriptor address cast to handle
   //

   return (HASISTREAM) STR;
}

//############################################################################
//#                                                                          #
//# Close stream, freeing handle and all internally-allocated resources      #
//#                                                                          #
//############################################################################

static ASIRESULT      AILCALL ASI_stream_close(ASISTREAM *STR)
{
   //
   // Clean up stream state
   //

   if (STR->C != NULL)
      {
      MINI_stream_close(STR);

      STR->free_CB(STR->C,STR->user,__FILE__,__LINE__);
      STR->C = NULL;
      }

   STR->free_CB(STR,STR->user,__FILE__,__LINE__);

   return ASI_NOERR;
}

//############################################################################
//#                                                                          #
//# Decode data from stream, returning # of bytes actually decoded           #
//#                                                                          #
//############################################################################

static S32 AILCALL ASI_stream_process (HASISTREAM  stream, //)
                                       void   *buffer,
                                       S32         request_size)
{
   ASISTREAM *STR = (ASISTREAM *) stream;

   //
   // Keep track of # of bytes originally requested
   //

   S32 original_request = request_size;

   //
   // Init buffer output offset
   //

   U8 *dest = (U8 *) buffer;
   S32 write_cursor = 0;

   //
   // If we've done any seeking, we may have to reinitialize the
   // context before any further buffer submission can occur
   //

   MINI_begin_stream_process(STR);

   //
   // Loop until requested output data is available
   //

#if 0
   S64 watchdog = AIL_us_count();
#endif

   TPRINTF("Entering decode loop, want %d bytes\n",request_size);

   while (1)
      {
#if 0
      if ((AIL_us_count() - watchdog) > 3000000)
         {
         //
         // If the decoder stops talking to us, force a crash here for easier debugging
         //

         MINI_dump_playback_state(STR);
         MSSBreakPoint();
         }
#endif

      int starved     = FALSE;
      int data_needed = FALSE;

      //
      // Give the minidriver a chance to reject attempts to read past the end of the
      // file (if we know where the end of the file is...)  
      //
      // Synchronous decoders may also choose not to request data here
      //
      // We may not actually need to feed data to the codec in this state, but we need
      // to set the flag anyway so that we won't hang in the loop once the output is
      // exhausted
      //

      if (!MINI_input_data_OK(STR)) 
         {
         data_needed = TRUE;
         }
      else
         {
         //
         // Try to keep input buffers full
         //
         // If the caller continues to request output after the input data
         // is exhausted, we'll try to fulfill the request with data that's 
         // still in the pipeline
         //
   
         if (MINI_acquire_input_lock(STR))
            {
            TPRINTF("Obtained modify lock for input\n");
   
            S32 bytes_requested_by_decoder = MINI_input_bytes_requested(STR);

            if (bytes_requested_by_decoder > 0)
               {
               data_needed = TRUE;
   
               TPRINTF("  Ready to accept more data\n");
   
               S32 result = STR->fetch_CB(STR->user,
                                          STR->input_fragment,
                                          bytes_requested_by_decoder,
                                          STR->seek_param);
           
               TPRINTF("  Fetched %d bytes (asked for %d)\n",result,bytes_requested_by_decoder);
   
               STR->current_offset += result;
               STR->seek_param = -1;
           
               if (result != bytes_requested_by_decoder)
                  {
                  TPRINTF("  Starved\n");     // TODO: go ahead and pass in what we have, for Ogg et al!
                  starved = TRUE;
                  }
               else
                  {
                  //
                  // For the benefit of hardware decoders like XMA, make sure we haven't 
                  // seeked back to the file (RIFF WAV) header -- if we have, correct it
                  // by moving the data block back to the beginning of the buffer 
                  // and fetching enough additional data to make up for the overwritten 
                  // header
                  //
                  // Afterward, ensure that STR->current_offset == S->pos
                  //
                  // The minidriver must deal with all other seeks to non-valid data points
                  // (where possible)
                  //
         
                  if (MINI_identify_file_header(STR->input_fragment) != NULL)
                     {                                   
                     AIL_memmove(&STR->input_fragment[0],
                                 &STR->input_fragment[STR->beginning_of_data_chunk],
                                  bytes_requested_by_decoder - STR->beginning_of_data_chunk);
                     
                     S32 result = STR->fetch_CB(STR->user,
                                               &STR->input_fragment[bytes_requested_by_decoder - STR->beginning_of_data_chunk],
                                                STR->beginning_of_data_chunk,
                                                STR->seek_param);
         
                     TPRINTF("  RIFF header found, fetching %d leading bytes\n",STR->beginning_of_data_chunk);
   
                     STR->current_offset = bytes_requested_by_decoder + result;
         
                     if (result != STR->beginning_of_data_chunk)
                        {
                        TPRINTF("  Starved\n");
                        starved = TRUE;
                        }
                     }
       
                  //
                  // Pass the data we just read to the hardware decoder
                  //
   
                  if (!starved)
                     {
                     TPRINTF("  Submitting %d bytes to decoder\n",bytes_requested_by_decoder);
   
                     MINI_submit_input_data(STR, 
                                            STR->input_fragment, 
                                            bytes_requested_by_decoder);
          
                     data_needed = FALSE;
   
                     //
                     // Use buffer fragments in a circular fashion, to avoid overwriting
                     // data that the hardware is still working on
                     //
               
                     STR->input_fragment += bytes_requested_by_decoder;
          
                     if (STR->input_fragment > &STR->input_frame[MAX_INPUT_FRAGMENT_BYTES * (N_INPUT_FRAGMENTS-1)])
                        {
                        STR->input_fragment = STR->input_frame;
                        }
                     }
                  }
               }
            else
               {
               TPRINTF("NOT ready to accept more data\n");
               }
   
            MINI_release_input_lock(STR);
            TPRINTF("Resuming after input\n");
            }
         else 
            MSSBreakPoint();
         }

      if (starved)
         {
         //
         // We weren't at the end of the data chunk, but we still failed to obtain source data for 
         // some reason....
         //

         break;
         }

      //
      // If any data is currently available in the output reservoir, return it first
      //

      if (STR->output_reservoir_level > 0)
         {
         S32 bytes_avail = STR->output_reservoir_level;

         if (bytes_avail > request_size)
            {
            bytes_avail = request_size;
            }

         TPRINTF("Returning %d bytes from output reservoir\n",bytes_avail);

         AIL_memcpy(&dest[write_cursor],
                    &STR->output_reservoir[STR->output_reservoir_cursor],
                     bytes_avail);

         STR->output_reservoir_cursor += bytes_avail;
         STR->output_reservoir_level  -= bytes_avail;
         write_cursor                 += bytes_avail;
         request_size                 -= bytes_avail;
         }

      //
      // Exit from loop if request completely fulfilled by reservoir contents
      //
      // We could check for new decoded data and store it in the reservoir before returning, but
      // it's probably better to wait until we can snag at least some of it without 
      // an intermediate copy
      //

      if (!request_size)
         {
         TPRINTF("Request fulfilled\n");
         break;
         }

      //
      // Since we still need data, the output reservoir must be empty...
      //

      radassert(STR->output_reservoir_level == 0);

      STR->output_reservoir_cursor = 0;

      //
      // Exit from loop if all available output data has been returned 
      // and input buffers are starved
      //

      if (MINI_acquire_output_lock(STR, TRUE))
         {
         S32 bytes_avail = MINI_output_bytes_available(STR);

         TPRINTF("Obtained modify lock for output: dn=%d, ba=%d, rs=%d\n",data_needed,bytes_avail,request_size);

         if (data_needed && (!bytes_avail))
            {
            MINI_release_output_lock(STR);

            if (!MINI_playback_is_idle(STR))
               {
               //
               // As long as the hardware has more data to process, we need to wait on it, or
               // we'll get dropouts (or worse, the sample will terminate prematurely)
               //

#ifdef IS_XENON
               Sleep(0);
#endif

               TPRINTF("  Waiting for data (still need %d bytes of %d)\n",request_size,original_request);
               continue;
               }

            TPRINTF("  No more returned samples pending, and more data was needed; exiting loop\n");
            break;
            }

         //
         // Consume as much of the available data as the caller can accept...
         //

         if (bytes_avail > 0)
            {
            S32 n = request_size;
            
            if (n > bytes_avail)
               {
               n = bytes_avail;
               }

            //
            // Copy as many output bytes as needed to satisfy the request
            // (note that if request_size is zero, we early-outed above)
            //

            TPRINTF("  Returning %d bytes\n",n);

            MINI_copy_LE16_output_data(STR,
                                       n,
                                      &dest[write_cursor]);

#if DEBUG_FILE
            FILE *f = fopen("d:\\test.raw","ab");
            fwrite(&dest[write_cursor],n,1,f);
            fclose(f);
#endif

            write_cursor += n;
            bytes_avail  -= n;
            request_size -= n;

            //
            // Store the rest in the output reservoir (which must be large enough
            // to swallow a whole frame's worth of output data, if necessary)
            //

            if (bytes_avail > 0)
               {
               radassert((U32)(STR->output_reservoir_level + bytes_avail) <= sizeof(STR->output_reservoir));
               TPRINTF("  Saving %d bytes in reservoir\n",bytes_avail);

               MINI_copy_LE16_output_data(STR,
                                          bytes_avail,
                                         &STR->output_reservoir[STR->output_reservoir_level]);
#if DEBUG_FILE
               FILE *f = fopen("d:\\test.raw","ab");
               fwrite(&STR->output_reservoir[STR->output_reservoir_level],bytes_avail,1,f);
               fclose(f);
#endif

               STR->output_reservoir_level += bytes_avail;
               }
            }

         MINI_release_output_lock(STR);
         TPRINTF("Resuming after output\n");
         }
      else MSSBreakPoint();

      //
      // Share CPU if we're blocking on the hardware
      //

      if (request_size > 0)
         {
#ifdef IS_XENON
         Sleep(0);
#endif
         }
      }

   //
   // If source stream exhausted, pad with zeroes to end of buffer
   //

   if (request_size > 0)
      {
      AIL_memset(&dest[write_cursor],
                  0,
                  request_size);
      }

   //
   // If the request was fulfilled exactly, ensure that the minidriver isn't 
   // keeping a 'reservoir' of its own that can be decoded without any further 
   // data-fetch callbacks... if it is, we need to copy at least some of the 
   // available data to our own output reservoir, so the sample doesn't end prematurely
   // 
#if 0
   if ((STR->output_reservoir_level == 0) && (request_size == 0))
      {
      if (MINI_acquire_output_lock(STR, FALSE))
         {
         S32 bytes_avail = MINI_output_bytes_available(STR);

         if (bytes_avail > 0)
            {
            radassert((U32)(STR->output_reservoir_level + bytes_avail) <= sizeof(STR->output_reservoir));

            TPRINTF("Decoder was able to return %d more bytes for next time\n",bytes_avail);

            MINI_copy_LE16_output_data(STR,
                                       bytes_avail,
                                       &STR->output_reservoir[STR->output_reservoir_level]);
#if DEBUG_FILE
            FILE *f = fopen("d:\\test.raw","ab");
            fwrite(&STR->output_reservoir[STR->output_reservoir_level],bytes_avail,1,f);
            fclose(f);
#endif

            STR->output_reservoir_level += bytes_avail;
            }

         MINI_release_output_lock(STR);
         }
      }
#endif

   //
   // Return # of bytes decoded
   //

   MINI_end_stream_process(STR);

   TPRINTF("Done with decode loop: %d bytes backfilled with zero\n\n", request_size);

   if ((STR->current_offset >= STR->data_chunk_len) && (request_size == original_request))
      {
      //
      // Issue a zero-byte request to terminate the sample upon ASI reservoir exhaustion
      //
      // We do this here for the XMA case because we're careful not to try to fetch
      // data past the end of the file above (it would hose the hardware).  Other decoders (like
      // MP3) don't bother to check STR->current_offset against STR->data_chunk_len; they let the
      // data-fetch callback function perform this check
      //
      // If we don't call the data-fetch callback function at least once after the last block is fetched,
      // the system will never have a chance to terminate the HSAMPLE properly
      //

      TPRINTF("Request HSAMPLE termination upon reservoir exhaustion\n");

      STR->fetch_CB(STR->user,                                                             
                    NULL,
                    0,
                    STR->seek_param);
      }

   return original_request - request_size;
}

//############################################################################
//#                                                                          #
//# Restart stream decoding process at new offset                            #
//#                                                                          #
//# Positive offset: Same as -1 below, but value will be treated as new      #
//#     stream position for the next data-fetch callback and subsequent      #
//#     position property queries                                            #
//#                                                                          #
//# -1: Clear output filter and bit reservoir, and discard both              #
//#     input and output buffers without specifying a new offset.            #
//#     Used for performing explicit asynchronous seeks when the caller      #
//#     doesn't care about updating the ASI codec's position property        #
//#     (which can be a bad idea with XMA, since the hardware doesn't like   #
//#      being fed arbitrary junk after the data chunk ends...)              #
//#                                                                          #
//# -2: Clear bit reservoir and input buffer, but do not discard accumulated #
//#     output data, filter state, or overlap contents.  Used for looping    #
//#                                                                          #
//# (Above is for MP3 decoder; XMA works a bit differently since we have     #
//# to reinitialize the entire hardware state when seeking...)               #
//#                                                                          #
//############################################################################

static ASIRESULT AILCALL ASI_stream_seek (HASISTREAM stream, //)
                                          S32        stream_offset)
{
   ASISTREAM *STR = (ASISTREAM *) stream;

   //
   // Initialize output buffer
   //

   if (stream_offset != -2)
      {
      STR->output_reservoir_level  = 0;
      STR->output_reservoir_cursor = 0;
      }
   else
      {
      stream_offset = -1;
      }

   //
   // Set up to resume frame processing at specified seek position
   //
   // -1 causes current position to be maintained
   //

   STR->seek_param = stream_offset;

   if (stream_offset != -1)
      {
      STR->current_offset = stream_offset;
      }

   MINI_preseek(STR,
                stream_offset);
   
   //
   // Return success
   //

   return ASI_NOERR;
}

