#ifndef _FONTMAN_H_
#define _FONTMAN_H_

#include <VGUI_Font.h>
#include <VGUI_Dar.h>
#define WIN32_LEAN_AND_MEAN
#define OEMRESOURCE
#include "..\..\common\winsani_in.h"
#include <windows.h>
#include "..\..\common\winsani_out.h"


class VFontData
{
public:
	VFontData();
	~VFontData();

public:
	int m_CharWidths[256];
	int m_BitmapCharWidth;
	int m_BitmapCharHeight;
	unsigned char *m_pBitmap;
};

class FileImageStream
{
public:
	virtual void	Read(void *pOut, int len) = 0;

	// Returns true if there were any Read errors.
	// Clears error status.
	virtual bool	ErrorStatus() = 0;
};

namespace
{
	class SurfacePlat
	{
	public:
		HWND    hwnd;
		HDC     hdc;
		HDC     hwndDC;
		HDC	  textureDC;
		HGLRC   hglrc;
		HRGN    clipRgn;
		HBITMAP bitmap;
		int     bitmapSize[2];
		int     restoreInfo[4];
		bool    isFullscreen;
		int     fullscreenInfo[3];
	};

	class BaseFontPlat
	{
	public:
		virtual ~BaseFontPlat() {}

		virtual bool equals(const char* name, int tall, int wide, float rotation, int weight, bool italic, bool underline, bool strikeout, bool symbol) = 0;
		virtual void getCharRGBA(int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, uchar* rgba) = 0;
		virtual void getCharABCwide(int ch, int& a, int& b, int& c) = 0;
		virtual int getTall() = 0;
		virtual void drawSetTextFont(SurfacePlat* plat) = 0;
	};

	class FontPlat : public BaseFontPlat
	{
	protected:
		HFONT m_hFont;
		HDC m_hDC;
		HBITMAP m_hDIB;
		TEXTMETRIC tm;
		enum { ABCWIDTHS_CACHE_SIZE = 256 };
		ABC m_ABCWidthsCache[ABCWIDTHS_CACHE_SIZE];

		int bufSize[2];
		uchar* buf;

		VFontData m_BitmapFont;
		bool m_bBitmapFont;

		char *m_szName;
		int m_iWide;
		int m_iTall;
		float m_flRotation;
		int m_iWeight;
		bool m_bItalic;
		bool m_bUnderline;
		bool m_bStrikeOut;
		bool m_bSymbol;

	public:
		FontPlat(const char* name, int tall, int wide, float rotation, int weight, bool italic, bool underline, bool strikeout, bool symbol);
		~FontPlat();

		virtual bool equals(const char* name, int tall, int wide, float rotation, int weight, bool italic, bool underline, bool strikeout, bool symbol);
		virtual void getCharRGBA(int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, uchar* rgba);
		virtual void getCharABCwide(int ch, int& a, int& b, int& c);
		virtual int getTall();
		virtual void drawSetTextFont(SurfacePlat* plat);
	};

	class FontPlat_Bitmap : public BaseFontPlat
	{
	private:
		VFontData m_FontData;
		char *m_pName;

	public:
		static FontPlat_Bitmap* Create(const char* name, FileImageStream* pStream);

	public:
		FontPlat_Bitmap();
		~FontPlat_Bitmap();

		virtual bool equals(const char* name, int tall, int wide, float rotation, int weight, bool italic, bool underline, bool strikeout, bool symbol);
		virtual void getCharRGBA(int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, uchar* rgba);
		virtual void getCharABCwide(int ch, int& a, int& b, int& c);
		virtual int getTall();
		virtual void drawSetTextFont(SurfacePlat* plat);
	};

	//TODO: cursors and fonts should work like gl binds
	class EngineFont
	{
	public:
		~EngineFont();
		EngineFont(const char* name, int tall, int wide, float rotation, int weight, bool italic, bool underline, bool strikeout, bool symbol);
		// If pFileData is non-NULL, then it will try to load the 32-bit (RLE) TGA file. If that fails,
		// it will create the font using the specified parameters.
		// pUniqueName should be set if pFileData and fileDataLen are set so it can determine if a font is already loaded.
		EngineFont(const char* name, void *pFileData, int fileDataLen, int tall, int wide, float rotation, int weight, bool italic, bool underline, bool strikeout, bool symbol);
	private:
		virtual void Init(const char* name, void *pFileData, int fileDataLen, int tall, int wide, float rotation, int weight, bool italic, bool underline, bool strikeout, bool symbol);
	public:
		BaseFontPlat* getPlat();
		virtual void getCharRGBA(int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, uchar* rgba);
		virtual void getCharABCwide(int ch, int& a, int& b, int& c);
		virtual void getTextSize(const char* text, int& wide, int& tall);
		virtual int  getTall();
#ifndef _WIN32
		virtual int getWide();
#endif
		virtual int  getId();
	protected:
		char*			_name;
		BaseFontPlat*	_plat;
		int			_id;
	};
}

void Font_Reset();

#endif
