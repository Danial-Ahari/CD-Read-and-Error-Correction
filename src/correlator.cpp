#include "../include/correlator.hpp"
#include <cstdint>
#include <cstddef>

// Function addRead()
// <- sector - a pointer pointing to a place in memory containing the data for this read.
// <- pos - an integer telling us where to put it in the matrix.
void Correlator::addRead(uint8_t* sector, int pos) {
	for(int i = 0; i < 2352; i++) {
		this->reReads[pos][i] = sector[i];
	}
}

// Function correlate()
// This function reads our matrix, looking for the most consistent values, and putting them into correctArray.
void Correlator::correlate() {
	for(int i = 0; i < 2352; i++) {
		int tempConfidenceArray[50] = { 0 };
		for(int j = 0; j < 50; j++) {
			for(int k = 0; k < 50; k++) {
				if(reReads[j][i] == reReads[k][i]) {
					tempConfidenceArray[j]++;
				}
			}
		}
		for(int l = 0; l < 49; l++) {
			if(tempConfidenceArray[l] > 49) {
				this->correctArray[i] = reReads[l][i];
				this->correctConfidenceArray[i] = tempConfidenceArray[l];
				break;
			}
			if(tempConfidenceArray[l+1] > tempConfidenceArray[l]) {
				this->correctArray[i] = reReads[l+1][i];
				this->correctConfidenceArray[i] = tempConfidenceArray[l+1]/50;
			}
			else {
				this->correctArray[i] = reReads[l][i];
				this->correctConfidenceArray[i] = tempConfidenceArray[l]/50;
			}
		}
	}
}
