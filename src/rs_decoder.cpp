#include "../include/rs.hpp"
#include <cstdint>
#include <cstddef>
#include <stdio.h>

int8_t ecmify(uint8_t* sector);


// Function getPMSBParityByte
// Generates a P parity byte from the buffer provided at the location
// defined by m and n.
// <- m - integer containing M_p
// <- n - integer containing N_p
// <- Buf - uint8_t pointer to the buffer
// -> return - a byte containing the parity byte
unsigned char getPMSBParityByte(int m, int n, uint8_t* Buf) {
	return Buf[2*(43*m+n)+13];
}

// Function getPLSBParityByte
// Generates a P parity byte from the buffer provided at the location
// defined by m and n.
// <- m - integer containing M_p
// <- n - integer containing N_p
// <- Buf - uint8_t pointer to the buffer
// -> return - a byte containing the parity byte
unsigned char getPLSBParityByte(int m, int n, uint8_t* Buf) {
	return Buf[2*(43*m+n)+12];
}

// Function getQMSBParityByte
// Generates a Q parity byte from the buffer provided at the location
// defined by m and n.
// <- m - integer containing M_q
// <- n - integer containing N_q
// <- Buf - uint8_t pointer to the buffer
// -> return - a byte containing the parity byte
unsigned char getQMSBParityByte(int m, int n, uint8_t* Buf) {
	return Buf[2*((44*m+43*n)%1118)+13];
}

// Function getQLSBParityByte
// Generates a Q parity byte from the buffer provided at the location
// defined by m and n.
// <- m - integer containing M_q
// <- n - integer containing N_q
// <- Buf - uint8_t pointer to the buffer
// -> return - a byte containing the parity byte
unsigned char getQLSBParityByte(int m, int n, uint8_t* Buf) {
	return Buf[2*((44*m+43*n)%1118)+12];
}

// Function putPMSBParityByte
// Puts the values from a repaired P parity back into the buffer.
// <- matrix[m][n] - an bytes matrix containing the bytes
// <- m - integer containing M_p
// <- n - integer containing N_p
// <- Buf - uint8_t pointer to the buffer.
void putPMSBParityByte(unsigned char matrix[43][26], int m, int n, uint8_t* Buf) {
	Buf[2*(43*m+n)+13] = matrix[m][n];
}

// Function putPLSBParityByte
// Puts the values from a repaired P parity back into the buffer.
// <- matrix[m][n] - an byte matrix containing the bytes
// <- m - integer containing M_p
// <- n - integer containing N_p
// <- Buf - uint8_t pointer to the buffer.
void putPLSBParityByte(unsigned char matrix[43][26], int m, int n, uint8_t* Buf) {
	Buf[2*(43*m+n)+12] = matrix[m][n] ;
}

// Function putQMSBParityByte
// Puts the values from a repaired Q parity back into the buffer.
// <- matrix[m][n] - a byte matrix containing the bytes
// <- m - integer containing M_q
// <- n - integer containing N_q
// <- Buf - uint8_t pointer to the buffer.
void putQMSBParityByte(unsigned char matrix[26][45], int m, int n, uint8_t* Buf) {
	// if(matrix[m][n]) {
		Buf[2*((44*m+43*n)%1118)+13] = matrix[m][n];
	// }
}

// Function putQLSBParityByte
// Puts the values from a repaired Q parity back into the buffer.
// <- matrix[m][n] - a byte matrix containing the bytes
// <- m - integer containing M_q
// <- n - integer containing N_q
// <- Buf - uint8_t pointer to the buffer.
void putQLSBParityByte(unsigned char matrix[26][45], int m, int n, uint8_t* Buf) {
	// if(matrix[m][n]) {
		Buf[2*((44*m+43*n)%1118)+12] = matrix[m][n];
	// }
}

// Function rsDecode
// <- Buf - uint8_t pointer to the buffer
void rsDecode(uint8_t* Buf) {
	// Set up our matrices
	printf("Making matrices.\n");
	unsigned char pMSBParityMatrix[43][26];
	unsigned char pLSBParityMatrix[43][26];
	unsigned char pMSBParityMatrixRepaired[43][26];
	unsigned char pLSBParityMatrixRepaired[43][26];
	unsigned char qMSBParityMatrix[26][45];
	unsigned char qLSBParityMatrix[26][45];
	unsigned char qMSBParityMatrixRepaired[26][45];
	unsigned char qLSBParityMatrixRepaired[26][45];
	memset(pMSBParityMatrix, 0, sizeof(pMSBParityMatrix));
	memset(pLSBParityMatrix, 0, sizeof(pLSBParityMatrix));
	memset(qMSBParityMatrix, 0, sizeof(qMSBParityMatrix));
	memset(qLSBParityMatrix, 0, sizeof(qLSBParityMatrix));
	memset(pMSBParityMatrixRepaired, 0, sizeof(pMSBParityMatrixRepaired));
	memset(pLSBParityMatrixRepaired, 0, sizeof(pLSBParityMatrixRepaired));
	memset(qMSBParityMatrixRepaired, 0, sizeof(qMSBParityMatrixRepaired));
	memset(qLSBParityMatrixRepaired, 0, sizeof(qLSBParityMatrixRepaired));
	
	// Iteration.
	int i = 0;
	while(i < 50) {
		// Make decoders.
		int pResult, qResult;
		printf("Making decoders.\n");
		RS::ReedSolomon<24, 2> p_rs_decoder;
		RS::ReedSolomon<43, 2> q_rs_decoder;
		
		// Fill up our matrices.
		printf("Filling matrices.\n");
		for(int n=0; n < 43; n++) {
			for(int m=0; m < 24; m++) {
				pMSBParityMatrix[n][m] = getPMSBParityByte(m, n, Buf);
			}
			for(int m=0; m < 24; m++) {
				pLSBParityMatrix[n][m] = getPLSBParityByte(m, n, Buf);
			}
			pMSBParityMatrix[n][24] = Buf[2*(43*24+n)+13];
			pMSBParityMatrix[n][25] = Buf[2*(43*25+n)+13];
			pLSBParityMatrix[n][24] = Buf[2*(43*24+n)+12];
			pLSBParityMatrix[n][25] = Buf[2*(43*25+n)+12];
		}
		
		for(int n=0; n < 26; n++) {
			for(int m=0; m < 43; m++) {
				qMSBParityMatrix[n][m] = getQMSBParityByte(m, n, Buf);
			}
			for(int m=0; m < 43; m++) {
				qLSBParityMatrix[n][m] = getQLSBParityByte(m, n, Buf);
			}
			qMSBParityMatrix[n][43] = Buf[2*(43*26+n)+13];
			qMSBParityMatrix[n][44] = Buf[2*(44*26+n)+13];
			qLSBParityMatrix[n][43] = Buf[2*(43*26+n)+12];
			qLSBParityMatrix[n][44] = Buf[2*(44*26+n)+12];
		}
		
		// Run the decoders on our matrices.
		printf("Decoding and fixing.\n");
		for(int j=0; j < 43; j++) {
			int pMSBResult = q_rs_decoder.Decode(pMSBParityMatrix[j], pMSBParityMatrixRepaired[j]);
			int pLSBResult = q_rs_decoder.Decode(pLSBParityMatrix[j], pLSBParityMatrixRepaired[j]);
			pResult = pMSBResult + pLSBResult;
		}
		for(int j=0; j < 26; j++) {
			int qMSBResult = q_rs_decoder.Decode(qMSBParityMatrix[j], qMSBParityMatrixRepaired[j]);
			int qLSBResult = q_rs_decoder.Decode(qLSBParityMatrix[j], qLSBParityMatrixRepaired[j]);
			qResult = qMSBResult + qLSBResult;
		}
		
		// If we fixed it, put it back into the buffer.
		printf("Pushing back to buffer if fixed.\n");
		for(int n=0; n < 43; n++) {
			for(int m=0; m < 24; m++) {
				if(!pResult) {
					putPMSBParityByte(pMSBParityMatrixRepaired, m, n, Buf);
					putPLSBParityByte(pLSBParityMatrixRepaired, m, n, Buf);
				}
			}
		}
		for(int n=0; n < 26; n++) {
			for(int m=0; m < 43; m++) {
				if(!qResult) {
					putQMSBParityByte(qMSBParityMatrixRepaired, m, n, Buf);
					putQLSBParityByte(qLSBParityMatrixRepaired, m, n, Buf);
				}
			}
		}
		
		// If it's fixed, stop trying to fix.
		if(!ecmify(Buf)) {
			break;
		}
		
		// If we fail at all methods of fixing, give up.
		if(pResult && qResult) {
			break;
		}
		i++;
	}
}
