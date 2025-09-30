#include <stdio.h>

#include "mss.h"

#define MPDEC
#define SRC_TABLES
#include "mp3dec.h"
#include "datatbl.h"


static U32 get_string_bits( HTABLE const * cur )
{
  S32 v = 0;
  char * s = cur->string;

  while (*s)
  {
    v <<= 1;
    if ( ( *s - '0' ) )//== 0 ) //invert
      v |= 1;
    ++s;
  }
  return v;
}


typedef struct to_huffman
{
  U16 num_of_each_len[ 32 ];
  S32 num;
  S32 min_len, max_len;
  U8  symbols[ 256 ];
  U8  lens[ 256 ];
  S32 codes[ 256 ];
} to_huffman;


static void sort_huff( to_huffman * th, to_huffman * tmp )
{
  S32 a;

  U8 llen = 0;
  S32 lcode = 0;

  for ( a = 0; a < th->num ; a++ )
  {
    S32 i, besti;

    U8 slen = 33;
    S32 scode = 0xffffff;

    besti = -1;

    for( i = 0; i < th->num ; i++ )
    {
      if ( ( th->lens[ i ] > llen ) || ( ( th->lens[ i ] == llen ) && ( th->codes[ i ] > lcode ) ) )
      {
        if ( ( th->lens[ i ] < slen ) || ( ( th->lens[ i ] == slen ) && ( th->codes[ i ] < scode ) ) )
        {
          slen = th->lens[ i ];
          scode = th->codes[ i ];
          besti = i;
        }
      }
    }
    
    tmp->symbols[ a ] = th->symbols[ besti ];
    tmp->lens[ a ] = th->lens[ besti ];
    tmp->codes[ a ] = th->codes[ besti ];
    llen = th->lens[ besti ];
    lcode = th->codes[ besti ];
  }

  memcpy( th->symbols, tmp->symbols, sizeof( th->symbols ) );
  memcpy( th->lens, tmp->lens, sizeof( th->lens ) );
  memcpy( th->codes, tmp->codes, sizeof( th->codes ) );
}


static void bit_to_string( char * out, S32 len, S32 code )
{
  S32 i, b;
  
  b = 1 << ( len - 1 );

  for( i = 0 ; i < len ; i++ )
  {
    *out++ = ( code & b ) ? '1' : '0';
    b >>= 1;
  }

  *out = 0;
}


static void print_huff( to_huffman * th )
{
  S32 a;
  F32 t;
  
  t = 0.0f;
  for ( a = 0 ; a < th->num ; a++ )
  {
    float s;
    char buf[ 32 ];
    bit_to_string( buf, th->lens[ a ], th->codes[ a ] );
    s = 1.0f / ( float )( 1 << th->lens[ a ] );
    t += s;
    printf( "%2dx%2d, %2d, %s %0.2f %0.2f\n", th->symbols[ a ] >> 4, th->symbols[ a ] & 15, th->lens[ a ], buf, s, t );
  }
}


static int get_huff( U16 * counts, void * bitsp, U32 * bits_used )
{
  U16 * c = counts + 1;
  U32 code = 0;
  U32 index = 0;
  U32 used = 0;
  U32 bits = *(U32*)bitsp;

  code = ( bits & 1 );
  bits >>= 1;
  ++used;

  for(;;)
  {
    U32 n = *c;

    if ( code < n )
      break;

    code -= n;
    index += n;
    code = code + code + ( bits & 1 );
    bits >>= 1;
    ++used;
    ++c;
  }

  *bits_used = used;
  return index + code;
}


static int is_symb( to_huffman * th, U32 bits, U32 bit_value, U32 * used, U32 * symb )
{
  S32 i;
  
  for( i = 0 ; i < th->num ; i++ )
  {
    if ( th->lens[ i ] <= bits )
    {
//      U32 c = th->codes[ i ] ^ ( ( 1 << th->lens[ i ] ) - 1 );
      if ( th->codes[ i ] == ( bit_value >> ( bits - th->lens[ i ] ) ) )
      {
        *used = th->lens[ i ];
        *symb = th->symbols[ i ];
        return 1;
      }
    }
  }
  return 0;
}

U8 dtables[ 16384 ];
U8 all[1024+1024]={0};


int ttbl=0;

static U8 * make_1_table( U8 * hdr, U8 * tbl, to_huffman * th, U32 pre_bits, U32 bit_value, S32 offset_can_be_16 )
{
  U8 * d;
  U32 diff;
  S32 i;
  
  diff = tbl - hdr;

  if ( offset_can_be_16 )
  {
    if ( diff > 0x3fff ) __asm int 3;
    if (((UINTa)hdr)&1) __asm int 3;
    *(U16*)hdr = diff << 4;
    ++hdr;
  }
  else
  {
    if ( diff > 0x3f ) __asm int 3;
    *hdr = (U8) diff;
  }

  d = tbl + 2;

  for( i = 1 ; i >= 0 ; i-- )
  {
    U32 used, symb;
    U32 bv = ( bit_value << 1 ) +i;
    
    if ( is_symb( th, pre_bits + 1, bv, &used, &symb ) )
    {
      *hdr |= ( 0x80 >> i );
      tbl[ i ] = symb;
    }
    else
    {
      d = make_1_table( tbl + i, d, th, pre_bits + 1, bv, 0 );
    }
  }
  return d;
}

static U8 * make_d_table( U8 * hdr, U8 * tbl, to_huffman * th, U32 this_bits )
{
  U8 * d;
  U32 i;
  U32 cnt = 1 << this_bits; ///lookup
  
  *(U16*)hdr = ( this_bits << 12 ) | ( ( tbl - hdr ) / 2 );
  if ( ( tbl - hdr ) & 1 ) __asm int 3;
  d = tbl + ( cnt * 2 );
  
  for( i = 0 ; i < cnt ; i++ )
  {
    U32 used, symb;

    if ( is_symb( th, this_bits, i, &used, &symb ) )
    {
      tbl[ i * 2 ] = used;
      tbl[ i * 2 + 1 ] = symb;
    }
    else
    {
      d = make_1_table( tbl + i * 2, d, th, this_bits, i, 1 );
    }
  }
  return d;
}

void main( int argc, char ** argv )
{
  S32 tbl;
  to_huffman th[ 34 ];

  U8 * d = dtables + 33 * 2;
  for (tbl=1; tbl < 34; tbl++)
  {
    if ( h_tab[tbl] == h_tab[tbl-1] )
    {
      printf( "table %d is a dup\n", tbl );
      ((U16*)( dtables + tbl * 2 - 2 ))[ 0 ] = ((U16*)( dtables + tbl * 2 - 2 ))[ -1 ] - 1;
      continue;
    }

    memset( th + tbl, 0, sizeof( to_huffman) );

    th[ tbl ].min_len = 33;
    const HTABLE *src = h_tab[tbl];
    for (;src->x != 100; src++)
    {
      ++th[tbl].num_of_each_len[ src->len ];
      if ( src->len < th[tbl].min_len )
        th[tbl].min_len = src->len;
      if ( src->len > th[tbl].max_len )
        th[tbl].max_len = src->len;

      th[tbl].codes[th[tbl].num] = get_string_bits( src );
      th[tbl].symbols[th[tbl].num] = ( src->y << 4 ) + src->x;
      th[tbl].lens[th[tbl].num] = src->len;
      ++th[tbl].num;
    }

    sort_huff( th + tbl, th );

    printf( "\ntable: %d (syms: %d)\n", tbl, th[tbl].num );
    print_huff( th + tbl );

    U8 * oldd = d;
    ttbl=tbl;
    if ( ( tbl>=32 ) && ( th[ tbl ].max_len > 6 ) )
    __asm int 3;
    
    d = make_d_table( dtables + tbl * 2 - 2, d, th+tbl, (tbl>=32)? 6 : ( ( th[ tbl ].max_len >= 8 ) ? 8 : 5 ) );
    printf("dtbl %d: %d (%d)\n",tbl,d-oldd,th[tbl].max_len);
    
  }

  printf( "total dtable: %d\n", d - dtables );

  printf( "\n\nstatic U8 mp3_htbls[] =\n{\n  " );
  for( S32 i = 0 ; i < ( d - dtables ) ; i++ )
  {
    if ( ( i % 15 ) == 0 )
      printf( "\n  " );
    printf( "%d, ", dtables[ i ] );
  }
  printf( "\n};\n\n" );


#if PRINT_CANONICAL


  printf( "\n\nstatic U8 tbls[] =\n{\n" );
  
  printf( "  " );
  for( tbl = 1 ; tbl < 34 ; tbl++ )
  {
    printf("%d, ", th[tbl].max_len );
    if ( tbl == 16 ) printf( "\n  " );
  }
  printf( "\n" );
  
  U32 ofs = ( ( 33 + 33 ) + 15 ) &~15;
  printf( "  " );
  for( tbl = 1 ; tbl < 34 ; tbl++ )
  {
    printf("%d, ", ofs / 16 );
    if ( tbl == 17 ) printf( "\n  " );

    if ( h_tab[tbl] != h_tab[tbl-1] )
    {
      ofs += th[ tbl ].max_len;
      ofs += th[ tbl ].num;
      ofs = ( ofs + 15 ) & ~15;
    }
  }
  printf( "\n  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\n" );

  for( tbl = 1 ; tbl < 34 ; tbl++ )
  {
    if ( h_tab[tbl] != h_tab[tbl-1] )
    {
      printf( "  // table %d, %d symbols, max bit_len %d\n", tbl, th[ tbl ].num, th[ tbl ].max_len );
      printf( "  " );
      for( S32 i = 1 ; i <= th[ tbl ].max_len ; i++ )
        printf( "%d, ", th[ tbl ].num_of_each_len[ i ] );
      printf( "\n" );
      
      printf( "  " );
      for( S32 i = 0 ; i < th[ tbl ].num ; i++ )
      {
        printf( "%d, ", th[ tbl ].symbols[ i ] );
        if ( ( ( i + 1 ) % 15 ) == 0 )
          printf( "\n  " );
      }
      printf( "\n" );

      printf( "  " );
      for( S32 i = 0 ; i < ( ( 16 - ( th[ tbl ].num + th[ tbl ].max_len ) ) & 15 ) ; i++ )
      {
        printf( "0, " );
      }
      printf( "\n" );
      
      
    }  
  }

  printf( "};\n\n" );
  
  #endif
}
