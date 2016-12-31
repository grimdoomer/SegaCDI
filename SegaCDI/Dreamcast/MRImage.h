/*
	SegaCDI - Sega Dreamcast cdi image validator.

	MRImage.h - Boot image compressor/decompressor.

	Sep 26th, 2014
*/

#pragma once
#include "../stdafx.h"

namespace Dreamcast
{
#define MR_IMAGE_MAGIC		'MR'

	// Max values for the 3rd party MR image
#define MR_MAX_WIDTH		320
#define MR_MAX_HEIGHT		94
#define MR_MAX_SIZE			0x2000

#pragma pack(1)
	struct MRHeader
	{
		WORD wMagic;		// Image magic 'MR'
		DWORD dwSize;		// Size of the whole MR image buffer
		DWORD dwReserved1;	// Reserved
		DWORD dwDataOffset;	// Offset of the pixel data
		DWORD dwWidth;		// Width of the image
		DWORD dwHeight;		// Height of the image
		DWORD dwReserved2;	// Reserved
		DWORD dwColors;		// Number of colors in the color palette
	};

	union PaletteColor
	{
		struct
		{
			char b;
			char g;
			char r;
		};
		DWORD Color;
	};

#define BMP_HEADER_MAGIC		'BM'

#pragma pack(2)
	struct _BITMAPINFOHEADER
	{
		DWORD dwHeaderSize;				// Size of this header
		DWORD dwWidth;					// Width of the image
		DWORD dwHeight;					// Height of the image
		WORD wColorPlanes;				// Number of color planes, must be 1
		WORD wBitsPerPixel;				// Bits per pixel, ex 32
		DWORD dwCompressionMethod;		// Compression method used, use 0 for no compression
		DWORD dwImageSize;				// Size of the pixel data
		DWORD dwHorizontalPpm;			// Horizontal pixels per meter
		DWORD dwVerticalPpm;			// Vertical pixels per meter
		DWORD dwColorsInPalette;		// Number of colors in the color palette
		DWORD dwImportantColors;		// Number of important colors
	};

#pragma pack(2)
	struct BMPHeader
	{
		WORD wMagic;
		DWORD dwFileSize;
		WORD wReserved1;
		WORD wReserved2;
		DWORD dwDataOffset;
		_BITMAPINFOHEADER sInfo;
	};

	bool SaveMRToBMP(const char *pbBuffer, int dwBufferSize, const char *psFileName);
	bool CreateMRFromBMP(const char *psFileName, char *ppbBuffer, int *pdwBufferSize);
};