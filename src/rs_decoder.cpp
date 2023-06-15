#include "../include/rs.hpp"
#include <cstdint>
#include <cstddef>
#include <stdio.h>

int8_t ecmify(uint8_t* sector);


// Function getPParityWord
// Generates a P parity word from the buffer provided at the location
// defined by m and n.
// <- m - integer containing M_p
// <- n - integer containing N_p
// <- Buf - uint8_t pointer to the buffer
// -> return - a short/word containing the parity word
unsigned short getPParityWord(int m, int n, uint8_t* Buf) {
	return (Buf[2*(43*m+n)+13] << 8) | Buf[2*(43*m+n)+12];
}

// Function getQParityWordinclude
// Generates a Q parity word from the buffer provided at the location
// defined by m and n.
// <- m - integer containing M_q
// <- n - integer containing N_q
// <- Buf - uint8_t pointer to the buffer
// -> return - a short/word containing the parity word
unsigned short getQParityWord(int m, int n, uint8_t* Buf) {
	return (Buf[2*((44*m+43*n)%1118)+13] << 8) | Buf[2*((44*m+43*n)%1118)+12];
}

// Function putPParityWord
// Puts the values from a repaired P parity back into the buffer.
// <- matrix[m][n] - an matrix array containing the words; I want this to be a short, but
// the library wants integers or characters, and integers work better.
// <- m - integer containing M_p
// <- n - integer containing N_p
// <- Buf - uint8_t pointer to the buffer.
void putPParityWord(int matrix[43][26], int m, int n, uint8_t* Buf) {
	Buf[2*(43*m+n)+12] = (matrix[m][n] >> 8) & 0xFF;
	Buf[2*(43*m+n)+13] = (matrix[m][n] >> 8) & 0xFF;
}

// Function putQParityWord
// Puts the values from a repaired Q parity back into the buffer.
// <- matrix[m][n] - an integer matrix containing the words; I want this to be a short, but
// the library wants integers or characters, and integers work better.
// <- m - integer containing M_q
// <- n - integer containing N_q
// <- Buf - uint8_t pointer to the buffer.
void putQParityWord(int matrix[26][45], int m, int n, uint8_t* Buf) {
	Buf[2*((44*m+43*n)%1118)+12] = (matrix[m][n] >> 8) & 0xFF;
	Buf[2*((44*m+43*n)%1118)+13] = (matrix[m][n] >> 8) & 0xFF;
}

// Function rsDecode
// <- Buf - uint8_t pointer to the buffer
// return - nothing at the moment
int rsDecode(uint8_t* Buf) {
	// Set up our matrices
	printf("Making matrices.\n");
	int pParityMatrix[43][26];
	int pParityMatrixRepaired[43][26];
	int qParityMatrix[26][45];
	int qParityMatrixRepaired[26][45];
	
	int pResult, qResult;
	int i = 0;
	while(i < 50) {
		printf("Making decoders.\n");
		RS::ReedSolomon<24, 2> p_rs_decoder;
		RS::ReedSolomon<43, 2> q_rs_decoder;
		printf("Filling matrices.\n");
		for(int n=0; n < 43; n++) {
			for(int m=0; m < 26; m++) {
				pParityMatrix[n][m] = getPParityWord(m, n, Buf);
			}
		}
		
		for(int n=0; n < 26; n++) {
			for(int m=0; m < 43; m++) {
				qParityMatrix[n][m] = getQParityWord(m, n, Buf);
			}
			qParityMatrix[n][43] = (Buf[2*(43*26+n)+13] << 8) | Buf[2*((43*26+n))+12];
			qParityMatrix[n][44] = (Buf[2*(44*26+n)+13] << 8) | Buf[2*((44*26+n))+12];
		}
		printf("Decoding and fixing.\n");
		for(int j=0; j < 43; j++) {
			pResult = p_rs_decoder.Decode(pParityMatrix[j], pParityMatrixRepaired[j]);
		}
		
		for(int j=0; j < 26; j++) {
			qResult = q_rs_decoder.Decode(qParityMatrix[j], qParityMatrixRepaired[j]);
		}

		printf("Pushing back to buffer.\n");
		for(int n=0; n < 43; n++) {
			for(int m=0; m < 24; m++) {
				putPParityWord(pParityMatrixRepaired, m, n, Buf);
			}
		}
		
		for(int n=0; n < 26; n++) {
			for(int m=0; m < 43; m++) {
				putQParityWord(qParityMatrixRepaired, m, n, Buf);
			}
		}
		if(!ecmify(Buf)) {
			break;
		}
		if(pResult && qResult) {
			break;
		}
		i++;
	}
	
	return 0; // Work out return value later.
}
