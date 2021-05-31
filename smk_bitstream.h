/**
	libsmacker - A C library for decoding .smk Smacker Video files
	Copyright (C) 2012-2021 Greg Kennedy

	See smacker.h for more information.

	smk_bitstream.h
		SMK bitstream structure. Presents a block of raw bytes one
		bit at a time, and protects against over-read.
*/

#ifndef SMK_BITSTREAM_H
#define SMK_BITSTREAM_H

/** Bitstream structure, Forward declaration */
struct smk_bit_t;

/* BITSTREAM Functions */
/** Initialize a bitstream */
struct smk_bit_t* smk_bs_init(const unsigned char* b, unsigned long size);

/** Free a bitstream */
void smk_bs_free(struct smk_bit_t * bs);

/** Read a single bit from the bitstream, and advance.
	Returns -1 on error. */
char smk_bs_read_1(struct smk_bit_t * bs);

/** Read eight bits from the bitstream (one byte), and advance.
	Returns -1 on error. */
short smk_bs_read_8(struct smk_bit_t * bs);

#endif
