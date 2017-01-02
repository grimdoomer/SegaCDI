// SegaCDI.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Dreamcast\CdiImage.h"
#include "ISO/Iso9660.h"

void printUse()
{
	// Print the program command line args.
	printf("SegaCDI.exe <cdi_file> <options>\n\n");

	printf("\tOptions:\n");
	printf("\t<cdi_file>\t\t.cdi image file\n\n");

	printf("\t-v\t\t\tprintf extended info\n");
	printf("\t-c\t\tconvert to data/data iso\n");
	printf("\t-o <output_folder>\toutput folder\n");
	printf("\t-s <session#:track#>\tdump track from session (value is optional)\n");

	// File extract options
	printf("\t-e <files>\t\textract files to output folder\n");
	printf("\t\ta\tdump all files\n");
	printf("\t\ti\tIP.BIN\n");
	printf("\t\tl\tboot image\n");
}

bool getCmdArg(int argc, CHAR* argv[], LPCSTR psCmd)
{
	// Loop through all the command line args.
	for (int i = 0; i < argc; i++)
	{
		// Check if this arg is the one we are looking for.
		if (strcmp(psCmd, argv[i]) == 0)
		{
			// Return true.
			return true;
		}
	}

	// The command line arg was not found.
	return false;
}

bool getCmdArgHasValue(int argc, CHAR* argv[], LPCSTR psCmd)
{
	// Loop through all the command line args.
	for (int i = 0; i < argc; i++)
	{
		// Check if this arg is the one we are looking for.
		if (strcmp(psCmd, argv[i]) == 0)
		{
			// Check there is one more argument after this one.
			if (i < argc - 1)
			{
				// Check if the next arg starts with a '-' character.
				if (argv[i + 1][0] == '-')
				{
					// The next arg is not a value.
					return false;
				}

				// Return true.
				return true;
			}
		}
	}

	// The command line arg was not found.
	return false;
}

bool getCmdArgValue(int argc, CHAR* argv[], LPCSTR psCmd, CString *psValue)
{
	// Loop through all the command line args.
	for (int i = 0; i < argc; i++)
	{
		// Check if this arg is the one we are looking for.
		if (strcmp(psCmd, argv[i]) == 0)
		{
			// Check there is one more argument after this one.
			if (i < argc - 1)
			{
				// Copy the arg value to the output string.
				*psValue = argv[i + 1];
				return true;
			}
		}
	}

	// The command line arg was not found.
	return false;
}

bool parseDumpInfoArg(LPCSTR sDumpInfo, DWORD *pdwSessionNum, DWORD *pdwTrackNum)
{
	// Get the index of the ':' character in the dump info and check it's valid.
	const CHAR *index = strrchr(sDumpInfo, ':');
	if ((int)index == 0 || (int)sDumpInfo - (int)index == strlen(sDumpInfo) - 1)
		return false;

	// Create two string pointers for the session and track strings.
	CHAR *psSession = (CHAR*)sDumpInfo;
	CHAR *psTrack = (CHAR*)&index[1];

	// Parse the session and track number.
	*pdwSessionNum = atoi(psSession);
	*pdwTrackNum = atoi(psTrack);

	// Done.
	return true;
}

bool extractFilesFropImage(Dreamcast::CdiImage *pImage, CString sOutputFolder, CString sDumpParam, bool bVerbos)
{
	// Check the dump param for what we should dump.
	if (sDumpParam == "a")
	{
		// Change the dump param to we dump everything.
		sDumpParam = "il";
	}

	// Check if we should dump the IP.BIN file.
	if (sDumpParam.Find("i") > -1)
	{
		// Dump the bootstrap file.
		if (pImage->ExtractIPBin(sOutputFolder) == false)
			return false;
	}

	// Check if we should dump the boot logo.
	if (sDumpParam.Find("l") > -1)
	{
		// Dump the boot logo.
		if (pImage->ExtractMRImage(sOutputFolder) == false)
			return false;
	}

	// Done, return true.
	return true;
}

int main(int argc, CHAR* argv[])
{
	//{
	//	// ISO 9660 sanity checks.
	//	int size = sizeof(ISO9660_DirectoryEntry);
	//	size = sizeof(ISO9660_PrimaryVolumeDescriptor);
	//	size = sizeof(ISO9660_VolumeDescriptor);
	//	return 0;
	//}

	// Debug case.
	if (IsDebuggerPresent())
	{
		CHAR* args[8] = 
		{
			argv[0],
			//"X:\\Dreamcast\\Games\\Toy Commander\\Toy Commander.cdi"		// Mode2/Mode2
			//"G:\\Dreamcast\\Games\\Crazy Taxi 2\\Crazy Taxi 2.cdi",		// Audio/Mode2
			//"X:\\Dreamcast\\Games\\Zombie Revenge\\Zombie Revenge.cdi"		// Mode2/Mode2
			//"X:\\Dreamcast\\Games\\Soul Calibur\\Soul Calibur\\scalibur\\scalibur.cdi",
			"G:\\Dreamcast\\Games\\Quake III Arena\\Quake III Arena.cdi",
			//"Y:\\DEVELOPMENT\\Dreamcast\\Games\\Zombie Revenge\\Zombie Revenge.cdi",
			//"G:\\Dreamcast\\Games\\GameShark_CDX\\GameShark CDX\\e-gscdx.cdi",

			"-v",
			"-o",
			"G:\\Dreamcast\\Games\\Quake III Arena\\extract3",
			//"Y:\\DEVELOPMENT\\Dreamcast\\Games\\Zombie Revenge\\extract",

			"-s",
			"-e", "a"
		};

		argc = 3;
		argv = args;
	}

	// Check the arg count.
	if (argc > 1)
	{
		// Check that the cdi file exists.
		CString sCdiImage = argv[1];
		if (FileExists(sCdiImage) == true)
		{
			// If there is an output folder pull it out.
			CString sOutputFolder = "";
			bool bOutput = getCmdArgValue(argc, argv, "-o", &sOutputFolder);

			// Check for the verbos cmd arg.
			bool bVerbos = getCmdArg(argc, argv, "-v");

			// Print the file name.
			printf("loading image %s\n", sCdiImage);

			// Create a new CdiImage object and parse the image.
			Dreamcast::CdiImage *pImage = new Dreamcast::CdiImage();
			if (pImage->LoadImage(sCdiImage, bVerbos) == false)
			{
				// Failed to load the CDI image, nothing else to do here.
				delete pImage;
				return 0;
			}

			// Check if we should dump a session/track to an iso file.
			if (getCmdArg(argc, argv, "-s") == true && bOutput == true)
			{
				// Check if we have indices for the track or if we should dump everything.
				if (getCmdArgHasValue(argc, argv, "-s") == true)
				{
					// Pull out the session and track info string.
					CString sDumpInfo = "";
					getCmdArgValue(argc, argv, "-s", &sDumpInfo);

					// Parse the session and track numbers.
					DWORD dwSessionNum, dwTrackNum;
					if (parseDumpInfoArg(sDumpInfo.GetString(), &dwSessionNum, &dwTrackNum) == false)
					{
						// Print error, close cdi image and return.
						printf("error parsing session/track numbers!\n");
						delete pImage;
						return 0;
					}

					// Dump the second session to an iso file.
					pImage->WriteTrackToFile(sOutputFolder, dwSessionNum, dwTrackNum);
				}
				else
				{
					// Dump all the tracks in the cdi image file.
					pImage->WriteAllTracks(sOutputFolder);
				}
			}

			// Check if we should extract any files.
			if (getCmdArg(argc, argv, "-e") == true && bOutput == true)
			{
				// Pull out the dump info param.
				CString sDumpInfo = "";
				getCmdArgValue(argc, argv, "-e", &sDumpInfo);

				// Try to extract the requested files.
				if (extractFilesFropImage(pImage, sOutputFolder, sDumpInfo, bVerbos) == false)
				{
					// Error extracting files.
					delete pImage;
					return 0;
				}
			}

			// Check if we should convert the image to a data/data image.
			if (getCmdArg(argc, argv, "-c") == true && bOutput == true)
			{

			}

			// Done.
			delete pImage;
			return 0;
		}
		else
		{
			// Print file not found error and return.
			printf("could not find file %s!", sCdiImage);
			return 0;
		}

		// Invalid arguments somewhere.
		printUse();
	}
	else
		printUse();

	return 0;
}

