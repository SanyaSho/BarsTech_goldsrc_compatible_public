#include "BaseUISurface_Font.h"
#include <stdio.h>
#include <public\vgui\Dar.h>
#include <public\vgui_controls\Panel.h>
#include <vgui\ILocalize.h>
#include <vgui2\vgui_surfacelib\FontManager.h>


// Use to read out of a memory rgba..
class FileImageStream_Memory : public FileImageStream
{
public:
	FileImageStream_Memory(void *pData, int dataLen);

	virtual void		Read(void *pOut, int len);
	virtual bool		ErrorStatus();


private:
	unsigned char		*m_pData;
	int					m_DataLen;
	int					m_CurPos;
	bool				m_bError;
};

// Generic image representation..
class FileImage
{
public:
	FileImage()
	{
		Clear();
	}

	~FileImage()
	{
		Term();
	}

	void			Term()
	{
		if (m_pData)
			delete[] m_pData;

		Clear();
	}

	// Clear the structure without deallocating.
	void			Clear()
	{
		m_Width = m_Height = 0;
		m_pData = NULL;
	}

	int				m_Width, m_Height;
	unsigned char	*m_pData;
};

VFontData::VFontData()
{
	m_BitmapCharWidth = m_BitmapCharHeight = 0;
	m_pBitmap = NULL;
}

VFontData::~VFontData()
{
	if (m_pBitmap)
		delete[] m_pBitmap;
}


// TGA header.
#pragma pack(1)
class TGAFileHeader
{
public:
	unsigned char	m_IDLength;
	unsigned char	m_ColorMapType;
	unsigned char	m_ImageType;
	unsigned short	m_CMapStart;
	unsigned short	m_CMapLength;
	unsigned char	m_CMapDepth;
	unsigned short	m_XOffset;
	unsigned short	m_YOffset;
	unsigned short	m_Width;
	unsigned short	m_Height;
	unsigned char	m_PixelDepth;
	unsigned char	m_ImageDescriptor;
};
#pragma pack()



// ---------------------------------------------------------------------------------------- //
// FileImageStream_Memory.
// ---------------------------------------------------------------------------------------- //
FileImageStream_Memory::FileImageStream_Memory(void *pData, int dataLen)
{
	m_pData = (unsigned char*)pData;
	m_DataLen = dataLen;
	m_CurPos = 0;
	m_bError = false;
}

void FileImageStream_Memory::Read(void *pData, int len)
{
	unsigned char *pOut;
	int i;

	pOut = (unsigned char*)pData;
	for (i = 0; i < len; i++)
	{
		if (m_CurPos < m_DataLen)
		{
			pOut[i] = m_pData[m_CurPos];
			++m_CurPos;
		}
		else
		{
			pOut[i] = 0;
			m_bError = true;
		}
	}
}

bool FileImageStream_Memory::ErrorStatus()
{
	bool ret = m_bError;
	m_bError = false;
	return ret;
}

bool Load32BitTGA(
	FileImageStream *fp,
	FileImage *pImage)
{
	TGAFileHeader hdr;
	char dummyChar;
	int i, x, y;
	long color;
	int runLength, curOut;
	unsigned char *pLine;
	unsigned char packetHeader;


	pImage->Term();

	// Read and verify the header.
	fp->Read(&hdr, sizeof(hdr));
	if (hdr.m_PixelDepth != 32 || hdr.m_ImageType != 10)
		return false;

	// Skip the ID area..
	for (i = 0; i < hdr.m_IDLength; i++)
		fp->Read(&dummyChar, 1);

	pImage->m_Width = hdr.m_Width;
	pImage->m_Height = hdr.m_Height;
	pImage->m_pData = new unsigned char[hdr.m_Width * hdr.m_Height * 4];
	if (!pImage->m_pData)
		return false;

	// Read in the data..
	for (y = pImage->m_Height - 1; y >= 0; y--)
	{
		pLine = &pImage->m_pData[y*pImage->m_Width * 4];

		curOut = 0;
		while (curOut < pImage->m_Width)
		{
			fp->Read(&packetHeader, 1);

			runLength = (int)(packetHeader & ~(1 << 7)) + 1;
			if (curOut + runLength > pImage->m_Width)
				return false;

			if (packetHeader & (1 << 7))
			{
				fp->Read(&color, 4);
				for (x = 0; x < runLength; x++)
				{
					*((long*)pLine) = color;
					pLine += 4;
				}
			}
			else
			{
				for (x = 0; x < runLength; x++)
				{
					fp->Read(&color, 4);
					*((long*)pLine) = color;
					pLine += 4;
				}
			}

			curOut += runLength;
		}
	}

	return true;
}

// ---------------------------------------------------------------------------------------- //
// Encode/decode functions.
// ---------------------------------------------------------------------------------------- //
static void WriteRun(unsigned char *pColor, FILE *fp, int runLength)
{
	unsigned char runCount;

	runCount = runLength - 1;
	runCount |= (1 << 7);
	fwrite(&runCount, 1, 1, fp);
	fwrite(pColor, 1, 4, fp);
}

bool LoadVFontDataFrom32BitTGA(FileImageStream *fp, VFontData *pData)
{
	FileImage fileImage;

	unsigned char *pInLine, *pOutLine;
	int i, x, y;
	int rightX;

	if (!Load32BitTGA(fp, &fileImage))
		return false;

	pData->m_pBitmap = new unsigned char[fileImage.m_Width*fileImage.m_Height];
	if (pData->m_pBitmap)
	{
		pData->m_BitmapCharWidth = fileImage.m_Width / 256;
		pData->m_BitmapCharHeight = fileImage.m_Height;

		for (i = 0; i < 256; i++)
		{
			rightX = 0;
			pInLine = &fileImage.m_pData[i*pData->m_BitmapCharWidth * 4];
			pOutLine = &pData->m_pBitmap[i*pData->m_BitmapCharWidth];

			for (y = 0; y < pData->m_BitmapCharHeight; y++)
			{
				for (x = 0; x < pData->m_BitmapCharWidth; x++)
				{
					if (pInLine[x * 4 + 0] != 0 || pInLine[x * 4 + 1] != 0 || pInLine[x * 4 + 2] != 0 || pInLine[x * 4 + 2] != 0)
					{
						pOutLine[x] = 1;
						if (x > rightX)
							rightX = x;
					}
					else
						pOutLine[x] = 0;
				}
				pInLine += 256 * pData->m_BitmapCharWidth * 4;
				pOutLine += 256 * pData->m_BitmapCharWidth;
			}
			pData->m_CharWidths[i] = (i == 32) ? pData->m_BitmapCharWidth / 4 : rightX;
		}
	}

	return true;
}

// Write a 32-bit TGA file.
void Save32BitTGA(
	FILE *fp,
	FileImage *pImage)
{
	TGAFileHeader hdr;
	int y, runStart, x;
	unsigned char *pLine;


	memset(&hdr, 0, sizeof(hdr));
	hdr.m_PixelDepth = 32;
	hdr.m_ImageType = 10;	// Run-length encoded RGB.
	hdr.m_Width = pImage->m_Width;
	hdr.m_Height = pImage->m_Height;

	fwrite(&hdr, 1, sizeof(hdr), fp);

	// Lines are written bottom-up.
	for (y = pImage->m_Height - 1; y >= 0; y--)
	{
		pLine = &pImage->m_pData[y*pImage->m_Width * 4];

		runStart = 0;
		for (x = 0; x < pImage->m_Width; x++)
		{
			if ((x - runStart) >= 128 ||
				*((long*)&pLine[runStart * 4]) != *((long*)&pLine[x * 4]))
			{
				// Encode this  Run.
				WriteRun(&pLine[runStart * 4], fp, x - runStart);
				runStart = x;
			}
		}

		// Encode the last Run.
		if (x - runStart > 0)
		{
			WriteRun(&pLine[runStart * 4], fp, x - runStart);
		}
	}
}


static int staticFontId = 100;
int s_iFontCount;
static vgui2::Dar<BaseFontPlat*> staticFontPlatDar;

FontPlat::FontPlat(const char* name, int tall, int wide, float rotation, int weight, bool italic, bool underline, bool strikeout, bool symbol)
{
	m_szName = strdup(name);
	m_iTall = tall;
	m_iWide = wide;
	m_flRotation = rotation;
	m_iWeight = weight;
	m_bItalic = italic;
	m_bUnderline = underline;
	m_bStrikeOut = strikeout;
	m_bSymbol = symbol;

	int charset = symbol ? SYMBOL_CHARSET : ANSI_CHARSET;
	m_hFont = ::CreateFontA(tall, wide, (int)(rotation * 10), (int)(rotation * 10),
		weight,
		italic,
		underline,
		strikeout,
		charset,
		OUT_DEFAULT_PRECIS,
		CLIP_DEFAULT_PRECIS,
		DEFAULT_QUALITY,
		DEFAULT_PITCH,
		m_szName);

	m_hDC = ::CreateCompatibleDC(NULL);

	// set as the active font
	::SelectObject(m_hDC, m_hFont);
	::SetTextAlign(m_hDC, TA_LEFT | TA_TOP | TA_UPDATECP);

	// get info about the font
	GetTextMetrics(m_hDC, &tm);

	// code for rendering to a bitmap
	bufSize[0] = tm.tmMaxCharWidth;
	bufSize[1] = tm.tmHeight + tm.tmAscent + tm.tmDescent;

	::BITMAPINFOHEADER header;
	memset(&header, 0, sizeof(header));
	header.biSize = sizeof(header);
	header.biWidth = bufSize[0];
	header.biHeight = -bufSize[1];
	header.biPlanes = 1;
	header.biBitCount = 32;
	header.biCompression = BI_RGB;

	m_hDIB = ::CreateDIBSection(m_hDC, (BITMAPINFO*)&header, DIB_RGB_COLORS, (void**)(&buf), NULL, 0);
	::SelectObject(m_hDC, m_hDIB);

	// get char spacing
	// a is space before character (can be negative)
	// b is the width of the character
	// c is the space after the character
	memset(m_ABCWidthsCache, 0, sizeof(m_ABCWidthsCache));
	if (!::GetCharABCWidthsA(m_hDC, 0, ABCWIDTHS_CACHE_SIZE - 1, m_ABCWidthsCache))
	{
		// since that failed, it must be fixed width, zero everything so a and c will be zeros, then
		// fill b with the value from TEXTMETRIC
		for (int i = 0; i < ABCWIDTHS_CACHE_SIZE; i++)
		{
			m_ABCWidthsCache[i].abcB = (char)tm.tmAveCharWidth;
		}
	}
}

FontPlat::~FontPlat()
{
}

bool FontPlat::equals(const char* name, int tall, int wide, float rotation, int weight, bool italic, bool underline, bool strikeout, bool symbol)
{
	if (!stricmp(name, m_szName)
		&& m_iTall == tall
		&& m_iWide == wide
		&& m_flRotation == rotation
		&& m_iWeight == weight
		&& m_bItalic == italic
		&& m_bUnderline == underline
		&& m_bStrikeOut == strikeout
		&& m_bSymbol == symbol)
		return true;

	return false;
}

void FontPlat::getCharRGBA(int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, uchar* rgba)
{
	// set us up to render into our dib
	::SelectObject(m_hDC, m_hFont);

	// use render-to-bitmap to get our font texture
	::SetBkColor(m_hDC, RGB(0, 0, 0));
	::SetTextColor(m_hDC, RGB(255, 255, 255));
	::SetBkMode(m_hDC, OPAQUE);
	::MoveToEx(m_hDC, -m_ABCWidthsCache[ch].abcA, 0, NULL);

	// render the character
	char wch = (char)ch;
	::ExtTextOutA(m_hDC, 0, 0, 0, NULL, &wch, 1, NULL);
	::SetBkMode(m_hDC, TRANSPARENT);

	int wide = m_ABCWidthsCache[ch].abcB;
	if (wide > bufSize[0])
	{
		wide = bufSize[0];
	}
	int tall = tm.tmHeight;
	if (tall > bufSize[1])
	{
		tall = bufSize[1];
	}

	// iterate through copying the generated dib into the texture
	for (int j = 0; j < tall; j++)
	{
		for (int i = 0; i < wide; i++)
		{
			int x = rgbaX + i;
			int y = rgbaY + j;
			if ((x < rgbaWide) && (y < rgbaTall))
			{
				unsigned char *src = &buf[(j*bufSize[0] + i) * 4];

				float r = (src[0]) / 255.0f;
				float g = (src[1]) / 255.0f;
				float b = (src[2]) / 255.0f;

				// Don't want anything drawn for tab characters.
				if (ch == '\t')
				{
					r = g = b = 0;
				}

				unsigned char *dst = &rgba[(y*rgbaWide + x) * 4];
				dst[0] = (unsigned char)(r * 255.0f);
				dst[1] = (unsigned char)(g * 255.0f);
				dst[2] = (unsigned char)(b * 255.0f);
				dst[3] = (unsigned char)((r * 0.34f + g * 0.55f + b * 0.11f) * 255.0f);
			}
		}
	}
}

void FontPlat::getCharABCwide(int ch, int& a, int& b, int& c)
{
	a = m_ABCWidthsCache[ch].abcA;
	b = m_ABCWidthsCache[ch].abcB;
	c = m_ABCWidthsCache[ch].abcC;

	if (a < 0)
	{
		a = 0;
	}
}

int FontPlat::getTall()
{
	return tm.tmHeight;
}

void FontPlat::drawSetTextFont(SurfacePlat* plat)
{
	::SelectObject(plat->hdc, m_hFont);
}

FontPlat_Bitmap::FontPlat_Bitmap()
{
	m_pName = null;
}

FontPlat_Bitmap::~FontPlat_Bitmap()
{
}

FontPlat_Bitmap* FontPlat_Bitmap::Create(const char* name, FileImageStream* pStream)
{
	FontPlat_Bitmap* pBitmap = new FontPlat_Bitmap();
	if (pBitmap == null)
	{
		return null;
	}

	if (!LoadVFontDataFrom32BitTGA(pStream, &pBitmap->m_FontData))
	{
		delete pBitmap;
		return null;
	}

	pBitmap->m_pName = new char[strlen(name) + 1];
	if (pBitmap->m_pName == null)
	{
		delete pBitmap;
		return null;
	}

	strcpy(pBitmap->m_pName, name);
	return pBitmap;
}

bool FontPlat_Bitmap::equals(const char* name, int tall, int wide, float rotation, int weight, bool italic, bool underline, bool strikeout, bool symbol)
{
	return false;
}

void FontPlat_Bitmap::getCharRGBA(int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, uchar* rgba)
{
	uchar* pSrcPos;
	uchar* pOutPos;
	int x, y, outX, outY;

	if (ch<0)
		ch = 0;
	else if (ch >= 256)
		ch = 256;

	for (y = 0; y<m_FontData.m_BitmapCharHeight; y++)
	{
		pSrcPos = &m_FontData.m_pBitmap[m_FontData.m_BitmapCharWidth*(ch + y * 256)];
		for (x = 0; x<m_FontData.m_BitmapCharWidth; x++)
		{
			outX = rgbaX + x;
			outY = rgbaY + y;
			if ((outX<rgbaWide) && (outY<rgbaTall))
			{
				pOutPos = &rgba[(outY*rgbaWide + outX) * 4];
				if (pSrcPos[x] != 0)
				{
					pOutPos[0] = pOutPos[1] = pOutPos[2] = pOutPos[3] = 255;
				}
				else
				{
					pOutPos[0] = pOutPos[1] = pOutPos[2] = pOutPos[3] = 0;
				}
			}
		}
	}
}

void FontPlat_Bitmap::getCharABCwide(int ch, int& a, int& b, int& c)
{
	if (ch<0)
		ch = 0;
	else if (ch>255)
		ch = 255;

	a = c = 0;
	b = m_FontData.m_CharWidths[ch] + 1;
}

int FontPlat_Bitmap::getTall()
{
	return m_FontData.m_BitmapCharHeight;
}

void FontPlat_Bitmap::drawSetTextFont(SurfacePlat* plat)
{
}

EngineFont::~EngineFont()
{
	if (this->_name != NULL)
		delete[] _name;

	s_iFontCount--;

	if (s_iFontCount == 0)
		Font_Reset();
}

EngineFont::EngineFont(const char* name, int tall, int wide, float rotation, int weight, bool italic, bool underline, bool strikeout, bool symbol)
{
	Init(name, null, 0, wide, tall, rotation, weight, italic, underline, strikeout, symbol);
	s_iFontCount++;
}

EngineFont::EngineFont(const char* name, void *pFileData, int fileDataLen, int tall, int wide, float rotation, int weight, bool italic, bool underline, bool strikeout, bool symbol)
{
	Init(name, pFileData, fileDataLen, wide, tall, rotation, weight, italic, underline, strikeout, symbol);
	s_iFontCount++;
}

void EngineFont::Init(const char* name, void *pFileData, int fileDataLen, int tall, int wide, float rotation, int weight, bool italic, bool underline, bool strikeout, bool symbol)
{
	FontPlat_Bitmap*pBitmapFont = null;

	_name = strdup(name);
	_id = -1;

	if (pFileData != null)
	{
		FileImageStream_Memory memStream(pFileData, fileDataLen);
		pBitmapFont = FontPlat_Bitmap::Create(name, &memStream);
		if (pBitmapFont != null)
		{
			_plat = pBitmapFont;
			staticFontPlatDar.AddElement(_plat);
			_id = staticFontId++;
		}
	}
	else
	{
		_plat = null;
		for (int i = 0; i<staticFontPlatDar.GetCount(); i++)
		{
			if (staticFontPlatDar[i]->equals(name, tall, wide, rotation, weight, italic, underline, strikeout, symbol))
			{
				_plat = staticFontPlatDar[i];
				break;
			}
		}
		if (_plat == null)
		{
			_plat = new FontPlat(name, tall, wide, rotation, weight, italic, underline, strikeout, symbol);
			staticFontPlatDar.AddElement(_plat);
			_id = staticFontId++;
		}
	}
}

void EngineFont::getCharRGBA(int ch, int rgbaX, int rgbaY, int rgbaWide, int rgbaTall, uchar* rgba)
{
	_plat->getCharRGBA(ch, rgbaX, rgbaY, rgbaWide, rgbaTall, rgba);
}

void EngineFont::getCharABCwide(int ch, int& a, int& b, int& c)
{
	_plat->getCharABCwide(ch, a, b, c);
}

int EngineFont::getTall()
{
	return _plat->getTall();
}

void EngineFont::getTextSize(const char* text, int& wide, int& tall)
{
#ifndef _WIN32
	wchar_t wtext[2048];
	vgui2::localize()->ConvertANSIToUnicode(text, wtext, 8192);
	FontManager().GetTextSize(NULL, wtext, wide, tall);
#else
	wide=0;
	tall=0;

	if(text==null)
	{
		return;
	}

	tall = getTall();

	int xx = 0;
	for (int i = 0;; i++)
	{
		char ch = text[i];

		if (ch == 0)
		{
			break;
		}
		if (ch == '\n')
		{
			tall += getTall();
			xx = 0;
			continue;
		}

		int a, b, c;
		getCharABCwide(ch, a, b, c);
		xx += a + b + c;
		if (xx>wide)
		{
			wide = xx;
		}
	}
#endif
}

int EngineFont::getId()
{
	return _id;
}

BaseFontPlat* EngineFont::getPlat()
{
	return _plat;
}

void Font_Reset()
{
	staticFontPlatDar.RemoveAll();
}
