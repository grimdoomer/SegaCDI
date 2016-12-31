/*
	SegaCDI - Sega Dreamcast cdi image validator.

	Bootstrap.cpp - IP.BIN bootstrap utilities.

	Sep 28th, 2014
*/

#include "../stdafx.h"
#include "Bootstrap.h"
#include "MRImage.h"
#include "../Misc/Utilities.h"

namespace Dreamcast
{
	/*
		For the checksum in the sDeviceInfo field.

	int calcCRC(const unsigned char *buf, int size)
	{
	  int i, c, n = 0xffff;
	  for (i = 0; i < size; i++)
	  {
		n ^= (buf[i]<<8);
		for (c = 0; c < 8; c++)
		  if (n & 0x8000)
		n = (n << 1) ^ 4129;
		  else
		n = (n << 1);
	  }
	  return n & 0xffff;
	}
	*/

	Bootstrap::Bootstrap()
	{

	}

	Bootstrap::~Bootstrap()
	{

	}

	bool Bootstrap::LoadBootstrap(char *pbBuffer, int dwBufferSize)
	{
		// Check the buffer size is valid.
		if (dwBufferSize < BOOTSTRAP_SIZE)
		{
			// Print error and return false.
			printf("ERROR: bootstrap buffer size is too small!\n");
			return false;
		}

		// Copy the bootstrap buffer locally.
		memcpy(&this->m_sBootstrap, pbBuffer, BOOTSTRAP_SIZE);

		// Check the hardware id.
		if (memcmp(this->m_sBootstrap.sIPHeader.sHardwareID, HARDWARE_ID, 16) != 0)
		{
			// Print error and return false.
			printf("ERROR: bootstrap contains invalid hardware id!\n");
			return false;
		}

		// Check the hardware vendor id.
		if (memcmp(this->m_sBootstrap.sIPHeader.sHardwareVendorID, HARDWARE_VENDOR_ID, 16) != 0)
		{
			// Print error and return false.
			printf("ERROR: bootstrap contains invalid hardware vendor is!\n");
			return false;
		}

		// Good enough for me, return true.
		return true;
	}

	void Bootstrap::PatchBootstrapForRegion(int dwRegion)
	{
		// Reset the region code in the IP header.
		memset(&this->m_sBootstrap.sIPHeader.sRegionCode, ' ', MAX_NUM_REGIONS);

		// Loop through the region symbol table and reset each one.
		for (int i = 0; i < MAX_NUM_REGIONS; i++)
		{
			// Reset the region symbol.
			//this->m_sBootstrap.sRegionSymbols[i].dwUnknown = REGION_SYMBOL_UNKNOWN;
			memset(&this->m_sBootstrap.sRegionSymbols[i].sDescription, ' ', REGION_SYMBOL_DESCRIPTION_SIZE);
		}

		// Check for Japan region.
		if (dwRegion & BootstrapRegion::Japan == BootstrapRegion::Japan)
		{
			// Add the Japan region code and symbol.
			this->m_sBootstrap.sIPHeader.sRegionCode[0] = REGION_CODE_JAPAN;
			memcpy(&this->m_sBootstrap.sRegionSymbols[0].sDescription, REGION_SYMBOL_JAPAN, REGION_SYMBOL_DESCRIPTION_SIZE);
		}

		// Check for USA region.
		if (dwRegion & BootstrapRegion::USA == BootstrapRegion::USA)
		{
			// Add the USA region code and symbol.
			this->m_sBootstrap.sIPHeader.sRegionCode[1] = REGION_CODE_USA;
			memcpy(&this->m_sBootstrap.sRegionSymbols[1].sDescription, REGION_SYMBOL_USA, REGION_SYMBOL_DESCRIPTION_SIZE);
		}

		// Check for the Europe region.
		if (dwRegion & BootstrapRegion::Europe == BootstrapRegion::Europe)
		{
			// Add the Europe region code and symbol.
			this->m_sBootstrap.sIPHeader.sRegionCode[2] = REGION_CODE_EUROPE;
			memcpy(&this->m_sBootstrap.sRegionSymbols[2].sDescription, REGION_SYMBOL_EUROPE, REGION_SYMBOL_DESCRIPTION_SIZE);
		}
	}

	void Bootstrap::PatchBootstrapForVGA()
	{
		// Set the "VGA Box" bit in the peripherals to 1.
		this->m_sBootstrap.sIPHeader.sPeripherals[5] = '1';
	}

	void Bootstrap::PatchBootstrapForOS(bool bIsWinCE)
	{
		// Check if the executable is win CE based or not.
		if (bIsWinCE == true)
		{
			// Set the "Uses Win CE" bit in the peripherals to 1.
			this->m_sBootstrap.sIPHeader.sPeripherals[6] = '1';
		}
		else
		{
			// Set the "Uses Win CE" bit in the peripherals to 0.
			this->m_sBootstrap.sIPHeader.sPeripherals[6] = '0';
		}
	}

	bool Bootstrap::CopyBootstrapBuffer(char *pbBuffer, int dwBufferSize)
	{
		// Check if the buffer size is large enough.
		if (dwBufferSize < BOOTSTRAP_SIZE)
		{
			// Print error and return.
			printf("ERROR: external bootstrap buffer is not large enough (what the actual fuck?)!\n");
			return false;
		}

		// Copy the bootstrap buffer.
		memcpy(pbBuffer, &this->m_sBootstrap.bData, BOOTSTRAP_SIZE);
		return true;
	}

	bool Bootstrap::Extract3rdPartyBootLogo(const char *psOutputFolder)
	{
		// Check if a 3rd party boot logo exists in bootstrap 1.
		if (*(short*)&this->m_sBootstrap.bBootstrap1[32] != ByteFlip16(MR_IMAGE_MAGIC))
		{
			// Print error and return true since it is not fatal.
			printf("WARNING: bootstrap does not contain a 3rd-party boot logo, skipping extraction!\n");
			return true;
		}

		// Format the file name.
		char sFileName[MAX_PATH] = { 0 };
		sprintf(sFileName, "%s\\bootlogo.bmp", psOutputFolder);

		// Extract the boot logo to a bmp file.
		return SaveMRToBMP(&this->m_sBootstrap.bBootstrap1[32], MR_MAX_SIZE, sFileName);
	}

	bool Bootstrap::Inject3rdPartyBootLogo(const char *pbBuffer, int dwBufferSize)
	{
		// Check the buffer size and make sure the image will fit in the IP.BIN file.
		if (dwBufferSize > MR_MAX_SIZE)
		{
			// Print error and return false.
			printf("ERROR: boot logo buffer is too large!\n");
			return false;
		}

		// Clear any old 3rd party boot logos from bootstrap 1.
		memset(&this->m_sBootstrap.bBootstrap1[32], 0, MR_MAX_SIZE);

		// Copy the new 3rd party logo to bootstrap 1.
		memcpy(&this->m_sBootstrap.bBootstrap1[32], pbBuffer, MR_MAX_SIZE);

		// Done, return true.
		return true;
	}
};