/*
	SegaCDI - Sega Dreamcast cdi image validator.

	CdiFileHandle.cpp - File system reading and writing functions for
	Disk Juggler images.

	Dec 15th, 2015
		- Initial creation.
*/

#include "../stdafx.h"
#include "../Misc/Utilities.h"
#include "CdiFileHandle.h"

namespace DiskJuggler
{
	//-----------------------------------------------------
	// CdiTrackHandle
	//-----------------------------------------------------
	DWORD CdiTrackHandle::LBA()
	{
		// Just return the lba from the track.
		return this->pTrack->dwLba;
	}

	DWORD CdiTrackHandle::TrackSize()
	{
		// Compute the raw size of the track.
		return this->pTrack->dwLength * RAW_SECTOR_SIZE;
	}

	bool CdiTrackHandle::ReadData(DWORD dwLBA, PBYTE pbBuffer, DWORD dwSize)
	{
		// Check to see if the size is a multiple of RAW_SECTOR_SIZE.
		if (dwSize % RAW_SECTOR_SIZE == 0)
		{
			// Delegate the functionality to the underlying CdiFileHandle.
			return this->pFileHandle->ReadSectors(this->dwSessionNumber, this->dwTrackNumber, dwLBA + this->pTrack->dwLba, pbBuffer, dwSize / RAW_SECTOR_SIZE);
		}
		else
		{
			// Allocate a scratch buffer to store the data into.
			DWORD dwReadSize = dwSize + (RAW_SECTOR_SIZE - (dwSize % RAW_SECTOR_SIZE));
			BYTE *pbScratchBuffer = (PBYTE)VirtualAlloc(NULL, dwReadSize, MEM_COMMIT, PAGE_READWRITE);
			if (pbScratchBuffer == NULL)
			{
				// Print an error and return false.
				printf("CdiTrackHandle::ReadSectors(): failed to allocate scratch buffer!\n");
				return false;
			}

			// Read the data from the track.
			if (this->pFileHandle->ReadSectors(this->dwSessionNumber, this->dwTrackNumber, dwLBA + this->pTrack->dwLba,
				pbScratchBuffer, dwReadSize / RAW_SECTOR_SIZE) == false)
			{
				// Failed to read the data, return false.
				VirtualFree(pbScratchBuffer, dwReadSize, MEM_DECOMMIT);
				return false;
			}

			// Copy the data to the output buffer.
			memcpy(pbBuffer, pbScratchBuffer, dwSize);

			// Free the scratch buffer and return true.
			VirtualFree(pbScratchBuffer, dwReadSize, MEM_DECOMMIT);
			return true;
		}
	}

	bool CdiTrackHandle::WriteData(DWORD dwLBA, PBYTE pbBuffer, DWORD dwSize)
	{
		// Delegate the functionality to the underlying CdiFileHandle.
		return false;
		//return this->pFileHandle->WriteSectors(this->dwSessionNumber, this->dwTrackNumber, dwLBA + this->pTrack->dwLba, pbBuffer, dwSectorCount);
	}

	//-----------------------------------------------------
	// CdiFileHandle
	//-----------------------------------------------------
	CdiFileHandle::CdiFileHandle()
	{
		// Initialize fields.
		this->m_hFile = INVALID_HANDLE_VALUE;
		this->m_dwFileSize = 0;
		this->m_dwCurrentLBA = -1;
		this->m_wSessionCount = 0;
		this->m_sSessions = nullptr;
	}

	CdiFileHandle::~CdiFileHandle()
	{

	}

	bool CdiFileHandle::Open(CString sFileName, bool bWrite, bool bVerbose)
	{
		DWORD dwBytesRead = 0;

		// Save the file name and open the cdi image file.
		this->m_sFileName = sFileName;
		this->m_hFile = CreateFile(this->m_sFileName, GENERIC_READ | (bWrite == true ? GENERIC_WRITE : 0), 0, 
			NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (this->m_hFile == INVALID_HANDLE_VALUE)
		{
			// Print error and return.
			printf("CdiFileHandle::Open: could not find file %s!\n", this->m_sFileName);
			return false;
		}

		// Get the file size of the image and check it is valid.
		this->m_dwFileSize = GetFileSize(this->m_hFile, NULL);
		if (this->m_dwFileSize == 0)
		{
			// Print error, close the file, and return.
			printf("CdiFileHandle::Open: image file %s has invalid size!", this->m_sFileName);
			CloseHandle(this->m_hFile);
			return false;
		}

		// Seek to the end of the file - 8 bytes to get the descriptor info block.
		CdiSessionDescriptorInfo sDescriptorInfo;
		SetFilePointer(this->m_hFile, -sizeof(CdiSessionDescriptorInfo), NULL, FILE_END);
		if (ReadFile(this->m_hFile, &sDescriptorInfo, sizeof(CdiSessionDescriptorInfo), &dwBytesRead, NULL) == false ||
			dwBytesRead != sizeof(CdiSessionDescriptorInfo))
		{
			// Failed to read the session descriptor.
			printf("CdiFileHandle::Open(): failed to read session descriptor!\n");
			CloseHandle(this->m_hFile);
			return false;
		}

		// Verify the descriptor type.
		if (sDescriptorInfo.eDescriptorType != CdiSessionDescriptorType::Type1 &&
			sDescriptorInfo.eDescriptorType != CdiSessionDescriptorType::Type2 &&
			sDescriptorInfo.eDescriptorType != CdiSessionDescriptorType::Type3)
		{
			// Print error, close the file, and return.
			printf("CdiFileHandle::Open: invalid descriptor version %d!", sDescriptorInfo.eDescriptorType);
			CloseHandle(this->m_hFile);
			return false;
		}

		// Print the cdi version.
		if (sDescriptorInfo.eDescriptorType == CdiSessionDescriptorType::Type1)
			printf("found cdi version 2\n");
		else if (sDescriptorInfo.eDescriptorType == CdiSessionDescriptorType::Type2)
			printf("found cdi version 3\n");
		else if (sDescriptorInfo.eDescriptorType == CdiSessionDescriptorType::Type3)
			printf("found cdi version 3.5\n");

		// Allocate a buffer for the session descriptor block.
		DWORD dwSessionDescriptorSize = (sDescriptorInfo.eDescriptorType == CdiSessionDescriptorType::Type3 ?
			sDescriptorInfo.dwDescriptorHelper : this->m_dwFileSize - sDescriptorInfo.dwDescriptorHelper);
		BYTE *pbSessionDescriptor = new BYTE[dwSessionDescriptorSize];

		// Read the session descriptor block from the image.
		SetFilePointer(this->m_hFile, -(dwSessionDescriptorSize), NULL, FILE_END);
		if (ReadFile(this->m_hFile, pbSessionDescriptor, dwSessionDescriptorSize, &dwBytesRead, NULL) == false ||
			dwBytesRead != dwSessionDescriptorSize)
		{
			// Failed to read the session descriptor.
			printf("CdiFileHandle::Open(): failed to read session descriptor data!\n");

			// Clean up resources and return false.
			CloseHandle(this->m_hFile);
			delete[] pbSessionDescriptor;
			return false;
		}

		// Parse the session descriptor block.
		if (bVerbose == true) printf("found session descriptor of size %d\n", dwSessionDescriptorSize);
		if (ParseSessionDescriptor(pbSessionDescriptor, dwSessionDescriptorSize, sDescriptorInfo.eDescriptorType, bVerbose) == false)
		{
			// There was an error reading the session descriptor, close the file and return.
			CloseHandle(this->m_hFile);
			delete[] pbSessionDescriptor;
			return false;
		}

		// Delete temp buffer.
		delete[] pbSessionDescriptor;

		// Done, return true.
		return true;
	}

	void CdiFileHandle::Close()
	{

	}

	bool CdiFileHandle::ParseSessionDescriptor(PBYTE pbSessionDescriptor, DWORD dwDescriptorSize, CdiSessionDescriptorType eDescriptorType, bool bVerbose)
	{
		// Get the session count and create our session info array.
		this->m_wSessionCount = CAST_TO_WORD(pbSessionDescriptor, 0);
		this->m_sSessions = new CdiSession[this->m_wSessionCount];
		printf("found %d sessions\n", this->m_wSessionCount);// + 1);
		pbSessionDescriptor += 2;

		// For now just loop through all of the sessions and parse each one.
		for (int i = 0; i < this->m_wSessionCount; i++)
		{
			// Initialize the session structure.
			this->m_sSessions[i] = CdiSession();

			// Set the session number.
			if (bVerbose == true) printf("\nsession %d:\n", i + 1);
			this->m_sSessions[i].dwSessionNumber = i;

			// The first word is the track count.
			this->m_sSessions[i].wTrackCount = CAST_TO_WORD(pbSessionDescriptor, 0);
			if (bVerbose == true) printf("\ttrack count: %d\n", this->m_sSessions[i].wTrackCount);
			pbSessionDescriptor += 2;

			// NOTE: 0 tracks means the session is open, we need to handle this.

			// Create the track array.
			this->m_sSessions[i].psTracks = new CdiTrack[this->m_sSessions[i].wTrackCount];

			// Loop through all the tracks and read them from the descriptor.
			for (int x = 0; x < this->m_sSessions[i].wTrackCount; x++)
			{
				// Initialize the track structure.
				this->m_sSessions[i].psTracks[x] = CdiTrack();

				// Set the track number.
				if (bVerbose == true) printf("\n\ttrack %d\n", x + 1);
				this->m_sSessions[i].psTracks[x].dwTrackNumber = x;

				// Check for extra data we need to skip (DJ 3.00.780 and up).
				if (CAST_TO_DWORD(pbSessionDescriptor, 0) != 0)
					pbSessionDescriptor += 8;

				// The next 20 bytes appear to be constant, I think they are some sort of track start marker (CDIRip src).
				BYTE pbStatic1[20] = { 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xFF,
					0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF };
				if (memcmp(&pbSessionDescriptor[4], pbStatic1, 20) != 0)
				{
					// For now print a warning and break execution.
					printf("CdiFileHandle::ParseSessionDescriptor(): session descriptor static field 1 (track start marker) mismatch!\n");
					DebugBreak();
				}

				// I really have no idea what the next 4 bytes are, the information at hand is rudimentary at best.
				BYTE pbUnknown1[4] = { pbSessionDescriptor[24], pbSessionDescriptor[25], pbSessionDescriptor[26], pbSessionDescriptor[27] };
				if (bVerbose == true) printf("\t\tunknown bytes 1: %x %x %x %x\n", pbUnknown1[0], pbUnknown1[1], pbUnknown1[2], pbUnknown1[3]);

				// Next we have the file name of this session.
				this->m_sSessions[i].psTracks[x].bFileNameLength = CAST_TO_BYTE(pbSessionDescriptor, 28);
				this->m_sSessions[i].psTracks[x].psFileName = new CHAR[this->m_sSessions[i].psTracks[x].bFileNameLength + 1];
				memcpy(this->m_sSessions[i].psTracks[x].psFileName, &pbSessionDescriptor[29], this->m_sSessions[i].psTracks[x].bFileNameLength);

				// Put a null terminating character at the end of the file name.
				this->m_sSessions[i].psTracks[x].psFileName[this->m_sSessions[i].psTracks[x].bFileNameLength] = 0;
				pbSessionDescriptor += 29 + this->m_sSessions[i].psTracks[x].bFileNameLength;
				if (bVerbose == true) printf("\t\tfile name: %s\n", this->m_sSessions[i].psTracks[x].psFileName);

				// Check some value that only appears in DiscJuggler 4, but changes the session descriptor structure.
				pbSessionDescriptor += 19;
				if (CAST_TO_DWORD(pbSessionDescriptor, 0) == 0x80000000)
				{
					// Skip the next 8 bytes.
					pbSessionDescriptor += 8;
				}

				// Read the pregap length, and other length value.
				this->m_sSessions[i].psTracks[x].dwPregapLength = CAST_TO_DWORD(pbSessionDescriptor, 6);
				this->m_sSessions[i].psTracks[x].dwLength = CAST_TO_DWORD(pbSessionDescriptor, 10);
				if (bVerbose == true)
				{
					printf("\t\tpregap length: %d\n", this->m_sSessions[i].psTracks[x].dwPregapLength);
					printf("\t\tlength: %d\n", this->m_sSessions[i].psTracks[x].dwLength);
				}

				// Read the session mode.
				this->m_sSessions[i].psTracks[x].eMode = (CdiTrackMode)CAST_TO_DWORD(pbSessionDescriptor, 20);
				if (bVerbose == true)
				{
					printf("\t\tmode: ");
					switch (this->m_sSessions[i].psTracks[x].eMode)
					{
					case CdiTrackMode::Audio:	printf("Audio\n");	break;
					case CdiTrackMode::Mode1:	printf("Mode1\n");	break;
					case CdiTrackMode::Mode2:	printf("Mode2\n");	break;
					default:					printf("Unknown\n");	break;
					}
				}

				// Check the track mode is valid.
				if (this->m_sSessions[i].psTracks[x].eMode > CdiTrackMode::Mode2)
				{
					// Print error and return.
					printf("CdiFileHandle::ParseSessionDescriptor(): invalid/unsupported track mode %d!\n", this->m_sSessions[i].psTracks[x].eMode);
					return false;
				}

				// Read the lba and total length.
				this->m_sSessions[i].psTracks[x].dwLba = CAST_TO_DWORD(pbSessionDescriptor, 36);
				this->m_sSessions[i].psTracks[x].dwTotalLength = CAST_TO_DWORD(pbSessionDescriptor, 40);
				if (bVerbose == true)
				{
					printf("\t\tlba: %d\n", this->m_sSessions[i].psTracks[x].dwLba);
					printf("\t\ttotal length: %d\n", this->m_sSessions[i].psTracks[x].dwTotalLength);
				}

				// Read the sector size value.
				this->m_sSessions[i].psTracks[x].eSectorType = (CdiSectorType)CAST_TO_DWORD(pbSessionDescriptor, 60);
				switch (this->m_sSessions[i].psTracks[x].eSectorType)
				{
				case CdiSectorType::Type_2048:	this->m_sSessions[i].psTracks[x].eSectorSize = CdiSectorSize::Size_2048;	break;
				case CdiSectorType::Type_2336:	this->m_sSessions[i].psTracks[x].eSectorSize = CdiSectorSize::Size_2336;	break;
				case CdiSectorType::Type_2352:	this->m_sSessions[i].psTracks[x].eSectorSize = CdiSectorSize::Size_2352;	break;
				case CdiSectorType::Type_2368:	this->m_sSessions[i].psTracks[x].eSectorSize = CdiSectorSize::Size_2368;	break;
				case CdiSectorType::Type_2448:	this->m_sSessions[i].psTracks[x].eSectorSize = CdiSectorSize::Size_2448;	break;
				default:						this->m_sSessions[i].psTracks[x].eSectorSize = (CdiSectorSize)0;			break;
				}

				// Check that the sector size is valid.
				if (bVerbose == true) printf("\t\tsector size: %d\n", this->m_sSessions[i].psTracks[x].eSectorSize);
				if (this->m_sSessions[i].psTracks[x].eSectorSize == 0)
				{
					// Print error and return.
					printf("CdiFileHandle::ParseSessionDescriptor(): invalid/unsupported sector size %d!\n", this->m_sSessions[i].psTracks[x].eSectorSize);
					return false;
				}

				// Next session.
				pbSessionDescriptor += 93;

				// If the image format isn't type 1 we need to check for extra data to skip over.
				if (eDescriptorType != CdiSessionDescriptorType::Type1)
				{
					// Read some dword and see if there is extra data we need to skip.
					if (CAST_TO_DWORD(pbSessionDescriptor, 5) == 0xFFFFFFFF)
						pbSessionDescriptor += 78; // (DJ 3.00.780 and up)

					// Skip the data we just read.
					pbSessionDescriptor += 9;
				}
				else
					DebugBreak();
			}

			// NOTE: There are 12 bytes unaccounted for.
			pbSessionDescriptor += 12;

			// Next session.
			if (eDescriptorType != CdiSessionDescriptorType::Type1)
				pbSessionDescriptor += 1;
		}

		// Create an initialize our shadow collection if the CdiSession objects.
		this->m_pSessionCollection = new DisjointCollection<CdiSession>(this->m_sSessions, this->m_wSessionCount);

		// Everything seems to check out.
		printf("\n");
		return true;
	}

	bool CdiFileHandle::ReadSectors(DWORD dwSessionNumber, DWORD dwTrackNumber, DWORD dwLBA, PBYTE pbBuffer, DWORD dwSectorCount)
	{
		DWORD dwBytesRead = 0;

		// Check that the session number and track number are valid.
		if (dwSessionNumber >= this->m_wSessionCount || dwTrackNumber >= this->m_sSessions[dwSessionNumber].wTrackCount)
			return false;

		// Check to make sure the data to be read wont go beyond the end of the track.
		if (dwSectorCount > this->m_sSessions[dwSessionNumber].psTracks[dwTrackNumber].dwLength)
		{
			// Print an error and return.
			printf("CdiFileHandle::ReadSectors(): read operation would go beyond the length of the track!\n");
			return false;
		}

		// Pull out the track struct for easy access.
		CdiTrack *pTargetTrack = &this->m_sSessions[dwSessionNumber].psTracks[dwTrackNumber];

		// Check to see if we are already at the target LBA or if we need to seek.
		if (dwLBA != this->m_dwCurrentLBA)
		{
			// Set the new current LBA.
			this->m_dwCurrentLBA = dwLBA;

			// Loop through all of the sessions until we get to the one we want.
			DWORD dwTargetOffset = 0;
			for (int i = 0; i <= dwSessionNumber; i++)
			{
				// Loop through all of the tracks until we get to the one we want.
				WORD wTrackCount = (i == dwSessionNumber ? dwTrackNumber : this->m_sSessions[i].wTrackCount);
				for (int x = 0; x < wTrackCount; x++)
				{
					// Skip over this track.
					dwTargetOffset += this->m_sSessions[i].psTracks[x].dwTotalLength * this->m_sSessions[i].psTracks[x].eSectorSize;
				}
			}

			// Skip the pregap section of the track.
			dwTargetOffset += pTargetTrack->dwPregapLength * pTargetTrack->eSectorSize;

			// Seek to the target LBA.
			dwTargetOffset += pTargetTrack->eSectorSize * (dwLBA - pTargetTrack->dwLba);
			SetFilePointer(this->m_hFile, dwTargetOffset, NULL, FILE_BEGIN);
		}

		// We need to know the header size of the track in order to read data from it.
		DWORD dwHeaderSize = 0;
		if (pTargetTrack->eMode == CdiTrackMode::Mode2)
		{
			// Check the sector size to determin the header size.
			if (pTargetTrack->eSectorSize == CdiSectorSize::Size_2352)
				dwHeaderSize = 24;
			else if (pTargetTrack->eSectorSize == CdiSectorSize::Size_2336)
				dwHeaderSize = 8;
		}
		else
		{
			// Check the sector size to determin the header size.
			if (pTargetTrack->eSectorSize == CdiSectorSize::Size_2352)
				dwHeaderSize = 16;
		}

		// Allocate a working buffer for the read operation.
		BYTE *pbTempBuffer = new BYTE[pTargetTrack->eSectorSize];

		// Loop through all of the requested sectors.
		for (int i = 0; i < dwSectorCount; i++)
		{
			// Read a raw sector from the image file.
			if (ReadFile(this->m_hFile, pbTempBuffer, pTargetTrack->eSectorSize, &dwBytesRead, NULL) == false ||
				dwBytesRead != pTargetTrack->eSectorSize)
			{
				// Failed to read the sector from the image file.
				printf("CdiFileHandle::ReadSectors(): failed to read sector! LBA=%d, Sector=%d, Size=%d!\n", 
					dwLBA, i, pTargetTrack->eSectorSize);

				// Free the temporary working buffer.
				delete[] pbTempBuffer;
				return false;
			}

			// Check if the track is an audio track or data track.
			if (pTargetTrack->eMode == CdiTrackMode::Audio)
			{
				// Copy the whole raw sector to the output buffer.
				memcpy(&pbBuffer[i * pTargetTrack->eSectorSize], pbTempBuffer, pTargetTrack->eSectorSize);
			}
			else
			{
				// Copy the real sector to the output buffer.
				memcpy(&pbBuffer[i * RAW_SECTOR_SIZE], &pbTempBuffer[dwHeaderSize], RAW_SECTOR_SIZE);
			}

			// Increment the current LBA.
			this->m_dwCurrentLBA++;
		}

		// Free the temporary working buffer.
		delete[] pbTempBuffer;

		// Done, successfully read the data.
		return true;
	}

	bool CdiFileHandle::WriteSectors(DWORD dwSessionNumber, DWORD dwTrackNumber, DWORD dwLBA, PBYTE pbBuffer, DWORD dwSectorCount)
	{
		DWORD dwBytesWritten = 0;

		// Check that the session number and track number are valid.
		if (dwSessionNumber >= this->m_wSessionCount || dwTrackNumber >= this->m_sSessions[dwSessionNumber].wTrackCount)
			return false;

		// Check to make sure the data to be written wont go beyond the end of the track.
		if (dwSectorCount > this->m_sSessions[dwSessionNumber].psTracks[dwTrackNumber].dwLength)
		{
			// Print an error and return.
			printf("CdiFileHandle::WriteSectors(): write operation would go beyond the length of the track!\n");
			return false;
		}

		// Pull out the track struct for easy access.
		CdiTrack *pTargetTrack = &this->m_sSessions[dwSessionNumber].psTracks[dwTrackNumber];

		// Check to see if we are already at the target LBA or if we need to seek.
		if (dwLBA != this->m_dwCurrentLBA)
		{
			// Set the new current LBA.
			this->m_dwCurrentLBA = dwLBA;

			// Loop through all of the sessions until we get to the one we want.
			DWORD dwTargetOffset = 0;
			for (int i = 0; i <= dwSessionNumber; i++)
			{
				// Loop through all of the tracks until we get to the one we want.
				WORD wTrackCount = (i == dwSessionNumber ? dwTrackNumber : this->m_sSessions[i].wTrackCount);
				for (int x = 0; x < wTrackCount; x++)
				{
					// Skip over this track.
					dwTargetOffset += this->m_sSessions[i].psTracks[x].dwTotalLength * this->m_sSessions[i].psTracks[x].eSectorSize;
				}
			}

			// Skip the pregap section of the track.
			dwTargetOffset += pTargetTrack->dwPregapLength * pTargetTrack->eSectorSize;

			// Seek to the target LBA.
			dwTargetOffset += pTargetTrack->eSectorSize * (dwLBA - pTargetTrack->dwLba);
			SetFilePointer(this->m_hFile, dwTargetOffset, NULL, FILE_BEGIN);
		}

		// We need to know the header size of the track in order to write data to it.
		DWORD dwHeaderSize = 0;
		if (pTargetTrack->eMode == CdiTrackMode::Mode2)
		{
			// Check the sector size to determin the header size.
			if (pTargetTrack->eSectorSize == CdiSectorSize::Size_2352)
				dwHeaderSize = 24;
			else if (pTargetTrack->eSectorSize == CdiSectorSize::Size_2336)
				dwHeaderSize = 8;
		}
		else
		{
			// Check the sector size to determin the header size.
			if (pTargetTrack->eSectorSize == CdiSectorSize::Size_2352)
				dwHeaderSize = 16;
		}

		// Compute the sector size we will be writing in.
		DWORD dwSectorSize = (pTargetTrack->eMode == CdiTrackMode::Audio ? pTargetTrack->eSectorSize : RAW_SECTOR_SIZE);

		// Loop through all of the sectors and write each one.
		for (int i = 0; i < dwSectorCount; i++)
		{
			// If the track is not an Audio track skip the header.
			if (pTargetTrack->eMode != CdiTrackMode::Audio)
			{
				// Skip the sector header.
				SetFilePointer(this->m_hFile, dwHeaderSize, NULL, FILE_CURRENT);
			}

			// Write the raw sector to the image.
			if (WriteFile(this->m_hFile, &pbBuffer[i * dwSectorSize], dwSectorSize, &dwBytesWritten, NULL) == false ||
				dwBytesWritten != dwSectorSize)
			{
				// Failed to write the sector to the file.
				printf("CdiFileHandle::WriteSectors(): failed to write sector! LBA=%d, Sector=%d, Size=%d!\n",
					dwLBA, i, pTargetTrack->eSectorSize);
				return false;
			}

			// Increment the current LBA.
			this->m_dwCurrentLBA++;
		}

		// Done, successfully wrote the sectors to file.
		return true;
	}

	DisjointCollection<CdiSession>& CdiFileHandle::GetSessionsCollection()
	{
		// Return our shadow collection of CdiSession objects.
		return *this->m_pSessionCollection;
	}

	CdiTrackHandle *CdiFileHandle::OpenTrackHandle(DWORD dwSessionNumber, DWORD dwTrackNumber)
	{
		// Check that the session number is valid.
		if (dwSessionNumber < 0 || dwSessionNumber >= this->m_wSessionCount)
		{
			// The session number is invalid, print an error and return.
			printf("CdiFileHandle::OpenTrackHandle(): session number specified is invalid!\n");
			return nullptr;
		}

		// Check that the track number is valid.
		if (dwTrackNumber < 0 || dwTrackNumber >= this->m_sSessions[dwSessionNumber].wTrackCount)
		{
			// The track number is invalid, print an error and return.
			printf("CdiFileHandle::OpenTrackHandle(): track number specified is invalid!\n");
			return nullptr;
		}

		// Create a new track handle and initialize it to the track specified.
		CdiTrackHandle *pTrackHandle = new CdiTrackHandle();
		pTrackHandle->dwSessionNumber = dwSessionNumber;
		pTrackHandle->dwTrackNumber = dwTrackNumber;
		pTrackHandle->pFileHandle = this;
		pTrackHandle->pTrack = &this->m_sSessions[dwSessionNumber].psTracks[dwTrackNumber];

		// Return the track handle.
		return pTrackHandle;
	}

	void CdiFileHandle::CloseTrackHandle(CdiTrackHandle *pTrackHandle)
	{
		// Free the handle allocation.
		delete pTrackHandle;
	}
};