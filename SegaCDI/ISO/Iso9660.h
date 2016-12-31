/*
	SegaCDI - Sega Dreamcast cdi image validator.

	Iso9660.h - ISO 9660 image support.

	Nov 30th, 2015
		- Initial creation.
*/

#pragma once
#include "../stdafx.h"
#include "Iso9660Types.h"
#include <list>

namespace ISO
{
	// Starting sector for the volume descriptors.
#define ISO9660_VOLUME_DESCRIPTORS_SECTOR		0x10
#define ISO9660_SECTOR_SIZE						0x800

/*
	File system cache entry structure, used to track cached directory sectors.
*/
	struct FileSystemSectorCacheEntry
	{
		DWORD		dwExtentLBA;			// LBA of the starting sector.
		DWORD		dwExtentSize;			// Size of the sector cache in bytes.
		CString		sFileIdentifier;		// Directory identifier.

		PBYTE		pbSectorData;			// Pointer to the sector cache buffer.
	};

	/*
		File system directory entry structure, used to track directory entries in the file system.
	*/
	struct FileSystemDirectoryEntry
	{
		ISO9660_DirectoryEntry *pValue;			// Pointer to the directory entry struct.
		FileSystemDirectoryEntry *pNextEntry;	// Pointer to the next directory entry in the same folder.
		CString sFileName;						// File name of this directory entry.
	};

	class ISO9660
	{
	protected:
		CString		m_sFileName;		// ISO image file path.
		HANDLE		m_hFile;			// File handle for reading/writing.
		DWORD		m_dwFileSize;		// Size of the ISO file.
		DWORD		m_dwLBA;			// LBA of the ISO.

		std::list<FileSystemSectorCacheEntry>	lSectorCache;		// List of cached directory sectors.
		std::list<FileSystemDirectoryEntry>		lDirectoryEntries;	// List of directory entries found in the cached directory sectors.

		bool ReadFileSystem(const ISO9660_DirectoryEntry& directoryEntry, bool bVerbose);

		/*
			Description: Creates a new FileSystemSectorCacheEntry object using the directory entry directoryEntry
				and caches the directory data to memory.

			Parameters:
				directoryEntry: Directory entry object that will be used to find the extent LBA and extent size values
					of the directory data to cache.
				ppCacheEntry: Out pointer to the new FileSystemSectorCacheEntry object that is created for the newly
					cached data.

			Returns: True if the cache operation is successful, false otherwise.
		*/
		bool AddToCache(const FileSystemDirectoryEntry& directoryEntry, FileSystemSectorCacheEntry **ppCacheEntry);

	public:
		ISO9660();
		~ISO9660();

		bool LoadISO(CString sFileName, DWORD dwLBA, bool bWriteMode, bool bVerbose);
	};
};