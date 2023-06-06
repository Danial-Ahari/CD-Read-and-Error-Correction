# RCEU-2023
Host-based CD-ROM Data Recovery

## Changelog

### Original Code

This code was built to read directly from a CD-ROM on Windows, using information gathered about the process from various sources. It was provided at the beginning of this project by Dr. Jacob Hauenstein.

### June 5th 12:00

This is a somewhat messy in-progress port to a Linux system. It is an edited version of the original code with mostly changed definitions, removal of error handling based on Windows' "Handles", and removal of the Visual Studio files that were present for reasons associated with the Windows-based version of this code.

### June 6th 14:00

Still a work in progress, but it's getting there. Changes made include:

- 0xBE mode now works!
	- Only for some disks. A random burn of some DOS software and BIOS updates worked, but production CDs especially with audio tracks embedded in them definitely do not.
- Removed commented out Windows code in many places; No longer necesarry as the Linux SG_IO ioctl() is working.
- Reformatted comments in some places to make them more consistent and look nicer.
- descramble.cpp and edcchk.cpp untouched. Both seem to be working on Linux.

Tests that were also performed:

- Used `cat` to put the contents of /dev/sr0 into a file (about 70 megs with this disk) and compared it to scramout and unscramout (about 80 megs with this disk). Without getting too precise, this about 14-15% increase in data is our Sync, Header, EDC, and ECC: 304 bytes for each 2048 bytes of data.

Still to be done:

- Not-0xBE mode does *not* work, and that needs to be fixed.
