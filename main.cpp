//*************************************************************
//Very crude experiments in reading discs using 0xd8
//This file is Based on Truman's "ReadCD" IOCTL example
//(original author information in comment below)
//The EDC/ECC code (in edccchk.cpp) is based on
//code from Natalia Portillo (original copyright notice
//in that source file).
//The descrambling code is by Jonathan Gevaryahu (taken here
//from Sarami's DiscImageCreator repository)

//Porting this to Linux should probably be fairly straightforward. Take a look at
//https://stackoverflow.com/questions/29884540/how-to-issue-a-read-cd-command-to-a-cd-rom-drive-in-windows
//(Scroll down -- there's an example read in Linux)

//*************************************************************
//ReadCD v1.00.
//28 May 2003.
//Written by Truman.
//
//Free source code for learning.
//
//Language and type: mixed C and C++, Win32 console.
//
//Code to demonstrate how to call and use Win32 IOCTL function
//with SCSI_PASS_THROUGH or SCSI_PASS_THROUGH_DIRECT.
//Only works in Windows NT/2K/XP and you must have the rights
//to make this call, i.e. you normally have to log in as
//administrator for this example to work.
//
//This shows how to send a readcd (CDB 12) to read sector 0
//from a CD/R/W and displays the first 24 bytes.
//
//The SCSI codes were taken from drafts documents of MMC1 and
//SPC1.
//
//You normally need the ntddscsi.h file from Microsoft DDK CD,
//but I shouldn't distribute it, so instead I have duplicated
//the neccessary parts.
//
//If you don't have windows.h, some of the define constants are
//listed as comments.
//
//Usage: readcd <drive letter>
//*************************************************************
#include <windows.h>
#include <winioctl.h>
#include <stdio.h>   
#include <conio.h>
#include <cassert>
#include <map>

//Lazy globals
long sector;
long endsector;
FILE* scramout;
FILE* unscramout;
const unsigned int numToRead = 26; // Value from DCDumper
std::map<int, unsigned int> reads;
bool seekback = false;

//ECC/EDC stuff
typedef signed __int8    int8_t;
typedef unsigned __int8  uint8_t;
typedef signed __int16   int16_t;
typedef unsigned __int16 uint16_t;
typedef signed __int32   int32_t;
typedef unsigned __int32 uint32_t;
int8_t ecmify(uint8_t* sector);
void eccedc_init(void);

//Descrambling stuff
extern unsigned char scrambled_table[2352];
void make_scrambled_table(void);

//prototypes
HANDLE OpenVolume(char cDriveLetter);
BOOL CloseVolume(HANDLE hVolume);
BOOL ReadCD(TCHAR cDriveLetter);

//** Defines taken from ntddscsi.h in MS Windows DDK CD
#define SCSI_IOCTL_DATA_OUT             0 //Give data to SCSI device (e.g. for writing)
#define SCSI_IOCTL_DATA_IN              1 //Get data from SCSI device (e.g. for reading)
#define SCSI_IOCTL_DATA_UNSPECIFIED     2 //No data (e.g. for ejecting)

#define IOCTL_SCSI_PASS_THROUGH         0x4D004
typedef struct ScsiPassThrough {
	unsigned short  Length;
	unsigned char   ScsiStatus;
	unsigned char   PathId;
	unsigned char   TargetId;
	unsigned char   Lun;
	unsigned char   CdbLength;
	unsigned char   SenseInfoLength;
	unsigned char   DataIn;
	unsigned int    DataTransferLength;
	unsigned int    TimeOutValue;
	unsigned int    DataBufferOffset;
	unsigned int    SenseInfoOffset;
	unsigned char   Cdb[16];
} SCSI_PASS_THROUGH;

#define IOCTL_SCSI_PASS_THROUGH_DIRECT  0x4D014
typedef struct _SCSI_PASS_THROUGH_DIRECT {
	USHORT Length;
	UCHAR ScsiStatus;
	UCHAR PathId;
	UCHAR TargetId;
	UCHAR Lun;
	UCHAR CdbLength;
	UCHAR SenseInfoLength;
	UCHAR DataIn;
	ULONG DataTransferLength;
	ULONG TimeOutValue;
	PVOID DataBuffer;
	ULONG SenseInfoOffset;
	UCHAR Cdb[16];
}SCSI_PASS_THROUGH_DIRECT, * PSCSI_PASS_THROUGH_DIRECT;
//** End of defines taken from ntddscsi.h from MS Windows DDK CD

//Global variables
SCSI_PASS_THROUGH_DIRECT sptd;
byte DataBuf[2352 * numToRead]; //buffer for holding sector data from drive
byte SenseBuf[255]; //buffer for holding sense data from drive (not used in this example)
HANDLE hVolume;

HANDLE OpenVolume(char cDriveLetter)
/*
	1. Checks the drive type to see if it's identified as a CDROM drive

	2. Uses Win32 CreateFile function to get a handle to a drive
	   that is specified by cDriveLetter.

   If you don't have windows.h, some of the define constants are
   listed as comments.
*/
{
	HANDLE hVolume;
	UINT uDriveType;
	char szVolumeName[8];
	char szRootName[5];
	DWORD dwAccessFlags;

	szRootName[0] = cDriveLetter;
	szRootName[1] = ':';
	szRootName[2] = '\\';
	szRootName[3] = '\0';

	uDriveType = GetDriveTypeA(szRootName);

	switch (uDriveType) {
	case DRIVE_CDROM:
		dwAccessFlags = GENERIC_READ | GENERIC_WRITE;	//#define GENERIC_READ 0x80000000L
														//#define GENERIC_WRITE 0x40000000L
		break;
	default:
		printf("Drive type is not CDROM/DVD.\n");
		return INVALID_HANDLE_VALUE;  //#define INVALID_HANDLE_VALUE (long int *)-1
	}

	szVolumeName[0] = '\\';
	szVolumeName[1] = '\\';
	szVolumeName[2] = '.';
	szVolumeName[3] = '\\';
	szVolumeName[4] = cDriveLetter;
	szVolumeName[5] = ':';
	szVolumeName[6] = '\0';

	//#define FILE_SHARE_READ 0x00000001
	//#define FILE_SHARE_WRITE 0x00000002
	//#define OPEN_EXISTING 3
	//#define FILE_ATTRIBUTE_NORMAL 0x00000080
	hVolume = CreateFileA(szVolumeName,
		dwAccessFlags,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (hVolume == INVALID_HANDLE_VALUE)
		printf("Could not create handle for CDROM/DVD.\n");

	return hVolume;

}

BOOL CloseVolume(HANDLE hVolume)
{
	return CloseHandle(hVolume);
}

//From sarami's EccEdc (EccEdc.cpp)
inline BYTE BcdToDec(
	BYTE bySrc
) {
	return (BYTE)(((bySrc >> 4) & 0x0f) * 10 + (bySrc & 0x0f));
}

static long firstInBuf = -LONG_MAX;
static long lastInBuf = -LONG_MAX;
static long bufferStartOffset = 0;

//Pass the read data into a crappy buffer structure
//Arguments: expected LBA for first, last sectors to see in buffer data
BOOL PassToBuffer(long first, long last)
{
	//Try to see if there's a sync somewhere in what we got, and verify it's the sector number we were expecting
	int offset = 0;
	for (offset = 0; offset < 2352 * (numToRead - 1); offset++)
	{
		printf("Searching for data sector\n");
		byte* SectorData = &(DataBuf[offset]);
		if (SectorData[0x000] == 0x00 && // sync (12 bytes)
			SectorData[0x001] == 0xFF && SectorData[0x002] == 0xFF && SectorData[0x003] == 0xFF && SectorData[0x004] == 0xFF &&
			SectorData[0x005] == 0xFF && SectorData[0x006] == 0xFF && SectorData[0x007] == 0xFF && SectorData[0x008] == 0xFF &&
			SectorData[0x009] == 0xFF && SectorData[0x00A] == 0xFF && SectorData[0x00B] == 0x00)
		{
			//Now descramble the sector enough to check the LBA against what we expect
			byte TempBufUnscrambled[16];
			for (int i = 12; i < 16; i++)
			{
				TempBufUnscrambled[i] = SectorData[i] ^ scrambled_table[i];
			}
			byte minutes = BcdToDec(TempBufUnscrambled[12]);
			byte seconds = BcdToDec(TempBufUnscrambled[13]);
			byte frames = BcdToDec(TempBufUnscrambled[14]);
			long foundSector = ((minutes * 60) + seconds) * 75 + frames;
			//Header sector numbering and CDB numbering differ by 150 -- adjust number from header
			foundSector -= 150;
			printf("Found sector header for %ld -- looking for %ld\n", foundSector, first);
			if (foundSector == first)
			{
				printf("Found sector %ld at offset %d\n", sector, offset);
				firstInBuf = first;
				lastInBuf = last;
				bufferStartOffset = offset;
				break;
			}
			if (foundSector < sector)
			{
				offset += 2200; // Don't jump all the way to the place we expect the next header -- sometimes crazy things happen with changing offsets
				continue;
			}
		}
	}

	return true;
}

BOOL GetFromBuffer(long sectorNo)
{
	if (sectorNo < firstInBuf || sectorNo > lastInBuf) return false;
	byte* NewBuf = &(DataBuf[bufferStartOffset + ((sectorNo - firstInBuf) * 2352)]);

	/*printf("Scrambled data:\n");
	for (int i = 0; i < 2352; i++)
	{
		printf("%02X", NewBuf[i]);
		if (!((i + 1) % 16)) printf("\n");
	}*/
	byte NewBufUnscrambled[2352];
	//printf("\n");
	//printf("Unscrambled data:\n");
	for (int i = 0; i < 2352; i++)
	{
		NewBufUnscrambled[i] = NewBuf[i] ^ scrambled_table[i];
		//printf("%02X", NewBufUnscrambled[i]);
		//if (!((i + 1) % 16)) printf("\n");
	}
	if (ecmify(NewBufUnscrambled))
	{
		printf("Error. Need to fix sector %ld. Tried %u times.\n", sector, reads[sector]);
		//Sector repair logic can go here
	}
	fwrite(NewBufUnscrambled, 1, 2352, unscramout);
	printf("Wrote sector %ld unscrambled\n", sector);
	fwrite(NewBuf, 1, 2352, scramout);
	printf("Wrote sector %ld scrambled\n", sector);
	return true;
	//printf("\n");
	//printf("Found sector %02X:%02X:%02X at offset of %d\n", NewBufUnscrambled[0x00C], NewBufUnscrambled[0x00D], NewBufUnscrambled[0x00E], offset);
	//printf("Result:\n %d\n", ecmify(NewBufUnscrambled));
}

BOOL ReadCD(char cDriveLetter, bool mode = false)
/*
	1. Set up the sptd values.
	2. Set up the CDB for MMC readcd (CDB12) command.
	3. Send the request to the drive.
*/
{
	BOOL success;
	long sectorM2 = sector - 2;

	//Don't read if it's in the buffer
	if (GetFromBuffer(sector)) return true;

	if (hVolume != INVALID_HANDLE_VALUE)
	{
		sptd.Length = sizeof(sptd);
		sptd.PathId = 0;   //SCSI card ID will be filled in automatically
		sptd.TargetId = 0; //SCSI target ID will also be filled in
		sptd.Lun = 0;      //SCSI lun ID will also be filled in
		sptd.CdbLength = 12; //CDB size is 12 for ReadCD D8 command
		sptd.SenseInfoLength = 0; //Don't return any sense data
		sptd.DataIn = SCSI_IOCTL_DATA_IN; //There will be data from drive
		sptd.DataTransferLength = 2352 * numToRead; //Size of data - in this case 1 sector of data
		sptd.TimeOutValue = 15;  //SCSI timeout value (60 seconds - maybe it should be longer)
		sptd.DataBuffer = (PVOID) & (DataBuf);
		sptd.SenseInfoOffset = 0;

		if (!mode) //if we're not using 0xbe mode
		{
			//CDB with values for ReadCD CDB12 command.  The values were taken from MMC1 draft paper.
			/*sptd.Cdb[0] = 0xBE;  //Code for ReadCD CDB12 command
			sptd.Cdb[1] = 0;
			sptd.Cdb[2] = 0;  //Most significant byte of:
			sptd.Cdb[3] = 0;  //3rd byte of:
			sptd.Cdb[4] = 0;  //2nd byte of:
			sptd.Cdb[5] = 0;  //Least sig byte of LBA sector no. to read from CD
			sptd.Cdb[6] = 0;  //Most significant byte of:
			sptd.Cdb[7] = 0;  //Middle byte of:
			sptd.Cdb[8] = 1;  //Least sig byte of no. of sectors to read from CD
			sptd.Cdb[9] = 0xF8;  //Raw read, 2352 bytes per sector
			sptd.Cdb[10] = 0;
			sptd.Cdb[11] = 0;
			sptd.Cdb[12] = 0;
			sptd.Cdb[13] = 0;
			sptd.Cdb[14] = 0;
			sptd.Cdb[15] = 0;*/
			sptd.Cdb[0] = 0xD8;  //Code for ReadCDDA command
			sptd.Cdb[1] = 0;//8; //FUA?
			//sptd.Cdb[2] = 0;  //Most significant byte of:
			//sptd.Cdb[3] = 0;  //3rd byte of:
			//sptd.Cdb[4] = 0;  //2nd byte of:
			//sptd.Cdb[5] = 0;  //Least sig byte of LBA sector no. to read from CD
			sptd.Cdb[2] = (BYTE)(sectorM2 >> 24);				// Set Start Sector 
			sptd.Cdb[3] = (BYTE)((sectorM2 >> 16) & 0xFF);		// Set Start Sector
			sptd.Cdb[4] = (BYTE)((sectorM2 >> 8) & 0xFF);		// Set Start Sector
			sptd.Cdb[5] = (BYTE)(sectorM2 & 0xFF);				// Set Start Sector
			sptd.Cdb[6] = 0;
			sptd.Cdb[7] = 0;  //MSB
			sptd.Cdb[8] = 0;  //Middle byte of:
			sptd.Cdb[9] = (BYTE)numToRead;  //Least sig byte of no. of sectors to read from CD
			sptd.Cdb[10] = 0;
			sptd.Cdb[11] = 0;
			sptd.Cdb[12] = 0;
			sptd.Cdb[13] = 0;
			sptd.Cdb[14] = 0;
			sptd.Cdb[15] = 0;
		}
		else
		{
			//CDB with values for ReadCD CDB12 command.  The values were taken from MMC1 draft paper.
			sptd.Cdb[0] = 0xBE;  //Code for ReadCD CDB12 command
			sptd.Cdb[1] = 0x4;
			sptd.Cdb[2] = (BYTE)(sectorM2 >> 24);				// Set Start Sector 
			sptd.Cdb[3] = (BYTE)((sectorM2 >> 16) & 0xFF);		// Set Start Sector
			sptd.Cdb[4] = (BYTE)((sectorM2 >> 8) & 0xFF);		// Set Start Sector
			sptd.Cdb[5] = (BYTE)(sectorM2 & 0xFF);				// Set Start Sector
			sptd.Cdb[6] = 0;  //MSB
			sptd.Cdb[7] = 0;  //Middle byte of:
			sptd.Cdb[8] = (BYTE)numToRead;  //Least sig byte of no. of sectors to read from CD
			sptd.Cdb[9] = 0xF8;
			sptd.Cdb[10] = 0;
			sptd.Cdb[11] = 0;
			sptd.Cdb[12] = 0;
			sptd.Cdb[13] = 0;
			sptd.Cdb[14] = 0;
			sptd.Cdb[15] = 0;
		}

		DWORD dwBytesReturned;

		//Send the command to drive
		success = DeviceIoControl(hVolume,
			IOCTL_SCSI_PASS_THROUGH_DIRECT,
			(PVOID)&sptd, (DWORD)sizeof(SCSI_PASS_THROUGH_DIRECT),
			NULL, 0,
			&dwBytesReturned,
			NULL);
		if (success)
		{
			reads[sector]++;
			PassToBuffer(sector, sector + (numToRead - 4));
			return GetFromBuffer(sector);
		}
		else
		{
			printf("DeviceIOControl with SCSI_PASS_THROUGH_DIRECT command failed.\n");
		}


		return success;
	}
	else
	{
		return FALSE;
	}
}

BOOL FlushCache(char cDriveLetter)
/*
	1. Set up the sptd values.
	2. Set up the CDB for MMC readcd (CDB12) command.
	3. Send the request to the drive.
*/
{
	BOOL success;

	if (hVolume != INVALID_HANDLE_VALUE)
	{
		sptd.Length = sizeof(sptd);
		sptd.PathId = 0;   //SCSI card ID will be filled in automatically
		sptd.TargetId = 0; //SCSI target ID will also be filled in
		sptd.Lun = 0;      //SCSI lun ID will also be filled in
		sptd.CdbLength = 12; //CDB size is 12 for ReadCD D8 command
		sptd.SenseInfoLength = 0; //Don't return any sense data
		sptd.DataIn = SCSI_IOCTL_DATA_IN; //There will be data from drive
		sptd.DataTransferLength = 2352; //Size of data - in this case 1 sector of data
		sptd.TimeOutValue = 15;  //SCSI timeout value (60 seconds - maybe it should be longer)
		sptd.DataBuffer = (PVOID) & (DataBuf);
		sptd.SenseInfoOffset = 0;

		//CDB with values for ReadCD CDB12 command.  The values were taken from MMC1 draft paper.
		if (reads[sector] + 1 % 50 == 0)
		{
			printf("Seeking back to zero for cache flush\n");
			//sptd.Cdb[0] = 0xBE;  //Code for ReadCD CDB12 command
			//sptd.Cdb[1] = 0;
			sptd.Cdb[0] = 0xA8;  //Code for ReadCD CDB12 command
			sptd.Cdb[1] = 8;
			sptd.Cdb[2] = 0;  //Most significant byte of:
			sptd.Cdb[3] = 0;  //3rd byte of:
			sptd.Cdb[4] = 0;  //2nd byte of:
			sptd.Cdb[5] = 0;  //Least sig byte of LBA sector no. to read from CD
			sptd.Cdb[6] = 0;  //Most significant byte of:
			sptd.Cdb[7] = 0;  //Middle byte of:
			//sptd.Cdb[8] = 1;  //Least sig byte of no. of sectors to read from CD
			sptd.Cdb[8] = 0;  //Least sig byte of no. of sectors to read from CD
			//sptd.Cdb[9] = 0xF8;  //Raw read, 2352 bytes per sector
			sptd.Cdb[9] = 1;  //Raw read, 2352 bytes per sector
			sptd.Cdb[10] = 0;
			sptd.Cdb[11] = 0;
			sptd.Cdb[12] = 0;
			sptd.Cdb[13] = 0;
			sptd.Cdb[14] = 0;
			sptd.Cdb[15] = 0;
		}
		else {
			sptd.Cdb[0] = 0xA8;  //Code for ReadCDDA command
			sptd.Cdb[1] = 8; //FUA
			sptd.Cdb[2] = (BYTE)(sector >> 24);				// Set Start Sector 
			sptd.Cdb[3] = (BYTE)((sector >> 16) & 0xFF);		// Set Start Sector
			sptd.Cdb[4] = (BYTE)((sector >> 8) & 0xFF);		// Set Start Sector
			sptd.Cdb[5] = (BYTE)(sector & 0xFF);				// Set Start Sector
			sptd.Cdb[6] = 0;
			sptd.Cdb[7] = 0;  //MSB
			sptd.Cdb[8] = 0;  //Middle byte of:
			sptd.Cdb[9] = 1;  //Least sig byte of no. of sectors to read from CD
			sptd.Cdb[10] = 0;
			sptd.Cdb[11] = 0;
			sptd.Cdb[12] = 0;
			sptd.Cdb[13] = 0;
			sptd.Cdb[14] = 0;
			sptd.Cdb[15] = 0;
		}

		DWORD dwBytesReturned;

		//Send the command to drive
		success = DeviceIoControl(hVolume,
			IOCTL_SCSI_PASS_THROUGH_DIRECT,
			(PVOID)&sptd, (DWORD)sizeof(SCSI_PASS_THROUGH_DIRECT),
			NULL, 0,
			&dwBytesReturned,
			NULL);
		if (success)
		{
			printf("Cleared cache\n");
		}
		else
		{
			printf("DeviceIOControl with SCSI_PASS_THROUGH_DIRECT command failed.\n");
		}


		return success;
	}
	else
	{
		return FALSE;
	}
}

void Usage()
{
	printf("Usage: readcd <drive letter> <first sector number> <last sector number> [put anything here to enable 0xBE mode]\n\n");
	return;
}

void main(int argc, char* argv[])
{
	if (argc < 4) {
		Usage();
		return;
	}

	make_scrambled_table();
	eccedc_init();

	sector = atoi(argv[2]);

	endsector = atoi(argv[3]);

	hVolume = OpenVolume(argv[1][0]);

	printf("Reading from %ld to %ld\n", sector, endsector);

	fopen_s(&scramout, "scramout.bin", "wb");
	fopen_s(&unscramout, "unscramout.bin", "wb");

	while (sector < endsector)
	{
		if (!ReadCD(argv[1][0], (argc > 4)))
		{
			//long offset = rand() % 6;
			//offset *= (rand() % 2 ? -1 : 1);
			//if (sector + offset < 0) offset = -sector;
			//if (sector + offset > endsector) offset = endsector - sector;
			//sector += offset;
			//FlushCache(argv[1][0]);
			//sector -= offset;
			sector--;
		}
		sector++;
	}

	//printf("Finished. Press any key.");
	//_getch();

	fclose(scramout);
	fclose(unscramout);
	CloseVolume(hVolume);
	return;
}