//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include "mss.h"
#include "con_util.i"

void AILCALL ErrorCallback(S64 i_Id, char const* i_Details)
{
    printf("\rError: %s\n", AIL_last_error());
    if (i_Details)
    {
        printf("Details: %s\n", i_Details);
    }
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int main(int argc, char** argv)
{
    set_up_console(0);

    if (argc < 2)
    {
        printf("Usage: \"eventtest <bankname> <event1> <event2> ...\"\n");
        return 0;
    }

#ifdef IS_STATIC
    Register_RIB(MP3Dec);
    Register_RIB(BinkADec);
    Register_RIB(OggDec);
#endif

    AIL_set_redist_directory( MSS_DIR_UP_TWO MSS_REDIST_DIR_NAME );
    AIL_startup();


    HDIGDRIVER hDriver = AIL_open_digital_driver(44100, 16, 2, 0);
    if (hDriver == 0)
    {
        printf("Couldn't open digital sound driver. (%s)\n", AIL_last_error());
        AIL_shutdown();
        return 0;
    }

    AIL_set_event_error_callback(ErrorCallback);

    if (AIL_startup_event_system(hDriver, 0, 0, 0) == 0)
    {
        printf("Couldn't init event system (%s).\n", AIL_last_error());
        AIL_close_digital_driver(hDriver);
        AIL_shutdown();
        return 0;
    }

    HMSOUNDBANK Banks[8];
    S32 BankCount = 0;
    char const* Events[36] = {0};
    S32 EventCount = 0;

    //
    // if we are in run mode, we are loading a sequence of banks, then a sequence
    // of events. Separate so that the basic usage is still straightforward and
    // not filled with switches.
    //
    S32 RunMode = 0;
    if (argv[1][0] == '-' &&
        argv[1][1] == 'r')
    {
        RunMode = 1;

        S32 OnBanks = 1;
        for (S32 i = 2; i < argc; i++)
        {
            if (argv[i][0] == '-' &&
                argv[i][1] == 'e')
            {
                OnBanks = 0;
                continue;
            }

            if (OnBanks)
            {
                if ((Banks[BankCount] = AIL_add_soundbank(argv[i], 0)) == 0)
                {
                    printf("Couldn't open soundbank: %s (%s)", argv[i], AIL_last_error());
                    return 0;
                }
                BankCount++;
            }
            else
            {
                AIL_enqueue_event_by_name(argv[i]);
            }
        }
    }
    else
    {
        // Open the soundbank provided.
        HMSOUNDBANK hBank = AIL_add_soundbank(argv[1], 0);
        if (hBank == 0)
        {
            printf("Couldn't open soundbank: %s (%s)\n", argv[1], AIL_last_error());
            AIL_close_digital_driver(hDriver);
            AIL_shutdown();
            return 0;
        }

        //
        // Print out the events.
        //
        HMSSENUM token = MSS_FIRST;
        while (AIL_enumerate_events(hBank, &token, 0, &Events[EventCount]))
        {
            if (EventCount <= 9)
            {
                printf("%d - %s\n", EventCount, Events[EventCount]);
            }
            else if (EventCount - 10 <= 25)
            {
                printf("%c - %s\n", 'a' + EventCount - 10, Events[EventCount]);
            }
            EventCount++;

            if (EventCount >= 36)
            {
                printf("[Too many events to show].\n");
                break;
            }
        }

        // Try to execute the events listed on the command line.
        for (S32 EventIndex = 2; EventIndex < argc; EventIndex++)
        {
            AIL_enqueue_event_by_name(argv[EventIndex]);
        }
    }

    // Loop until the events complete.
    S32 ProfileCounter = 0;
    S32 ProfileTimer = 0;
    S32 ProfileMax = 0;
    S32 CurrentlyPaused = 0;
    for (;;)
    {
        S32 ProfileStart = AIL_us_count();

        // The events are queued - process them. (this should happen once per frame)
        if (AIL_begin_event_queue_processing() == 0)
        {
            printf("Failed to process event queue: \"%s\"\n", AIL_last_error());
            AIL_shutdown_event_system();
            AIL_close_digital_driver(hDriver);
            AIL_shutdown();
            return 0;
        }

        // Iterate over the sounds
        S32 StartedCount = 0, CompletedCount = 0, TotalCount = 0;
        HMSSENUM token = MSS_FIRST;
        MILESEVENTSOUNDINFO SoundInfo;
        while (AIL_enumerate_sound_instances(0, &token, 0, 0, 0, &SoundInfo))
        {
            TotalCount++;

            if (SoundInfo.Status == MILESEVENT_SOUND_STATUS_PENDING) StartedCount++;
            else if (SoundInfo.Status == MILESEVENT_SOUND_STATUS_COMPLETE) 
            {
                // Find the sound instance in the bank.
                for (S32 i = 0; i < BankCount; i++)
                {
                    MILESBANKSOUNDINFO Info;
                    if (AIL_sound_asset_info(Banks[i], SoundInfo.UsedSound, 0, &Info))
                    {
                        printf("\nCompleted: %s (@%d x %d)\n", SoundInfo.UsedSound, Info.Rate, Info.ChannelCount);
                    }
                }
                CompletedCount++;
            }

            if (SoundInfo.Status == MILESEVENT_SOUND_STATUS_PENDING)
            {
                if (SoundInfo.Flags & MILESEVENT_SOUND_FLAG_FAILED_HITCH)
                    printf("\r [ERROR] Requested sound wasn't in memory!\n");
                if (SoundInfo.Flags & MILESEVENT_SOUND_FLAG_FAILED_ASYNC)
                    printf("\r [ERROR] Async load failed for sound!\n");
                if (SoundInfo.Flags & MILESEVENT_SOUND_FLAG_MISSING_SOUND)
                    printf("\r [ERROR] Can't find referenced sound!\n");
            }
        }

        // Complete the queue.
        AIL_complete_event_queue_processing();

        S32 ProfileEnd = AIL_us_count();
        ProfileCounter ++;
        ProfileTimer += ProfileEnd - ProfileStart;
        if (ProfileEnd - ProfileStart > ProfileMax) ProfileMax = ProfileEnd - ProfileStart;


        if (TotalCount == 0)
        {
            printf("\r No active sounds.");
            if (RunMode)
            {
                AIL_delay(1000); // Wait for the mixer to put out the remaining samples.
                return 1;
            }
        }
        else
        {
            printf("\r %d active sounds.", TotalCount);
        }


        while (kbhit())
        {
            int c = getch();
            if (c == ' ')
            {
                if (CurrentlyPaused)
                {
                    AIL_resume_sound_instances(0, 0);
                }
                else
                {
                    AIL_pause_sound_instances(0, 0);
                }
                CurrentlyPaused ^= 1;
            }
            else if (c == 27) // escape
            {
                AIL_stop_sound_instances(0, 0);
                AIL_begin_event_queue_processing();
                AIL_complete_event_queue_processing();
                AIL_clear_event_queue();
                goto done;
            }
            else if (c == 0xd) // return
            {
                AIL_enqueue_event_by_name(argv[argc-1]);
            }
            else if (c == 'i')
            {
                MILESEVENTSTATE State;
                AIL_event_system_state(0, &State);

                printf("CommandBufferSize: %d\n", State.CommandBufferSize);
                printf("LoadedBankCount: %d\n", State.LoadedBankCount);
                printf("LoadedSoundCount: %d\n", State.LoadedSoundCount);
                printf("PlayingSoundCount: %d\n", State.PlayingSoundCount);
                printf("HeapSize: %d\n", State.HeapSize);
                printf("HeapRemaining: %d\n", State.HeapRemaining);
                printf("SoundDataMemory: %d\n", State.SoundDataMemory);
                printf("SoundBankManagementMemory: %d\n", State.SoundBankManagementMemory);
                printf("PersistCount: %d\n", State.PersistCount);
                printf("================\n");
            }
            else if (c >= '0' && c <= '9')
            {
                S32 Index = c - '0';
                if (Events[Index])
                {
                    AIL_enqueue_event_by_name(Events[Index]);
                }
            }
            else if (c >= 'a' && c <= 'z')
            {
                S32 Index = c - 'a' + 10;
                if (Events[Index])
                {
                    AIL_enqueue_event_by_name(Events[Index]);
                }
            }
        }

        // Wait for a frame
        AIL_delay( 10 );
    } // end inf loop
   
   done: 
    printf("\n");
    printf("Event processing averaged %d us, max %d.\n", ProfileTimer / ProfileCounter, ProfileMax);

    AIL_shutdown_event_system();
    AIL_close_digital_driver(hDriver);
    AIL_shutdown();
    return 0;
}

/*! @cdep pre
    $DefaultsConsoleMSS
*/

/*! @cdep post
    $BuildConsoleMSS
*/