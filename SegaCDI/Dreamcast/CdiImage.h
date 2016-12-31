/*
	SegaCDI - Sega Dreamcast cdi image validator.

	CdiImage.h - Support for DiscJuggler cdi images.

	Mar 6th, 2013
		- Initial creation.

	Dec 15th, 2015
		- Added overload for LoadImage() to read the ISO9660 data session
			when reading the CDI image file.
*/

#pragma once
#include "../stdafx.h"
#include "../DiskJuggler/CdiFileHandle.h"
#include "Bootstrap.h"

namespace Dreamcast
{
	#define IP_BIN_SECTOR_COUNT			16
	#define IP_BIN_SIZE					0x8000

	class CdiImage
	{
	protected:
		DiskJuggler::CdiFileHandle *m_pCdiFile;

		// IP.BIN bootstrap.
		Bootstrap	m_sBootstrap;

		/*
			Description: Searches each session in the CDI file for the IP.BIN bootstrap.

			Parameters:
				bVerbose: boolean indicating if extra information should be printed to the console.

			Returns: True if the bootstrap was found and successfully parsed, false otherwise.
		*/
		bool LoadBootstrap(bool bVerbos);

	public:
		CdiImage();
		~CdiImage();

		/*
			Description: Loads the CDI image specified, finding the bootstrap sector and reading
				any file system sessions within the image.

			Parameters:
				sFileName: file path of the CDI image to load.
				bVerbose: boolean indicating if extra information should be printed to the console.

			Returns: True if the CDI image and file sub systems were successfully read and initialize, false otherwise.
		*/
		bool LoadImage(CString sFileName, bool bVerbos);

		bool WriteTrackToFile(CString sOutputFolder, DWORD dwSessionNumber, DWORD dwTrackNumber);
		bool WriteAllTracks(CString sOutputFolder);

		bool ExtractIPBin(CString sOutputFolder);
		bool ExtractMRImage(CString sOutputFolder);
	};
};