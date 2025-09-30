//
// Side Chain Compression Example
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "mss.h"

#include "con_util.i"

// @cdep pre $DefaultsConsoleMSS
// @cdep post $BuildConsoleMSS

int MSS_MAIN_DEF main( int argc, char *argv[] )
{
    HDIGDRIVER dig;             // Handle to digital audio driver

    set_up_console( 1 );

    printf( "SideChain - Version " MSS_VERSION "          " MSS_COPYRIGHT " \n" );
    printf( "-------------------------------------------------------------------------------\n\n" );

    printf( "This program demonstrates how to set up side chain compression.\n\n" );

    // set the redist directory and startup the system

#ifdef IS_STATIC 
    Register_RIB(MP3Dec);
#endif

    AIL_set_redist_directory( MSS_DIR_UP_TWO MSS_REDIST_DIR_NAME );

    AIL_startup();

    // Allocate output driver.
    dig = AIL_open_digital_driver( 48000, 16, MSS_MC_STEREO, 0 );
    if (dig==0)
    {
        printf( "%s", AIL_last_error() );

        AIL_shutdown();
        exit( 1 );
    }

    //
    // Allocate two samples - a music and a dialog that will duck the music.
    //
    HSAMPLE MusicSample = AIL_allocate_sample_handle(dig);
    HSAMPLE DialogSample = AIL_allocate_sample_handle(dig);

    // Load source
    U32* MusicSource = (U32*)AIL_file_read(MSS_DIR_UP "media" MSS_DIR_SEP "music.mp3", FILE_READ_WITH_SIZE);
    U32* DialogSource = (U32*)AIL_file_read(MSS_DIR_UP "media" MSS_DIR_SEP "ogres.mp3", FILE_READ_WITH_SIZE);

    // Set source.
    AIL_set_named_sample_file( MusicSample, ".mp3", MusicSource + 1, MusicSource[0], 0);
    AIL_set_named_sample_file( DialogSample, ".mp3", DialogSource + 1, DialogSource[0], 0);

    //
    // Set up the bus - first allocate a new bus that will serve as the input to the side chain.
    //
    AIL_allocate_bus(dig);

    // Tell the dialog sample to play on the second bus.
    AIL_set_sample_bus(DialogSample, 1);

    // Install a compressor on the 0th bus (main bus), on the first filter stage (SP_FILTER_0), using bus 1 as the input for the compressor.
    AIL_install_bus_compressor(dig, 0, SP_FILTER_0, 1);

    // Set up a variety of parameters on the compressor.
    float f = -40;
    AIL_sample_stage_property(AIL_bus_sample_handle(dig, 0), SP_FILTER_0, "Threshold", -1, 0, &f, 0);
    f = 2;
    AIL_sample_stage_property(AIL_bus_sample_handle(dig, 0), SP_FILTER_0, "Ratio", -1, 0, &f, 0);
    f = 100;
    AIL_sample_stage_property(AIL_bus_sample_handle(dig, 0), SP_FILTER_0, "Attack", -1, 0, &f, 0);
    f = 1000;
    AIL_sample_stage_property(AIL_bus_sample_handle(dig, 0), SP_FILTER_0, "Release", -1, 0, &f, 0);

    // Start the music.
    AIL_start_sample(MusicSample);

    S32 State = 0;
    U32 StartTime = AIL_ms_count();
    for (;;)
    {
        // After 2 seconds, start playing the dialog.
        if (!State && AIL_ms_count() - StartTime > 2000)
        {
            State = 1;
            AIL_start_sample(DialogSample);
        }

        // Once the dialog is done, start waiting.
        if (State == 1 && AIL_sample_status(DialogSample) == SMP_DONE)
        {
            State = 2;
            StartTime = AIL_ms_count();
        }

        // 2 seconds after the dialog is done, exit.
        if (State == 2 && (AIL_ms_count() - StartTime > 2000))
            break;

    }
    
    AIL_shutdown();
    return 0;
}

