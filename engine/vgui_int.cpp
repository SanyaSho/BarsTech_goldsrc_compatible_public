#include "quakedef.h"
#include "vgui_EngineSurface.h"
#include "vgui_int.h"

void VGui_Startup()
{
	VGuiWrap_Startup();
	VGuiWrap2_Startup();
}

void VGui_Shutdown()
{
	VGuiWrap_Shutdown();
	VGuiWrap2_Shutdown();
	EngineSurface::freeEngineSurface();
}

void VGui_CallEngineSurfaceAppHandler( void* event, void* userData )
{
	if( !VGuiWrap2_CallEngineSurfaceAppHandler( event, userData ) )
		VGuiWrap_CallEngineSurfaceAppHandler( event, userData );
}

void* VGui_GetPanel()
{
	return VGuiWrap_GetPanel();
}

void VGui_ReleaseMouse()
{
	VGuiWrap_ReleaseMouse();
	VGuiWrap2_ReleaseMouse();
}

void VGui_GetMouse()
{
	VGuiWrap_GetMouse();
	VGuiWrap2_GetMouse();
}

void VGui_SetVisible( int state )
{
	VGuiWrap_SetVisible( state );
	VGuiWrap2_SetVisible( state );
}

void VGui_Paint()
{
	if( VGuiWrap2_UseVGUI1() )
	{
		VGuiWrap_Paint( VGuiWrap2_IsGameUIVisible() == 0 );
	}
	else
	{
		VGuiWrap_Paint( false );
	}
	
	VGuiWrap2_Paint();
}

int VGui_GameUIKeyPressed()
{
	return VGuiWrap2_GameUIKeyPressed();
}

int VGui_Key_Event( int down, int keynum, const char* pszCurrentBinding )
{
	return VGuiWrap2_Key_Event( down, keynum, pszCurrentBinding ) != 0;
}

#ifndef WIN32
#ifdef WIN32
#pragma pack( push )
#pragma pack( 1 )
#endif

struct
#ifndef WIN32
	__attribute__( ( packed ) ) __attribute__( ( aligned( 2 ) ) )
#endif
	BITMAPFILEHEADER
{
	uint16 bfType;
	uint32 bfSize;
	uint16 bfReserved1;
	uint16 bfReserved2;
	uint32 bfOffBits;
};

#ifdef WIN32
#pragma pack( pop )
#endif

struct BITMAPINFOHEADER
{
	uint32 biSize;
	int32 biWidth;
	int32 biHeight;
	uint16 biPlanes;
	uint16 biBitCount;
	uint32 biCompression;
	uint32 biSizeImage;
	int32 biXPelsPerMeter;
	int32 biYPelsPerMeter;
	uint32 biClrUsed;
	uint32 biClrImportant;
};
#endif

struct BMPQuad
{
	byte b, g, r, reserved;
};

/*struct BITMAPINFO
{
	BITMAPINFOHEADER    bmiHeader;
	BMPQuad             bmiColors[ 1 ];
};*/
#define BMP_TYPE 0x4D42

int VGui_LoadBMP(FileHandle_t file, byte* buffer, int bufsize, int* width, int* height)
{
	BITMAPFILEHEADER bmfHeader;

	DWORD dwFileSize = FS_Size(file);

	FS_Read(&bmfHeader, sizeof(BITMAPFILEHEADER), 1, file);

	int success = false;

	if (bmfHeader.bfType == BMP_TYPE)
	{
		DWORD dwBitsSize = dwFileSize - sizeof(BITMAPFILEHEADER);

		char* pDIB = (char*)malloc(dwBitsSize);

		FS_Read(pDIB, dwBitsSize, 1, file);

		*width = ((BITMAPINFO*)pDIB)->bmiHeader.biWidth;
		*height = ((BITMAPINFO*)pDIB)->bmiHeader.biHeight;

		int readWidth = *width;

		if (*width & 3)
			readWidth += (readWidth + 4) % 4;

		RGBQUAD* pPalette = ((BITMAPINFO*)pDIB)->bmiColors;
		byte* src = (byte*)pDIB + bmfHeader.bfOffBits - sizeof(bmfHeader);

		//Convert into an RGBA format.
		for (int y = 0; y < *height; y++)
		{
			for (int x = 0; x < *width; x++)
			{
				int offs = x + readWidth * (*height - y - 1);
				char* dst = (char*)&buffer[4 * x + 4 * y * *width];

				dst[0] = pPalette[src[offs]].rgbRed;
				dst[1] = pPalette[src[offs]].rgbGreen;
				dst[2] = pPalette[src[offs]].rgbBlue;
				dst[3] = 0xFF;
			}
		}

		free(pDIB);

		success = true;
	}

	FS_Close(file);

	return success;
}
