/*
	SegaCDI - Sega Dreamcast cdi image validator.

	Bootstrap.h - IP.BIN bootstrap utilities.

	Sep 28th, 2014
*/

#pragma once
#include "../stdafx.h"

namespace Dreamcast
{
#define HARDWARE_ID				"SEGA SEGAKATANA "
#define HARDWARE_VENDOR_ID		"SEGA ENTERPRISES"

#define MAX_NUM_REGIONS			8
	enum BootstrapRegion
	{
		Japan = 1,
		USA = 2,
		Europe = 4
	};

#define REGION_CODE_JAPAN		'J'
#define REGION_SYMBOL_JAPAN		"For JAPAN,TAIWAN,PHILIPINES."

#define REGION_CODE_USA			'U'
#define REGION_SYMBOL_USA		"For USA and CANADA.         "

#define REGION_CODE_EUROPE		'E'
#define REGION_SYMBOL_EUROPE	"For EUROPE.                 "

#pragma pack(1)
	struct IP_BIN_HEADER
	{
		char sHardwareID[16];			// "SEGA SEGAKATANA "
		char sHardwareVendorID[16];		// "SEGA ENTERPRISES"

		/*
			The Device Information field begins with a four digit hexadecimal number, which
			is some kind of checksum on the Product number and Product version fields. Then
			comes the string " GD-ROM", and finally an indication of how many discs this
			software uses, and which of these discs that this is. This is indicated by two
			positive numbers separated with a slash. So if this is the second disc of three,
			the Device Information string might be "8B40 GD-ROM2/3  ".
		*/
		char sDeviceInfo[16];

		/*
			The Area Symbols string consists of eight characters, which are either space or a
			specific letter. Each of these represent a geographical region in which the disc is
			designed to work. So far, only the first three are assigned. These are Japan (and
			the rest of East Asia), USA + Canada, and Europe, respectively. If the character for
			a particular region is a space, the disc will not be playable in that region. If it
			contains the correct region character, it will be. The region characters for the
			first three regions are J, U, and E, respectively. So a disc only playable in Europe
			would have an Area Symbols string of "  E     ".

			Slot	Region		Text
			0 		Japan 		"For JAPAN,TAIWAN,PHILIPINES."
			1 		USA 		"For USA and CANADA.         "
			2 		Europe 		"For EUROPE.                 "
			3 		Unassigned 	"                            "
			4 		Unassigned 	"                            "
			5 		Unassigned 	"                            "
			6 		Unassigned 	"                            "
			7 		Unassigned 	"                            "
		*/
		char sRegionCode[MAX_NUM_REGIONS];

		/*
			The Device Information field is a 28 bit long bitfield represented by a 7 digit hexadecimal
			number. The meaning of the individual bits in each digit is given below:

			<A><-------B------> <C->
			0000 0000 0000 0000 0000 0000 0000
			^^^^ ^^^^ ^^^^ ^^^^ ^^^^    ^    ^
			|||| |||| |||| |||| ||||    |    |
			|||| |||| |||| |||| ||||    |    +----- Uses Windows CE
			|||| |||| |||| |||| ||||    |
			|||| |||| |||| |||| ||||    +-----  VGA box support
			|||| |||| |||| |||| ||||
			|||| |||| |||| |||| |||+----- Other expansions
			|||| |||| |||| |||| ||+----- Puru Puru pack
			|||| |||| |||| |||| |+----- Mike device
			|||| |||| |||| |||| +----- Memory card
			|||| |||| |||| |||+------ Start + A + B + Directions
			|||| |||| |||| ||+------ C button
			|||| |||| |||| |+------ D button
			|||| |||| |||| +------ X button
			|||| |||| |||+------- Y button
			|||| |||| ||+------- Z button
			|||| |||| |+------- Expanded direction buttons
			|||| |||| +------- Analog R trigger
			|||| |||+-------- Analog L trigger
			|||| ||+-------- Analog horizontal controller
			|||| |+-------- Analog vertical controller
			|||| +-------- Expanded analog horizontal
			|||+--------- Expanded analog vertical
			||+--------- Gun
			|+--------- Keyboard
			+--------- Mouse
		*/
		char sPeripherals[8];

		char sProductNumber[10];
		char sVersion[6];
		char sReleaseDate[16];		// YYYYMMDD
		char sBootFileName[16];		// "1ST_READ.BIN"
		char sManufacturersID[16];
		char sApplicationTitle[128];
	};

#define REGION_SYMBOL_UNKNOWN			0x0EA00900
#define REGION_SYMBOL_DESCRIPTION_SIZE	28
	struct RegionSymbol
	{
		int dwUnknown;			// Slot used = 0x0EA00900, slot not used = 0xFEAF0900
		char sDescription[28];
	};

#define BOOTSTRAP_SECTOR_COUNT	16
#define BOOTSTRAP_SIZE			0x8000

	/*
		Offset		Load address		Contents
		0000-00FF 	8C008000-8C0080FF 	Meta information
		0100-02FF 	8C008100-8C0082FF 	Table of contents
		0300-36FF 	8C008300-8C00B6FF 	SEGA license screen code
		3700-37FF 	8C00B700-8C00B7FF 	Area protection symbols
		3800-5FFF 	8C00B800-8C00DFFF 	Bootstrap 1
		6000-7FFF 	8C00E000-8C00FFFF 	Bootstrap 2
	*/
#pragma pack(1)
	struct BOOTSTRAP_BINARY
	{
		union
		{
			struct
			{
				IP_BIN_HEADER sIPHeader;

				char bTableOfContents[0x200];
				char bLicenseScreenCode[0x3400];
				RegionSymbol sRegionSymbols[MAX_NUM_REGIONS];
				char bBootstrap1[0x2800];
				char bBootstrap2[0x2000];
			};

			char bData[BOOTSTRAP_SIZE];
		};
	};

	class Bootstrap
	{
	protected:
		BOOTSTRAP_BINARY m_sBootstrap;

	public:
		Bootstrap();
		~Bootstrap();

		bool LoadBootstrap(char *pbBuffer, int dwBufferSize);

		void PatchBootstrapForRegion(int dwRegion);
		void PatchBootstrapForVGA();
		void PatchBootstrapForOS(bool bIsWinCE);

		bool CopyBootstrapBuffer(char *pbBuffer, int dwBufferSize);
		bool Extract3rdPartyBootLogo(const char *psOutputFolder);
		bool Inject3rdPartyBootLogo(const char *pbBuffer, int dwBufferSize);
	};
};