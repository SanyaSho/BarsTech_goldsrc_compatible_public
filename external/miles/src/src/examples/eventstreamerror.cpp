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
        printf("Usage: \"eventstreamerror <bankname>\"\n");
        return 0;
    }

#ifdef IS_STATIC
    Register_RIB(MP3Dec);
    Register_RIB(BinkADec);
    Register_RIB(OggDec);
#endif

    AIL_set_redist_directory( MSS_DIR_UP_TWO MSS_REDIST_DIR_NAME );
    AIL_startup();

    // Startup a stereo device to play out of.
    HDIGDRIVER hDriver = AIL_open_digital_driver(44100, 16, 2, 0);
    if (hDriver == 0)
    {
        printf("Couldn't open digital sound driver. (%s)\n", AIL_last_error());
        AIL_shutdown();
        return 0;
    }

    // Startup the event system using the overrides we just set.
    if (!AIL_startup_event_system(hDriver, 0, 0, 0))
    {
        printf("Couldn't init event system (%s).\n", AIL_last_error());
        AIL_close_digital_driver(hDriver);
        AIL_shutdown();
        return 0;
    }

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

        Counts First;
        memset(&First, 0, sizeof(Counts));

        // Iterate over the sounds
        HMSSENUM token = MSS_FIRST;
        MILESEVENTSOUNDINFO SoundInfo;
        while (AIL_enumerate_sound_instances(0, &token, 0, 0, 0, &SoundInfo))
        {
            First.Total++;

            if (SoundInfo.Status == MILESEVENT_SOUND_STATUS_PENDING) 
            {
                // If there is a user buffer of length 8, then this was enqueued with
                // a start offset, and we need to resume the stream.
                if (SoundInfo.UserBufferLen == 8)
                {
                    S32 StartOffset = ((S32*)SoundInfo.UserBuffer)[1];
                    AIL_set_sound_start_offset(token, StartOffset, 0);
                }

                First.Pending++;
            }

            if (SoundInfo.Status == MILESEVENT_SOUND_STATUS_PENDING && SoundInfo.Flags)
            {
                if (SoundInfo.Flags & MILESEVENT_SOUND_FLAG_PENDING_ASYNC) First.AsyncPending++;
                if (SoundInfo.Flags & MILESEVENT_SOUND_FLAG_WAITING_ASYNC) First.AsyncWait++;
            }

            if (SoundInfo.Status == MILESEVENT_SOUND_STATUS_COMPLETE &&
                SoundInfo.Stream &&
                SoundInfo.Stream->error)
            {
                // A playing stream has gone in to error state.

                // What we need to do is restart the stream - however if we just recreate the stream
                // and sample handle directly, we lose any adjustments the start sound event has made (delays,
                // presets, etc), so we'll need to restart it via event.
                //
                // Unfortunatly this means you have to track the event that started the sound. I've chosen to do
                // this via the userbuffer - on enqueue i pass in the index I used. I'll do the same here in
                // case this one also fails, but I'll send in an extra 4 bytes for the position to resume the
                // stream at.
                //
                S32 EventIndex = *(S32*)SoundInfo.UserBuffer;

                S32 SendBuffer[2] = {EventIndex, AIL_stream_position(SoundInfo.Stream)};
                AIL_enqueue_event(AIL_find_event(hBank, Events[EventIndex]), SendBuffer, 8, 0, 0);
            }
        }

        // Complete the queue.
        AIL_complete_event_queue_processing();

        S32 ProfileEnd = AIL_us_count();
        ProfileCounter ++;
        ProfileTimer += ProfileEnd - ProfileStart;
        if (ProfileEnd - ProfileStart > ProfileMax) ProfileMax = ProfileEnd - ProfileStart;


        printf("\r Playing - %d   Pending - %d   Async [%d/%d]", 
            First.Total,
            First.Pending,
            First.AsyncPending, First.AsyncWait
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
            else if (c == '?')
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
                    AIL_enqueue_event(AIL_find_event(hBank, Events[Index]), &Index, 4, 0, 0);
                }
            }
            else if (c >= 'a' && c <= 'z')
            {
                S32 Index = c - 'a' + 10;
                if (Events[Index])
                {
                    AIL_enqueue_event(AIL_find_event(hBank, Events[Index]), &Index, 4, 0, 0);
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