/* see smacker.h */
#include "smk_hufftree.h"

/* defines NULL... */
#include <stdlib.h>
/* memset */
#include <string.h>
/* error handling ("C Exceptions") */
#include <setjmp.h>

/* function to recursively delete a huffman tree */
void smk_del_huffman(struct smk_huff_t *t)
{
    if (t->b0 != NULL) smk_del_huffman(t->b0);
    if (t->b1 != NULL) smk_del_huffman(t->b1);
    free(t);
}

static struct smk_huff_t *smk_tree_rec(struct smk_bit_t *bs)
{
    struct smk_huff_t *ret = malloc(sizeof(struct smk_huff_t));

    if (smk_bs_1(bs))
    {
        ret->b0 = smk_tree_rec(bs);
        ret->b1 = smk_tree_rec(bs);
        ret->value = 0;
        ret->escapecode = 0xFF;
    } else {
        ret->b0 = NULL;
        ret->b1 = NULL;
        ret->value = smk_bs_8(bs);
        ret->escapecode = 0xFF;
    }
    return ret;
}

struct smk_huff_t *smk_build_tree(struct smk_bit_t* bs)
{
    struct smk_huff_t *ret = NULL;

    if (smk_bs_1(bs))
    {
        ret = smk_tree_rec(bs);
        if ( smk_bs_1(bs) ) printf("ERROR ERROR ERROR\n");
    }

    return ret;
}

unsigned short smk_tree_lookup (struct smk_bit_t *bs, struct smk_huff_t *t)
{
    if (t->b0 == NULL)
        return t->value;
    else if (smk_bs_1(bs))
        return smk_tree_lookup(bs,t->b1);
    else
        return smk_tree_lookup(bs,t->b0);
}

static struct smk_huff_t *smk_bigtree_rec(struct smk_huff_big_t *big, struct smk_huff_t *low8, struct smk_huff_t *hi8, struct smk_bit_t *bs)
{
    struct smk_huff_t *ret = malloc(sizeof(struct smk_huff_t));

    unsigned short val;

    if (smk_bs_1(bs))
    {
        ret->b0 = smk_bigtree_rec(big,low8,hi8,bs);
        ret->b1 = smk_bigtree_rec(big,low8,hi8,bs);
        ret->value = 0;
    } else {
        ret->b0 = NULL;
        ret->b1 = NULL;
        val = smk_tree_lookup(bs,low8) | (smk_tree_lookup(bs,hi8) << 8);
        if (val == big->s16[0])
        {
            ret->escapecode = 0;
            val = 0;
        } else if (val == big->s16[1]) {
            ret->escapecode = 1;
            val = 0;
        } else if (val == big->s16[2]) {
            ret->escapecode = 2;
            val = 0;
        } else {
            ret->escapecode = 0xFF;
        }

        ret->value = val;
    }
    return ret;
}
struct smk_huff_big_t *smk_build_bigtree(struct smk_bit_t* bs)
{
    struct smk_huff_big_t *big = NULL;

    struct smk_huff_t *low8 = NULL;
    struct smk_huff_t *hi8 = NULL;

    if (smk_bs_1(bs))
    {
        printf("Going to build a tree for low8\n");
        low8 = smk_tree_rec(bs);
        printf("Going to build a tree for hi8\n");
        hi8 = smk_tree_rec(bs);

        big = malloc(sizeof(struct smk_huff_big_t));

        big->s16[0] = smk_bs_8(bs) | (smk_bs_8(bs) << 8);
        big->s16[1] = smk_bs_8(bs) | (smk_bs_8(bs) << 8);
        big->s16[2] = smk_bs_8(bs) | (smk_bs_8(bs) << 8);
        printf("s16s are: %u %u %u\n",big->s16[0],big->s16[1],big->s16[2]);

/*        if (smk_bs_1(bs))
        {*/
            big->t = smk_bigtree_rec(big,low8,hi8,bs);
            // skip bits
/*        } else {
            printf("uh what\n");
        }*/
        if ( smk_bs_1(bs) ) printf("ERROR ERROR ERROR\n");

        smk_del_huffman(hi8);
        smk_del_huffman(low8);

    } else {
        printf("This movie has no BIGtree here\n");
    }

    return big;
}

static unsigned short smk_tree_big_lookup_rec (struct smk_bit_t *bs, struct smk_huff_big_t *big, struct smk_huff_t *t)
{
//    printf("checkang\n");

    if (t->b0 == NULL)
    {
//    printf("A leaf\n");
        if (t->escapecode == 0)
        {
//            printf("match escape code 0\n");
            return big->s16[0];
        }
        else if (t->escapecode == 1)
        {
//            printf("match escape code 1\n");
            return big->s16[1];
        }
        else if (t->escapecode == 2){

//            printf("match escape code 2\n");
            return big->s16[2];
        }
        else {
//    printf("A new leaf\n");

            big->s16[2] = big->s16[1];
            big->s16[1] = big->s16[0];
            big->s16[0] = t->value;
            return big->s16[0];
        }
    } else if (smk_bs_1(bs)) {
//            printf("leftbranch\n");

        return smk_tree_big_lookup_rec(bs,big,t->b1);
    } else {
//            printf("rightbranch\n");
        return smk_tree_big_lookup_rec(bs,big,t->b0);
    }
}

unsigned short smk_tree_big_lookup (struct smk_bit_t *bs, struct smk_huff_big_t *big)
{
//    printf("Calling a recursive function\n");
    if (big->t == NULL) { printf("This is likely to bomb.\n");}
    return smk_tree_big_lookup_rec(bs,big,big->t);
}

