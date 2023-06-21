# RCEU-2023
Host-based CD-ROM Data Recovery

## Acknowledgements

- Original source code for example - Natalia Portillo (as Truman) via "ReadCD"  
- Initial commit code - Dr. Jacob Hauenstein  
- Example used for porting to Linux - https://stackoverflow.com/questions/29884540/how-to-issue-a-read-cd-command-to-a-cd-rom-drive-in-windows  
- Descramble - Jonathan Gevaryahu via DiscImageCreator  
- EDC/ECC Checker - https://github.com/claunia/edccchk (License in source file)  
- Reed Solomon code library - https://github.com/mersinvald/Reed-Solomon (License in include folder)  

## Build

- Clone source code or download ZIP file and extract to a folder.

`git clone https://github.com/Danial-Ahari/RCEU-2023.git`

- Enter folder and run make.

`make readcd`

## Usage

`Usage: readcd <device file> <first sector number> <last sector number> <mode> correction> [scrambled output] [unscrambled output]`
  
		<device file> - path to the device file, ex. /dev/sr0  
		<first sector number> - the first sector to read from disc  
		<last sector number> - the last sector to read from disc  
		<mode> - 0 for 0xD8 mode, 1 for 0xBE mode  
		<correction> - See below
		scrambled output - file to output scrambled data to; if used, unscrambled output must be included as well  
		
		Usage of <correction>:
		0 - do not perform any correction
		1 - perform correction type 1 (Reed Solomon ECC with multiple re-read attempts)
		2 - perform re-read correction (Re-read an errored sector 50 times and check for correlation between reads)
		3 - full correction (RS ECC, and store re-reads to correlate on failure)`

## Changelog

Decided to move the changelog to it's own file to save space: [CHANGELOG](CHANGELOG.md)
