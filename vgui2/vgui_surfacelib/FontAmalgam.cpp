#include <tier0/platform.h>

#include "FontAmalgam.h"
//#include "Win32Font.h"

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CFontAmalgam::CFontAmalgam()
{
	m_Fonts.EnsureCapacity(4);
	m_iMaxHeight = 0;
	m_iMaxWidth = 0;
}

//-----------------------------------------------------------------------------
// Purpose: Destructor
//-----------------------------------------------------------------------------
CFontAmalgam::~CFontAmalgam()
{
}

//-----------------------------------------------------------------------------
// Purpose: Data accessor
//-----------------------------------------------------------------------------
const char *CFontAmalgam::Name()
{
	return m_szName;
}

//-----------------------------------------------------------------------------
// Purpose: Data accessor
//-----------------------------------------------------------------------------
void CFontAmalgam::SetName(const char *name)
{
	Q_strncpy(m_szName, name, sizeof(m_szName));
}

//-----------------------------------------------------------------------------
// Purpose: adds a font to the amalgam
//-----------------------------------------------------------------------------
void CFontAmalgam::AddFont(font_t *font, int lowRange, int highRange)
{
	int i = m_Fonts.AddToTail();

	m_Fonts[i].font = font;
	m_Fonts[i].lowRange = lowRange;
	m_Fonts[i].highRange = highRange;

	m_iMaxHeight = max(font->GetHeight(), m_iMaxHeight);
	m_iMaxWidth = max(font->GetMaxCharWidth(), m_iMaxWidth);
}

//-----------------------------------------------------------------------------
// Purpose: clears the fonts
//-----------------------------------------------------------------------------
void CFontAmalgam::RemoveAll()
{
	// clear out
	m_Fonts.RemoveAll();
	m_iMaxHeight = 0;
	m_iMaxWidth = 0;
}

//-----------------------------------------------------------------------------
// Purpose: returns the font for the given character
//-----------------------------------------------------------------------------
font_t *CFontAmalgam::GetFontForChar(int ch)
{
	for (int i = 0; i < m_Fonts.Count(); i++)
	{
#if defined(LINUX)
		if (ch >= m_Fonts[i].lowRange && ch <= m_Fonts[i].highRange && m_Fonts[i].font->HasChar(ch))
#else
		if (ch >= m_Fonts[i].lowRange && ch <= m_Fonts[i].highRange)
#endif
		{
			Assert(m_Fonts[i].font->IsValid());
			return m_Fonts[i].font;
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: returns the max height of the font set
//-----------------------------------------------------------------------------
int CFontAmalgam::GetFontHeight()
{
	if (!m_Fonts.Count())
	{
		return m_iMaxHeight;
	}
	return m_Fonts[0].font->GetHeight();
}

//-----------------------------------------------------------------------------
// Purpose: returns the maximum width of a character in a font
//-----------------------------------------------------------------------------
int CFontAmalgam::GetFontMaxWidth()
{
	return m_iMaxWidth;
}

//-----------------------------------------------------------------------------
// Purpose: returns the name of the font that is loaded
//-----------------------------------------------------------------------------
const char *CFontAmalgam::GetFontName(int i)
{
	return m_Fonts[i].font->GetName();
}

//-----------------------------------------------------------------------------
// Purpose: returns the name of the font that is loaded
//-----------------------------------------------------------------------------
int CFontAmalgam::GetFlags(int i)
{
	if (m_Fonts[i].font)
	{
		return m_Fonts[i].font->GetFlags();
	}
	else
	{
		return 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: returns the number of fonts this amalgam contains
//-----------------------------------------------------------------------------
int CFontAmalgam::GetCount()
{
	return m_Fonts.Count();
}


//-----------------------------------------------------------------------------
// Purpose: returns the max height of the font set
//-----------------------------------------------------------------------------
bool CFontAmalgam::GetUnderlined()
{
	if (!m_Fonts.Count())
	{
		return false;
	}
	return m_Fonts[0].font->GetUnderlined();
}
