#ifndef __RAD_INCLUDE_MP3_DIRECT_H__
#define __RAD_INCLUDE_MP3_DIRECT_H__

#include "rrCore.h"

//
// lightweight interface for accessing outside of Miles
//

typedef struct
{
   S32 rate;
   S32 channels;
} mp3_info;

typedef void * (RADLINK MEM_ALLOC_TYPE)( UINTa size, UINTa user, char const * filename, U32 line ); // allocation must be 16-byte aligned
typedef void (RADLINK MEM_FREE_TYPE)( void * ptr, UINTa user, char const * filename, U32 line );

typedef S32 (RADLINK MILES_MP3_FETCH) (UINTa     user,            // User value passed to ASI_open_stream()
                                       void      *des,            // Location to which stream data should be copied by app
                                       S32       bytes_requested, // # of bytes requested by ASI codec
                                       S32       offset);         // If not -1, application should seek to this point in stream

RADEXPFUNC void   RADEXPLINK miles_mp3_init(void); // safe to call multiple times
RADEXPFUNC void * RADEXPLINK miles_mp3_open( MEM_ALLOC_TYPE * palloc, MEM_FREE_TYPE  * pfree, void *info, MILES_MP3_FETCH *getdata, S32 datalen);
RADEXPFUNC void   RADEXPLINK miles_mp3_close(void *decoder);
RADEXPFUNC S32    RADEXPLINK miles_mp3_getdata(void *ptr, S16 *samples, S32 num_samples, mp3_info *info);
// returns number of bytes of audio data produced


//
// interface for accessing from Iggy, slightly more indirect
//

struct IggyMP3Interface
{
   S32 struct_size;

   // mp3-to-iggy calls
   void * (RADLINK *iggy_malloc)(UINTa user, S32 len);
   void   (RADLINK *iggy_free)(UINTa user, void *ptr);

   // iggy-to-mp3 calls
   void* (RADLINK *open   )(void *info, MILES_MP3_FETCH *getdata, S32 datalen);
   void  (RADLINK *close  )(void *decoder);
   S32   (RADLINK *getdata)(void *ptr, S16 *samples, S32 num_samples, mp3_info *info);
};

#ifndef __RAD_DEFINE_IGGYMP3__
#define __RAD_DEFINE_IGGYMP3__
typedef struct IggyMP3Interface IggyMP3Interface;
typedef rrbool IggyGetMP3Decoder(IggyMP3Interface *decoder);
#endif

RADEXPFUNC IggyGetMP3Decoder* RADEXPLINK IggyAudioGetMP3Decoder(void);

#endif
