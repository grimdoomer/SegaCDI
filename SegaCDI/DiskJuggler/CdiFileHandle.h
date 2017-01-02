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
	// Forward declarations.
	class CdiFileHandle;

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

		CdiTrack() 
		{
			// Initialize fields.
			this->dwTrackNumber = 0;
			this->bFileNameLength = 0;
			this->psFileName = nullptr;
			this->dwPregapLength = 0;
			this->dwLength = 0;
			this->eMode = (CdiTrackMode)0;
			this->dwLba = 0;
			this->dwTotalLength = 0;
			this->eSectorType = (CdiSectorType)0;
			this->eSectorSize = (CdiSectorSize)0;
		}

		CdiTrack(const CdiTrack& other)
		{
			// Initialize fields.
			this->dwTrackNumber = other.dwTrackNumber;
			this->bFileNameLength = other.bFileNameLength;
			this->psFileName = new CHAR[this->bFileNameLength + 1];
			memcpy(this->psFileName, other.psFileName, this->bFileNameLength);
			this->dwPregapLength = other.dwPregapLength;
			this->dwLength = other.dwLength;
			this->eMode = other.eMode;
			this->dwLba = other.dwLba;
			this->dwTotalLength = other.dwTotalLength;
			this->eSectorType = other.eSectorType;
			this->eSectorSize = other.eSectorSize;
		}
	};

	//-----------------------------------------------------
	// CdiTrackHandle
	//-----------------------------------------------------
	class CdiTrackHandle
	{
		friend class CdiFileHandle;
	private:
		CdiFileHandle *pFileHandle;		// CDI image file handle this handle is to be used with
		DWORD dwSessionNumber;			// Session number this handle is located in
		DWORD dwTrackNumber;			// Track number this handle is located in
		CdiTrack *pTrack;				// CDI track structure this handle is for

	public:
		/*
			Description: Gets the base LBA for this track.
		*/
		DWORD LBA();

		DWORD TrackSize();

		/*
			Description: Reads dwSize number of bytes from the track stream at dwLBA.

			Parameters:
				dwLBA: LBA to begin reading at, relative to the start of the track.
				pbBuffer: Buffer to read data into.
				dwSize: Size of data to be read.

			Returns: True if the data was successfully read from the track, false otherwise.
		*/
		bool ReadData(DWORD dwLBA, PBYTE pbBuffer, DWORD dwSize);

		/*
			Description: Writes dwSize number of bytes to the track stream at dwLBA.

			Parameters:
				dwLBA: LBA to begin writing at, relative to the start of the track.
				pbBuffer: Buffer containing the sector data to be written.
				dwSize: Number of bytes to be written.

			Returns: True if the data was successfully written to the track, false otherwise.
		*/
		bool WriteData(DWORD dwLBA, PBYTE pbBuffer, DWORD dwSize);
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

		CdiSession() 
		{ 
			// Initialize fields.
			this->dwSessionNumber = 0;
			this->wTrackCount = 0;
			this->psTracks = nullptr;
		}

		CdiSession(const CdiSession& other)
		{
			// Initialize fields.
			this->dwSessionNumber = other.dwSessionNumber;
			this->wTrackCount = other.wTrackCount;

			this->psTracks = new CdiTrack[this->wTrackCount];
			for (int i = 0; i < this->wTrackCount; i++)
				this->psTracks[i] = CdiTrack(other.psTracks[i]);
		}
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

		DWORD		m_dwCurrentLBA;					// Current offset in the input file, used for sequential reads/writes

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

		/*
			Description: Opens a track handle on the specified track in the specified session.

			Parameters:
				dwSessionNumber: session number the track is located in.
				dwTrackNumber: track number to open.

			Returns: A CdiTrackHandle object is the track was successfully opened, nullptr otherwise.
		*/
		CdiTrackHandle *OpenTrackHandle(DWORD dwSessionNumber, DWORD dwTrackNumber);

		/*
			Description: Closes an opened track handle.

			Parameters:
				pTrackHandle: the track handle to close.
		*/
		void CloseTrackHandle(CdiTrackHandle *pTrackHandle);
	};
};