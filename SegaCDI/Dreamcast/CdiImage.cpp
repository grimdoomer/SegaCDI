/*
	SegaCDI - Sega Dreamcast cdi image validator.

	CdiImage.cpp - Support for DiscJuggler cdi images.

	Mar 6th, 2013
		- Initial creation.

	Dec 1st, 2015
		- Added ReadSectors() function to reduce code bloat on repetitive operations.
*/

#include "../stdafx.h"
#include "CdiImage.h"
#include "MRImage.h"

namespace Dreamcast
{
	CdiImage::CdiImage()
	{
		// Initialize fields.
		this->m_pCdiFile = nullptr;
		this->m_dwFsSessionNumber = -1;
		this->m_dwFsTrackNumber = -1;
		this->m_phFsTrackHandle = nullptr;
		this->m_pFsIsoHandle = nullptr;
	}

	CdiImage::~CdiImage()
	{
	}

	bool CdiImage::LoadImage(CString sFileName, bool bVerbos)
	{
		// Initialize the file handle which will take care of parsing the disk juggler
		// format and giving us an easy to use api to read and write data with.
		this->m_pCdiFile = new DiskJuggler::CdiFileHandle();
		if (this->m_pCdiFile->Open(sFileName, false, bVerbos) == false)
		{
			// Failed to initialize the cdi file handle, close any file handles and return.
			goto Cleanup;
		}

		// Load and parse the bootstrap (ip.bin).
		if (this->LoadBootstrap(bVerbos) == false)
		{
			// Failed to read the bootstrap sector.
			printf("CdiImage::LoadImage(): failed to load bootstrap sector!\n");
			goto Cleanup;
		}

		// Initialize the file system track handle using the session and track numbers we found the bootstrap in.
		this->m_phFsTrackHandle = this->m_pCdiFile->OpenTrackHandle(this->m_dwFsSessionNumber, this->m_dwFsTrackNumber);
		if (this->m_phFsTrackHandle == nullptr)
		{
			// Failed to get a handle on the file system track.
			printf("CdiImage::LoadImage(): failed to open file system track!\n");
			goto Cleanup;
		}

		// Parse the ISO9660 structure and read out the file system.
		this->m_pFsIsoHandle = new ISO::ISO9660();
		if (this->m_pFsIsoHandle->LoadISOFromCDI(this->m_phFsTrackHandle, bVerbos) == false)
		{
			// Failed to load the ISO file system.
			goto Cleanup;
		}

		// Everything loaded okay, return true.
		return true;

	Cleanup:
		// Check if we created the ISO fs subsystem.
		if (this->m_pFsIsoHandle)
			delete this->m_pFsIsoHandle;

		// Cleanup the CDI image subsystem resources.
		if (this->m_phFsTrackHandle)
			this->m_pCdiFile->CloseTrackHandle(this->m_phFsTrackHandle);
		if (this->m_pCdiFile)
			delete this->m_pCdiFile;

		// Return false.
		return false;
	}

	bool CdiImage::LoadBootstrap(bool bVerbos)
	{
		// Allocate a scratch buffer for the bootstrap data.
		PBYTE pbBootstrapBuffer = (PBYTE)VirtualAlloc(NULL, BOOTSTRAP_SIZE, MEM_COMMIT, PAGE_READWRITE);
		if (pbBootstrapBuffer == NULL)
		{
			// Failed to allocate scratch memory.
			printf("CdiImage::LoadBootstrap(): failed to allocate memory for bootstrap data!\n");
			return false;
		}

		// Loop through all the sessions and search for one with a DATA track.
		printf("searching for IP.BIN...\n");
		DisjointCollection<DiskJuggler::CdiSession> sessionCollection = this->m_pCdiFile->GetSessionsCollection();
		for (int i = 0; i < sessionCollection.size(); i++)
		{
			// Search for a track that is DATA.
			for (int x = 0; x < sessionCollection[i]->wTrackCount; x++)
			{
				// Check if this track is a DATA track, if it's not skip it.
				if (sessionCollection[i]->psTracks[x].eMode == DiskJuggler::CdiTrackMode::Audio)
					continue;

				// Print status.
				printf("\rsearching session %d track %d...", i + 1, x + 1);
				fflush(stdout);

				// Read the first sector of the track so we can check for the bootstrap markings.
				if (this->m_pCdiFile->ReadSectors(i, x, sessionCollection[i]->psTracks[x].dwLba, pbBootstrapBuffer, 1) == false)
				{
					// Failed to read the first sector of the track.
					printf("\nCdiImage::LoadBootstrap(): failed to read first sector of session %d track %d!\n", i, x);
					continue;
				}

				// Check the firt 4 bytes for 'SEGA'.
				if (memcmp(pbBootstrapBuffer, HARDWARE_ID, 16) != 0)
					continue;

				// We found the correct track, save the session and track numbers for later.
				printf("\nfound IP.BIN\n");
				this->m_dwFsSessionNumber = i;
				this->m_dwFsTrackNumber = x;

				// Read the remaining sectors from the track.
				if (this->m_pCdiFile->ReadSectors(i, x, sessionCollection[i]->psTracks[x].dwLba + 1, &pbBootstrapBuffer[2048], BOOTSTRAP_SECTOR_COUNT - 1) == false)
				{
					// Failed to read the remaining bootstrap sectors.
					printf("\nCdiImage::LoadBootstrap(): failed to read bootstrap data in session %d track %d!\n", i, x);
					continue;
				}

				// Try to parse the bootstrap data.
				if (this->m_sBootstrap.LoadBootstrap((char*)pbBootstrapBuffer, BOOTSTRAP_SIZE) == false)
				{
					// Print error and return false.
					printf("ERROR: IP.BIN is invalid!\n");

					// Deallocate temp buffers.
					VirtualFree(pbBootstrapBuffer, BOOTSTRAP_SIZE, MEM_DECOMMIT);

					// Return false.
					return false;
				}

				// Bootstrap appears to be valid.
				printf("bootstrap appears to be valid\n");

				// Deallocate the sector buffer.
				VirtualFree(pbBootstrapBuffer, BOOTSTRAP_SIZE, MEM_DECOMMIT);

				// Done, return true.
				return true;
			}
		}

		// If the scratch buffer is still valid, free it.
		if (pbBootstrapBuffer)
			VirtualFree(pbBootstrapBuffer, BOOTSTRAP_SIZE, MEM_DECOMMIT);

		// Bootstrap was not found.
		printf("ERROR: could not find bootstrap!n");
		return false;
	}

	bool CdiImage::WriteTrackToFile(CString sOutputFolder, DWORD dwSessionNumber, DWORD dwTrackNumber)
	{
		// Correct the session and track numbers.
		dwSessionNumber--;
		dwTrackNumber--;

		// Get the collection of session objects from the file handle.
		DisjointCollection<DiskJuggler::CdiSession> sessionCollection = this->m_pCdiFile->GetSessionsCollection();

		// Check the session number is valid.
		if (dwSessionNumber < 0 || dwSessionNumber >= sessionCollection.size())
		{
			// Print error and return.
			printf("ERROR: requested session %d is invalid!\n", dwSessionNumber + 1);
			return false;
		}

		// Check there is at least 1 closed track for this session.
		if (sessionCollection[dwSessionNumber]->wTrackCount == 0)
		{
			// Print error and return.
			printf("ERROR: no tracks found or track is open for session %d!\n", dwSessionNumber + 1);
			return false;
		}

		// Check if the track number is valid.
		if (dwTrackNumber < 0 || dwTrackNumber >= sessionCollection[dwSessionNumber]->wTrackCount)
		{
			// Print error and return.
			printf("ERROR: requested track %d is invalid!\n", dwTrackNumber + 1);
			return false;
		}

		// Format the output file name.
		CHAR sFileName[MAX_PATH] = { 0 };
		LPCSTR sTrackName = (sessionCollection[dwSessionNumber]->psTracks[dwTrackNumber].eMode == DiskJuggler::CdiTrackMode::Audio ? "Audio" : "Data");
		LPCSTR sTrackExt = (sessionCollection[dwSessionNumber]->psTracks[dwTrackNumber].eMode == DiskJuggler::CdiTrackMode::Audio ? "wav" : "iso");
		sprintf(sFileName, "%s\\T%s%d-%d.%s", sOutputFolder, sTrackName, dwSessionNumber + 1, dwTrackNumber + 1, sTrackExt);

		// Create the iso file.
		HANDLE hTrackFile = CreateFile(sFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hTrackFile == INVALID_HANDLE_VALUE)
		{
			// Print error and return.
			printf("ERROR: could not create output file %s!\n", sFileName);
			return false;
		}

		// Check if the track is of Audio type.
		if (sessionCollection[dwSessionNumber]->psTracks[dwTrackNumber].eMode == DiskJuggler::CdiTrackMode::Audio)
		{
			// Write a wav header for it.
			if (WriteWavHeader(hTrackFile, sessionCollection[dwSessionNumber]->psTracks[dwTrackNumber].dwLength) == false)
			{
				// Print error, close file and return false.
				printf("ERROR: could not write wav header for file %s!\n", sFileName);
				CloseHandle(hTrackFile);
				return false;
			}
		}

		// Allocate a working buffer.
		DWORD Count = 0;
		BYTE *pbBuffer = new BYTE[sessionCollection[dwSessionNumber]->psTracks[dwTrackNumber].eSectorSize];

		// Loop through all the sectors for this track.
		for (int i = 0; i < sessionCollection[dwSessionNumber]->psTracks[dwTrackNumber].dwLength; i++)
		{
			// Print progress.
			printf("\rsaving track \t%d \t%s/%d \t%.2f%%", dwTrackNumber, sTrackName, sessionCollection[dwSessionNumber]->psTracks[dwTrackNumber].eSectorSize,
				(float)((float)(i + 1) / (float)sessionCollection[dwSessionNumber]->psTracks[dwTrackNumber].dwLength) * 100.0f);
			fflush(stdout);

			// Read sector from the track.
			this->m_pCdiFile->ReadSectors(dwSessionNumber, dwTrackNumber, sessionCollection[dwSessionNumber]->psTracks[dwTrackNumber].dwLba + i, pbBuffer, 1);

			// Check if we are writing an audio track or data track.
			if (sessionCollection[dwSessionNumber]->psTracks[dwTrackNumber].eMode == DiskJuggler::CdiTrackMode::Audio)
			{
				// Write the full sector.
				WriteFile(hTrackFile, pbBuffer, sessionCollection[dwSessionNumber]->psTracks[dwTrackNumber].eSectorSize, &Count, NULL);
			}
			else
			{
				// Write the sector minus the header size and extra bytes.
				WriteFile(hTrackFile, pbBuffer, RAW_SECTOR_SIZE, &Count, NULL);
			}
		}

		// Delete temp buffer.
		delete[] pbBuffer;

		// Close the output iso file and return.
		printf("\n");
		CloseHandle(hTrackFile);
		return true;
	}

	bool CdiImage::WriteAllTracks(CString sOutputFolder)
	{
		// Get the collection of session objects from the file handle.
		DisjointCollection<DiskJuggler::CdiSession> sessionCollection = this->m_pCdiFile->GetSessionsCollection();

		// Loop through all the sessions and dump every track.
		for (int i = 0; i < sessionCollection.size(); i++)
		{
			// Loop through all the tracks and dump each one.
			for (int x = 0; x < sessionCollection[i]->wTrackCount; x++)
			{
				// Dump the track and check the result.
				if (this->WriteTrackToFile(sOutputFolder, i + 1, x + 1) == false)
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

	bool CdiImage::ExtractIPBin(CString sOutputFolder)
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

	bool CdiImage::ExtractMRImage(CString sOutputFolder)
	{
		// Try to dump the boot logo.
		if (this->m_sBootstrap.Extract3rdPartyBootLogo(sOutputFolder) == true)
			printf("successfully extracted boot logo\n");
		else
			return false;

		// Done, return.
		return true;
	}

	bool CdiImage::ExtractISOFileSystem(CString sOutputFolder)
	{
		// Check that we have a valid fs iso handle.
		if (this->m_pFsIsoHandle == nullptr)
			return false;

		// 
	}
};