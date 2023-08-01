# CD Read and Error Correction
Part of a project on Host-based CD-ROM Data Recovery

Find the other parts of the project here:
[CD Visualizer](https://github.com/Danial-Ahari/CD-Visualizer)
[Standalone Corrector (takes BIN images)](https://github.com/Danial-Ahari/Standalone-BIN-Image-Corrector)

## Acknowledgements

- Original source code for example - Natalia Portillo (as Truman) via "ReadCD"  
- Initial commit code - Dr. Jacob Hauenstein  
- Example used for porting to Linux - https://stackoverflow.com/questions/29884540/how-to-issue-a-read-cd-command-to-a-cd-rom-drive-in-windows  
- Descramble - Jonathan Gevaryahu via DiscImageCreator  
- EDC/ECC Checker - https://github.com/claunia/edccchk (License in source file)  
- Reed Solomon code library - https://github.com/mersinvald/Reed-Solomon (License in include folder)  


## Licensing

- Natalia Portillo's EDC/ECC Checker's license is included in the file edcchk.cpp (under /src/)
	- The license requires a declaration of changes made. Out of the original project, only the one file was included. The main function entry point has been commented out. Only the business code is being utilized. No alterations to that code have been made.

- Mike Lubinets' RS decoder library's license is included in the RS_LICENSE file (under /include/)
	- The license requires the inclusion of itself in the project utilizing a copy of the original project. Furthermore it is used as follows: Only the header file portions are included in this project (gf.hpp, poly.hpp, and rs.hpp). The source file utilizing the library (/src/rs_decoder.cpp) was made using information from the example in the original project.
	
- Truman/Natalia Portillo's original statement on the use of source code from a forum is included in /src/main.cpp where it was used in an instructional way to produce the read code. The original definitions from the original work are not included as it has been implemented in a somewhat different way.

- Jonathan Gevaryahu's code in /src/descramble.cpp also includes a copyright notice.

- This program is released under the GPL v3 license as required and wished for by the copyright holders.

## Build

- Clone source code or download ZIP file and extract to a folder.

`git clone https://github.com/Danial-Ahari/CD-Read-and-Error-Correction.git`

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

## Known Issues

- Sometimes, a read will go thorugh properly but for whatever reason, the last sector (not dependent on what sector it is) will just read bad, as though it has an error in it. It's not clear why it does this or how it can be fixed, but it's an intermittent issue.

## Changelog

Decided to move the changelog to it's own file to save space: [CHANGELOG](CHANGELOG.md)
