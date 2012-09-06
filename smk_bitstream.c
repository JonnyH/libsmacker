/* see smacker.h */
#include "smk_bitstream.h"

/* defines NULL... */
#include <stdlib.h>
/* memset */
#include <string.h>

/* BITSTREAM Functions */
struct smk_bit_t *smk_bs_init(unsigned char *b, unsigned int size)
{
    /* allocate a bitstream struct */
    struct smk_bit_t *ret;
    ret = malloc(sizeof(struct smk_bit_t));

    /* set up the pointer to bitstream, and the size counter */
    ret->bitstream = b;
    ret->size = size;

    /* point to initial byte */
    ret->byte_num = -1;
    ret->bit_num = 7;

    return ret;
}

/* aligns struct to start of next byte */
void smk_bs_align(struct smk_bit_t *bs)
{
    bs->bit_num = 7;
}

unsigned char smk_bs_1(struct smk_bit_t *bs)
{
    /* don't die when running out of bits, but signal */
    if (bs->bit_num == 7 && bs->byte_num + 1 == bs->size) return 0xFF;

    /* advance to next bit */
    bs->bit_num ++;

    /* Out of bits in this byte: next! */
    if (bs->bit_num > 7)
    {
        bs->byte_num ++;
        bs->bit_num = 0;
    }

    return (((bs->bitstream[bs->byte_num]) & (0x01 << bs->bit_num)) != 0);
}

unsigned char smk_bs_8(struct smk_bit_t *bs)
{
    unsigned char ret = 0, i;
    for (i = 0; i < 8; i ++)
    {
        ret = ret >> 1;
        ret |= (smk_bs_1(bs) << 7);
    }
    return ret;
}

