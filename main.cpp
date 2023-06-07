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
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <cassert>
#include <map>
#include <cstdint>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <scsi/sg.h>
#include <scsi/scsi.h>
#include <linux/cdrom.h>
#include <sys/ioctl.h>
#include <limits.h>

// Lazy globals
long sector;
long endsector;
FILE* scramout;
FILE* unscramout;
const unsigned int numToRead = 26; // Value from DCDumper
std::map<int, unsigned int> reads;
bool seekback = false;

//ECC/EDC stuff
/*typedef signed __int8    int8_t;
typedef unsigned __int8  uint8_t;
typedef signed __int16   int16_t;
typedef unsigned __int16 uint16_t;
typedef signed __int32   int32_t;
typedef unsigned __int32 uint32_t;*/
int8_t ecmify(uint8_t* sector);
void eccedc_init(void);

//Descrambling stuff
extern unsigned char scrambled_table[2352];
void make_scrambled_table(void);

//prototypes
int OpenVolume(char* cdDeviceFile);
bool CloseVolume(int hVolume);
bool ReadCD(int hVolume);
int testReedSolomon(uint8_t Buf[2352]);

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
	unsigned short Length;
	unsigned char ScsiStatus;
	unsigned char PathId;
	unsigned char TargetId;
	unsigned char Lun;
	unsigned char CdbLength;
	unsigned char SenseInfoLength;
	unsigned char DataIn;
	unsigned long DataTransferLength;
	unsigned long TimeOutValue;
	void* DataBuffer;
	unsigned long SenseInfoOffset;
	unsigned char  Cdb[16];
}SCSI_PASS_THROUGH_DIRECT, * PSCSI_PASS_THROUGH_DIRECT;
//** End of defines taken from ntddscsi.h from MS Windows DDK CD

//Global variables
struct sg_io_hdr sptd;
uint8_t DataBuf[2352 * numToRead]; //buffer for holding sector data from drive
uint8_t SenseBuf[255]; //buffer for holding sense data from drive (not used in this example)
int hVolume;

int OpenVolume(char* cdDeviceFile)
/*
	1. Checks the drive type to see if it's identified as a CDROM drive

	2. Uses Win32 CreateFile function to get a handle to a drive
	   that is specified by cDriveLetter.

   If you don't have windows.h, some of the define constants are
   listed as comments.
   
   Edit: The above is no longer correct. Instead, it does not check since
   we can assume we're always giving it a CDROM drive, and because of
   Linux, we now use a file descriptor instead of a handle. Notably, the
   rest of the program has been altered around this as well.
*/
{
	int hVolume = open(cdDeviceFile, O_RDONLY | O_NONBLOCK);
	
	return hVolume;

}

bool CloseVolume(int hVolume)
{
	return close(hVolume);
}

//From sarami's EccEdc (EccEdc.cpp)
inline uint8_t BcdToDec(
	uint8_t bySrc
) {
	return (uint8_t)(((bySrc >> 4) & 0x0f) * 10 + (bySrc & 0x0f));
}

static long firstInBuf = -LONG_MAX;
static long lastInBuf = -LONG_MAX;
static long bufferStartOffset = 0;

//Pass the read data into a crappy buffer structure
//Arguments: expected LBA for first, last sectors to see in buffer data
bool PassToBuffer(long first, long last)
{
	//Try to see if there's a sync somewhere in what we got, and verify it's the sector number we were expecting
	int offset = 0;
	for (offset = 0; offset < 2352 * (numToRead - 1); offset++)
	{
		printf("Searching for data sector\n");
		uint8_t* SectorData = &(DataBuf[offset]);
		if (SectorData[0x000] == 0x00 && // sync (12 bytes)
			SectorData[0x001] == 0xFF && SectorData[0x002] == 0xFF && SectorData[0x003] == 0xFF && SectorData[0x004] == 0xFF &&
			SectorData[0x005] == 0xFF && SectorData[0x006] == 0xFF && SectorData[0x007] == 0xFF && SectorData[0x008] == 0xFF &&
			SectorData[0x009] == 0xFF && SectorData[0x00A] == 0xFF && SectorData[0x00B] == 0x00 )
		{
			//Now descramble the sector enough to check the LBA against what we expect
			uint8_t TempBufUnscrambled[16];
			for (int i = 12; i < 16; i++)
			{
				TempBufUnscrambled[i] = SectorData[i] ^ scrambled_table[i];
			}
			uint8_t minutes = BcdToDec(TempBufUnscrambled[12]);
			uint8_t seconds = BcdToDec(TempBufUnscrambled[13]);
			uint8_t frames = BcdToDec(TempBufUnscrambled[14]);
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

bool GetFromBuffer(long sectorNo)
{
	if (sectorNo < firstInBuf || sectorNo > lastInBuf) return false;
	uint8_t* NewBuf = &(DataBuf[bufferStartOffset + ((sectorNo - firstInBuf) * 2352)]);

	/*printf("Scrambled data:\n");
	for (int i = 0; i < 2352; i++)
	{
		printf("%02X", NewBuf[i]);
		if (!((i + 1) % 16)) printf("\n");
	}*/
	uint8_t NewBufUnscrambled[2352];
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
	// printf("\n");
	// printf("Found sector %02X:%02X:%02X at offset of %d\n", NewBufUnscrambled[0x00C], NewBufUnscrambled[0x00D], NewBufUnscrambled[0x00E], offset);
	// printf("Result:\n %d\n", ecmify(NewBufUnscrambled));
}

bool ReadCD(int hVolume, bool mode = false)
/*
	1. Set up the sptd values.
	2. Set up the CDB for MMC readcd (CDB12) command.
	3. Send the request to the drive.
*/
{
	bool success; // Create a boolean that is returned to indicate success or lack thereof.
	long sectorM2 = sector - 2; // Sector minus 2.
	
	if (GetFromBuffer(sector)) return true; // If there is content in the buffer, do not read.

	if (hVolume != -1)
	{
		unsigned char CMD[15];
		sptd.interface_id = 'S';
		sptd.sbp = NULL;
		sptd.mx_sb_len = 0;
		sptd.dxfer_len = 2352 * numToRead;
		sptd.dxfer_direction = SG_DXFER_FROM_DEV;
		sptd.dxferp = (void*) &DataBuf; 
		sptd.timeout = 60000;

		if (!mode) // Conditional: If we are not using 0xBE mode.
		{
			// CDB with values for ReadCDDA proprietary Plextor command.
			CMD[0] = 0xD8;						// Code for ReadCDDA command
			CMD[1] = 0; 						// Alternate value of 8 was in original comment.
			CMD[2] = (uint8_t)(sectorM2 >> 24);			// Set Start Sector; Most significant byte of:
			CMD[3] = (uint8_t)((sectorM2 >> 16) & 0xFF);		// Set Start Sector; 3rd byte of:
			CMD[4] = (uint8_t)((sectorM2 >> 8) & 0xFF);		// Set Start Sector; 2nd byte of:
			CMD[5] = (uint8_t)(sectorM2 & 0xFF);			// Set Start Sector; Least sig byte of LBA sector no. to read from CD
			CMD[6] = 0;
			CMD[7] = 0;  						// MSB
			CMD[8] = 0;  						// Middle byte of:
			CMD[9] = (uint8_t)numToRead; 				// Least sig byte of no. of sectors to read from CD
			CMD[10] = 0;
			CMD[11] = 0;
			CMD[12] = 0;
			CMD[13] = 0;
			CMD[14] = 0;
			CMD[15] = 0;
		}
		else // Any other condition.
		{
			// CDB with values for ReadCD CDB12 command. The values were taken from MMC1 draft paper.
			CMD[0] = 0xBE;  					// Code for ReadCD CDB12 command
			CMD[1] = 0x4;
			CMD[2] = (uint8_t)(sectorM2 >> 24);			// Set Start Sector 
			CMD[3] = (uint8_t)((sectorM2 >> 16) & 0xFF);		// Set Start Sector
			CMD[4] = (uint8_t)((sectorM2 >> 8) & 0xFF);		// Set Start Sector
			CMD[5] = (uint8_t)(sectorM2 & 0xFF);			// Set Start Sector
			CMD[6] = 0; 						// MSB
			CMD[7] = 0;						// Middle byte of:
			CMD[8] = (uint8_t)numToRead;				// Least sig byte of no. of sectors to read from CD
			CMD[9] = 0xF8;
			CMD[10] = 0;
			CMD[11] = 0;
			CMD[12] = 0;
			CMD[13] = 0;
			CMD[14] = 0;
			CMD[15] = 0;
		}
		sptd.cmdp = CMD;
		sptd.cmd_len = sizeof(CMD);
		success = ioctl(hVolume, SG_IO, &sptd);				//Send the command to drive
		if (success != -1) // Conditional: If we succeed.
		{
			reads[sector]++;
			PassToBuffer(sector, sector + (numToRead - 4));
			return GetFromBuffer(sector);
		}
		else // Any other condition.
		{
			printf("DeviceIOControl with SCSI_PASS_THROUGH_DIRECT command failed.\n");
		}
		return 1; // Success
	}
	else
	{
		return 0; // Failure
	}
}

bool FlushCache(char cDriveLetter)
/*
	1. Set up the sptd values.
	2. Set up the CDB for MMC readcd (CDB12) command.
	3. Send the request to the drive.
*/
{
	bool success;
	if (hVolume != -1)
	{
		unsigned char CMD[15];
		sptd.interface_id = 'S';
		sptd.sbp = NULL;
		sptd.mx_sb_len = 0;
		sptd.dxfer_len = 2352 * numToRead;
		sptd.dxfer_direction = SG_DXFER_FROM_DEV;
		sptd.dxferp = (void*) &DataBuf; 
		sptd.timeout = 60000;

		if (reads[sector] + 1 % 50 == 0)
		{
			// CDB with values for ReadCD CDB12 command.  The values were taken from MMC1 draft paper.
			printf("Seeking back to zero for cache flush\n");
			CMD[0] = 0xA8;						// Code for ReadCD CDB12 command; Value of 0xBE was in original comment
			CMD[1] = 8;						// Alternate value of 0 was in original comment
			CMD[2] = 0;						// Most significant byte of:
			CMD[3] = 0;						// 3rd byte of:
			CMD[4] = 0;						// 2nd byte of:
			CMD[5] = 0;						// Least sig byte of LBA sector no. to read from CD
			CMD[6] = 0;						// Most significant byte of:
			CMD[7] = 0;						// Middle byte of:
			CMD[8] = 0;						// Least sig byte of no. of sectors to read from CD; Value of 1 was in original comment
			CMD[9] = 1;						// Raw read, 2352 bytes per sector; Value of 0xF8 was in original comment
			CMD[10] = 0;
			CMD[11] = 0;
			CMD[12] = 0;
			CMD[13] = 0;
			CMD[14] = 0;
			CMD[15] = 0;
		}
		else {
			CMD[0] = 0xA8;  					// Code for ReadCD CDB12 command; Original comment claimed it was the ReadCDDA command.
			CMD[1] = 8; 						// FUA
			CMD[2] = (uint8_t)(sector >> 24);			// Set Start Sector 
			CMD[3] = (uint8_t)((sector >> 16) & 0xFF);		// Set Start Sector
			CMD[4] = (uint8_t)((sector >> 8) & 0xFF);		// Set Start Sector
			CMD[5] = (uint8_t)(sector & 0xFF);			// Set Start Sector
			CMD[6] = 0;
			CMD[7] = 0;						// MSB
			CMD[8] = 0;						// Middle byte of:
			CMD[9] = 1;						// Least sig byte of no. of sectors to read from CD
			CMD[10] = 0;
			CMD[11] = 0;
			CMD[12] = 0;
			CMD[13] = 0;
			CMD[14] = 0;
			CMD[15] = 0;
		}
		
		sptd.cmdp = CMD;
		sptd.cmd_len = sizeof(CMD);

		success = ioctl(hVolume, SG_IO, &sptd);				// Send the command to drive
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
		return 0;
	}
}

void Usage()
{
	printf("Usage: readcd <device file> <first sector number> <last sector number> [put anything here to enable 0xBE mode]\n\n");
	return;
}

int main(int argc, char* argv[])
{
	memset(&sptd, 0, sizeof(struct sg_io_hdr));
	if (argc < 4) {
		Usage();
		return 0;
	}

	make_scrambled_table();
	eccedc_init();

	sector = atoi(argv[2]);

	endsector = atoi(argv[3]);

	hVolume = OpenVolume(argv[1]);

	printf("Reading from %ld to %ld\n", sector, endsector);
	
	scramout = fopen("scramout.bin", "wb");
	unscramout = fopen("unscramout.bin", "wb");

	while (sector < endsector)
	{
		if (!ReadCD(hVolume, (argc > 4)))
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

	printf("Finished. Press any key.");
	getchar();

	fclose(scramout);
	fclose(unscramout);
	close(hVolume);
	return 0;
}
