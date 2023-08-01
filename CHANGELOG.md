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

### June 8th 17:00

Something I thought about after pushing today that could save me time in the future: A Makefile. This push is identical to the last, except the directory structure has been changed, and a Makefile has been implemented with `make readcd` and `make clean` so that compiling, updating (even when not every file has been changed), and cleaning can be done way easier. I don't know why I didn't do this sooner.

### June 12th 11:30

Needless to say, my Makefile was kind of bad, and didn't really work. I had a misunderstanding of how object files work (mostly because I've never compiled shared libraries), so that's been sorted now. I've also re-done the Reed Solomon error correction code, because I realized I did it wrong, and had a kind of naïve understanding of how it worked. I also incorporated a new Reed Solomon library, from https://github.com/mersinvald/Reed-Solomon because libfec just didn't want to work no matter what I did. I'm pushing this bfore noon because I wanted to get that Makefile updated and also get this new code up there, before I continue working. I'm currently working on testing to see if this code works for error correction, and will probably push again this afternoon.

### June 14th 13:30

This is the first build that can correct errors! The error correction code I wrote implementing that RS library from before is actually functional. Unfortunately, I wasn't able to get re-reading and recursively attempting to fix errors to work in this case, but that's on the to-do list. I was able to correct errors on a pressed disc and a few CD-Rs. I also decided to expand the README. In the future, I'm going to split this changelog off into it's own file.

### June 15th 16:00

Whew, big push today. This contains the same error correction code, but now it does it iteratively and can re-read the CD sector (got FlushCache working). It does it a bit inconsistently, as if it gets out of alignment with where the sector actually is, but it's definitely progress. With some cleaning up of the code, it should work better. I also decided to move the changelog to it's own file.

New stuff:

- Better user interface (see Usage)
- Iterative Error correction
- Working FlushCache and in turn, re-reading

New tests:

- Can successfully read even very damaged discs, by iteratively error correcting
	- Being fair, even that disc only took two times through, insinuating it was a glitch to begin with

Todo next:

- Use re-reads to look for the most accurate bytes we believe exist on the disc, by using correlation between different re-reads.
- Implement different choices of correction (RS, Re-read, Full) in the user interface

### June 20th 14:30

Today I implemented a way to check for consistency across multiple reads, and implemented the customizability in how we recover errors that I intend to from the command line. Other than that, no major changes. Tests are still showing accurate results, but the advantage of this system over CD drives is questionable. I have been looking into reading more exotic types of data, such as subchannel data, and will be working on that.

### June 21st 16:20

Changed almost nothing. Today was mostly research, brainstorming, etc. and testing. I did however change main.cpp to include a comment that lets me switch between getting subchannel data and not getting subchannel data. That's relevant to further things we might do with this. Also fixed README formatting issues.

### July 3rd 15:05

Minor changes (e.g. not bitshifting directly in the buffer, but instead making a copy of the data first; not moving the "repaired" data back unless we're sure it's repaired, etc.) while I tried to find out what's wrong with it. Turns out, we're having read issues. The next update will probably be when I've fixed that.

### July 10th 15:00

I have no idea where to start with this. Basically, I just incorporated a way to view sense data that's moderately human readable and also made a new calculation for how many sectors we should try to read toward the end of a given disc. I also did a lot of tests, and frankly, their results are concerningly unpredictable. I think the main cause of this might be copy protections used on the discs in question though. I'm still fiddling with it, but this is currently working, which most of the intermediate steps from last push and this weren't. So I'm pushing this now.

### July 12th 11:40

Mostly just more tweaking, and implementing a different way of handling what seem to be read errors.

### July 24th 13:20

Nothing particularly big was changed, but I'm just keeping up with putting code to the repository, because there are issues I can't find the solutions for, and I'm trying a lot of stuff and need to have a record of what things I'm changing.

### July 24th 14:00

Suddenly realized I was being stupid. Turns out the standard refers to things as words, but says you should process them as bytes. Now that I'm processing things as bytes with my RS code, I think it's working better than it was? Not 100% sure, but hey, it works now. The standalone corrector will be updated accordingly as well.

### July 25th 10:00

Added better licensing information and brought RS decoder up to date with the other project.

### August 1st 14:15

Changed README to show known issues and link to the other parts of the project.
