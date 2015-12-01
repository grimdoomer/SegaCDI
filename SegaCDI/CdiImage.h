/*
	SegaCDI - Sega Dreamcast cdi image validator.

	CdiImage.h - Support for DiscJuggler cdi images.

	Mar 6th, 2013
*/

#pragma once
#include "stdafx.h"
#include "Bootstrap.h"

struct CdiSessionDescriptorInfo
{
	DWORD dwDescriptorType;		// Type of this session descriptor block
	DWORD dwDescriptorHelper;	// This is either the size of the descriptor or the offset of it
};

#define CDI_SESSION_DESCRIPTOR_TYPE1		0x80000004	// Helper value is offset of session descriptor
#define CDI_SESSION_DESCRIPTOR_TYPE2		0x80000005	// Helper value is offset of session descriptor
#define CDI_SESSION_DESCRIPTOR_TYPE3		0x80000006	// Helper value is size of session descriptor

struct CdiTrack
{
	DWORD dwTrackNumber;			// The track number for this track in the current session

	BYTE bFileNameLength;			// Length of the file name
	CHAR *psFileName;				// The file name of this track

	DWORD dwPregapLength;			// Size of the track pregap in sectors
	DWORD dwLength;					// Size of the track in sectors

	DWORD dwMode;					// Mode of the track

	DWORD dwLba;					// Layer break address of the track
	DWORD dwTotalLength;			//

	DWORD dwSectorSizeValue;		// Enumerator value for the sector size
	DWORD dwSectorSize;				// Actual sector size
};

#define CDI_TRACK_MODE_AUDIO		0
#define CDI_TRACK_MODE_1			1
#define CDI_TRACK_MODE_2			2

#define CDI_SECTOR_SIZE_2048		0
#define CDI_SECTOR_SIZE_2336		1
#define CDI_SECTOR_SIZE_2352		2
#define CDI_SECTOR_SIZE_2368		3
#define CDI_SECTOR_SIZE_2448		4

struct CdiSession
{
	DWORD dwSessionNumber;			// The session number for this session

	WORD wTrackCount;				// Number of tracks in this session
	CdiTrack *psTracks;				// Array of tracks in this session
};

#define IP_BIN_SECTOR_COUNT			16
#define IP_BIN_SIZE					0x8000

class CCdiImage : public CObject
{
protected:
	CString		m_sFileName;	// Cdi image file path
	HANDLE		m_hFile;		// Image handle
	DWORD		m_dwFileSize;	// Size of the cdi image

	// Descriptor information.
	//DWORD		m_dwDescriptorSize;		// Size of the image descriptor block
	WORD		m_wSessionCount;		// Number of sessions in the image
	CdiSession	*m_sSessions;		// Session info array

	// IP.BIN bootstrap.
	CBootstrap	m_sBootstrap;

	bool ParseSessionDescriptor(PBYTE pbSessionDescriptor, DWORD dwDescriptorSize, DWORD dwDescriptorType, bool bVerbos);
	bool LoadBootstrap(bool bVerbos);
	bool ReadSectors(DWORD dwSessionNumber, DWORD dwTrackNumber, DWORD dwLBA, PBYTE pbBuffer, DWORD dwSectorCount);

public:
	CCdiImage();
	~CCdiImage();

	bool LoadImage(CString sFileName, bool bVerbos);
	bool DumpTrackToFile(CString sOutputFolder, DWORD dwSessionNumber, DWORD dwTrackNumber);
	bool DumpAllTracks(CString sOutputFolder);

	bool DumpIPBin(CString sOutputFolder);
	bool DumpMRImage(CString sOutputFolder);
};