/*
	SegaCDI - Sega Dreamcast cdi image validator.

	Utilities.h - Random fuctions and shit, ya dig?

	Mar 6th, 2013
*/

#pragma once
#include "stdafx.h"

// Some macros to pull out various integers.
#define CAST_TO_BYTE(array, index)		*reinterpret_cast<BYTE*>(&array[index])
#define CAST_TO_WORD(array, index)		*reinterpret_cast<WORD*>(&array[index])
#define CAST_TO_DWORD(array, index)		*reinterpret_cast<DWORD*>(&array[index])

// Byte flipping functions.
static int ByteFlip16(int value)
{
	return (int)((value & 0xFF00) >> 8 | (value & 0xFF) << 8);
}

static int ByteFlip32(int value)
{
	return (int)((value & 0xFF000000) >> 24 | (value & 0xFF0000) >> 8 | (value & 0xFF00) << 8 | (value & 0xFF) << 24);
}

static bool FileExists(LPCSTR sFileName)
{
	// Open the file and check the handle is valid.
	HANDLE hFile = CreateFile(sFileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;

	// Get the file size and make sure it is greater than 0.
	DWORD dwFileSize = GetFileSize(hFile, NULL);

	// Close the file handle.
	CloseHandle(hFile);

	// Return true if the file size is larger than 0.
	return (dwFileSize > 0);
}

static bool WriteWavHeader(HANDLE hFile, DWORD dwTrackLength)
{
	unsigned long  wTotal_length;
	unsigned long  wData_length;
	unsigned long  wHeaderLength = 16;
	unsigned short wFormat = 1;
	unsigned short wChannels = 2;
	unsigned long  wSampleRate = 44100;
	unsigned long  wBitRate = 176400;
	unsigned short wBlockAlign = 4;
	unsigned short wBitsPerSample = 16;

	// Create a temp working buffer.
	BYTE *pbWaveHeader = new BYTE[44];
	memset(pbWaveHeader, 0, 44);

	wData_length = dwTrackLength * 2352;
	wTotal_length = wData_length + 8 + 16 + 12;

	// Fill out the header structure.
	*(DWORD*)(&pbWaveHeader[0]) = ByteFlip32('RIFF');
	*(DWORD*)(&pbWaveHeader[4]) = wTotal_length;
	*(DWORD*)(&pbWaveHeader[8]) = ByteFlip32('WAVE');
	*(DWORD*)(&pbWaveHeader[12]) = ByteFlip32('fmt ');
	*(DWORD*)(&pbWaveHeader[16]) = wHeaderLength;
	*(WORD*)(&pbWaveHeader[20]) = wFormat;
	*(WORD*)(&pbWaveHeader[22]) = wChannels;
	*(DWORD*)(&pbWaveHeader[24]) = wSampleRate;
	*(DWORD*)(&pbWaveHeader[28]) = wBitRate;
	*(WORD*)(&pbWaveHeader[32]) = wBlockAlign;
	*(WORD*)(&pbWaveHeader[34]) = wBitsPerSample;
	*(DWORD*)(&pbWaveHeader[36]) = ByteFlip32('data');
	*(DWORD*)(&pbWaveHeader[40]) = wData_length;

	// Write the header buffer to the file.
	DWORD Count = 0;
	if (WriteFile(hFile, pbWaveHeader, 44, &Count, NULL) == FALSE || Count != 44)
		return false;

	// Delete the temp buffer and return.
	delete[] pbWaveHeader;
	return true;
}