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

### June 6th 14:01

Extra edit right after that last commit. Oops, forgot to update FlushCache. Fixed now!

### June 7th 14:30

Today, not much progress was made. However, it was found out that the believed non-functional part of the code may actually be functional. The 0xD8 mode is apparently a Plextor proprietary mode which I was unaware of. Nevertheless, it is assumed to be functional. So today, was mostly cleaning up code, cleaning up comments, and also attempting to implement a Reed Solomon decoder. That didn't work out, so it's nowhere to be found here, but hopefully that will be the next major update. Other than that, nothing has been changed, and it is still functional.

More tests done:

- Used `iat` to convert unscramout.bin to an ISO file to verify maintenance of data integrity. Passed perfectly.


### June 8th 16:00

Today, everything went pretty smoothly. After reading the ECMA 130 standard, I feel as though I very much understand how the parity bytes are arranged. That being said, I can't really test it. I'm having linker errors consistent with C -> C++ issues went attempting to compile this code that uses libfec's rs decoder. Here's what's been done.

- Went on a commenting spree in main.cpp. Everything is explicitly explained now.
- Cleaned up the remaining Windows code we didn't need. Maybe there's still some hanging around, but it'll get removed too.
	- Comments have been left where significant parts were removed.
- Added rs_decoder.cpp
	- Name might change in the future.
	- Most of the code is untested, because the part that interfaces with libfec does not work.
	- Hopefully a few debugging sessions later this will be functional.

How to recreate the linker errors I've been getting from the new code:

- Install libfec and it's dev package
- Attempt to compile using g++: `g++ -lfec main.cpp descramble.cpp edcchk.cpp rs_decoder.cpp -o output`
- Receive errors:
```/usr/bin/ld: /tmp/ccYqa8lo.o: in function `rsDecode(unsigned char*)':
rs_decoder.cpp:(.text+0x2f3): undefined reference to `init_rs_int(int, int, int, int, int, int)'
/usr/bin/ld: rs_decoder.cpp:(.text+0x31f): undefined reference to `init_rs_int(int, int, int, int, int, int)'
/usr/bin/ld: rs_decoder.cpp:(.text+0x434): undefined reference to `decode_rs_int(void*, int*, int*, int)'
/usr/bin/ld: rs_decoder.cpp:(.text+0x454): undefined reference to `decode_rs_int(void*, int*, int*, int)'
collect2: error: ld returned 1 exit status```

This is a known isssue that I am now working on. It's very possible that uses a different library or a self-built one will solve all these issues. In fact, since we're dealing with shorts/words, and the closest thing that libfec will take is an int, it would actually be nice to find a more appropriate library or use it as direction to build my own.
