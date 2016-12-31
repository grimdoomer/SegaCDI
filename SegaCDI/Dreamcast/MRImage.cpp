/*
	SegaCDI - Sega Dreamcast cdi image validator.

	MRImage.cpp - Boot image compressor/decompressor.

	Sep 26th, 2014
*/

#include "../stdafx.h"
#include "MRImage.h"

namespace Dreamcast
{
	bool SaveMRToBMP(const char *pbBuffer, int dwBufferSize, const char *psFileName)
	{
		// Check if the buffer size is valid.
		if (dwBufferSize <= sizeof(MRHeader))
		{
			// Print error and return false.
			printf("ERROR: MR image is invalid!\n");
			return false;
		}

		// Parse the MR image header and check if it is valid.
		MRHeader *pHeader = (MRHeader*)pbBuffer;
		if (pHeader->wMagic != ByteFlip16(MR_IMAGE_MAGIC))
		{
			// Print error and return.
			printf("ERROR: MR image contains invalid magic!\n");
			return false;
		}

		// Check the size of the MR image.
		if (pHeader->dwSize <= sizeof(MRHeader) || dwBufferSize < pHeader->dwSize)
		{
			// Print error and return.
			printf("ERROR: MR image has invalid size!\n");
			return false;
		}

		// Parse the color patlette array.
		PaletteColor *pColorPalette = (PaletteColor*)&pbBuffer[sizeof(MRHeader)];

		// Create the output file.
		DWORD Count = 0;
		HANDLE hFile = CreateFile(psFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			// Print error and return.
			printf("ERROR: failed to create file %s!\n", psFileName);
			return false;
		}

		// Create a BMP header with the info we have right now.
		BMPHeader sBmpHeader = { 0 };
		sBmpHeader.wMagic = ByteFlip16(BMP_HEADER_MAGIC);
		sBmpHeader.dwFileSize = (pHeader->dwWidth * pHeader->dwHeight * 4) + sizeof(BMPHeader);
		sBmpHeader.dwDataOffset = sizeof(BMPHeader);
		sBmpHeader.sInfo.dwHeaderSize = sizeof(_BITMAPINFOHEADER);
		sBmpHeader.sInfo.dwWidth = pHeader->dwWidth;
		sBmpHeader.sInfo.dwHeight = pHeader->dwHeight;
		sBmpHeader.sInfo.wColorPlanes = 1;
		sBmpHeader.sInfo.wBitsPerPixel = 32;
		sBmpHeader.sInfo.dwCompressionMethod = 0;
		sBmpHeader.sInfo.dwImageSize = pHeader->dwWidth * pHeader->dwHeight * 4;
		sBmpHeader.sInfo.dwHorizontalPpm = 0x120B;
		sBmpHeader.sInfo.dwVerticalPpm = 0x120B;
		sBmpHeader.sInfo.dwColorsInPalette = 0;
		sBmpHeader.sInfo.dwImportantColors = 0;

		// Write a 32bbp BMP header.
		WriteFile(hFile, &sBmpHeader, sizeof(BMPHeader), &Count, NULL);

		// Compute the size of the pixel data.
		DWORD dwPixelDataSize = pHeader->dwSize - (sizeof(MRHeader) + pHeader->dwColors * 4);

		// Loop through the MR image buffer and decompress it.
		int ptr = 0;
		BYTE *pbPixelBuffer = (PBYTE)&pbBuffer[pHeader->dwDataOffset];
		while (ptr < dwPixelDataSize - 1)
		{
			int length = 1;

			// Read the length marker.
			int id = pbPixelBuffer[ptr++];

			// Use the block marker to determin the length of the pixel run.
			if (id == 0x82)
			{
				// Read the next byte and determin if the block is greater than 0x100.
				if ((pbPixelBuffer[ptr] & 0x80) == 0x80)
				{
					// The length is greater than 0x100.
					length = (pbPixelBuffer[ptr++] & 0x7F) + 0x100;
				}
				else
				{
					// The length is greater than 1.
					length = id & 0x7F;
				}
			}
			else if (id == 0x81)
			{
				// The length is less than 0x100.
				length = pbPixelBuffer[ptr++];
			}
			else if ((id & 0x80) == 0x80)
			{
				// The length is greater than 1.
				length = id & 0x7F;
			}
			else
			{
				// The block marker is the color index.
				ptr--;
			}

			// Read the palette index from the buffer.
			int colorIndex = pbPixelBuffer[ptr++];
			if (colorIndex > pHeader->dwColors)
			{
				// Color index is out of range, print error and return false.
				printf("ERROR: color index out of range for boot image!\n");
				CloseHandle(hFile);
				return false;
			}
			// Write the color run to the output file.
			for (int x = 0; x < length; x++)
				WriteFile(hFile, &pColorPalette[colorIndex].Color, 4, &Count, NULL);
		}

		// Close the output file and return.
		CloseHandle(hFile);
		return true;
	}

	bool CreateMRFromBMP(const char *psFileName, char *ppbBuffer, int *pdwBufferSize)
	{
		// Open the BMP file.
		HANDLE hFile = CreateFile(psFileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			// Print error and return.
			printf("ERROR: failed to open file %s!\n", psFileName);
			return false;
		}

		// Get the size of the file.
		int dwFileSize = GetFileSize(hFile, NULL);

		// Allocate a buffer for the BMP file.
		BYTE *pbBmpBuffer = new BYTE[dwFileSize];

		// Read the BMP file into memory.
		DWORD Count = 0;
		ReadFile(hFile, pbBmpBuffer, dwFileSize, &Count, NULL);

		// Close the BMP file.
		CloseHandle(hFile);

		// Parse the BMP header and check if the magic is valid.
		BMPHeader *pBmpHeader = (BMPHeader*)pbBmpBuffer;
		if (pBmpHeader->wMagic != ByteFlip16(BMP_HEADER_MAGIC))
		{
			// Print an error and return.
			printf("ERROR: BMP image has invalid magic!\n");
			delete[] pbBmpBuffer;
			return false;
		}

		// Check the width and height are within range.
		if (pBmpHeader->sInfo.dwWidth > MR_MAX_WIDTH || pBmpHeader->sInfo.dwHeight > MR_MAX_HEIGHT)
		{
			// Print a warning and try to continue.
			printf("WARNING: bmp image has invalid resolution, should be 320x94 or smaller!, continuing anyway...\n");
		}

		// Check to make sure the bmp image is 32bbp.
		if (pBmpHeader->sInfo.wBitsPerPixel != 32)
		{
			printf("ERROR: bmp image should be 32bbp!\n");
			delete[] pbBmpBuffer;
			return false;
		}

		// Allocate a temp working buffer for the MR image.
		BYTE *pbMRTempBuffer = new BYTE[pBmpHeader->sInfo.dwWidth * pBmpHeader->sInfo.dwHeight * 4];
		memset(pbMRTempBuffer, 0, pBmpHeader->sInfo.dwWidth * pBmpHeader->sInfo.dwHeight * 4);

		// Create a MR image header.
		MRHeader *pMRHeader = (MRHeader*)pbMRTempBuffer;
		pMRHeader->wMagic = ByteFlip16(MR_IMAGE_MAGIC);
		pMRHeader->dwWidth = pBmpHeader->sInfo.dwWidth;
		pMRHeader->dwHeight = pBmpHeader->sInfo.dwHeight;

		// Create the color palette.
		int colors = 0;
		PaletteColor *pColorPalette = (PaletteColor*)&pbMRTempBuffer[sizeof(MRHeader)];

		// Create a pixel buffer for the image data.
		BYTE *pbMRPixelData = (BYTE*)&pbMRTempBuffer[sizeof(MRHeader) + (sizeof(PaletteColor) * 128)];
		DWORD *pdwBmpPixelData = (DWORD*)&pbBmpBuffer[pBmpHeader->dwDataOffset];

		// Loop through the pixel buffer and process each one.
		DWORD length = 0;
		for (int pos = 0; pos < pBmpHeader->sInfo.dwImageSize; )
		{
			// Determine the size of the pixel run.
			int run = 1;
			while ((pdwBmpPixelData[pos] == pdwBmpPixelData[pos + run]) && (run < 0x17f) &&
				(pos + run <= pBmpHeader->sInfo.dwImageSize))
			{
				// Pixel n is the same as the current pixel, so increase the run length by 1.
				run++;
			}

			// Check if this color is in the color palette and if not add it.
			int colorIndex = -1;
			for (int i = 0; i < colors; i++)
			{
				// Check if this color matches the current pixel.
				if (pdwBmpPixelData[pos] == pColorPalette[i].Color)
				{
					// Found the color, break the loop.
					colorIndex = i;
					break;
				}
			}

			// Check if we found the color in the color palette.
			if (colorIndex == -1)
			{
				// We did not find the color in the color palette, see if there is room to add it.
				if (colors < 128)
				{
					// Add the color to the palette.
					pColorPalette[colors].Color = pdwBmpPixelData[pos];
					colorIndex = colors++;
				}
				else
				{
					// The color palette is full, print a warning and continue.
					colorIndex = 0;
					printf("WARNING: bmp image should have 128 colors or less! continuing anyway...\n");
				}
			}

			// Check the size of the pixel run and create the block accordingly.
			if (run > 0xFF)
			{
				pbMRPixelData[length++] = 0x82;
				pbMRPixelData[length++] = 0x80 | (run - 0x100);
				pbMRPixelData[length++] = (BYTE)colorIndex;
			}
			else if (run > 0x7F)
			{
				pbMRPixelData[length++] = 0x81;
				pbMRPixelData[length++] = run;
				pbMRPixelData[length++] = (BYTE)colorIndex;
			}
			else if (run > 1)
			{
				pbMRPixelData[length++] = 0x80 | run;
				pbMRPixelData[length++] = (BYTE)colorIndex;
			}
			else
			{
				pbMRPixelData[length++] = (BYTE)colorIndex;
			}

			// Increase the position by the run length.
			pos += run;
		}

		// Check if the length of the MR image will fit in the IP.BIN file.
		if (sizeof(MRHeader) + (sizeof(PaletteColor) * colors) + length > MR_MAX_SIZE)
		{
			// Delete our bmp buffer and temp mr buffer.
			delete[] pbBmpBuffer;
			delete[] pbMRTempBuffer;

			// Print error and return false.
			printf("ERROR: MR image is too large and will corrupt IP.BIN bootstrap!\n");
			return false;
		}

		// Finish creating the MR image header.
		pMRHeader->dwSize = sizeof(MRHeader) + (sizeof(PaletteColor) * colors) + length;
		pMRHeader->dwDataOffset = sizeof(MRHeader) + (colors * sizeof(PaletteColor));
		pMRHeader->dwColors = colors;

		// Move the pixel data to directly after the color palette.
		memmove(&pbMRTempBuffer[pMRHeader->dwDataOffset], pbMRPixelData, length);

		// Check to make sure the output buffer can hold the MR image buffer.
		if (*pdwBufferSize < pMRHeader->dwSize)
		{
			// Delete our bmp buffer and temp mr buffer.
			delete[] pbBmpBuffer;
			delete[] pbMRTempBuffer;

			// Print an error and return false.
			printf("ERROR: MR image is too large for buffer!\n");
			return false;
		}

		// Copy the MR image buffer to our new buffer.
		memcpy(ppbBuffer, pbMRTempBuffer, pMRHeader->dwSize);
		*pdwBufferSize = pMRHeader->dwSize;

		// Delete our bmp buffer and temp mr buffer.
		delete[] pbBmpBuffer;
		delete[] pbMRTempBuffer;

		// Done, return true.
		return true;
	}
};