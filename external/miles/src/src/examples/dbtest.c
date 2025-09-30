//############################################################################
//##                                                                        ##
//##  DBTEST.C                                                              ##
//##                                                                        ##
//##  Double-buffered digital audio test facility                           ##
//##                                                                        ##
//##  V1.00 of  2-Jul-94: Initial                                           ##
//##  V1.01 of 20-Nov-94: Use minimum possible buffer size                  ##
//##  V1.02 of 18-Jan-95: Set up driver with .INI file                      ##
//##  V1.03 of  9-Jun-95: Use new HMDIDRIVER/HDIGDRIVER driver handle types ##
//##                      Check driver installation error code              ##
//##  V1.04 of 15-Nov-05: Support MP3 files instead of raw files            ##
//##                                                                        ##
//##   Author: John Miles                                                   ##
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
#include <string.h>
#include <fcntl.h>

#include "mss.h"

#ifdef IS_WINDOWS
  #include <io.h>
#else
  #define _open open
  #define _read read
  #define _lseek lseek
  #define _close close
  #define O_BINARY 0
#endif

#include "con_util.i"

// @cdep pre $DefaultsConsoleMSS
// @cdep post $BuildConsoleMSS

#define N_BUFS 8

S8 *buf[N_BUFS];
S32 n = 0;

//############################################################################
//##                                                                        ##
//## Check to see if any of the sample's buffers need to be filled with     ##
//## audio data, and, if necessary, refresh them                            ##
//##                                                                        ##
//############################################################################

static void serve_sample( HSAMPLE sample, S32 size, S32 file )
{
   S32 len;

   //
   // In this simple example, we prefill as many buffers as we can, then initiate playback...
   //

   while (AIL_sample_buffer_available(sample))
      {
      printf("%d ",n);

      //
      // At least one input buffer is free - read some data into it
      //
   
      len = _read(file, 
                  buf[n], 
                  size);
   
      //
      // Pass the data into Miles
      //
   
      AIL_load_sample_buffer( sample,
                              MSS_BUFFER_HEAD,
                              buf[n],
                              len );

      n = (n + 1) % N_BUFS;
      }

   //
   // No more buffers need to be filled; start playback and exit
   //
   // (The AIL_start_sample() call is ignored for low-level streams that are 
   // already playing)
   //

   AIL_start_sample(sample);
}

//############################################################################
//##                                                                        ##
//## Double-buffering example program                                       ##
//##                                                                        ##
//############################################################################

int MSS_MAIN_DEF main( int argc, char *argv[] )
{
  HDIGDRIVER  dig;
  HSAMPLE     sample;
  HPROVIDER   HP;
  S32         i,len;
  S32         file;
  S32         buffer_size;
  static U8   temp_buffer[AIL_MAX_FILE_HEADER_SIZE];

  setbuf( stdout, NULL );

  printf( "DBTEST - Version " MSS_VERSION "             " MSS_COPYRIGHT " \n" );
  printf( "-------------------------------------------------------------------------------\n\n" );

  printf( "This program plays an .MP3, .OGG, or 16-bit 44 kHz .RAW file using low-level \n");
  printf( "streaming calls.\n\n" );

  if (argc != 2)
  {
    printf( "Usage: DBTEST filename.MP3 / filename.OGG / filename.RAW\n" );
    exit( 1 );
  }

  //
  // Set the redist directory and start MSS
  //

  AIL_set_redist_directory( MSS_DIR_UP_TWO MSS_REDIST_DIR_NAME );

  AIL_startup();

  //
  // Initialize digital sound system
  //

  dig = AIL_open_digital_driver( 44100, 16, MSS_MC_USE_SYSTEM_CONFIG, 0);

  if (dig == 0)
  {
    printf("%s\n", AIL_last_error() );

    AIL_shutdown();
    exit( 1 );
  }

  //
  // Open the file supplied
  //

  file = _open( argv[1], O_RDONLY | O_BINARY );

  if (file == -1)
  {
    printf( "Could not open file '%s'\n", argv[1] );

    AIL_shutdown();
    exit( 1 );
  }

  //
  // Allocate an HSAMPLE with the specified # of source buffers.  By default, all
  // HSAMPLEs have two source buffers, but we'll allocate N_BUFS for testing purposes
  //

  sample = AIL_allocate_sample_handle(dig);

  AIL_set_sample_buffer_count(sample, N_BUFS);

  //
  // Identify file's type
  //
  // If no specific ASI provider is available for this file type, bail
  //

  HP = RIB_find_files_provider( "ASI codec",
                                "Input file types",
                                 argv[1],
                                "Output file types",
                                ".RAW");

  if (HP == 0)
     {
     printf("No ASI provider available for file %s; assuming 16-bit mono PCM\n", argv[1]);

     AIL_init_sample(sample, DIG_F_MONO_16);
     AIL_set_sample_playback_rate(sample, 44100);
     }
  else
     {
     //
     // It's an MP3 or other ASI-supported file -- set sample format according to 
     // file name and contents
     //
     // We'll need to load the first 4K of the file's contents to determine the output 
     // data format, then rewind the file to its beginning
     //
 
     memset(temp_buffer, 0, AIL_MAX_FILE_HEADER_SIZE);
 
     len = _read(file, temp_buffer, AIL_MAX_FILE_HEADER_SIZE);
 
     if (!len)
        {
        printf( "Could not read file '%s'\n", argv[1]);
 
        AIL_shutdown();
        exit(1);
        }
 
     _lseek(file, 0, SEEK_SET);

     //
     // Set the address of the sample to the temp header buffer, so the sample 
     // parameters (rate, width, channels, etc.) can be determined correctly.  
     // The buffered data is ignored (and should be considered invalid) after 
     // AIL_set_sample_processor() returns.
     //
     // AIL_set_sample_processor(SP_ASI_DECODER) will set the sample's format
     // (bit depth, channels, playback rate) to match what the ASI codec reports
     // about the source data.  Since this is a freshly-allocated HSAMPLE, we
     // don't need to call AIL_init_sample() first.
     //
     // (This functionality is pretty similar to the AIL_set_sample_file() function, but
     // that function isn't meant for use with double-buffered HSAMPLEs.)
     // 
     
     AIL_set_sample_address(sample,
                            temp_buffer,
                            len);
     
     AIL_set_sample_processor(sample,
                              SP_ASI_DECODER,
                              HP);
     }
      
  //
  // Ask Miles for the size of the buffers that we need
  //
  // We'll use worst-case rate and type information for the decompressed data stream here,
  // to make sure the buffers are big enough to handle the (more granular) compressed data
  //
  // Minimum-buffer size limits are calculated assuming PCM data will be played through
  // two stream buffers; applications that use more than 2 buffers, or which use compressed
  // data, can safely use smaller ones.  The only hard requirement is that enough data be
  // available to avoid starvation until the next buffer-service interval
  //

  buffer_size = AIL_minimum_sample_buffer_size(dig, 44100, DIG_F_MONO_16);

  //
  // Allocate buffers for low-level streaming
  //

  for (i=0; i < N_BUFS; i++)
    {
    buf[i] = (S8*)AIL_mem_alloc_lock( buffer_size );
    }

  //
  // Example of application's main event loop
  //
  // Read data from file in buffer_size chunks, passing each chunk to the
  // MSS API when requested
  //

  printf( "Servicing buffers: " );

  while ( !kbhit() )
  {
    //
    // Give other threads a time slice
    //  

    AIL_delay( 10 );  // sleep

    //
    // (Process application events here....)
    //

    //
    // Service audio buffers
    //

    serve_sample( sample, buffer_size, file );

    //
    // Exit test loop when final buffer stops playing
    //

    if (AIL_sample_status( sample ) != SMP_PLAYING)
    {
      break;
    }
  }

  if (kbhit())
    getch();

  //
  // Clean up
  //

  AIL_release_sample_handle( sample );

  _close( file );

  for (i=0; i < N_BUFS; i++)
    {
    AIL_mem_free_lock( buf[i] );
    }

  AIL_close_digital_driver( dig );

  AIL_shutdown();

  printf( "\n\nDBTEST stopped.\n" );

  return 0;
}

