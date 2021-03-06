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
#include "..\DiskJuggler\CdiFileHandle.h"

namespace ISO
{
	// Forward declaration for ISO9660.
	class ISO9660;

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
	class FileSystemDirectoryEntry
	{
		friend class ISO9660;

	protected:
		ISO9660_DirectoryEntry *pValue;							// Pointer to the directory entry struct.
		FileSystemDirectoryEntry *pParentEntry;					// Pointer to parent folder.
		std::list<FileSystemDirectoryEntry*> lChildEntries;		// Child directory entries.

		CString sName;					// Name of this directory entry
		CString sFullName;				// Full file path of this entry

	public:
		FileSystemDirectoryEntry(ISO9660_DirectoryEntry *pDirectoryEntry, FileSystemDirectoryEntry *pParentDirectory = nullptr)
		{
			// Initialize fields.
			this->pValue = pDirectoryEntry;
			this->pParentEntry = pParentDirectory;
			this->lChildEntries = std::list<FileSystemDirectoryEntry*>();

			// Initialize the folder/file names.
			this->sName = CString(pDirectoryEntry->sFileIdentifier, pDirectoryEntry->bFileIdentifierLength);
			this->sFullName = CString();

			// Check if we need to clean up the name at all.
			if (pDirectoryEntry->sFileIdentifier[0] == '\0')
				this->sName = CString(".");
			else if (pDirectoryEntry->sFileIdentifier[0] == '\1')
				this->sName = CString("..");
			else if (this->sName.Find(';', 0) != -1)
			{
				// Just trim the end of the string to remove the characters.
				this->sName.Delete(this->sName.GetLength() - 2, 2);
			}

			// Check if there is a parent object for this entry, and set the parent related fields accordingly.
			if (pParentDirectory != nullptr)
			{
				// Add this entry to the parent's list of children.
				pParentDirectory->lChildEntries.push_back(this);

				// Format the full file path of this directory entry.
				this->sFullName.Format("%s\\%s", this->pParentEntry->sFullName, this->sName);
			}
		}

		/*
			Description: Gets the name of the directory entry.
		*/
		const CString GetName()
		{
			// Just return the name of the file.
			return this->sName;
		}

		/*
			Description: Gets the full file path of the directory entry.
		*/
		const CString GetFullName()
		{
			// Return the full name of the file.
			return this->sFullName;
		}

		/*
			Description: Gets a boolean indicating if this directory entry is a directory or not.
		*/
		bool IsDirectory()
		{
			// Check the file flags of the directory entry structure.
			return (this->pValue->bFileFlags & FileFlags::FileIsDirectory) != 0;
		}

		/*
			Description: Gets the LBA of this directory extent.
		*/
		DWORD GetExtentLBA()
		{
			// Return the LBA of this directory extent.
			return this->pValue->dwExtentLBA.LE;
		}

		DWORD GetExtentSize()
		{
			// Return the size of this directory extent.
			return this->pValue->dwExtentSize.LE;
		}

		/*
			Description: Gets a list of child entries nested within this directory entry.
		*/
		const std::list<FileSystemDirectoryEntry*> GetChildren()
		{
			// Get the list of child entries.
			return this->lChildEntries;
		}

		/*
			Description: Gets the parent of this directory entry.
		*/
		const FileSystemDirectoryEntry* GetParent()
		{
			// Return the parent directory entry.
			return this->pParentEntry;
		}
	};

	class ISO9660
	{
	protected:
		CString							m_sFileName;		// ISO image file path.
		HANDLE							m_hFileHandle;		// File handle for reading/writing from a file
		DiskJuggler::CdiTrackHandle		*m_phTrackHandle;	// Track handle for reading/writing from a CDI image
		DWORD							m_dwFileSize;		// Size of the ISO file.
		DWORD							m_dwLBA;			// LBA of the ISO.

		std::list<FileSystemSectorCacheEntry*>		lSectorCache;		// List of cached directory sectors.
		std::list<FileSystemDirectoryEntry*>		lDirectoryEntries;	// List of root directory entries

		bool ReadDirectoryBlock(ISO9660_DirectoryEntry *pDirectoryEntry, FileSystemDirectoryEntry *pParentDirectory, bool bVerbose);

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
		bool AddToCache(FileSystemDirectoryEntry *pDirectoryEntry, FileSystemSectorCacheEntry **ppCacheEntry);

		const FileSystemSectorCacheEntry* FindCacheEntry(DWORD dwLBA);

	public:
		ISO9660();
		~ISO9660();

		/*
			Description: Loads an ISO image from a file using the LBA specified and parses the file system.

			Parameters:
				sFileName: File path of the ISO image to load.
				dwLBA: LBA of the ISO image if extracted from a CDI image.
				bWriteMode: Boolean indicating if write access should be acquired for the file.
				bVerbose: Boolean indicating if debug information should be printed.

			Returns: True if the image was successfully loaded, false otherwise.
		*/
		bool LoadISOFromFile(CString sFileName, DWORD dwLBA, bool bWriteMode, bool bVerbose);

		/*
			Description: Loads an ISO image from a CDI track handle and parses the file system.

			Parameters:
				pTrackHandle: CDI track handle for the ISO image track.
				bVerbose: Boolean indicating if debug information should be printed.

			Returns: True if the image was successfully loaded, false otherwise.
		*/
		bool LoadISOFromCDI(DiskJuggler::CdiTrackHandle* pTrackHandle, bool bVerbose);
	};
};