//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include "mss.h"
#include "con_util.i"

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

    HDIGDRIVER hDriverTwo = AIL_open_digital_driver(44100, 16, 2, 0);
    if (hDriverTwo == 0)
    {
        printf("Driver 2 failed to open. (%s)\n", AIL_last_error());
        AIL_close_digital_driver(hDriver);
        AIL_shutdown();
        return 0;
    }

    HEVENTSYSTEM FirstSystem = AIL_startup_event_system(hDriver, 0, 0, 0);
    if (FirstSystem == 0)
    {
        printf("Couldn't init event system (%s).\n", AIL_last_error());
        AIL_close_digital_driver(hDriver);
        AIL_shutdown();
        return 0;
    }

    HEVENTSYSTEM SecondSystem = AIL_add_event_system(hDriverTwo);

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
    char const* Events[36] = {0};
    S32 EventCount = 0;
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
            printf("[Too many events to show.]\n");
            break;
        }
    }

    // Try to execute the events listed on the command line.
    for (S32 EventIndex = 2; EventIndex < argc; EventIndex++)
    {
        const U8* pEvent = AIL_find_event(hBank, argv[EventIndex]);
        if (pEvent == 0)
        {
            printf("Couldn't find event: %s\n", argv[EventIndex]);
            continue;
        }

        AIL_enqueue_event_system(FirstSystem, pEvent, 0, 0, 0, 0);
        AIL_enqueue_event_system(SecondSystem, pEvent, 0, 0, 0, 0);
    }

    HEVENTSYSTEM EnqueueSystem = FirstSystem;

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

        struct Counts
        {
            S32 Pending, Total;
            S32 AsyncWait, AsyncPending;
        };

        Counts First, Second;
        memset(&First, 0, sizeof(Counts));
        memset(&Second, 0, sizeof(Counts));

        // Iterate over the sounds
        HMSSENUM token = MSS_FIRST;
        MILESEVENTSOUNDINFO SoundInfo;
        while (AIL_enumerate_sound_instances(FirstSystem, &token, 0, 0, 0, &SoundInfo))
        {
            First.Total++;

            if (SoundInfo.Status == MILESEVENT_SOUND_STATUS_PENDING) First.Pending++;

            if (SoundInfo.Status == MILESEVENT_SOUND_STATUS_PENDING && SoundInfo.Flags)
            {
                if (SoundInfo.Flags & MILESEVENT_SOUND_FLAG_PENDING_ASYNC) First.AsyncPending++;
                if (SoundInfo.Flags & MILESEVENT_SOUND_FLAG_WAITING_ASYNC) First.AsyncWait++;
            }
        }

        token = MSS_FIRST;
        while (AIL_enumerate_sound_instances(SecondSystem, &token, 0, 0, 0, &SoundInfo))
        {
            Second.Total++;

            if (SoundInfo.Status == MILESEVENT_SOUND_STATUS_PENDING) Second.Pending++;

            if (SoundInfo.Status == MILESEVENT_SOUND_STATUS_PENDING && SoundInfo.Flags)
            {
                if (SoundInfo.Flags & MILESEVENT_SOUND_FLAG_PENDING_ASYNC) Second.AsyncPending++;
                if (SoundInfo.Flags & MILESEVENT_SOUND_FLAG_WAITING_ASYNC) Second.AsyncWait++;
            }
        }


        // Complete the queue.
        AIL_complete_event_queue_processing();

        S32 ProfileEnd = AIL_us_count();
        ProfileCounter ++;
        ProfileTimer += ProfileEnd - ProfileStart;
        if (ProfileEnd - ProfileStart > ProfileMax) ProfileMax = ProfileEnd - ProfileStart;


        printf("\r Playing - %d/%d   Pending - %d/%d   Async [%d/%d] [%d/%d]", 
            First.Total, Second.Total,
            First.Pending, Second.Pending,
            First.AsyncPending, First.AsyncWait,
            Second.AsyncPending, Second.AsyncWait
            );

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
                AIL_enqueue_event_system(EnqueueSystem, AIL_find_event(hBank, argv[argc-1]), 0, 0, 0, 0);
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
                    AIL_enqueue_event_system(EnqueueSystem, AIL_find_event(hBank, Events[Index]), 0, 0, 0, 0);
                }
            }
            else if (c >= 'a' && c <= 'z')
            {
                S32 Index = c - 'a' + 10;
                if (Events[Index])
                {
                    AIL_enqueue_event_system(EnqueueSystem, AIL_find_event(hBank, Events[Index]), 0, 0, 0, 0);
                }
            }
            else if (c == '<') EnqueueSystem = FirstSystem;
            else if (c == '>') EnqueueSystem = SecondSystem;
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