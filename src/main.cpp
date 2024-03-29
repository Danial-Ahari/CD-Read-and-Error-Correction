// CD Reader and Corrector
// Danial Ahari, mentored by Dr. Jacob Hauenstein
// The University of Alabama in Huntsville
// Summer 2023
//*************************************************************
// This project is the main component of a Summer 2023, RCEU
// conducted at the University of Alabama in Huntsville to
// recover data from CD-ROMs more effectively, efficiently,
// and usably, using host-based methods.
//
// The code within this document is originally based on Truman/
// Natalia Portillo's code for reading discs. See comment below.
//
// The code contained in edcchk.cpp is based on Natalia
// Portillo's code that can be found at 
// https://github.com/claunia/edccchk
// The licensing information is contained within that file.
//
// The code contained in descramble.cpp is by Jonathan
// Gevaryahu. It is taken here from Sarami's DiscImageCreator
// which can be found at
// https://github.com/saramibreak/DiscImageCreator
// 
// The code in header files related to rs decoding and the
// code used instructionally to generate rs_decoder.cpp was made
// by Mike Lubinets and can be found here:
// https://github.com/mersinvald/Reed-Solomon
//
// Finally, the example used for porting this code to Linux
// was found on StackOverflow at:
// https://stackoverflow.com/questions/29884540/how-to-issue-a-read-cd-command-to-a-cd-rom-drive-in-windows
//
// Many thanks are given to the people that contributed to
// projects used here. Without you, none of this would have
// been possible.

//
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
#include <string.h>
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
#include "../include/correlator.hpp"
#include <iostream>

// Lazy globals
long sector;
long endsector;
FILE* scramout;
FILE* unscramout;
const unsigned int numToRead = 26; // Value from DCDumper
std::map<int, unsigned int> reads;
bool seekback = false;
int correctionMode;
int lookThrough = 0;
// int sectorMode;
bool emergencyMode = 0;

// ECC/EDC stuff
int8_t ecmify(uint8_t* sector);
void eccedc_init(void);

// Descrambling stuff
extern unsigned char scrambled_table[2352];
void make_scrambled_table(void);

// Function prototypes
int OpenVolume(char* cdDeviceFile);
bool CloseVolume(int cdFileDesc);
bool ReadCD(int cdFileDesc);
void rsDecode(uint8_t* Buf);
bool FlushCache();
void emptyBuffer();
bool syncAssumed = false;

// Global variables
struct sg_io_hdr sgio; // Struct that will contain all the sgio data we need.
uint8_t DataBuf[2352 * numToRead]; // Buffer for holding sector data from drive
int cdFileDesc; // Global integer to store the cd drive's file descriptor

// Function OpenVolume
// Opens a volume and returns it as a file descriptor
// <- cdDeviceFile - char* that contains the device file path of the cd drive
// -> return cdFileDesc - int that contains the file descriptor of the cd drive
int OpenVolume(char* cdDeviceFile) {
	int cdFileDesc = open(cdDeviceFile, O_RDONLY | O_NONBLOCK);
	return cdFileDesc;
}

// Function CloseVolume
// Closes a volume given by a file desciptor, and returns 1 if successful.
// <- cdFileDesc - int that contains the file descriptor
// -> return - bool that is 1 if successful
bool CloseVolume(int cdFileDesc) {
	return close(cdFileDesc);
}

//From sarami's EccEdc (EccEdc.cpp)
inline uint8_t BcdToDec(uint8_t bySrc) {
	return (uint8_t)(((bySrc >> 4) & 0x0f) * 10 + (bySrc & 0x0f));
}

void outputDataBuf() {
	FILE* Buffer = fopen("databuf.bin", "wb");
	fwrite(&DataBuf, 1, 2352 * numToRead, Buffer);
	fclose(Buffer);
}

// Sets parameters of our buffer.
static long firstInBuf = -LONG_MAX;
static long lastInBuf = -LONG_MAX;
static long bufferStartOffset = 0;

// Function PassToBuffer
// Passes the sectors we designate into a buffer structure.
// <- first - long that contains the first sector to read
// <- last - long that contains the last sector to read
// -> return - a bool that uhh...just seems to return true. We'll see if we ever need this for error handling.
bool PassToBuffer(long first, long last) {
	int lastSector = 0;
	int uberLastSector = 0;
	// Check if there's a sync in the data we got and see if it's where we expected it to be.
	for (int offset = 0; offset < 2352 * (numToRead - 1); offset++) {		// Loop: for all offsets between 0 and 2351 * (numToRead - 1) inclusive
	
		// printf("Searching for data sector\n"); 					// Notify that we are looking for data.
		uint8_t* SectorData = &(DataBuf[offset]);				// Put the data from our buffer into an array to work with.
		int syncFound = 0;
		
		if (SectorData[0x000] == 0x00) { syncFound++; }
		if (SectorData[0x001] == 0xFF) { syncFound++; }
		if (SectorData[0x002] == 0xFF) { syncFound++; }
		if (SectorData[0x003] == 0xFF) { syncFound++; }
		if (SectorData[0x004] == 0xFF) { syncFound++; }
		if (SectorData[0x005] == 0xFF) { syncFound++; }
		if (SectorData[0x006] == 0xFF) { syncFound++; }
		if (SectorData[0x007] == 0xFF) { syncFound++; }
		if (SectorData[0x008] == 0xFF) { syncFound++; }
		if (SectorData[0x009] == 0xFF) { syncFound++; }
		if (SectorData[0x00A] == 0xFF) { syncFound++; }
		if (SectorData[0x00B] == 0x00) { syncFound++; }
		if (syncFound == 12) { syncAssumed = false; }
		
		if ((syncFound == 12) || (syncAssumed == true)) { // Conditional: Did we find at least part of a sync?
			
			// Now descramble the sector enough to check the LBA against what we expect
			uint8_t TempBufUnscrambled[16];					// Temporary buffer for unscrambled data.
			for (int i = 12; i < 16; i++) {					// Loop: to descramble
				TempBufUnscrambled[i] = SectorData[i] ^ scrambled_table[i];
			}
			
			// uint8_t's to store the minute, second, frame, and foundsector location on the disk.
			uint8_t minutes = BcdToDec(TempBufUnscrambled[12]);
			uint8_t seconds = BcdToDec(TempBufUnscrambled[13]);
			uint8_t frames = BcdToDec(TempBufUnscrambled[14]);
			long foundSector = ((minutes * 60) + seconds) * 75 + frames;
			
			// Here's some code to grab the mode (not form), commented out because I have no use for it yet. When used, also uncomment the lazy global that goes with it.
			/* switch(TempBufUnscrambled[15]) {
				case 0x00:
					sectorMode = 0;
					break;
				case 0x01:
					sectorMode = 1;
					break;
				case 0x02:
					sectorMode = 2;
					break;
				default:
					sectorMode = 1; // Assume we can correct it.
			} */
			
			
			
			foundSector -= 150; // Header sector numbering and CDB numbering differ by 150 -- adjust number from header
			
			printf("Found sector header for %ld -- looking for %ld\n", foundSector, first); // Report that we found the header for a sector.
			
			if ((foundSector == first) || (lastSector = first-1)) { // Conditional: If the sector we found is the first one.
				printf("Found sector %ld at offset %d\n", sector, offset); // Report that we found the sector we were looking for at which offset we found it.
				firstInBuf = first;
				lastInBuf = last;
				bufferStartOffset = offset;
				break;
			}
			if (foundSector < sector) { // Conditional: If the found sector is before the current sector
				 offset += 2352; // Jump ahead, but don't jump all the way to the place we expect the next header -- sometimes crazy things happen with changing offsets
				 lastSector = foundSector;
				 syncAssumed = true;
				continue;
			}
			if (foundSector > sector) {
				if(lookThrough >= 2048) {
					emergencyMode = 1;
					lookThrough = 0;
					return false;
				}
				lookThrough++;
				return false;
			}
		}
	}
	return true;
}

// Function GetFromBuffer
// Gets from the buffer and writes into our output files.
// <- sectorNo - long that contains the sector number we are getting from the buffer
// -> return - a bool that returns true if successful
bool GetFromBuffer(long sectorNo) { 
	if (sectorNo < firstInBuf || sectorNo > lastInBuf) return false;  // Returns false if the sector is not in the buffer.
	
	uint8_t* NewBuf = &(DataBuf[bufferStartOffset + ((sectorNo - firstInBuf) * 2352)]); // Creates a new buffer for outputting scrambeld data.
	
	// Formerly, there was a commented out piece of code here that just printed scrambled data. Removed.
	
	uint8_t NewBufUnscrambled[2352]; // Creates a new buffer for outputting unscrambeld data.

	for (int i = 0; i < 2352; i++) { // Loop: for every piece of data in scrambled_table
		NewBufUnscrambled[i] = NewBuf[i] ^ scrambled_table[i];
	}
	
	if (ecmify(NewBufUnscrambled)) { // Conditional: If ecmify() reports that the sector is bad.
		if (correctionMode) { // Are we actually correcting data?
			// Sector repair logic can go here. Fully implemented!
			Correlator* c = new Correlator; // Correlator object for corrections type 2 and 3.
			int k = 0; // Iteration.
			while(k < 2500) { // Run over this 2500 times, RS gets run once in mode 1, and every time in mode 3. Correlation gets used every 50 times in mode 2 and 3.
				if(correctionMode == 3) { printf("Error. Need to fix sector %ld. Tried %u times.\n", sector, reads[sector]); } // If we're doing full correction, this will tell the user how many times we've tried.
				if((k+1)%50==0 && (correctionMode == 2 || correctionMode == 3)) { c->addRead(NewBufUnscrambled, k/50); } // Adds a read to the correlator every 50 times.
				if(correctionMode == 1 || correctionMode == 3) { // If we're doing any RS...do RS checking.
					rsDecode(NewBufUnscrambled); 
					if(correctionMode == 1) { k=2499; }
				}
				if(!ecmify(NewBufUnscrambled)) { // If we've fixed it at this point, hoorah! Just return and move on.
					printf("Successfully fixed!\n");
					break;
				} else if(correctionMode == 2 || correctionMode == 3) { // Otherwise, if we're doing correlation, run FlushCache to do a re-read. (Every 50 times, the cache gets completely flushed, and we will store it as a read again.)
						printf("Re-reading the broken sector.\n");
						FlushCache();
						PassToBuffer(sector, sector + (numToRead - 4));
						for (int l = 0; l < 2352; l++) { // Loop: for every piece of data in scrambled_table
							NewBufUnscrambled[l] = NewBuf[l] ^ scrambled_table[l];
						}
				} 
				if(ecmify(NewBufUnscrambled) && k == 2499) { // If we've tried basically everything and nothing worked, give up.
					printf("Failed to fix.\n");
					break;
				}
				k++;
			}
			if(ecmify(NewBufUnscrambled) && (correctionMode == 2 || correctionMode == 3)) { // Make a good guess using the correlation data we got before, if we're in mode 2 or 3.
				printf("Comparing all reading, and picking the most consistent values.");
				c->correlate();
				for(int w = 0; w < 2352; w++) {
					NewBufUnscrambled[w] = c->correctArray[w];
				}
			}
			delete c; // Clear our memory.
		} else { // If we've been told not to do any correction at all.
			printf("Not performing correction.\n");
		}
	}
	
	// Writes unscrambled sector in unscrambled output file.
	fwrite(NewBufUnscrambled, 1, 2352, unscramout);
	printf("Wrote sector %ld unscrambled\n", sector);
	
	// Writes scrambled sector in unscrambled output file.
	fwrite(NewBuf, 1, 2352, scramout);
	printf("Wrote sector %ld scrambled\n", sector);
	return true;
	
	// This code appears to tell the user if we succeeded in repairing it or not. Not implemented yet.
	// printf("\n");
	// printf("Found sector %02X:%02X:%02X at offset of %d\n", NewBufUnscrambled[0x00C], NewBufUnscrambled[0x00D], NewBufUnscrambled[0x00E], offset);
	// printf("Result:\n %d\n", ecmify(NewBufUnscrambled));
}


// Function ReadCD
// This function reads a CD, by setting up an sgio structure, an sgio command, and sending it to the drive, then passing the data to the buffer.
// <- cdFileDesc - int containing the file descriptor
// <- mode - bool containing whether we want to use 0xBE mode or not
// -> return - bool that is 1 when successful
bool ReadCD(int cdFileDesc, bool mode = false) {
	int success; // Create an int to store the response from ioctl(), successful if greater than or equal to 0
	long sectorM2 = sector - 2; // Sector minus 2.
	int readLength;
	
	if (GetFromBuffer(sector)) return true; // If this sector is in the buffer, do not read.

	if (cdFileDesc != -1) { // Conditional: If we don't have a bad file descriptor.
	
		if(endsector-sector < numToRead) {
			readLength = endsector-sector;
		} else {
			readLength = numToRead;
		}
		if(readLength < 1) { readLength = 1; }
		if(emergencyMode) {
			readLength = 2;
			emptyBuffer();
			FlushCache();
			emergencyMode = 0;
		}
		unsigned char CMD[15]; // This array will store the command.
		unsigned char sense[128];
		
		sgio.interface_id = 'S'; // Linux identifies all SCSI devices (at the moment) as 'S'
		sgio.sbp = sense;
		memset(sense, 0, sizeof(sense));
   		sgio.mx_sb_len = sizeof(sense);
		sgio.dxfer_len = 2352 * readLength; // How many sectors we want to read.
		sgio.dxfer_direction = SG_DXFER_FROM_DEV; // Designates that we want to read data from the device.
		sgio.dxferp = (void*) &DataBuf; // Tells it that we want our data in DataBuf
		sgio.timeout = 60000; // SGIO timeout set to 60 seconds.

		if (!mode) { // Conditional: If we are not using 0xBE mode.
			// CDB with values for ReadCDDA proprietary Plextor command.
			CMD[0] = 0xD8;						// Code for ReadCDDA command
			CMD[1] = 0; 						// Alternate value of 8 was in original comment.
			CMD[2] = (uint8_t)(sectorM2 >> 24);			// Set Start Sector; Most significant byte of:
			CMD[3] = (uint8_t)((sectorM2 >> 16) & 0xFF);		// Set Start Sector; 3rd byte of:
			CMD[4] = (uint8_t)((sectorM2 >> 8) & 0xFF);		// Set Start Sector; 2nd byte of:
			CMD[5] = (uint8_t)(sectorM2 & 0xFF);			// Set Start Sector; Least sig byte of LBA sector no. to read from CD
			CMD[6] = 0;
			CMD[7] = 0;  						// Most significant byte of...
			CMD[8] = 0;  						// Middle byte of...
			CMD[9] = (uint8_t)readLength; 				// Least sig byte of no. of sectors to read from CD
			CMD[10] = 0;
			CMD[11] = 0;
			CMD[12] = 0;
			CMD[13] = 0;
			CMD[14] = 0;
			CMD[15] = 0;
		}
		else { // Any other condition.
			// CDB with values for ReadCD CDB12 command. The values were taken from MMC1 draft paper.
			CMD[0] = 0xBE;  					// Code for ReadCD CDB12 command
			CMD[1] = 0x4;
			CMD[2] = (uint8_t)(sectorM2 >> 24);			// Set Start Sector 
			CMD[3] = (uint8_t)((sectorM2 >> 16) & 0xFF);		// Set Start Sector
			CMD[4] = (uint8_t)((sectorM2 >> 8) & 0xFF);		// Set Start Sector
			CMD[5] = (uint8_t)(sectorM2 & 0xFF);			// Set Start Sector
			CMD[6] = 0; 						// Most significant byte of...
			CMD[7] = 0;						// Middle byte of...
			CMD[8] = (uint8_t)readLength;				// Least sig byte of no. of sectors to read from CD
			CMD[9] = 0xF8;
			CMD[10] = 0; // A value of 0b001 here should make it so we get subchannel data. 
			CMD[11] = 0;
			CMD[12] = 0;
			CMD[13] = 0;
			CMD[14] = 0;
			CMD[15] = 0;
		}
		
		sgio.cmdp = CMD;
		sgio.cmd_len = sizeof(CMD);
		
		success = ioctl(cdFileDesc, SG_IO, &sgio); // Send the command to the drive
		
		if(sense[0]) {
			int reset = ioctl(cdFileDesc, SG_SCSI_RESET, 3);
			for (int i = 0; i < 18; i++) {
				std::cout << std::hex << (int)sense[i] << '\n';
			}
			std::cout << "Hard resetting.\n";
			if (reset != 0) { std::cout << "Fail." << '\n'; }
			return 0;
		}
		
		if (success > -1) { // Conditional: If we succeed.
			if(sense[0]) {
				for (int i = 0; i < 18; i++) {
					std::cout << std::hex << (int)sense[i] << '\n';
				}
				std::cout << '\n';
			}
			reads[sector]++;
			if(PassToBuffer(sector, sector + (numToRead - 4))) {
				return GetFromBuffer(sector);
			}
		}
		
		else { // Any other condition.
			printf("ioctl() with SG_IO command failed.\n");
		}
		return 0; // Failure
	}
	else { // Any other condition
		return 0; // Failure
	}
}

// Function FlushCache
// This function flushes cache on a drive, by setting up an sgio structure, an sgio command, and sending it to the drive.
// -> return - bool that is 1 when successful
bool FlushCache()
{
	int readLength;
	int success; // Create an int to store the response from ioctl(), successful if greater than or equal to 0
	if (cdFileDesc != -1) { // Conditional: If we don't have a bad file descriptor.
	
		if(endsector-sector < numToRead) {
			readLength = endsector-sector;
		} else {
			readLength = numToRead;
		}
		if(readLength < 1) { readLength = 1; }
		unsigned char CMD[15];
		sgio.interface_id = 'S';
		sgio.sbp = NULL;
		sgio.mx_sb_len = 0;
		sgio.dxfer_len = 2352 * readLength;
		sgio.dxfer_direction = SG_DXFER_FROM_DEV;
		sgio.dxferp = (void*) &DataBuf; 
		sgio.timeout = 60000;
		if ((reads[sector] + 1) % 50 == 0) { // Conditional
			// CDB with values for ReadCD CDB12 command.  The values were taken from MMC1 draft paper.
			printf("Seeking back to zero for cache flush\n");
			CMD[0] = 0xBE;						// Code for ReadCD CDB12 command; Value of 0xBE was in original comment
			CMD[1] = 0;						// Alternate value of 0 was in original comment
			CMD[2] = 0;						// Most significant byte of:
			CMD[3] = 0;						// 3rd byte of:
			CMD[4] = 0;						// 2nd byte of:
			CMD[5] = 0;						// Least sig byte of LBA sector no. to read from CD
			CMD[6] = 0;						// Most significant byte of...
			CMD[7] = 0;						// Middle byte of...
			CMD[8] = 0;						// Least sig byte of no. of sectors to read from CD; Value of 1 was in original comment
			CMD[9] = 0;						// Raw read, 2352 bytes per sector; Value of 0xF8 was in original comment
			CMD[10] = 0;
			CMD[11] = 0;
			CMD[12] = 0;
			CMD[13] = 0;
			CMD[14] = 0;
			CMD[15] = 0;
		}
		else { // Any condition not addressed above.
			CMD[0] = 0xBE;  					// Code for ReadCD CDB12 command; Original comment claimed it was the ReadCDDA command.
			CMD[1] = 0x4; 						// FUA
			CMD[2] = (uint8_t)((sector-2) >> 24);			// Set Start Sector 
			CMD[3] = (uint8_t)(((sector-2) >> 16) & 0xFF);		// Set Start Sector
			CMD[4] = (uint8_t)(((sector-2) >> 8) & 0xFF);		// Set Start Sector
			CMD[5] = (uint8_t)((sector-2) & 0xFF);			// Set Start Sector
			CMD[6] = 0;
			CMD[7] = 0;						// Most significant byte of...
			CMD[8] = (uint8_t)readLength;						// Middle byte of...
			CMD[9] = 0xF8;						// Least sig byte of no. of sectors to read from CD
			CMD[10] = 0;
			CMD[11] = 0;
			CMD[12] = 0;
			CMD[13] = 0;
			CMD[14] = 0;
			CMD[15] = 0;
		}
		
		sgio.cmdp = CMD;
		sgio.cmd_len = sizeof(CMD);

		success = ioctl(cdFileDesc, SG_IO, &sgio); // Send the command to drive		
		if (success != -1) { // Conditional: If the cache was cleared.
			reads[sector]++;
		}
		else { // Any other condition.
			printf("ioctl() with SG_IO command failed.\n");
		}
		return 1;
	}
	else
	{
		return 0;
	}
}

void emptyBuffer() {
	for(int i = 0; i < 2352 * numToRead; i++) {
		DataBuf[i] = 0;
	}
}

// Function Usage
// Simple function that prints the usage of this application.
// -> return - nothing
void Usage() {
	printf("Usage: readcd <device file> <first sector number> <last sector number> <mode> <correction> [scrambled output] [unscrambled output]\n\n" 
		"<device file> - path to the device file, ex. /dev/sr0\n"
		"<first sector number> - the first sector to read from disc\n"
		"<last sector number> - the last sector to read from disc\n"
		"<mode> - 0 for 0xD8 mode, 1 for 0xBE mode\n"
		"<correction> - See below\n"
		"scrambled output - file to output scrambled data to; if used, unscrambled output must be included as well\n"
		"unscrambled output - file to output unscrambled data to; if used, scrambled output must be included as well\n"
		"\nUsage of <correction>:\n"
		"0 - do not perform any correction\n"
		"1 - perform correction type 1 (Reed Solomon ECC with multiple re-read attempts)\n"
		"2 - perform re-read correction (Re-read an errored sector 50 times and check for correlation between reads)\n"
		"3 - full correction (RS ECC, and store re-reads to correlate on failure of RS)\n");
	return;
}

// Function main() - entry point
// The entry point and driver of our program.
// <- argc - int containing the number of arguments
// <- argv[] - char* containing all the arguments
	// argv[1] - device file path expected
	// argv[2] - the first sector number to read
	// argv[3] - the last sector number to read
	// argv[4] - Enable 0xBE mode?
	// argv[5] - Which type of error correction?
		// 0 - no
		// 1 - type 1 (just RSPC)
		// 2 - type 2 (just re-reading; takes awhile)
		// 3 - full (both; takes a long while)
	// argv[6] - Optional; File to output scrambled data to.
	// argv[7] - Optional; File to output unscrambled data to.
// -> return - Always return 0 to say the program did it's thing.
int main(int argc, char* argv[]) {
	memset(&sgio, 0, sizeof(struct sg_io_hdr)); // Make sure sgio has been given enough space in memory, before we do anything else.
	
	if (argc < 6) { // Conditional: If we don't have at least 4 arguments.
		Usage();
		return 0;
	}

	make_scrambled_table(); // Make a table to store scrambled data.
	eccedc_init(); // Initialize ECCEDC

	sector = atoi(argv[2]); // Store which sector we start with.

	endsector = atoi(argv[3]); // Store which sector we'll end with.

	cdFileDesc = OpenVolume(argv[1]); // Get a file descriptor for our device file.

	printf("Reading from %ld to %ld\n", sector, endsector); // Notify that we're beginning to read.
	
	// Create files for our output.
	if (argc == 8) {
		scramout = fopen(argv[6], "wb");
		unscramout = fopen(argv[7], "wb");
	} else {
		scramout = fopen("scramout.bin", "wb");
		unscramout = fopen("unscramout.bin", "wb");
	}

	// Set correction boolean.
	if ((strcmp(argv[5], "3")) == 0) {
		correctionMode = 3;
	} else if ((strcmp(argv[5], "2")) == 0) {
		correctionMode = 2;
	} else if ((strcmp(argv[5], "1")) == 0) {
		correctionMode = 1;
	} else if ((strcmp(argv[5], "0")) == 0) {
		correctionMode = 0;
	} else {
		printf("No valid value given, setting correction mode to 0.");
		correctionMode = 0;
	}
	
	// Set mode boolean.
	bool mode;
	if ((strcmp(argv[4], "1")) == 0) {
		mode = true;
	} else if ((strcmp(argv[4], "0")) == 0) {
		mode = false;
	}

	while (sector < endsector) { // Loop: while there are still more sectors to read.
		if (!ReadCD(cdFileDesc, mode)) { // Conditional: If ReadCD() was not successful.
			// This appears to want to flush the cache. I need to look into how this works.
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

	printf("Finished. Press any key."); // Notify that we're done.
	getchar(); // Wait for user.

	// Clean up and close.
	fclose(scramout);
	fclose(unscramout);
	close(cdFileDesc);
	return 0;
}
