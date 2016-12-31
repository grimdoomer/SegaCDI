/*
	SegaCDI - Sega Dreamcast cdi image validator.

	Iso9660.cpp - ISO 9660 image support.

	Dec 2nd, 2015
		- Initial creation.

	Dec 5th, 2015
		- Added support to read ISO9660 directory entries.
*/

#include "../stdafx.h"
#include "Iso9660.h"
#include "Iso9660Types.h"

namespace ISO
{
	ISO9660::ISO9660()
	{
		// Initialize fields.
		this->m_hFile = INVALID_HANDLE_VALUE;
		this->m_dwFileSize = 0;
		this->m_dwLBA = 0;
	}

	ISO9660::~ISO9660()
	{
		// Check if the file is still open.
		if (this->m_hFile != INVALID_HANDLE_VALUE)
		{
			// Close the file handle.
			CloseHandle(this->m_hFile);
			this->m_hFile = INVALID_HANDLE_VALUE;
		}
	}

	bool ISO9660::LoadISO(CString sFileName, DWORD dwLBA, bool bWriteMode, bool bVerbose)
	{
		DWORD dwCount = 0;
		ISO9660_VolumeDescriptor *pVolDesc = NULL;
		ISO9660_PrimaryVolumeDescriptor *pPrimaryVolDesc = NULL;

		// Save the file name and open the iso image.
		this->m_sFileName = sFileName;
		this->m_hFile = CreateFile(this->m_sFileName, GENERIC_READ | (bWriteMode == true ? GENERIC_WRITE : 0),
			NULL, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (this->m_hFile == INVALID_HANDLE_VALUE)
		{
			// Error opening the iso file.
			printf("ISO9660::LoadISO(): failed to open file '%s'!\n", this->m_sFileName);
			return false;
		}

		// Get the file size of the iso.
		this->m_dwFileSize = GetFileSize(this->m_hFile, NULL);
		if (this->m_dwFileSize == 0)
		{
			// Invalid file size.
			printf("ISO9660::LoadISO(): file '%s' has invalid size!\n", this->m_sFileName);
			return false;
		}

		// Allocate a scratch buffer to work with.
		PBYTE pbScratchBuffer = (PBYTE)VirtualAlloc(NULL, ISO9660_SECTOR_SIZE, MEM_COMMIT, PAGE_READWRITE);
		if (pbScratchBuffer == NULL)
		{
			// Failed to allocate scratch buffer, out of memory.
			printf("ISO9660::LoadISO(): failed to allocate scratch buffer!\n");
			return false;
		}

		// Initialize the two volume descriptor pointers to point at the scratch buffer.
		pVolDesc = (ISO9660_VolumeDescriptor*)pbScratchBuffer;
		pPrimaryVolDesc = (ISO9660_PrimaryVolumeDescriptor*)pbScratchBuffer;

		// Seek to the volume descriptors section, after the reserved area.
		SetFilePointer(this->m_hFile, ISO9660_VOLUME_DESCRIPTORS_SECTOR * ISO9660_SECTOR_SIZE, NULL, FILE_BEGIN);

		do
		{
			// Read the volume descriptor block.
			if (ReadFile(this->m_hFile, pbScratchBuffer, ISO9660_SECTOR_SIZE, &dwCount, NULL) == false ||
				dwCount != ISO9660_SECTOR_SIZE)
			{
				// Failed to read the volume descriptor block.
				printf("ISO9660::LoadISO(): failed to read volume descriptor block, file too short!\n");

				// Free the scratch buffer and return.
				VirtualFree(pbScratchBuffer, ISO9660_SECTOR_SIZE, MEM_DECOMMIT);
				return false;
			}
		} while (pVolDesc->bType != VolumeDescriptorTypes::PrimaryVolumeDescriptor &&
			pVolDesc->bType != VolumeDescriptorTypes::VolumeDescriptorSetTerminator);

		// Check if we found the primary descriptor or the terminator.
		if (pVolDesc->bType != VolumeDescriptorTypes::PrimaryVolumeDescriptor)
		{
			// Failed to find the primary volume descriptor.
			printf("ISO9660::LoadISO(): failed to find the primary volume descriptor!\n");

			// Free the scratch buffer and return.
			VirtualFree(pbScratchBuffer, ISO9660_SECTOR_SIZE, MEM_DECOMMIT);
			return false;
		}

		// Found the primary volume descriptor.
		printf("found primary volume descriptor: %s\n", pPrimaryVolDesc->sVolumeIdentifier);
		if (bVerbose == true)
		{
			// Print the file table banner.
			printf("LBA\t\tSize\t\tName\n");
		}

		// Seek to the root directory and parse the filesystem.
		if (ReadFileSystem(pPrimaryVolDesc->sRootDirectoryEntry, bVerbose) == false)
		{
			// Failed to read the filesystem, free the scratch buffer and return.
			VirtualFree(pbScratchBuffer, ISO9660_SECTOR_SIZE, MEM_DECOMMIT);
			return false;
		}

		// Skip a line.
		printf("\n");

		// Successfully loaded the ISO file.
		return true;
	}

	bool ISO9660::ReadFileSystem(const ISO9660_DirectoryEntry& directoryEntry, bool bVerbose)
	{
		// Create a new FileSystemDirectoryEntry object for the directory entry provided.
		FileSystemDirectoryEntry *pDirectoryEntry = new FileSystemDirectoryEntry();
		pDirectoryEntry->pValue = (ISO9660_DirectoryEntry*)&directoryEntry;
		pDirectoryEntry->sFileName = CString(directoryEntry.sFileIdentifier, directoryEntry.bFileIdentifierLength);
		pDirectoryEntry->pNextEntry = NULL;

		// Cache the directory entry data so we can parse it.
		FileSystemSectorCacheEntry *pCacheEntry = NULL;
		if (AddToCache(*pDirectoryEntry, &pCacheEntry) == false)
		{
			// Failed to cache directory entry data.
			delete pDirectoryEntry;
			return false;
		}

		// Add the new fs directory entry to the list.
		this->lDirectoryEntries.push_back(*pDirectoryEntry);

		// Initialize a new fs root entry.
		FileSystemDirectoryEntry *pPrevEntry = pDirectoryEntry;
		FileSystemDirectoryEntry *pEntry = NULL;

		// Initialize the loop counter and interator.
		DWORD dwRemainingData = directoryEntry.dwExtentSize.LE;
		PBYTE pbCacheData = pCacheEntry->pbSectorData;

		// Loop through the directory sectors until we reach the last directory entry.
		do
		{
			// Check if the next entry would cross a sector boundary.
			DWORD dwRemainder = ISO9660_SECTOR_SIZE - (dwRemainingData % ISO9660_SECTOR_SIZE);
			if (dwRemainder < sizeof(ISO9660_DirectoryEntry))
			{
				// Skip to the next sector.
				dwRemainingData -= dwRemainder;
				pbCacheData += dwRemainder;
			}

			// Get the next ISO9660 directory entry from the cache data.
			ISO9660_DirectoryEntry *pDirEntry = (ISO9660_DirectoryEntry*)pbCacheData;

			// Check if the current directory entry is valid.
			if (pDirEntry->bEntryLength == 0)
				break;

			// Setup the fs child entry.
			pEntry = new FileSystemDirectoryEntry();
			pEntry->pValue = pDirEntry;
			pEntry->sFileName = CString(pDirEntry->sFileIdentifier, pDirEntry->bFileIdentifierLength);

			// Add a link in the previous entry to the one we just created.
			pPrevEntry->pNextEntry = pEntry;
			pPrevEntry = pEntry;

			// Check if we should print the file information.
			if (bVerbose == true)
			{
				// Print the file information.
				printf("%d\t\t%d\t\t%s\n", pDirEntry->dwExtentLBA.LE, pDirEntry->dwExtentSize.LE, pEntry->sFileName);
			}

			// Check if this entry is a directory, and if so recursively parse it.
			if ((pDirEntry->bFileFlags & FileFlags::FileIsDirectory) != 0)
			{
				// Recursively parse the child directory.
				if (ReadFileSystem(*pDirEntry, bVerbose) == false)
				{
					// Failed to read child directory entry data.
					return false;
				}
			}

			// Next entry.
			dwRemainingData -= pDirEntry->bEntryLength;
		} while (dwRemainingData - sizeof(ISO9660_DirectoryEntry) > 0);

		// Successfully parsed the file system for this directory entry.
		return true;
	}

	bool ISO9660::AddToCache(const FileSystemDirectoryEntry& directoryEntry, FileSystemSectorCacheEntry **ppCacheEntry)
	{
		DWORD dwBytesRead = 0;

		// Assign a null value to the cache entry pointer until we succeed in caching the directory data.
		*ppCacheEntry = NULL;

		// Initialize a new FileSystemSectorCacheEntry object.
		FileSystemSectorCacheEntry *pCacheEntry = new FileSystemSectorCacheEntry();
		pCacheEntry->dwExtentLBA = directoryEntry.pValue->dwExtentLBA.LE;
		pCacheEntry->dwExtentSize = directoryEntry.pValue->dwExtentSize.LE;
		pCacheEntry->sFileIdentifier = directoryEntry.sFileName;

		// Allocate the cache buffer for the directory entry.
		pCacheEntry->pbSectorData = (PBYTE)VirtualAlloc(NULL, pCacheEntry->dwExtentSize, MEM_COMMIT, PAGE_READWRITE);
		if (pCacheEntry->pbSectorData == NULL)
		{
			// Failed to allocate cache buffer.
			printf("ISO9660::AddToCache(): failed to allocate cache buffer for entry '%s'!\n",
				pCacheEntry->sFileIdentifier);
			goto Cleanup;
		}

		// Compute the directory entry data offset and seek to it.
		int dwDataOffset = (pCacheEntry->dwExtentLBA - this->m_dwLBA) * ISO9660_SECTOR_SIZE;
		SetFilePointer(this->m_hFile, dwDataOffset, NULL, FILE_BEGIN);

		// Read the directory data into the cache buffer.
		if (ReadFile(this->m_hFile, pCacheEntry->pbSectorData, pCacheEntry->dwExtentSize, &dwBytesRead, NULL) == false ||
			dwBytesRead != pCacheEntry->dwExtentSize)
		{
			// Failed to read the directory data into the cache buffer.
			printf("ISO9660::AddToCache(): failed to read directory data for entry '%s'!\n",
				pCacheEntry->sFileIdentifier);
			goto Cleanup;
		}

		// Add the new cache entry object to the list.
		this->lSectorCache.push_back(*pCacheEntry);

		// Successfully cached the directory data, assign the cache entry pointer and return true.
		*ppCacheEntry = pCacheEntry;
		return true;

	Cleanup:
		if (pCacheEntry)
		{
			// Check if the cache buffer was allocated.
			if (pCacheEntry->pbSectorData)
				VirtualFree(pCacheEntry->pbSectorData, pCacheEntry->dwExtentSize, MEM_DECOMMIT);

			// Free the cache entry object.
			delete pCacheEntry;
		}

		// Done with errors, return false.
		return false;
	}
};