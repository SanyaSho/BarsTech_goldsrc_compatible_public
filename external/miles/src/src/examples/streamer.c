//############################################################################
//##                                                                        ##
//##  STREAMER.C                                                            ##
//##                                                                        ##
//##  Streaming API example program                                         ##
//##                                                                        ##
//##  V1.00 of  2-Jul-94: Initial                                           ##
//##                                                                        ##
//##   Author: Jeff Roberts                                                 ##
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

#include "mss.h"

#include "con_util.i"

// @cdep pre $DefaultsConsoleMSS
// @cdep post $BuildConsoleMSS

//############################################################################
//##                                                                        ##
//## Streaming example program                                              ##
//##                                                                        ##
//############################################################################

int MSS_MAIN_DEF main( int argc, char *argv[] )
{
  HDIGDRIVER  dig;
  HSTREAM  stream;
  HSAMPLE  HS;
  C8  *filename=0;
#ifndef IS_DOS
  C8 filename_buffer[512];
#endif
  S32 size,advance,mem,i,rate,last_per = -1;

  S32 paused = 0;
  U32 lasttime = 0;

  set_up_console( 0 );

  printf( "STREAMER - Version " MSS_VERSION "          " MSS_COPYRIGHT " \n" );
  printf( "-------------------------------------------------------------------------------\n\n" );

  printf( "This program streams a digital audio MP3 or WAV file with the streaming API.\n\n" );

  if (argc < 2)
  {
    printf( "Enter name of file to play (use '" MSS_DIR_UP "media" MSS_DIR_SEP "Music.MP3', for example)\n\n"
            "To stream from an MSS library file, enter the name of the library followed by \n"
            "the name of the resource to play.\n\n: " );
    get_args( &argc, &argv );
  }

  //
  // Set the redist directory and start up MSS
  //

  AIL_set_redist_directory( MSS_DIR_UP_TWO MSS_REDIST_DIR_NAME );

  AIL_startup();

  //
  // Open a digital driver
  //

  dig = AIL_open_digital_driver( 44100, 16, MSS_MC_USE_SYSTEM_CONFIG, 0 );

  if ( dig == 0 )
  {
    printf( "%s", AIL_last_error() );
    exit( 1 );
  }

  //
  // Open the stream handle
  //
  // If two arguments supplied, assume the first is the library name and the second
  // the resource name, and convert it to a subfilename descriptor
  //

  if (argc == 2)
  {
    //
    // Filename to stream is specified directly
    // 

    filename = argv[1];
  }
  else
  {
    //
    // File to stream is inside a larger library file -- we must create a descriptor that 
    // allows AIL_open_stream() to access the embedded resource
    //
    // Note that the HSOUNDLIB doesn't have to remain open, since AIL_open_stream() will
    // access the library like any other file!
    //

#ifndef IS_DOS
    HMSOUNDBANK L = AIL_open_soundbank(argv[1], 0);
    S32 namelen = 0;
    MILESBANKSOUNDINFO info;

    if (L == NULL)
    {
      printf( "%s", AIL_last_error() );
      exit( 1 );
    }

    namelen = AIL_sound_asset_info(L, argv[2], 0, 0);
    if (namelen == 0)
    {
        printf("Sound not found.\n");
        AIL_close_soundbank(L);
        exit( 1 );
    }

    if (namelen >= 512)
    {
        printf("Name is too long(%d).", namelen);
        AIL_close_soundbank(L);
        exit( 1 );
    }

    AIL_sound_asset_info(L, argv[2], filename_buffer, &info);
    filename = filename_buffer;
    AIL_close_soundbank( L );
#endif
  }

  //
  // Open the stream using the default amount of buffer memory
  //

  stream = AIL_open_stream( dig, filename, 0 );

  if ( stream == 0 )
  {
    printf( "Could not stream from '%s': %s\n", argv[1] , AIL_last_error());

    AIL_shutdown();
    exit( 1 );
  }

  //
  // Loop the stream forever
  //

  AIL_set_stream_loop_count( stream, 0 );

  AIL_stream_info( stream, &rate, 0, &size, &mem );

  advance = size / 20; if ( advance < 500 ) advance = 500;

  printf( "Hit Enter to restart the stream.\n"
          "Hit Space to pause and restart the stream.\n"
          "Hit '<' and '>' to skip forwards and backwards in the stream.\n"
          "Hit '[' and ']' to change the playback rate, '\\' to return to normal.\n"
          "Hit ESC to exit.\n\n"
          "Playing a %i byte sound file with %i bytes of RAM.\n\n",
          size, mem );

  AIL_start_stream( stream );

  HS = AIL_stream_sample_handle( stream );

  for(;;)
  {
    //
    // Give other threads a time slice
    //  

    AIL_delay( 10 );  // sleep

    //
    // (You could process other application events here....)
    //


    //
    // Process keystrokes
    //

    while ( kbhit() )
    {
      switch ( getch() )
      {
        case 27:
          goto done;

        case ' ':
          AIL_pause_stream( stream, paused ^= 1 );
          break;

        case 13:
          paused=0;
          AIL_start_stream( stream );
          break;

        case '<':
        case ',':
          i = AIL_stream_position( stream );
          if ( i < advance )
            i = advance;
          AIL_set_stream_position( stream, i - advance );
          break;

        case '>':
        case '.':
          AIL_set_stream_position( stream, AIL_stream_position( stream ) + advance );
          break;

        case '[':
          i=AIL_sample_playback_rate( HS )-1000;
          if ( i < ( rate / 2 ) )
            i = rate / 2;
          AIL_set_sample_playback_rate( HS , i );
          break;

        case ']':
          i=AIL_sample_playback_rate( HS ) + 1000;
          if (i > ( rate * 2 ) )
            i = rate * 2;
          AIL_set_sample_playback_rate( HS, i );
          break;

        case '\\':
          AIL_set_sample_playback_rate( HS, rate );
          break;
      }
    }

    //
    // Show the percentage complete and service audio every 250 ms
    //

    if ( ( AIL_ms_count() - lasttime ) > 250 )
    {
      S32 per = (S32) ( ( (F32) AIL_stream_position( stream ) * 100.0f) / (F32) size );

      if ( per != last_per )
      {
        printf( "\rPercent played: %3i%%", per );
        last_per = per;
      }

      //
      // Service audio buffers
      //

      AIL_service_stream( stream, 0 );

      lasttime = AIL_ms_count();
    }

  }

done:

  //
  // Clean up
  //

  AIL_close_stream( stream );

  AIL_close_digital_driver( dig );

  AIL_shutdown();

  printf( "\n\nSTREAMER stopped.\n" );

  return( 0 );
}

