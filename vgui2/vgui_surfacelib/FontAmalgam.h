#ifndef VGUI2_VGUI_SURFACELIB_FONTAMALGAM_H
#define VGUI2_VGUI_SURFACELIB_FONTAMALGAM_H

#include <UtlVector.h>

#include "vguifont.h"

class CFontAmalgam
{
public:
	CFontAmalgam();
	~CFontAmalgam();

	const char* Name();

	void SetName( const char* name );

	void AddFont(font_t* font, int lowRange, int highRange);

	font_t* GetFontForChar(int ch);

	int GetFontHeight();

	int GetFontMaxWidth();

	const char* GetFontName( int i );

	int GetFlags( int i );

	int GetCount();

	bool GetUnderlined();

	void RemoveAll();

private:
	struct TFontRange
	{
		int lowRange;
		int highRange;
		font_t* font;
	};

	CUtlVector<TFontRange> m_Fonts;

	char m_szName[ 32 ];

	int m_iMaxWidth = 0;
	int m_iMaxHeight = 0;

private:
	CFontAmalgam( const CFontAmalgam& ) = delete;
	CFontAmalgam& operator=( const CFontAmalgam& ) = delete;
};

#endif //VGUI2_VGUI_SURFACELIB_FONTAMALGAM_H
