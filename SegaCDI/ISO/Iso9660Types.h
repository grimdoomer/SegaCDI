/*
	SegaCDI - Sega Dreamcast cdi image validator.

	Iso9660Types.h - ISO 9660 type definitions.

	Dec 1st, 2015
		- Initial creation.
*/

#pragma once
#include "../stdafx.h"

namespace ISO
{
	//-----------------------------------------------------
	// Primitive Types
	//-----------------------------------------------------
#pragma pack(1)
	struct LSBMSB_Int16
	{
		short LE;
		short BE;

		bool operator==(const LSBMSB_Int16& a) const
		{
			// We shouldn't have to compare the BE value since it should just
			// be byteswapped, but lets do it anyway to be sure.
			return (LE == a.LE && BE == a.BE);
		}

		bool operator!=(const LSBMSB_Int16& a) const
		{
			return (LE != a.LE || BE != a.BE);
		}
	};

#pragma pack(1)
	struct LSBMSB_Int32
	{
		int LE;
		int BE;

		bool operator==(const LSBMSB_Int32& a) const
		{
			return (LE == a.LE && BE == a.BE);
		}

		bool operator!=(const LSBMSB_Int32& a) const
		{
			return (LE != a.LE || BE != a.BE);
		}
	};

	// Date/Time structure used in the volume descriptors.
#pragma pack(1)
	struct ISO_datetime1
	{
		char sYear[4];
		char sMonth[2];
		char sDay[2];
		char sHour[2];
		char sMinute[2];
		char sSecond[2];
		char sHundreths[2];
		char bTimeZone;
	};

	// Date/Time structure used for directory entries.
#pragma pack(1)
	struct ISO_datetime2
	{
		char bYear;
		char bMonth;
		char bDay;
		char bHour;
		char bMinute;
		char bSecond;
		char bTimeZone;
	};

	//-----------------------------------------------------
	// Directory Entry
	//-----------------------------------------------------
	enum FileFlags : char
	{
		FileIsHidden = 1,
		FileIsDirectory = 2,
		FileIsAssociatedFile = 4,
		ExtendedAttributeContainsFormat = 8,
		ExtendedAttributeContainsPermissions = 0x10,
		FileIsSpanning = 0x80
	};

#define ISO9660_DIR_ENTRY_MAX_SIZE 255
#pragma pack(1)
	struct ISO9660_DirectoryEntry
	{
		/* 0x00 */ unsigned char bEntryLength;
		/* 0x01 */ char bExtendedAttributeLength;
		/* 0x02 */ LSBMSB_Int32 dwExtentLBA;
		/* 0x0A */ LSBMSB_Int32 dwExtentSize;
		/* 0x12 */ ISO_datetime2 dtRecordingDateTime;
		/* 0x19 */ FileFlags bFileFlags;
		/* 0x1A */ char bFileUnitSize;
		/* 0x1B */ char bInterleveGapSize;
		/* 0x1C */ LSBMSB_Int16 wVolumeSequenceNumber;
		/* 0x20 */ char bFileIdentifierLength;
		/* 0x21 */ char sFileIdentifier[1];
		/* 0x00 */ char bPadding;
	};

	//-----------------------------------------------------
	// Volume Descriptor Types
	//-----------------------------------------------------
	enum VolumeDescriptorTypes : unsigned char
	{
		BootRecord = 0,
		PrimaryVolumeDescriptor = 1,
		SupplementaryVolumeDescriptor = 2,
		VolumePartitionDescriptor = 3,
		VolumeDescriptorSetTerminator = 255
	};

#pragma pack(1)
	struct ISO9660_VolumeDescriptor
	{
		VolumeDescriptorTypes bType;
		char sIdentifier[5];
		unsigned char bVersion;
		char bData[2041];
	};

#pragma pack(1)
	struct ISO9660_PrimaryVolumeDescriptor
	{
		VolumeDescriptorTypes bType;
		char sIdentifier[5];
		char bVersion;
		char bUnused1;
		char sSystemIdentifier[32];
		char sVolumeIdentifier[32];
		char bUnused2[8];
		LSBMSB_Int32 dwVolumeSpaceSize;
		char bUnused3[32];
		LSBMSB_Int16 wVolumeSetSize;
		LSBMSB_Int16 wVolumeSequenceNumber;
		LSBMSB_Int16 wLogicalBlockSize;
		LSBMSB_Int32 dwPathTableSize;
		int dwTypeLPathTableLBA;
		int dwOptionalTypeLPathTableLBA;
		int dwTypeMPathTableLBA;
		int dwOptionalTypeMPathTableLBA;
		ISO9660_DirectoryEntry sRootDirectoryEntry;
		char sVolumeSetIdentifier[128];
		char sPublisherIdentifier[128];
		char sDataPreparerIdenfifier[128];
		char sApplicationIdentifier[128];
		char sCopyrightFileIdentifier[38];
		char sAbstractFileIdentifier[36];
		char sBibliographicFileIdentifier[36];
		ISO_datetime1 dtVolumeCreationDateTime;
		ISO_datetime1 dtVolumeModificationDateTime;
		ISO_datetime1 dtVolumeExpirationDateTime;
		ISO_datetime1 dtVolumeEffectiveDateTime;
		unsigned char bFileStructureVersion;
		char bUnused4;
		char bApplicationUsed[512];
		char bReserved[653];
	};
};