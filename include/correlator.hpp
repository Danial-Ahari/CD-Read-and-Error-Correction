// This header file is not associated with the others, and exists to define the class
// "Correlator"

#include <cstdint>
#include <cstddef>

// Class "Correlator"
// reReads - a matrix to hold all the re-reads of a sector.
// correctArray - an array to store what we think is the most consistent.
// correctConfidenceArray - probably has no external use, just for internal debugging; stores how confident we are in each byte.
// addRead() - adds a read to the matrix.
// correlate() - checks for consistency in the reads, and stores the most consistent values in correctArray.
class Correlator {
	private:
		uint8_t reReads[50][2352];
	
	public:
		uint8_t correctArray[2352];
		float correctConfidenceArray[2352];
		void addRead(uint8_t* sector, int pos);
		void correlate();
};
