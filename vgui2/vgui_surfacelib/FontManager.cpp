#include <cwctype>
#include <clocale>

#include <tier0/platform.h>

#include "FontManager.h"
//#include "Win32Font.h"
#include "vguifont.h"

#undef CreateFont

#ifdef WIN32
extern bool s_bSupportsUnicode;
#endif

#if !defined( _X360 )
#define MAX_INITIAL_FONTS	100
#else
#define MAX_INITIAL_FONTS	1
#endif

static CFontManager s_FontManager;

struct Win98ForeignFallbackFont_t
{
	const char* language;
	const char* fallbackFont;
};

Win98ForeignFallbackFont_t g_Win98ForeignFallbackFonts[] = 
{
	{ "russian", "system" },
	{ "japanese", "win98japanese" },
	{ "thai", "system" },
	{ nullptr, "Tahoma" }
};

struct FallbackFont_t
{
	const char* font;
	const char* fallbackFont;
};

#ifdef WIN32
const char *g_szValidAsianFonts[] = { "Marlett", NULL };
// list of how fonts fallback
FallbackFont_t g_FallbackFonts[] =
{
	{ "Times New Roman", "Courier New" },
	{ "Courier New", "Courier" },
	{ "Verdana", "Arial" },
	{ "Trebuchet MS", "Arial" },
	{ "Tahoma", NULL },
	{ NULL, "Tahoma" },		// every other font falls back to this
};
#elif defined(OSX)
static const char *g_szValidAsianFonts[] = { "Apple Symbols", NULL };
// list of how fonts fallback
FallbackFont_t g_FallbackFonts[] =
{
	{ "Marlett", "Apple Symbols" },
	{ "Lucida Console", "Lucida Grande" },
	{ "Tahoma", "Helvetica" },
	{ "Helvetica", "Monaco" },
	{ "Monaco", NULL },
	{ NULL, "Monaco" }		// every other font falls back to this
};

#elif defined(LINUX)
static const char *g_szValidAsianFonts[] = { "Marlett", "WenQuanYi Zen Hei", "unifont", NULL };

// list of how fonts fallback
FallbackFont_t g_FallbackFonts[] =
{
	{ "DejaVu Sans", NULL },
	{ NULL, "DejaVu Sans" },		// every other font falls back to this
};
#elif defined(_PS3)
// list of how fonts fallback
FallbackFont_t g_FallbackFonts[] =
{
	{ NULL, "Tahoma" },		// every other font falls back to this
};
#else
#error
#endif

CFontManager& FontManager()
{
	return s_FontManager;
}

CFontManager::CFontManager()
{
	// add a single empty font, to act as an invalid font handle 0
	m_FontAmalgams.EnsureCapacity(MAX_INITIAL_FONTS);
	m_FontAmalgams.AddToTail();
	m_Win32Fonts.EnsureCapacity(MAX_INITIAL_FONTS);

#ifdef LINUX
	FT_Error error = FT_Init_FreeType(&library);
	if (error)
		Error("Unable to initalize freetype library, is it installed?");
	m_pFontDataHelper = NULL;
#endif

	// setup our text locale
	setlocale(LC_CTYPE, "");
	setlocale(LC_TIME, "");
	setlocale(LC_COLLATE, "");
	setlocale(LC_MONETARY, "");
}

CFontManager::~CFontManager()
{
	ClearAllFonts();
	m_FontAmalgams.RemoveAll();
#ifdef LINUX
	FT_Done_FreeType(library);
#endif
}

void CFontManager::SetLanguage( const char* language )
{
	Q_strncpy(m_szLanguage, language, sizeof(m_szLanguage));
}

void CFontManager::ClearAllFonts()
{
	// free the fonts
	for (int i = 0; i < m_Win32Fonts.Count(); i++)
	{
		delete m_Win32Fonts[i];
	}
	m_Win32Fonts.RemoveAll();

	for (int i = 0; i < m_FontAmalgams.Count(); i++)
	{
		m_FontAmalgams[i].RemoveAll();
	}
}

int CFontManager::GetFontByName( const char* name )
{
	for( int i = 1; i < m_FontAmalgams.Count(); ++i )
	{
		if( !stricmp( name, m_FontAmalgams[ i ].Name() ) )
		{
			return i;
		}
	}

	return 0;
}

font_t* CFontManager::GetFontForChar(vgui2::HFont font, wchar_t ch)
{
	return m_FontAmalgams[ font ].GetFontForChar( ch );
}

void CFontManager::GetCharABCwide( vgui2::HFont font, int ch, int& a, int& b, int& c )
{
	if (!m_FontAmalgams.IsValidIndex(font))
	{
		a = b = c = 0;
		return;
	}

	font_t *winFont = m_FontAmalgams[font].GetFontForChar(ch);
	if (winFont)
	{
		winFont->GetCharABCWidths(ch, a, b, c);
	}
	else
	{
		// no font for this range, just use the default width
		a = c = 0;
		b = m_FontAmalgams[font].GetFontMaxWidth();
	}
}

int CFontManager::GetFontTall( vgui2::HFont font )
{
	return m_FontAmalgams[ font ].GetFontHeight();
}

int CFontManager::GetFontAscent( vgui2::HFont font, wchar_t wch )
{
	font_t *winFont = m_FontAmalgams[font].GetFontForChar(wch);
	if (winFont)
	{
		return winFont->GetAscent();
	}
	else
	{
		return 0;
	}
}

int CFontManager::GetCharacterWidth( vgui2::HFont font, int ch )
{
	if (iswprint(ch))
	{
		int a, b, c;
		GetCharABCwide(font, ch, a, b, c);
		return (a + b + c);
	}
	return 0;
}

void CFontManager::GetTextSize( vgui2::HFont font, const wchar_t* text, int& wide, int& tall )
{
	wide = 0;
	tall = 0;

	if (!text)
		return;

	tall = GetFontTall(font);

	int xx = 0;
	for (int i = 0;; i++)
	{
		wchar_t ch = text[i];
		if (ch == 0)
		{
			break;
		}
		else if (ch == '\n')
		{
			tall += GetFontTall(font);
			xx = 0;
		}
		else if (ch == '&')
		{
			// underscore character, so skip
		}
		else
		{
			xx += GetCharacterWidth(font, ch);
			if (xx > wide)
			{
				wide = xx;
			}
		}
	}
}

bool CFontManager::IsFontForeignLanguageCapable( const char* windowsFontName )
{
	for (int i = 0; g_szValidAsianFonts[i] != NULL; i++)
	{
		if (!stricmp(g_szValidAsianFonts[i], windowsFontName))
			return true;
	}

	// typeface isn't supported by asian languages
	return false;
}

const char* CFontManager::GetFallbackFontName( const char* windowsFontName )
{
	int i;
	for ( i = 0; g_FallbackFonts[i].font != NULL; i++ )
	{
		if (!stricmp(g_FallbackFonts[i].font, windowsFontName))
			return g_FallbackFonts[i].fallbackFont;
	}

	// the ultimate fallback
	return g_FallbackFonts[i].fallbackFont;
}

const char* CFontManager::GetForeignFallbackFontName()
{
#ifdef WIN32
	// tahoma has all the necessary characters for asian/russian languages for winXP/2K+
	if (s_bSupportsUnicode)
		return "Tahoma";

	int i;

	for( i = 0; g_Win98ForeignFallbackFonts[ i ].language; ++i )
	{
		if (!stricmp(g_Win98ForeignFallbackFonts[i].language, m_szLanguage))
			break;
	}

	return g_FallbackFonts[i].fallbackFont;
#elif defined(OSX)
	return "Helvetica";
#elif defined(LINUX)
	return "WenQuanYi Zen Hei";
#elif defined(_PS3)
	return "Tahoma";
#else
#error
#endif
}

bool CFontManager::GetFontUnderlined( vgui2::HFont font )
{
	return m_FontAmalgams[ font ].GetUnderlined();
}

vgui2::HFont CFontManager::CreateFont()
{
	int i = m_FontAmalgams.AddToTail();
	return i;
}

font_t* CFontManager::CreateOrFindWin32Font(const char* windowsFontName, int tall, int weight, int blur, int scanlines, int flags)
{
	// see if we already have the win32 font
	font_t *winFont = NULL;
	int i;
	for (i = 0; i < m_Win32Fonts.Count(); i++)
	{
		if (m_Win32Fonts[i]->IsEqualTo(windowsFontName, tall, weight, blur, scanlines, flags))
		{
			winFont = m_Win32Fonts[i];
			break;
		}
	}

	// create the new win32font if we didn't find it
	if (!winFont)
	{
		MEM_ALLOC_CREDIT();

		i = m_Win32Fonts.AddToTail();
		m_Win32Fonts[i] = NULL;

#ifdef LINUX
		int memSize = 0;
		void *pchFontData = m_pFontDataHelper(windowsFontName, memSize, NULL);

		if (!pchFontData)
		{
			// If we didn't find the font data in the font cache, then get the font filename...
			char *filename = CLinuxFont::GetFontFileName(windowsFontName, flags);
			if (filename)
			{
				// ... and try to add it to the font cache.
				pchFontData = m_pFontDataHelper(windowsFontName, memSize, filename);
				free(filename);
			}
		}

		if (pchFontData)
		{
			m_Win32Fonts[i] = new font_t();
			if (m_Win32Fonts[i]->CreateFromMemory(windowsFontName, pchFontData, memSize, tall, weight, blur, scanlines, flags))
			{
				// add to the list
				winFont = m_Win32Fonts[i];
			}
		}

		if (!winFont)
		{
			// failed to create, remove
			if (m_Win32Fonts[i])
				delete m_Win32Fonts[i];
			m_Win32Fonts.Remove(i);
			return NULL;
		}

#else
		m_Win32Fonts[i] = new font_t();
		if (m_Win32Fonts[i]->Create(windowsFontName, tall, weight, blur, scanlines, flags))
		{
			// add to the list
			winFont = m_Win32Fonts[i];
		}
		else
		{
			// failed to create, remove
			delete m_Win32Fonts[i];
			m_Win32Fonts.Remove(i);
			return NULL;
		}
#endif

	}

	return winFont;
}

bool CFontManager::AddGlyphSetToFont( vgui2::HFont font, const char* windowsFontName, int tall, int weight, int blur, int scanlines, int flags )
{
	if (m_FontAmalgams[font].GetCount() > 0)
		return false;

	font_t *winFont = CreateOrFindWin32Font(windowsFontName, tall, weight, blur, scanlines, flags);

	do
	{
		if (IsFontForeignLanguageCapable(windowsFontName))
		{
			if (winFont)
			{
				m_FontAmalgams[font].AddFont(winFont, 0x0, 0xFFFF);
				return true;
			}
		}
		else
		{
			const char *localizedFontName = GetForeignFallbackFontName();

			if (winFont && !stricmp(localizedFontName, windowsFontName))
			{
				m_FontAmalgams[font].AddFont(winFont, 0x0, 0xFFFF);
				return true;
			}

			font_t *asianFont = CreateOrFindWin32Font(localizedFontName, tall, weight, blur, scanlines, flags);

			if (winFont && asianFont)
			{
				m_FontAmalgams[font].AddFont(winFont, 0x0, 0xFF);
				m_FontAmalgams[font].AddFont(asianFont, 0x100, 0xFFFF);
				return true;
			}
			else if (asianFont)
			{
				m_FontAmalgams[font].AddFont(asianFont, 0x0, 0xFFFF);
				return true;
			}
		}
	} while (NULL != (windowsFontName = GetFallbackFontName(windowsFontName)));

	return false;
}
