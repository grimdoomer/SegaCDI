/*
	SegaCDI - Sega Dreamcast cdi image validator.

	CdiImage.cpp - Support for DiscJuggler cdi images.

	Mar 6th, 2013
*/

#include "stdafx.h"
#include "CdiImage.h"
#include "Utilities.h"
#include "MRImage.h"

CCdiImage::CCdiImage()
{
	// Initialize the handle value.
	this->m_hFile = INVALID_HANDLE_VALUE;
}

CCdiImage::~CCdiImage()
{
	// Check if the file handle is still open.
	if (this->m_hFile != INVALID_HANDLE_VALUE)
	{
		// Close the cdi image.
		CloseHandle(this->m_hFile);
		this->m_hFile = INVALID_HANDLE_VALUE;
	}
}

bool CCdiImage::LoadImage(CString sFileName, bool bVerbos)
{
	DWORD dwCount = 0;

	// Save the file name and open the cdi image file.
	this->m_sFileName = sFileName;
	this->m_hFile = CreateFile(this->m_sFileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (this->m_hFile == INVALID_HANDLE_VALUE)
	{
		// Print error and return.
		printf("ERROR: could not find file %s!\n", this->m_sFileName);
		return false;
	}

	// Get the file size of the image and check it is valid.
	this->m_dwFileSize = GetFileSize(this->m_hFile, NULL);
	if (this->m_dwFileSize == 0)
	{
		// Print error, close the file, and return.
		printf("ERROR: image file %s has invalid size!", this->m_sFileName);
		CloseHandle(this->m_hFile);
		return false;
	}

	// Seek to the end of the file - 8 bytes to get the descriptor info block.
	CdiSessionDescriptorInfo sDescriptorInfo = { 0 };
	SetFilePointer(this->m_hFile, -sizeof(CdiSessionDescriptorInfo), NULL, FILE_END);
	ReadFile(this->m_hFile, &sDescriptorInfo, sizeof(CdiSessionDescriptorInfo), &dwCount, NULL);

	// Verify the descriptor type.
	if (sDescriptorInfo.dwDescriptorType != CDI_SESSION_DESCRIPTOR_TYPE1 &&
		sDescriptorInfo.dwDescriptorType != CDI_SESSION_DESCRIPTOR_TYPE2 &&
		sDescriptorInfo.dwDescriptorType != CDI_SESSION_DESCRIPTOR_TYPE3)
	{
		// Print error, close the file, and return.
		printf("ERROR: invalid descriptor version %d!", sDescriptorInfo.dwDescriptorType);
		CloseHandle(this->m_hFile);
		return false;
	}

	// Print the cdi version.
	if (sDescriptorInfo.dwDescriptorType == CDI_SESSION_DESCRIPTOR_TYPE1)
		printf("found cdi version 2\n");
	else if (sDescriptorInfo.dwDescriptorType == CDI_SESSION_DESCRIPTOR_TYPE2)
		printf("found cdi version 3\n");
	else if (sDescriptorInfo.dwDescriptorType == CDI_SESSION_DESCRIPTOR_TYPE3)
		printf("found cdi version 3.5\n");

	// Allocate a buffer for the session descriptor block.
	DWORD dwSessionDescriptorSize = (sDescriptorInfo.dwDescriptorType == CDI_SESSION_DESCRIPTOR_TYPE3 ?
		sDescriptorInfo.dwDescriptorHelper : this->m_dwFileSize - sDescriptorInfo.dwDescriptorHelper);
	BYTE *pbSessionDescriptor = new BYTE[dwSessionDescriptorSize];

	// Read the session descriptor block from the image.
	SetFilePointer(this->m_hFile, -(dwSessionDescriptorSize), NULL, FILE_END);
	ReadFile(this->m_hFile, pbSessionDescriptor, dwSessionDescriptorSize, &dwCount, NULL);

	// Parse the session descriptor block.
	if (bVerbos == true) printf("found session descriptor of size %d\n", dwSessionDescriptorSize);
	if (ParseSessionDescriptor(pbSessionDescriptor, dwSessionDescriptorSize, sDescriptorInfo.dwDescriptorType, bVerbos) == FALSE)
	{
		// There was an error reading the session descriptor, close the file and return.
		CloseHandle(this->m_hFile);
		delete[] pbSessionDescriptor;
		return false;
	}

	// Delete temp buffer.
	delete[] pbSessionDescriptor;

	// Try to parse the bootstrap.
	if (LoadBootstrap(bVerbos) == false)
	{
		// The bootstrap is invalid, close the file and return.
		CloseHandle(this->m_hFile);
		return false;
	}

	// Done, return true.
	//CloseHandle(this->m_hFile);
	return true;
}

bool CCdiImage::ParseSessionDescriptor(PBYTE pbSessionDescriptor, DWORD dwDescriptorSize, DWORD dwDescriptorType, bool bVerbos)
{
	// Get the session count and create our session info array.
	this->m_wSessionCount = CAST_TO_WORD(pbSessionDescriptor, 0);
	this->m_sSessions = new CdiSession[this->m_wSessionCount];
	printf("found %d sessions\n", this->m_wSessionCount);// + 1);
	pbSessionDescriptor += 2;

	// For now just loop through the tracks until we make a structure for it.
	for (int i = 0; i < this->m_wSessionCount; i++)
	{
		// Set the session number.
		if (bVerbos == true) printf("\nsession %d:\n", i + 1);
		this->m_sSessions[i].dwSessionNumber = i;

		// The first word is the track count.
		this->m_sSessions[i].wTrackCount = CAST_TO_WORD(pbSessionDescriptor, 0);
		if (bVerbos == true) printf("\ttrack count: %d\n", this->m_sSessions[i].wTrackCount);
		pbSessionDescriptor += 2;

		// NOTE: 0 tracks means the session is open, we need to handle this.

		// Create the track array.
		this->m_sSessions[i].psTracks = new CdiTrack[this->m_sSessions[i].wTrackCount];

		// Loop through all the tracks and read them from the descriptor.
		for (int x = 0; x < this->m_sSessions[i].wTrackCount; x++)
		{
			// Set the track number.
			if (bVerbos == true) printf("\n\ttrack %d\n", x + 1);
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
				printf("WARNING: session descriptor static field 1 (track start marker) mismatch!\n");
				DebugBreak();
			}

			// I really have no idea what the next 4 bytes are, the information at hand is rudimentary at best.
			BYTE pbUnknown1[4] = { pbSessionDescriptor[24], pbSessionDescriptor[25], pbSessionDescriptor[26], pbSessionDescriptor[27] };
			if (bVerbos == true) printf("\t\tunknown bytes 1: %x %x %x %x\n", pbUnknown1[0], pbUnknown1[1], pbUnknown1[2], pbUnknown1[3]);

			// Next we have the file name of this session.
			this->m_sSessions[i].psTracks[x].bFileNameLength = CAST_TO_BYTE(pbSessionDescriptor, 28);
			this->m_sSessions[i].psTracks[x].psFileName = new CHAR[this->m_sSessions[i].psTracks[x].bFileNameLength + 1];
			memcpy(this->m_sSessions[i].psTracks[x].psFileName, &pbSessionDescriptor[29], this->m_sSessions[i].psTracks[x].bFileNameLength);

			// Put a null terminating character at the end of the file name.
			this->m_sSessions[i].psTracks[x].psFileName[this->m_sSessions[i].psTracks[x].bFileNameLength] = 0;
			pbSessionDescriptor += 29 + this->m_sSessions[i].psTracks[x].bFileNameLength;
			if (bVerbos == true) printf("\t\tfile name: %s\n", this->m_sSessions[i].psTracks[x].psFileName);

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
			if (bVerbos == true)
			{
				printf("\t\tpregap length: %d\n", this->m_sSessions[i].psTracks[x].dwPregapLength);
				printf("\t\tlength: %d\n", this->m_sSessions[i].psTracks[x].dwLength);
			}

			// Read the session mode.
			this->m_sSessions[i].psTracks[x].dwMode = CAST_TO_DWORD(pbSessionDescriptor, 20);
			if (bVerbos == true) 
			{
				printf("\t\tmode: ");
				switch (this->m_sSessions[i].psTracks[x].dwMode)
				{
				case CDI_TRACK_MODE_AUDIO:	printf("Audio\n");	break;
				case CDI_TRACK_MODE_1:		printf("Mode1\n");	break;
				case CDI_TRACK_MODE_2:		printf("Mode2\n");	break;
				default:					printf("Unknown\n");	break;
				}
			}

			// Check the track mode is valid.
			if (this->m_sSessions[i].psTracks[x].dwMode > 2)
			{
				// Print error and return.
				printf("ERROR: invalid/unsupported track mode %d!\n", this->m_sSessions[i].psTracks[x].dwMode);
				return false;
			}

			// Read the lba and total length.
			this->m_sSessions[i].psTracks[x].dwLba = CAST_TO_DWORD(pbSessionDescriptor, 36);
			this->m_sSessions[i].psTracks[x].dwTotalLength = CAST_TO_DWORD(pbSessionDescriptor, 40);
			if (bVerbos == true)
			{
				printf("\t\tlba: %d\n", this->m_sSessions[i].psTracks[x].dwLba);
				printf("\t\ttotal length: %d\n", this->m_sSessions[i].psTracks[x].dwTotalLength);
			}

			// Read the sector size value.
			this->m_sSessions[i].psTracks[x].dwSectorSizeValue = CAST_TO_DWORD(pbSessionDescriptor, 60);
			switch (this->m_sSessions[i].psTracks[x].dwSectorSizeValue)
			{
			case CDI_SECTOR_SIZE_2048:	this->m_sSessions[i].psTracks[x].dwSectorSize = 2048;	break;
			case CDI_SECTOR_SIZE_2336:	this->m_sSessions[i].psTracks[x].dwSectorSize = 2336;	break;
			case CDI_SECTOR_SIZE_2352:	this->m_sSessions[i].psTracks[x].dwSectorSize = 2352;	break;
			case CDI_SECTOR_SIZE_2368:	this->m_sSessions[i].psTracks[x].dwSectorSize = 2368;	break;
			case CDI_SECTOR_SIZE_2448:	this->m_sSessions[i].psTracks[x].dwSectorSize = 2448;	break;
			default:					this->m_sSessions[i].psTracks[x].dwSectorSize = 0;		break;
			}

			// Check that the sector size is valid.
			if (bVerbos == true) printf("\t\tsector size: %d\n", this->m_sSessions[i].psTracks[x].dwSectorSize);
			if (this->m_sSessions[i].psTracks[x].dwSectorSize == 0)
			{
				// Print error and return.
				printf("ERROR: invalid/unsupported sector size value %d!\n", this->m_sSessions[i].psTracks[x].dwSectorSizeValue);
				return false;
			}

			// Next session.
			pbSessionDescriptor += 93;

			// If the image format isn't type 1 we need to check for extra data to skip over.
			if (dwDescriptorType != CDI_SESSION_DESCRIPTOR_TYPE1)
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
		if (dwDescriptorType != CDI_SESSION_DESCRIPTOR_TYPE1)
			pbSessionDescriptor += 1;
	}

	// Everything seems to check out.
	printf("\n");
	return true;
}

bool CCdiImage::ReadSectors(DWORD dwSessionNumber, DWORD dwTrackNumber, DWORD dwLBA, PBYTE pbBuffer, DWORD dwSectorCount)
{
	DWORD dwCount = 0;

	// Check that the session number and track number are valid.
	if (dwSessionNumber >= this->m_wSessionCount || dwTrackNumber >= this->m_sSessions[dwSessionNumber].wTrackCount)
		return false;

	// Pull out the track struct for easy access.
	CdiTrack *pTargetTrack = &this->m_sSessions[dwSessionNumber].psTracks[dwTrackNumber];

	// Seek to the beginning of the file.
	SetFilePointer(this->m_hFile, 0, NULL, FILE_BEGIN);

	// Loop through all of the sessions until we get to the one we want.
	for (int i = 0; i <= dwSessionNumber; i++)
	{
		// Loop through all of the tracks until we get to the one we want.
		WORD wTrackCount = (i == dwSessionNumber ? dwTrackNumber : this->m_sSessions[i].wTrackCount);
		for (int x = 0; x < wTrackCount; x++)
		{
			// Skip over this track.
			SetFilePointer(this->m_hFile,
				this->m_sSessions[i].psTracks[x].dwTotalLength * this->m_sSessions[i].psTracks[x].dwSectorSize, NULL, FILE_CURRENT);
		}
	}

	// Skip the pregap section of the track.
	SetFilePointer(this->m_hFile, pTargetTrack->dwPregapLength * pTargetTrack->dwSectorSize, NULL, FILE_CURRENT);

	// Seek to the target LBA.
	SetFilePointer(this->m_hFile, pTargetTrack->dwSectorSize * (dwLBA - pTargetTrack->dwLba), NULL, FILE_CURRENT);

	// We need to know the header size of the track in order to read data from it.
	DWORD dwHeaderSize = 0;
	if (pTargetTrack->dwMode == CDI_TRACK_MODE_2)
	{
		// Check the sector size to determin the header size.
		if (pTargetTrack->dwSectorSizeValue == CDI_SECTOR_SIZE_2352)
			dwHeaderSize = 24;
		else if (pTargetTrack->dwSectorSizeValue == CDI_SECTOR_SIZE_2336)
			dwHeaderSize = 8;
	}
	else
	{
		// Check the sector size to determin the header size.
		if (pTargetTrack->dwSectorSizeValue == CDI_SECTOR_SIZE_2352)
			dwHeaderSize = 16;
	}

	// Allocate a working buffer for the read operation.
	BYTE *pbTempBuffer = new BYTE[pTargetTrack->dwSectorSize];

	// Loop through all of the requested sectors.
	for (int i = 0; i < dwSectorCount; i++)
	{
		// Read a raw sector from the image file.
		if (ReadFile(this->m_hFile, pbTempBuffer, pTargetTrack->dwSectorSize, &dwCount, NULL) == false ||
			dwCount != pTargetTrack->dwSectorSize)
		{
			// Failed to read the sector from the image file.
		}

		// Check if the track is an audio track or data track.
		if (pTargetTrack->dwMode == CDI_TRACK_MODE_AUDIO)
		{
			// Copy the whole raw sector to the output buffer.
			memcpy(&pbBuffer[i * pTargetTrack->dwSectorSize], pbTempBuffer, pTargetTrack->dwSectorSize);
		}
		else
		{
			// Copy the real sector to the output buffer.
			memcpy(&pbBuffer[i * 2048], &pbTempBuffer[dwHeaderSize], 2048);
		}
	}

	// Free the temporary working buffer.
	delete[] pbTempBuffer;

	// Done, successfully read the data.
	return true;
}

bool CCdiImage::LoadBootstrap(bool bVerbos)
{
	// Set the file position to the cdi image start.
	SetFilePointer(this->m_hFile, 0, NULL, FILE_BEGIN);
	printf("searching for IP.BIN...\n");

	// Loop through all the sessions and search for one with a DATA track.
	for (int i = 0; i < this->m_wSessionCount; i++)
	{
		// Search for a track that is DATA.
		for (int x = 0; x < this->m_sSessions[i].wTrackCount; x++)
		{
			// Print status.
			printf("\rsearching session %d track %d...", i + 1, x + 1);
			fflush(stdout);

			// Check the mode of this track.
			if (this->m_sSessions[i].psTracks[x].dwMode == CDI_TRACK_MODE_AUDIO)
			{
				// Skip over this track.
				SetFilePointer(this->m_hFile, 
					this->m_sSessions[i].psTracks[x].dwTotalLength * this->m_sSessions[i].psTracks[x].dwSectorSize, NULL, FILE_CURRENT);
			}
			else
			{
				// We need to know the header size of the track in order to properly convert it to iso/binary.
				DWORD dwHeaderSize = 0;
				if (this->m_sSessions[i].psTracks[x].dwMode == CDI_TRACK_MODE_2)
				{
					// Check the sector size to determin the header size.
					if (this->m_sSessions[i].psTracks[x].dwSectorSizeValue == CDI_SECTOR_SIZE_2352)
						dwHeaderSize = 24;
					else if (this->m_sSessions[i].psTracks[x].dwSectorSizeValue == CDI_SECTOR_SIZE_2336)
						dwHeaderSize = 8;
				}
				else
				{
					// Check the sector size to determin the header size.
					if (this->m_sSessions[i].psTracks[x].dwSectorSizeValue == CDI_SECTOR_SIZE_2352)
						dwHeaderSize = 16;
				}

				// Skip the pregap section of the track.
				SetFilePointer(this->m_hFile, this->m_sSessions[i].psTracks[x].dwPregapLength * 
					this->m_sSessions[i].psTracks[x].dwSectorSize, NULL, FILE_CURRENT);

				// Allocate a working buffer.
				DWORD Count = 0;
				BYTE *pbBuffer = new BYTE[this->m_sSessions[i].psTracks[x].dwSectorSize];

				// Read the first sector from the track.
				ReadFile(this->m_hFile, pbBuffer, this->m_sSessions[i].psTracks[x].dwSectorSize, &Count, NULL);

				// Check the firt 4 bytes for 'SEGA'.
				if (memcmp(&pbBuffer[dwHeaderSize], HARDWARE_ID, 16) == 0)
				{
					// We found the correct track.
					printf("\nfound IP.BIN\n");

					// Allocate a buffer for the bootstrap data.
					char *pbBootstrapBuffer = new char[BOOTSTRAP_SIZE];

					// Copy the sector we already read.
					memcpy(pbBootstrapBuffer, &pbBuffer[dwHeaderSize], 2048);

					// Loop and dump the rest of the IP.BIN file.
					for (int k = 0; k < IP_BIN_SECTOR_COUNT - 1; k++)
					{
						// Read the next sector from the track.
						ReadFile(this->m_hFile, pbBuffer, this->m_sSessions[i].psTracks[x].dwSectorSize, &Count, NULL);

						// Copy to bootstrap buffer.
						memcpy(&pbBootstrapBuffer[2048 + (k * 2048)], &pbBuffer[dwHeaderSize], 2048);
					}

					// Try to parse the bootstrap data.
					if (this->m_sBootstrap.LoadBootstrap(pbBootstrapBuffer, BOOTSTRAP_SIZE) == false)
					{
						// Print error and return false.
						printf("ERROR: IP.BIN is invalid!\n");

						// Deallocate temp buffers.
						delete[] pbBootstrapBuffer;
						delete[] pbBuffer;

						// Return false.
						return false;
					}

					// Bootstrap appears to be valid.
					printf("bootstrap appears to be valid\n");

					// Deallocate the sector buffer.
					delete[] pbBootstrapBuffer;
					delete[] pbBuffer;
					
					// Done, return true.
					return true;
				}
				else
				{
					// Skip the rest of this track.
					SetFilePointer(this->m_hFile, (this->m_sSessions[i].psTracks[x].dwLength - 1) * this->m_sSessions[i].psTracks[x].dwSectorSize, NULL, FILE_CURRENT);
				}

				// Deallocate the sector buffer.
				delete[] pbBuffer;
			}
		}
	}

	// Bootstrap was not found.
	printf("ERROR: could not find bootstrap!n");
	return false;
}

bool CCdiImage::DumpTrackToFile(CString sOutputFolder, DWORD dwSessionNumber, DWORD dwTrackNumber)
{
	// Correct the session and track numbers.
	dwSessionNumber--;
	dwTrackNumber--;

	// Check the session number is valid.
	if (dwSessionNumber < 0 || dwSessionNumber >= this->m_wSessionCount)
	{
		// Print error and return.
		printf("ERROR: requested session %d is invalid!\n", dwSessionNumber + 1);
		return false;
	}

	// Check there is at least 1 closed track for this session.
	if (this->m_sSessions[dwSessionNumber].wTrackCount == 0)
	{
		// Print error and return.
		printf("ERROR: no tracks found or track is open for session %d!\n", dwSessionNumber + 1);
		return false;
	}

	// Check if the track number is valid.
	if (dwTrackNumber < 0 || dwTrackNumber >= this->m_sSessions[dwSessionNumber].wTrackCount)
	{
		// Print error and return.
		printf("ERROR: requested track %d is invalid!\n", dwTrackNumber + 1);
		return false;
	}

	// Format the output file name.
	CHAR sFileName[MAX_PATH] = { 0 };
	LPCSTR sTrackName = (this->m_sSessions[dwSessionNumber].psTracks[dwTrackNumber].dwMode == CDI_TRACK_MODE_AUDIO ? "Audio" : "Data");
	LPCSTR sTrackExt = (this->m_sSessions[dwSessionNumber].psTracks[dwTrackNumber].dwMode == CDI_TRACK_MODE_AUDIO ? "wav" : "iso");
	sprintf(sFileName, "%s\\T%s%d-%d.%s", sOutputFolder, sTrackName, dwSessionNumber + 1, dwTrackNumber + 1, sTrackExt);

	// Create the iso file.
	HANDLE hTrackFile = CreateFile(sFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hTrackFile == INVALID_HANDLE_VALUE)
	{
		// Print error and return.
		printf("ERROR: could not create output file %s!\n", sFileName);
		return false;
	}

	// Set the file position to the cdi image start.
	SetFilePointer(this->m_hFile, 0, NULL, FILE_BEGIN);

	// Seek to the session start by skipping over every preceeding session.
	for (int i = 0; i <= dwSessionNumber; i++)
	{
		// Skip over all the tracks in this session.
		int trackCount = (i == dwSessionNumber ? dwTrackNumber : this->m_sSessions[i].wTrackCount);
		for (int x = 0; x < trackCount; x++)
		{
			// Skip over this track.
			SetFilePointer(this->m_hFile, 
				this->m_sSessions[i].psTracks[x].dwTotalLength * this->m_sSessions[i].psTracks[x].dwSectorSize, NULL, FILE_CURRENT);
		}
	}

	// We need to know the header size of the track in order to properly convert it to iso.
	DWORD dwHeaderSize = 0;
	if (this->m_sSessions[dwSessionNumber].psTracks[dwTrackNumber].dwMode == CDI_TRACK_MODE_2)
	{
		// Check the sector size to determin the header size.
		if (this->m_sSessions[dwSessionNumber].psTracks[dwTrackNumber].dwSectorSizeValue == CDI_SECTOR_SIZE_2352)
			dwHeaderSize = 24;
		else if (this->m_sSessions[dwSessionNumber].psTracks[dwTrackNumber].dwSectorSizeValue == CDI_SECTOR_SIZE_2336)
			dwHeaderSize = 8;
	}
	else
	{
		// Check the sector size to determin the header size.
		if (this->m_sSessions[dwSessionNumber].psTracks[dwTrackNumber].dwSectorSizeValue == CDI_SECTOR_SIZE_2352)
			dwHeaderSize = 16;
	}

	// Check if the track is of Audio type.
	if (this->m_sSessions[dwSessionNumber].psTracks[dwTrackNumber].dwMode == CDI_TRACK_MODE_AUDIO)
	{
		// Write a wav header for it.
		if (WriteWavHeader(hTrackFile, this->m_sSessions[dwSessionNumber].psTracks[dwTrackNumber].dwLength) == false)
		{
			// Print error, close file and return false.
			printf("ERROR: could not write wav header for file %s!\n", sFileName);
			CloseHandle(hTrackFile);
			return false;
		}
	}

	// Allocate a working buffer.
	DWORD Count = 0;
	BYTE *pbBuffer = new BYTE[this->m_sSessions[dwSessionNumber].psTracks[dwTrackNumber].dwSectorSize];

	// Skip the pregap section of the track.
	SetFilePointer(this->m_hFile, this->m_sSessions[dwSessionNumber].psTracks[dwTrackNumber].dwPregapLength * 
		this->m_sSessions[dwSessionNumber].psTracks[dwTrackNumber].dwSectorSize, NULL, FILE_CURRENT);

	// Loop through all the sectors for this track.
	for (int i = 0; i < this->m_sSessions[dwSessionNumber].psTracks[dwTrackNumber].dwLength; i++)
	{
		// Print progress.
		printf("\rsaving track \t%d \t%s/%d \t%.2f%%", dwTrackNumber, sTrackName, this->m_sSessions[dwSessionNumber].psTracks[dwTrackNumber].dwSectorSize,
			(float)((float)(i + 1) / (float)this->m_sSessions[dwSessionNumber].psTracks[dwTrackNumber].dwLength) * 100.0f);
		fflush(stdout);

		// Read sector from the track.
		ReadFile(this->m_hFile, pbBuffer, this->m_sSessions[dwSessionNumber].psTracks[dwTrackNumber].dwSectorSize, &Count, NULL);

		// Check if we are writing an audio track or data track.
		if (this->m_sSessions[dwSessionNumber].psTracks[dwTrackNumber].dwMode == CDI_TRACK_MODE_AUDIO)
		{
			// Write the full sector.
			WriteFile(hTrackFile, pbBuffer, this->m_sSessions[dwSessionNumber].psTracks[dwTrackNumber].dwSectorSize, &Count, NULL);
		}
		else
		{
			// Write the sector minus the header size and extra bytes.
			WriteFile(hTrackFile, &pbBuffer[dwHeaderSize], 2048, &Count, NULL);
		}
	}

	// Delete temp buffer.
	delete[] pbBuffer;

	// Close the output iso file and return.
	printf("\n");
	CloseHandle(hTrackFile);
	return true;
}

bool CCdiImage::DumpAllTracks(CString sOutputFolder)
{
	// Loop through all the sessions and dump every track.
	for (int i = 0; i < this->m_wSessionCount; i++)
	{
		// Loop through all the tracks and dump each one.
		for (int x = 0; x < this->m_sSessions[i].wTrackCount; x++)
		{
			// Dump the track and check the result.
			if (this->DumpTrackToFile(sOutputFolder, i + 1, x + 1) == false)
			{
				// Print error and return.
				printf("ERROR: failed to dump track %d from session %d!\n", i + 1, x + 1);
				return false;
			}
		}
	}

	// Done.
	return true;
}

bool CCdiImage::DumpIPBin(CString sOutputFolder)
{
	// Format the output file name.
	CHAR sFileName[MAX_PATH] = { 0 };
	sprintf(sFileName, "%s\\IP.BIN", sOutputFolder);

	// Create the output file.
	HANDLE hOutputFile = CreateFile(sFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hOutputFile == INVALID_HANDLE_VALUE)
	{
		// Print error and return.
		printf("ERROR: failed to create output file %s!", sFileName);
		return false;
	}

	// Allocate a temp buffer for the bootstrap data.
	char *pbBootstrapBuffer = new char[BOOTSTRAP_SIZE];

	// Copy the bootstrap data.
	if (this->m_sBootstrap.CopyBootstrapBuffer(pbBootstrapBuffer, BOOTSTRAP_SIZE) == false)
	{
		// Print error and return false.
		printf("ERROR: what the actual fuck...\n");
		delete[] pbBootstrapBuffer;
		return false;
	}

	// Write the bootstrap data to file.
	DWORD count = 0;
	WriteFile(hOutputFile, pbBootstrapBuffer, BOOTSTRAP_SIZE, &count, NULL);

	// Close the output IP.BIN file.
	printf("successfully extracted IP.BIN\n");
	CloseHandle(hOutputFile);

	// Deallocate the bootstrap buffer.
	delete[] pbBootstrapBuffer;
	
	// Done, return true.
	return true;
}

bool CCdiImage::DumpMRImage(CString sOutputFolder)
{
	// Try to dump the boot logo.
	if (this->m_sBootstrap.Extract3rdPartyBootLogo(sOutputFolder) == true)
		printf("successfully extracted boot logo\n");
	else
		return false;

	// Done, return.
	return true;
}