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
		this->m_hFileHandle = INVALID_HANDLE_VALUE;
		this->m_phTrackHandle = nullptr;
		this->m_dwFileSize = 0;
		this->m_dwLBA = 0;
	}

	ISO9660::~ISO9660()
	{
		// Check if the file is still open.
		if (this->m_hFileHandle != INVALID_HANDLE_VALUE)
		{
			// Close the file handle.
			CloseHandle(this->m_hFileHandle);
			this->m_hFileHandle = INVALID_HANDLE_VALUE;
		}
	}

	bool ISO9660::LoadISOFromFile(CString sFileName, DWORD dwLBA, bool bWriteMode, bool bVerbose)
	{
		DWORD dwCount = 0;
		ISO9660_VolumeDescriptor *pVolDesc = NULL;
		ISO9660_PrimaryVolumeDescriptor *pPrimaryVolDesc = NULL;

		// Save the LBA value.
		this->m_dwLBA = dwLBA;

		// Save the file name and open the iso image.
		this->m_sFileName = sFileName;
		this->m_hFileHandle = CreateFile(this->m_sFileName, GENERIC_READ | (bWriteMode == true ? GENERIC_WRITE : 0),
			NULL, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (this->m_hFileHandle == INVALID_HANDLE_VALUE)
		{
			// Error opening the iso file.
			printf("ISO9660::LoadISOFromFile(): failed to open file '%s'!\n", this->m_sFileName);
			return false;
		}

		// Get the file size of the iso.
		this->m_dwFileSize = GetFileSize(this->m_hFileHandle, NULL);
		if (this->m_dwFileSize == 0)
		{
			// Invalid file size.
			printf("ISO9660::LoadISOFromFile(): file '%s' has invalid size!\n", this->m_sFileName);
			return false;
		}

		// Allocate a scratch buffer to work with.
		PBYTE pbScratchBuffer = (PBYTE)VirtualAlloc(NULL, ISO9660_SECTOR_SIZE, MEM_COMMIT, PAGE_READWRITE);
		if (pbScratchBuffer == NULL)
		{
			// Failed to allocate scratch buffer, out of memory.
			printf("ISO9660::LoadISOFromFile(): failed to allocate scratch buffer!\n");
			return false;
		}

		// Initialize the two volume descriptor pointers to point at the scratch buffer.
		pVolDesc = (ISO9660_VolumeDescriptor*)pbScratchBuffer;
		pPrimaryVolDesc = (ISO9660_PrimaryVolumeDescriptor*)pbScratchBuffer;

		// Seek to the volume descriptors section, after the reserved area.
		SetFilePointer(this->m_hFileHandle, ISO9660_VOLUME_DESCRIPTORS_SECTOR * ISO9660_SECTOR_SIZE, NULL, FILE_BEGIN);

		do
		{
			// Read the volume descriptor block.
			if (ReadFile(this->m_hFileHandle, pbScratchBuffer, ISO9660_SECTOR_SIZE, &dwCount, NULL) == false ||
				dwCount != ISO9660_SECTOR_SIZE)
			{
				// Failed to read the volume descriptor block.
				printf("ISO9660::LoadISOFromFile(): failed to read volume descriptor block, file too short!\n");

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
			printf("ISO9660::LoadISOFromFile(): failed to find the primary volume descriptor!\n");

			// Free the scratch buffer and return.
			VirtualFree(pbScratchBuffer, ISO9660_SECTOR_SIZE, MEM_DECOMMIT);
			return false;
		}

		// Found the primary volume descriptor.
		printf("found primary volume descriptor: %.*s\n", sizeof(pPrimaryVolDesc->sVolumeIdentifier), 
			sizeof(pPrimaryVolDesc->sVolumeIdentifier), pPrimaryVolDesc->sVolumeIdentifier);
		if (bVerbose == true)
		{
			// Print the file table banner.
			printf("LBA\t\tSize\t\tName\n");
		}

		// Seek to the root directory and parse the filesystem.
		if (ReadDirectoryBlock(&pPrimaryVolDesc->sRootDirectoryEntry, nullptr, bVerbose) == false)
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

	bool ISO9660::LoadISOFromCDI(DiskJuggler::CdiTrackHandle* pTrackHandle, bool bVerbose)
	{
		ISO9660_VolumeDescriptor *pVolDesc = NULL;
		ISO9660_PrimaryVolumeDescriptor *pPrimaryVolDesc = NULL;

		// Save the track handle and get the size of the ISO image.
		this->m_phTrackHandle = pTrackHandle;
		this->m_dwFileSize = this->m_phTrackHandle->TrackSize();
		this->m_dwLBA = this->m_phTrackHandle->LBA();

		// Allocate a scratch buffer to work with.
		PBYTE pbScratchBuffer = (PBYTE)VirtualAlloc(NULL, ISO9660_SECTOR_SIZE, MEM_COMMIT, PAGE_READWRITE);
		if (pbScratchBuffer == NULL)
		{
			// Failed to allocate scratch buffer, out of memory.
			printf("ISO9660::LoadISOFromCDI(): failed to allocate scratch buffer!\n");
			return false;
		}

		// Initialize the two volume descriptor pointers to point at the scratch buffer.
		pVolDesc = (ISO9660_VolumeDescriptor*)pbScratchBuffer;
		pPrimaryVolDesc = (ISO9660_PrimaryVolumeDescriptor*)pbScratchBuffer;

		DWORD i = 0;
		do
		{
			// Read the volume descriptor block.
			if (this->m_phTrackHandle->ReadData(ISO9660_VOLUME_DESCRIPTORS_SECTOR + i, pbScratchBuffer, ISO9660_SECTOR_SIZE) == false)
			{
				// Failed to read the volume descriptor block.
				printf("ISO9660::LoadISOFromCDI(): failed to read volume descriptor block!\n");

				// Free the scratch buffer and return.
				VirtualFree(pbScratchBuffer, ISO9660_SECTOR_SIZE, MEM_DECOMMIT);
				return false;
			}

			// Next block.
			i++;
		} 
		while (pVolDesc->bType != VolumeDescriptorTypes::PrimaryVolumeDescriptor &&
			pVolDesc->bType != VolumeDescriptorTypes::VolumeDescriptorSetTerminator);

		// Check if we found the primary descriptor or the terminator.
		if (pVolDesc->bType != VolumeDescriptorTypes::PrimaryVolumeDescriptor)
		{
			// Failed to find the primary volume descriptor.
			printf("ISO9660::LoadISOFromCDI(): failed to find the primary volume descriptor!\n");

			// Free the scratch buffer and return.
			VirtualFree(pbScratchBuffer, ISO9660_SECTOR_SIZE, MEM_DECOMMIT);
			return false;
		}

		// Found the primary volume descriptor.
		printf("found primary volume descriptor: %.*s\n", sizeof(pPrimaryVolDesc->sVolumeIdentifier), pPrimaryVolDesc->sVolumeIdentifier);
		if (bVerbose == true)
		{
			// Print the file table banner.
			printf("LBA\t\tSize\t\tName\n");
		}

		// Seek to the root directory and parse the filesystem.
		if (ReadDirectoryBlock(&pPrimaryVolDesc->sRootDirectoryEntry, nullptr, bVerbose) == false)
		{
			// Failed to read the filesystem, free the scratch buffer and return.
			VirtualFree(pbScratchBuffer, ISO9660_SECTOR_SIZE, MEM_DECOMMIT);
			return false;
		}

		// Skip a line.
		printf("\n");

		// Successfully loaded the ISO image.
		return true;
	}

	bool ISO9660::ReadDirectoryBlock(const ISO9660_DirectoryEntry *pDirectoryEntry, const FileSystemDirectoryEntry *pParentDirectory, bool bVerbose)
	{
		// Check if this directory entry has already been cached.
		FileSystemSectorCacheEntry *pCacheEntry = (FileSystemSectorCacheEntry*)FindCacheEntry(pDirectoryEntry->dwExtentLBA.LE);
		if (pCacheEntry != nullptr)
		{
			// The directory entry has already been cached so there is nothing to do here.
			return true;
		}

		// Create a new FileSystemDirectoryEntry object for the directory entry provided.
		FileSystemDirectoryEntry *pFsDirectoryEntry = new FileSystemDirectoryEntry(pDirectoryEntry, pParentDirectory);

		// The directory entry has not been cached yet so add it to the directory cache.
		if (AddToCache(pFsDirectoryEntry, &pCacheEntry) == false)
		{
			// Failed to cache directory entry data.
			delete pDirectoryEntry;
			return false;
		}

		// Add the new fs directory entry to the list.
		this->lDirectoryEntries.push_back(*pFsDirectoryEntry);

		// Initialize the loop counter and interator.
		DWORD dwRemainingData = pDirectoryEntry->dwExtentSize.LE;
		PBYTE pbCacheData = pCacheEntry->pbSectorData;

		// Loop through the directory sectors until we reach the last directory entry.
		FileSystemDirectoryEntry *pPrevEntry = nullptr;
		do
		{
			// Check if the next entry would cross a sector boundary.
			DWORD dwRemainder = dwRemainingData % ISO9660_SECTOR_SIZE;
			if (dwRemainder > 0 && dwRemainder < sizeof(ISO9660_DirectoryEntry))
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
			FileSystemDirectoryEntry *pEntry = new FileSystemDirectoryEntry(pDirEntry, pFsDirectoryEntry);

			// Add a link in the previous entry to the one we just created.
			if (pPrevEntry != nullptr)
				pPrevEntry->pNextEntry = pEntry;
			pPrevEntry = pEntry;

			// Check if we should print the file information.
			if (bVerbose == true)
			{
				// Make sure that if the entry is a directory that we only print the parent directory traversal.
				if ((pDirEntry->bFileFlags & FileFlags::FileIsDirectory) == 0 || 
					((pDirEntry->bFileFlags & FileFlags::FileIsDirectory) != 0 && pEntry->sName == ".."))
					printf("%d\t\t%d\t\t%s\n", pDirEntry->dwExtentLBA.LE, pDirEntry->dwExtentSize.LE, pEntry->sFullName);
			}

			// Check if this entry is a directory, and if so recursively parse it.
			if ((pDirEntry->bFileFlags & FileFlags::FileIsDirectory) != 0 && 
				pDirEntry->dwExtentLBA.LE != pDirectoryEntry->dwExtentLBA.LE)
			{
				// Recursively parse the child directory.
				if (ReadDirectoryBlock(pDirEntry, pFsDirectoryEntry, bVerbose) == false)
				{
					// Failed to read child directory entry data.
					return false;
				}
			}

			// Next entry.
			dwRemainingData -= pDirEntry->bEntryLength;
			pbCacheData += pDirEntry->bEntryLength;
		} while (dwRemainingData > 0 && ((ISO9660_DirectoryEntry*)pbCacheData)->bEntryLength != 0);

		// Successfully parsed the file system for this directory entry.
		return true;
	}

	bool ISO9660::AddToCache(const FileSystemDirectoryEntry *pDirectoryEntry, FileSystemSectorCacheEntry **ppCacheEntry)
	{
		DWORD dwBytesRead = 0;

		// Check to see if there is already a cache entry for this directory.
		*ppCacheEntry = (FileSystemSectorCacheEntry*)FindCacheEntry(pDirectoryEntry->pValue->dwExtentLBA.LE);
		if (*ppCacheEntry != nullptr)
		{
			// There is already a cache entry for this directory entry, so we are done here.
			return true;
		}

		// Initialize a new FileSystemSectorCacheEntry object.
		FileSystemSectorCacheEntry *pCacheEntry = new FileSystemSectorCacheEntry();
		pCacheEntry->dwExtentLBA = pDirectoryEntry->pValue->dwExtentLBA.LE;
		pCacheEntry->dwExtentSize = pDirectoryEntry->pValue->dwExtentSize.LE;
		pCacheEntry->sFileIdentifier = pDirectoryEntry->sName;

		// Allocate the cache buffer for the directory entry.
		pCacheEntry->pbSectorData = (PBYTE)VirtualAlloc(NULL, pCacheEntry->dwExtentSize, MEM_COMMIT, PAGE_READWRITE);
		if (pCacheEntry->pbSectorData == NULL)
		{
			// Failed to allocate cache buffer.
			printf("ISO9660::AddToCache(): failed to allocate cache buffer for entry '%s'!\n",
				pCacheEntry->sFileIdentifier);
			goto Cleanup;
		}

		// Check if we are reading from a file or from a CDI image.
		if (this->m_hFileHandle != INVALID_HANDLE_VALUE)
		{
			// Compute the directory entry data offset and seek to it.
			int dwDataOffset = (pCacheEntry->dwExtentLBA - this->m_dwLBA) * ISO9660_SECTOR_SIZE;
			SetFilePointer(this->m_hFileHandle, dwDataOffset, NULL, FILE_BEGIN);

			// Read the directory data into the cache buffer.
			if (ReadFile(this->m_hFileHandle, pCacheEntry->pbSectorData, pCacheEntry->dwExtentSize, &dwBytesRead, NULL) == false ||
				dwBytesRead != pCacheEntry->dwExtentSize)
			{
				// Failed to read the directory data into the cache buffer.
				printf("ISO9660::AddToCache(): failed to read directory data for entry '%s'!\n",
					pCacheEntry->sFileIdentifier);
				goto Cleanup;
			}
		}
		else
		{
			// Read the directory data into the cache buffer.
			if (this->m_phTrackHandle->ReadData(pCacheEntry->dwExtentLBA - this->m_dwLBA, 
				pCacheEntry->pbSectorData, pCacheEntry->dwExtentSize) == false)
			{
				// Failed to read the directory data into the cache buffer.
				printf("ISO9660::AddToCache(): failed to read directory data for entry '%s'!\n",
					pCacheEntry->sFileIdentifier);
				goto Cleanup;
			}
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

	const FileSystemSectorCacheEntry* ISO9660::FindCacheEntry(DWORD dwLBA)
	{
		// Search the directory list for an entry with the target LBA.
		for (std::list<FileSystemSectorCacheEntry>::const_iterator iter = this->lSectorCache.begin(); 
			iter != this->lSectorCache.end(); ++iter)
		{
			// Check if the current cache entry has the same LBA as what we are searching for.
			if ((*iter).dwExtentLBA == dwLBA)
				return &(*iter);
		}

		// No cache entry with the target LBA was found.
		return nullptr;
	}
};