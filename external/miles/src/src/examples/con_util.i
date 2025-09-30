#if defined(IS_MACHO) || defined(IS_LINUX)

#include <termios.h>
#include <unistd.h>

static struct termios initial_settings, new_settings;
static int peek_character =  -1;

int kbhit();
int kbhit()
{
  unsigned char ch;
  int nread;
  if  (peek_character != -1) return 1;
  
  new_settings.c_cc[VMIN]=0;
  tcsetattr(0,  TCSANOW, &new_settings);
  nread =  read(0,&ch,1);
  new_settings.c_cc[VMIN]=1;
  tcsetattr(0,  TCSANOW, &new_settings);
  if(nread ==  1)
  {
    peek_character = ch;
    return 1;
  }
  return  0;
}

int getch();
int getch()
{
  unsigned char ch;
  int i;
  if(peek_character != -1)
  {
    ch = (unsigned char)peek_character;
    peek_character = -1;
    return ch;
  }
  i=read(0,&ch,1);
  if (i!=1) ch = 0;
  return ch;
}

char * ourgets( char * buf );
char * ourgets( char * buf ) // oh man, this routine is awesome
{
  char * b = buf;
  int pos = 0;
  while(1)
  {
    int ch = getch();
    int bs;
    switch ( ch )
    {
      case 27:
        bs = pos;
        if ( pos == 0 )
        {
          buf=0;
      case 13:
          printf("\n");
          break;
      case 8:
      case 127:
          if (pos==0)
            continue;
          bs=1;  
        }
        do
        {
          --b;
          --pos;
          printf("\b \b");
        } while (--bs);
        continue;

      default:
        printf("%c",ch);
        *b++=(char)ch;
        ++pos;
        continue;
    }
    break;
  }
  *b=0;
  return( buf );
}

#define gets ourgets

static void set_up_console( int autoclose )
{
  autoclose = autoclose;

  setbuf( stdout, 0 );
  
  tcgetattr(0,&initial_settings);
  new_settings =  initial_settings;
  new_settings.c_lflag &= ~ICANON;
  new_settings.c_lflag &= ~ECHO;
  new_settings.c_iflag &= ~ICRNL;
  new_settings.c_cc[VMIN] = 1;
  new_settings.c_cc[VTIME] = 0;
  tcsetattr(0,  TCSANOW, &new_settings);
}

#else

#include <conio.h>

static void set_up_console( int autoclose )
{
  autoclose = autoclose;

  #ifdef __WATCOMC__
     setbuf(stdout, NULL);
  #endif
}

#if !defined(__WATCOMC__)

#define getch _getch
#define kbhit _kbhit

#endif

#endif

//
//  Read a set of arguments from a user inputted string.
//

void get_args( int* argc, char*** argv );
void get_args( int* argc, char*** argv )
{
  static char filename[260];
  static char* filenamev[16];
  char *c;
  int i;
  
  gets( filename );
   
  filenamev[0] = (*argc) ? *argv[0] : (char*)"con_util";
  filenamev[1] = filename;
  
  c = filename;
  i = 1;
  while ( *c )
  {
    filenamev[ i++ ] = c;
    while ( ( *c ) > 32 )
      c++;

    *c++ = 0;
    
    while ( ( *c ) && ( ( *c ) <= 32 ) )
      c++;
  }
  
  *argc = i;
  *argv = filenamev;
}
