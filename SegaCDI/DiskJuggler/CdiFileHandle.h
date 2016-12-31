/*
	SegaCDI - Sega Dreamcast cdi image validator.

	CdiFileHandle.h - File system reading and writing functions for
		Disk Juggler images.

	Dec 15th, 2015
		- Initial creation.
*/

#pragma once
#include "../stdafx.h"
#include "../Misc/FlatMemoryIterator.h"
#include "..\Misc\DisjointCollection.h"

namespace DiskJuggler
{
	#define RAW_SECTOR_SIZE		2048

	//-----------------------------------------------------
	// CDI Track Definitions
	//-----------------------------------------------------
	enum CdiTrackMode : int
	{
		Audio,
		Mode1,
		Mode2
	};

	enum CdiSectorType : int
	{
		Type_2048 = 0,
		Type_2336 = 1,
		Type_2352 = 2,
		Type_2368 = 3,
		Type_2448 = 4
	};

	enum CdiSectorSize : int
	{
		Size_2048 = 2048,
		Size_2336 = 2336,
		Size_2352 = 2352,
		Size_2368 = 2368,
		Size_2448 = 2448
	};

	struct CdiTrack
	{
		DWORD dwTrackNumber;			// The track number for this track in the current session

		BYTE bFileNameLength;			// Length of the file name
		CHAR *psFileName;				// The file name of this track

		DWORD dwPregapLength;			// Size of the track pregap in sectors
		DWORD dwLength;					// Size of the track in sectors

		CdiTrackMode eMode;				// Mode of the track

		DWORD dwLba;					// Layer break address of the track
		DWORD dwTotalLength;			// Size of the track including the pregap length in sectors

		CdiSectorType eSectorType;		// Type of sector, used to determine the sector size
		CdiSectorSize eSectorSize;		// Size of a single sector
	};

	//-----------------------------------------------------
	// CDI Session Definitions
	//-----------------------------------------------------
	enum CdiSessionDescriptorType : unsigned int
	{
		Type1 = 0x80000004,		// Helper value is offset of session descriptor
		Type2 = 0x80000005,		// Helper value is offset of session descriptor
		Type3 = 0x80000006		// Helper value is size of session descriptor
	};

	struct CdiSessionDescriptorInfo
	{
		CdiSessionDescriptorType eDescriptorType;		// Type of this session descriptor block
		DWORD dwDescriptorHelper;						// This is either the size of the descriptor or the offset of it
	};

	struct CdiSession
	{
		DWORD dwSessionNumber;			// The session number for this session

		WORD wTrackCount;				// Number of tracks in this session
		CdiTrack *psTracks;				// Array of tracks in this session
	};

	//-----------------------------------------------------
	// CdiFileHandle
	//-----------------------------------------------------
	class CdiFileHandle
	{
	protected:
		CString		m_sFileName;					// Cdi image file path
		HANDLE		m_hFile;						// Image handle
		DWORD		m_dwFileSize;					// Size of the cdi image

		// Descriptor information.
		WORD		m_wSessionCount;				// Number of sessions in the image
		CdiSession	*m_sSessions;					// Session info array
		DisjointCollection<CdiSession> *m_pSessionCollection;	// Publicly accessible collection of CdiSession objects

		/*
		*/
		bool ParseSessionDescriptor(PBYTE pbSessionDescriptor, DWORD dwDescriptorSize, CdiSessionDescriptorType eDescriptorType, bool bVerbose);

	public:
		CdiFileHandle();
		~CdiFileHandle();

		/*
			Description: Opens the CDI image file for processing and parses the session descriptor for the image.

			Parameters:
				sFileName: File name of the image file.
				bWrite: Boolean indicating that the image file should be opened for writing.
				bVerbose: Boolean indicating if verbose information should be printed to the console while reading.

			Returns: True if the image was successfully opened and the session descriptor was parsed without errors, false otherwise.
		*/
		bool Open(CString sFileName, bool bWrite, bool bVerbose);

		/*
			Description: Closes the image file and flushes any buffers from memory.
		*/
		void Close();

		/*
			Description: Reads dwSectorCount number of sectors into buffer pbBuffer at LBA dwLBA in track dwTrackNumber of session dwSessionNumber.

			Parameters:
				dwSessionNumber: Session number that track dwTrackNumber is located in.
				dwTrackNumber: Track number to read from.
				dwLBA: LBA to start reading at relative to the beginning of the image file.
				pbBuffer: Buffer to read the sectors into. Buffer should be a multiple of RAW_SECTOR_SIE if the track is a 
					Mode1/Mode2 data track, or a multiple of the track's sector size if the track is an Audio track.
				dwSectorCount: Number of sectors to read from the track.

			Returns: True if the sectors are successfully read from the track, false otherwise.
		*/
		bool ReadSectors(DWORD dwSessionNumber, DWORD dwTrackNumber, DWORD dwLBA, PBYTE pbBuffer, DWORD dwSectorCount);

		/*
			Description: Writes dwSectorCount sectors from buffer pbBuffer at LBA dwLBA in track dwTrackNumber of session dwSessionNumber.

			Parameters:
				dwSessionNumber: Session number that track dwTrackNumber is located in.
				dwTrackNumber: Track number to write to.
				dwLBA: LBA to start writing at relative to the beginning of the image file.
				pbBuffer: Buffer containing the sector data to be written. Buffer should be a multiple of RAW_SECTOR_SIE if the track is a 
					Mode1/Mode2 data track, or a multiple of the track's sector size if the track is an Audio track.
				dwSectorCount: Number of sectors to write to the track.

			Returns: True if the sector data is successfully written to the track, false otherwise.
		*/
		bool WriteSectors(DWORD dwSessionNumber, DWORD dwTrackNumber, DWORD dwLBA, PBYTE pbBuffer, DWORD dwSectorCount);

		/*
			Description: Gets a collection of CdiSession's found in the cdi image file.

			Returns: A DisjointCollection<CdiSession> instance.
		*/
		DisjointCollection<CdiSession>& GetSessionsCollection();
	};
};