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
	int first = Buf[2*(43*m+n)+13];
	int second = Buf[2*(43*m+n)+12];
	return ((unsigned short)first << 8) | (unsigned short)second;
}

// Function getQParityWordinclude
// Generates a Q parity word from the buffer provided at the location
// defined by m and n.
// <- m - integer containing M_q
// <- n - integer containing N_q
// <- Buf - uint8_t pointer to the buffer
// -> return - a short/word containing the parity word
unsigned short getQParityWord(int m, int n, uint8_t* Buf) {
	int first = Buf[2*((44*m+43*n)%1118)+13];
	int second = Buf[2*((44*m+43*n)%1118)+12];
	return ((unsigned short)first << 8) | (unsigned short)second;
}

// Function putPParityWord
// Puts the values from a repaired P parity back into the buffer.
// <- matrix[m][n] - an short matrix containing the words
// <- m - integer containing M_p
// <- n - integer containing N_p
// <- Buf - uint8_t pointer to the buffer.
void putPParityWord(unsigned short matrix[43][26], int m, int n, uint8_t* Buf) {
	// if(matrix[m][n]) {
		Buf[2*(43*m+n)+12] = matrix[m][n] & 0x00FF;
		Buf[2*(43*m+n)+13] = (matrix[m][n] >> 8) & 0x00FF;
	// }
}

// Function putQParityWord
// Puts the values from a repaired Q parity back into the buffer.
// <- matrix[m][n] - a short matrix containing the words
// <- m - integer containing M_q
// <- n - integer containing N_q
// <- Buf - uint8_t pointer to the buffer.
void putQParityWord(unsigned short matrix[26][45], int m, int n, uint8_t* Buf) {
	// if(matrix[m][n]) {
		Buf[2*((44*m+43*n)%1118)+12] = matrix[m][n] & 0x00FF;
		Buf[2*((44*m+43*n)%1118)+13] = (matrix[m][n] >> 8) & 0x00FF;
	// }
}

// Function rsDecode
// <- Buf - uint8_t pointer to the buffer
// return - nothing at the moment
int rsDecode(uint8_t* Buf) {
	// Set up our matrices
	printf("Making matrices.\n");
	unsigned short pParityMatrix[43][26];
	unsigned short pParityMatrixRepaired[43][26];
	unsigned short qParityMatrix[26][45];
	unsigned short qParityMatrixRepaired[26][45];
	memset(pParityMatrix, 0, sizeof(pParityMatrix));
	memset(qParityMatrix, 0, sizeof(pParityMatrix));
	memset(pParityMatrixRepaired, 0, sizeof(pParityMatrixRepaired));
	memset(qParityMatrixRepaired, 0, sizeof(qParityMatrixRepaired));
	
	int i = 0;
	// while(i < 50) {
		int pResult, qResult;
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
		

		printf("Pushing back to buffer if fixed.\n");
		for(int n=0; n < 43; n++) {
			for(int m=0; m < 24; m++) {
				if(!pResult) {
					putPParityWord(pParityMatrixRepaired, m, n, Buf);
				}
			}
		}
		
		for(int n=0; n < 26; n++) {
			for(int m=0; m < 43; m++) {
				if(!qResult) {
					putQParityWord(qParityMatrixRepaired, m, n, Buf);
				}
			}
		}
		/*if(!ecmify(Buf)) {
			break;
		}
		if(pResult && qResult) {
			break;
		}*/
		i++;
	// }
	
	return 0; // Work out return value later.
}
