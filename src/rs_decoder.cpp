#include <fec.h>
#include <cstdint>
#include <cstddef>

// As of right now, this doesn't work in my attempts of compiling. It might be a mistake
// that I am making, as the errors I'm getting are consistent with C -> C++ issues.
// That being said, this should generate arrays of (unfortunately) integers that contain
// words each made up of two bytes as stated in the ECMA 130 standard from 1996.
// Whether they do or not, no clue. Once I can get libfec working correctly, hopefully
// it's RS function will be able to decode the RS and conduct repairs, which I think
// I've set it up correctly to do. If it does so, then the part that disassembles the
// words back into bytes and puts them back into the buffer should work fine.


// Function getPParityWord
// If this is working, it generates a P parity word from the buffer provided at the location
// defined by m and n.
// <- m - integer containing M_p
// <- n - integer containing N_p
// <- Buf - uint8_t pointer to the buffer
// -> return - a short/word containing the parity word
unsigned short getPParityWord(int m, int n, uint8_t* Buf) {
	return (Buf[2*(43*m+n)+13] << 8) | Buf[2*(43*m+n)+12];
}

// Function getQParityWord
// If this is working, it generates a Q parity word from the buffer provided at the location
// defined by m and n.
// <- m - integer containing M_q
// <- n - integer containing N_q
// <- Buf - uint8_t pointer to the buffer
// -> return - a short/word containing the parity word
unsigned short getQParityWord(int m, int n, uint8_t* Buf) {
	return (Buf[2*((44*m+32*n)%1118)+13] << 8) | Buf[2*((44*m+43*n)%1118)+12];
}

// Function putPParityWord
// If this is working, it puts the values from a repaired P parity back into the buffer.
// <- vector[1118] - an integer array containing the words; I want this to be a short, but
// the library wants integers or characters, and integers work better.
// <- m - integer containing M_p
// <- n - integer containing N_p
// <- Buf - uint8_t pointer to the buffer.
void putPParityWord(int vector[1118], int m, int n, uint8_t* Buf) {
	for(int i = 0; i < 1118; i++) {
		Buf[2*(43*m+n)+12] = (vector[i] >> 8) & 0xFF;
		Buf[2*(43*m+n)+13] = (vector[i] >> 8) & 0xFF;
	}
}

// Function putQParityWord
// If this is working, it puts the values from a repaired Q parity back into the buffer.
// <- vector[1118] - an integer array containing the words; I want this to be a short, but
// the library wants integers or characters, and integers work better.
// <- m - integer containing M_q
// <- n - integer containing N_q
// <- Buf - uint8_t pointer to the buffer.
void putQParityWord(int vector[1118], int m, int n, uint8_t* Buf) {
	for(int i = 0; i < 1118; i++) {
		Buf[2*((44*m+43*n)%1118)+12] = (vector[i] >> 8) & 0xFF;
		Buf[2*((44*m+32*n)%1118)+13] = (vector[i] >> 8) & 0xFF;
	}
}

// Function rsDecode
// <- Buf - uint8_t pointer to the buffer
// return - nothing at the moment
int rsDecode(uint8_t* Buf) {
	// Set up our decoders.
	void *p_rs_decoder, *q_rs_decoder;
	p_rs_decoder = init_rs_int(26, 0x11D, 0, 2, 24, 0);
	q_rs_decoder = init_rs_int(45, 0x11D, 0, 2, 43, 0);
	// Set up our vectors and a helpful iterator.
	int pParityVector[1118];
	int qParityVector[1118];
	int i = 0;
	
	for(int n=0; n < 26; n++) {
		for(int m=0; m < 43; m++) {
			pParityVector[i] = getPParityWord(m, n, Buf);
			i++;
		}
	}
	
	i = 0;
	for(int n=0; n < 26; n++) {
		for(int m=0; m < 43; m++) {
			qParityVector[i] = getQParityWord(m, n, Buf);
			i++;
		}
	}
	
	decode_rs_int(p_rs_decoder, pParityVector, NULL, 1);
	decode_rs_int(q_rs_decoder, qParityVector, NULL, 1);
	
	for(int n=0; n < 26; n++) {
		for(int m=0; m < 43; m++) {
			putPParityWord(pParityVector, m, n, Buf);
		}
	}
	
	for(int n=0; n < 26; n++) {
		for(int m=0; m < 43; m++) {
			putQParityWord(pParityVector, m, n, Buf);
		}
	}
	
	return 0; // Work out return value later.
}
