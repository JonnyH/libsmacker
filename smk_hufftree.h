#ifndef SMK_HUFFTREE_H
#define SMK_HUFFTREE_H

#include "smk_bitstream.h"

struct smk_huff_t
{
    struct smk_huff_t *b0;
    struct smk_huff_t *b1;
    unsigned short value;
};

struct smk_huff_t *smk_build_tree(struct smk_bit_t*);

unsigned char smk_tree_lookup (struct smk_bit_t *, struct smk_huff_t *);

/* function to recursively delete a huffman tree */
void smk_del_huffman(struct smk_huff_t *);

#endif
