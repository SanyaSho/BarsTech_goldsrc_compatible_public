//############################################################################
//##                                                                        ##
//##  SSTEST.C                                                              ##
//##                                                                        ##
//##  Digital audio mixer test facility                                     ##
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
#include <ctype.h>

#include "mss.h"

#include "con_util.i"

#if defined(IS_MAC) || defined(IS_LINUX)
#define _stricmp strcasecmp
#endif

#if defined(IS_WIN32) && !defined(IS_WIN64) && !defined(NTELEMETRY)
// uncomment the next line to turn on TmLite profiling!
//#define USE_TMLITE 
#endif

#ifdef USE_TMLITE 
  #include "tmlite.h"
#endif

// @cdep pre $DefaultsConsoleMSS
// @cdep pre $ requiresbinary($clipfilename($file)/../../../../telemetry/ship/lite/win/redist/lib/tmlite32.link.lib)
// @cdep post $BuildConsoleMSS

#define N_FILES  8         // # of .RAW file samples to load and play
#define N_VOICES 64        // # of mixer channels to allocate

char const *filenames[N_FILES] =   // data files to load
{
   "welcome.wav"  ,
   "horn.wav"     ,
   "charge.wav"   ,
   "ogres.mp3"    ,
   "retreat.ogg"  ,
   "crow.wav"     ,
   "glass.wav"    ,
   "cat.binka"
};


//############################################################################
//##                                                                        ##
//## Digital playback example program                                       ##
//##                                                                        ##
//############################################################################

int MSS_MAIN_DEF main( int argc, char *argv[] )
{
  HDIGDRIVER dig;             // Handle to digital audio driver
  U32       *snd[N_FILES];    // Pointer to beginning of audio data for each .RAW
  HSAMPLE    smp[N_VOICES];   // Sample handles for each mixer channel
  S32        num_voices;      // Actual number of mixer channels available

  char       ch;
  S32        i,n;
  U32        lastupdate;
  U32        rate;
  S32        bits, chans;
  F32        vol, pan;

  set_up_console( 1 );

  printf( "SSTEST - Version " MSS_VERSION "            " MSS_COPYRIGHT " \n" );
  printf( "-------------------------------------------------------------------------------\n\n" );

  printf( "This program plays multiple sound files.\n\n" );

  if ((argc > 1) && (!isdigit( argv[1][0] )))
  {
    printf( "Usage: SSTEST [Frequency] [8 or 16 bit] [1 or 2 channels] \n" );
    exit( 1 );
  }


  //
  // Try to init tmLite
  //  

  #ifdef USE_TMLITE
  {
    U8 * tmbuffer;
    HTMLITE context;
    U32 tmframe = 0;

    tmLiteLoad();
    tmbuffer = AIL_mem_alloc_lock( 2 * 1024 * 1024 );
    context = tmLiteOpen( "127.0.0.1", 9180, tmbuffer, 2 * 1024 * 1024 );
    MilesUseTmLite( context );
    AIL_configure_logging( 0, 0, 1 );
  #endif

  //
  // set the redist directory and startup the system
  //

  #ifdef IS_STATIC 
    Register_RIB(MP3Dec);
  #endif

  AIL_set_redist_directory( MSS_DIR_UP_TWO MSS_REDIST_DIR_NAME );

  AIL_startup();

  //
  // get the initialization parameters
  //

  rate  = 44100;
  bits  = 16;
  chans = 2;

  if (argc > 1)
    rate = (U32) atoi( argv[1] );

  if (argc > 2)
    bits = atoi( argv[2] );

  if (argc > 3)
    chans = atoi( argv[3]);

  //
  // Initialize digital sound system
  //

  dig = AIL_open_digital_driver( rate, bits, chans, 0 );
  if (dig==0)
  {
    printf( "%s", AIL_last_error() );

    AIL_shutdown();
    exit( 1 );
  }

  //
  // Allocate 16 digital mixer channels, or as many as available
  //

  for (i=0; i < N_VOICES; i++)
  {
    smp[i] = AIL_allocate_sample_handle( dig );

    if (smp[i] == NULL)
    {
      break;
    }
  }

  num_voices = i;

  //
  // Load samples and show help screen
  //

  for (i=0; i < N_FILES; i++)
  {
    char filename[260];
    S32 has_decoder = 1; // default to true (for wavs)

    //
    // load each file image into memory
    //

    strcpy( filename, MSS_DIR_UP "media" MSS_DIR_SEP );
    strcat( filename, filenames[i] );

    snd[i] = (U32*) AIL_file_read( filename, FILE_READ_WITH_SIZE );

    //
    // Try to find the ASI provider for this if it isn't a wav.
    //
    {
        S32 NeedsASI = 1;
        if (strlen(filename) >= 4)
        {
            if (_stricmp(&filename[strlen(filename)-4], ".WAV") == 0)
            {
                NeedsASI = 0;
            }
        }

        if (NeedsASI)
        {
            // Ensure the provider exists.
            HPROVIDER hProvider = RIB_find_file_provider("ASI codec", "Input file types", filename);
            if (hProvider == 0)
            {
                // Nope!
                has_decoder = 0;
            }
        }
    }

    printf( "    %d: %s (%s)\n", i+1, filenames[i], snd[i] ? has_decoder ? "valid" : "no decoder" : "null" );
  }



  printf( "\n");
  printf( " + - : Adjust the volume of current sample\n" );
  printf( " [ ] : Adjust the pan position of current sample\n" );
  printf( " < > : Adjust the pitch of current sample\n\n" );

  printf( "ENTER: Loop current sample\n" );
  printf( "SPACE: Stop all samples\n" );
  printf( " ESC : Quit\n\n" );

  //
  // Fetch and interpret keypresses
  //

  i = -1;

  lastupdate=0;

  for(;;)
  {
    U32 time;

    time = AIL_ms_count();

    //
    // tick tmlite
    //

    #ifdef USE_TMLITE
      if ( ( tmframe == 0 ) || ( ( time - tmframe ) >= 33 ) )
      { 
        tmLiteTick( context ); 
        tmframe = time;
      }
    #endif
  
  
    //
    // Give other threads a time slice
    //

    AIL_sleep( 10 );  // wait

    if ( ( time - lastupdate ) > 1000 )
    {
      //
      // only update the CPU percentage every second
      //

      printf( "\r  CPU: %i %% (%i samples playing)  ", AIL_digital_CPU_percent( dig ), AIL_active_sample_count(dig) );
      lastupdate = time;
    }

    while (kbhit())
    {
      ch = (char) getch();

      switch (ch)
      {
        //
        // Start playback of sample
        //
        case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8':

          //
          // Get file # and find an HSAMPLE to play it
          //
          // If no samples available, ignore request
          //

          n = ch-'1';

          for (i=0; i < num_voices; i++)
          {
            if ( AIL_sample_status( smp[i] ) == SMP_DONE)
            {
              break;
            }
          }

          if ( ( i == num_voices ) || ( snd[ n ] == 0 ) )
          {
            i = -1;
            break;
          }

          //
          // point the sample at the file image (the size is in the first dword)
          //

          if ( AIL_set_named_sample_file( smp[i],
                                          filenames[n],
                                          snd[n]+1,
                                          snd[n][0],
                                          0 ) == 0)
          {
              // If it isn't recognized, bail.
              i = -1;
              break;
          }

          //
          // Finally, activate sample
          //

          n = AIL_sample_playback_rate( smp[i] );
          AIL_start_sample( smp[i] );
          break;

        //
        // Stop all samples
        //
        case ' ':
          for (n=0; n < num_voices; n++)
          {
            AIL_end_sample( smp[n] );
          }
          break;

        //
        // Loop sample indefinitely
        //
        case 13:
          if (i != -1)
          {
            AIL_set_sample_loop_count( smp[i], 0 );
          }
          break;

        //
        // Pitch-shift sample by 1 kHz increments
        //

        case '.': case '>':
          if (i != -1)
          {
            //
            // get the playback rate and then speed it up
            //

            n = AIL_sample_playback_rate( smp[i] );

            AIL_set_sample_playback_rate( smp[i], n + 1000 );
          }
          break;

        case ',': case '<':
          if (i != -1)
          {
            //
            // get the playback rate and then slow it down
            //

            n = AIL_sample_playback_rate( smp[i] );

            AIL_set_sample_playback_rate( smp[i], n - 1000 );
          }
          break; 

        //
        // Change volume/pan of sample
        //

        case '+': case '=':
          //
          // get the volume and then turn it up
          //

          AIL_sample_volume_pan( smp[i], &vol, &pan );

          vol *= 1.05f;
          if (vol > 1.0f)
            vol = 1.0f;

          AIL_set_sample_volume_pan( smp[i], vol, pan );
          break;

        case '-': case '_':
          //
          // get the volume and then turn it down
          //

          AIL_sample_volume_pan( smp[i], &vol, &pan );
          vol /= 1.05f;
          if (vol < 0.01f)
            vol = 0.01f;

          AIL_set_sample_volume_pan( smp[i], vol, pan );
          break;

        //
        // Pan left/right
        //

        case ']': case '}':
          //
          // get the pan and then shift it right
          //

          AIL_sample_volume_pan( smp[i], &vol, &pan );
          pan *= 1.05f;
          if (pan > 1.0f)
            pan = 1.0f;

          AIL_set_sample_volume_pan( smp[i], vol, pan );
          break;

        case '[': case '{':
          //
          // get the pan and then shift it left
          //

          AIL_sample_volume_pan( smp[i], &vol, &pan );
          pan /= 1.05f;
          if (pan < 0.01f)
            pan = 0.01f;

          AIL_set_sample_volume_pan( smp[i], vol, pan );
          break;

        //
        // exit on escape
        //

        case 27:
          goto done;
      }
    }
  }

 done:

  //
  // Shut down AIL system and exit
  //

  AIL_shutdown();

  #ifdef USE_TMLITE
    if ( context )
    {
      tmLiteClose( context );
      context = 0;
      MilesUseTmLite( 0 );
    }
    if ( tmbuffer)
    {
      AIL_mem_free_lock( tmbuffer );
      tmbuffer = 0;
    }
  }
  #endif    

  printf( "\n\nSSTEST stopped.\n" );
  return( 0 );
}

