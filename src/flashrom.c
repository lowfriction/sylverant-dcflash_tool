/*
    This file is part of Sylverant Flashrom Tool
    Copyright (C) 2018 Lawrence Sebald

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 3 as
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <inttypes.h>

#include <dc/flashrom.h>

#include "flashrom.h"

/* From utils.c */
extern void fprint_buf(FILE *fp, const unsigned char *pkt, int len);

int erase_partition(int p) {
    int rv, offset, len;
    uint8_t hdr_block[64];

    /* Make sure it's a sensible partition to delete. */
    if(p < FLASHROM_PT_BLOCK_1 || p > FLASHROM_PT_BLOCK_2) {
        printf("Request to delete a bogus partition: %d\n", p);
        return -1;
    }

    /* Figure out where we'll be writing. */
    rv = flashrom_info(p, &offset, &len);
    if(rv) {
        printf("Error finding partition!\n");
        return -1;
    }

    printf("Partition %d: Offset: %d, length: %d\n", p, offset, len);

    /* Delete the entire partition... */
    rv = flashrom_delete(offset);
    printf("Flashrom delete of partition %d returned %d\n", p, rv);

    /* Set up a new header block. */
    memset(hdr_block, 0xFF, 64);
    memcpy(hdr_block, "KATANA_FLASH____", 16);
    hdr_block[16] = (uint8_t)p;
    hdr_block[17] = 0;

    /* Write it to the flashrom. */
    rv = flashrom_write(offset, hdr_block, 64);
    printf("Write flashrom returned %d\n", rv);
    return 0;
}

int rewrite_partition(int p, uint8_t *buf, int len, int ilen) {
    int rv, offset, len2;
    int bmlen;
    uint8_t *bitmap;

    /* Make sure it's a sensible partition to delete. */
    if(p < FLASHROM_PT_BLOCK_1 || p > FLASHROM_PT_BLOCK_2) {
        printf("Request to delete a bogus partition: %d\n", p);
        return -1;
    }

    /* Figure out where we'll be writing. */
    rv = flashrom_info(p, &offset, &len2);
    if(rv) {
        printf("Error finding partition!\n");
        return -1;
    }

    if(len != len2) {
        printf("Bogus partition length! Bailing out.\n");
        return -1;
    }
    else if(ilen > len) {
        printf("Bogus amount of blocks to rewrite!\n");
        return -1;
    }

    printf("Partition %d: Offset: %d, length: %d\n", p, offset, len);

    /* Delete the entire partition... */
    rv = flashrom_delete(offset);
    printf("Flashrom delete of partition %d returned %d\n", p, rv);

    /* Write the initial blocks that we've got. */
    rv = flashrom_write(offset, buf, ilen);
    printf("Write flashrom returned %d\n", rv);

    /* Write the bitmap. See the remove_blocks function for the logic here. */
    bmlen = ((((len >> 6) + 511) & ~511) >> 3);
    bitmap = buf + len - bmlen;

    printf("Writing bitmap at %d (%d)\n", len - bmlen, offset);
    rv = flashrom_write(offset + len - bmlen, bitmap, bmlen);
    printf("Write bitmap returned %d\n", rv);
    return 0;
}

int remove_blocks(uint16_t bn[], int bnc, uint8_t *buf, int len, int *nr) {
    uint8_t *bitmap, *bitmap2;
    uint8_t *b2;
    int nremoved = 0, i, j, k, removed = 0;

    /* The bitmap is stored at the end of the partition, and has to take up some
       number of blocks. Thus, we need to figure out how many blocks/bytes it
       takes up... The loopy math here is as follows:
       One bit per block => 512 blocks can be represented per bitmap block
       Each block is 64 bytes in length, thus we take the total partition
       length, find the number of blocks, figure out how many bitmap bits that
       would require (rounded up to an even number of blocks), and divide by
       8 to get the length in bytes. */
    bitmap = buf + len - ((((len >> 6) + 511) & ~511) >> 3);

    /* Sanity check. */
    if(memcmp(buf, "KATANA_FLASH____", 16)) {
        printf("Partition dump looks bad...\n");
        return -1;
    }

    /* This shouldn't really happen often, but just in case. */
    if(bitmap[0] == 0xff) {
        printf("Partition is empty, nothing to do.\n");
        return 0;
    }

    /* Allocate our replacement buffer. */
    if(!(b2 = malloc(len))) {
        printf("Can't allocate buffer memory\n");
        return -1;
    }

    /* Copy the header and clear the rest of the buffer for now. */
    memcpy(b2, buf, 64);
    memset(b2 + 64, 0xff, len - 64);
    bitmap2 = b2 + len - ((((len >> 6) + 511) & ~511) >> 3);

    /* This is not efficient, but it is simple and clear... */
    for(i = 0, j = 0; i < (len >> 6) - 2; ++i) {
        /* First, see if we've run out of blocks. */
        if(bitmap[i >> 3] & (0x80 >> (i & 7)))
            break;

        /* No, we have a block here, so let's see if it's one we care about. */
        removed = 0;

        for(k = 0; k < bnc && !removed; ++k) {
            if((uint16_t)(buf[(i + 1) << 6]) == bn[k]) {
                printf("Removing block %d (%d so far): blknum: %d\n", i,
                       nremoved + 1, bn[k]);
                ++nremoved;
                removed = 1;
                break;
            }
        }

        if(!removed) {
            /* Nope, don't care about it, copy it over blindly. */
            memcpy(b2 + ((j + 1) << 6), buf + ((i + 1) << 6), 64);
            bitmap2[j >> 3] &= ~(0x80 >> (j & 7));
            ++j;
        }
    }

    memcpy(buf, b2, len);
    free(b2);
    *nr = nremoved;
    return j + 1;
}

int remove_block(uint16_t b, uint8_t *buf, int len, int *nr) {
    uint16_t b2[] = { b };
    return remove_blocks(b2, 1, buf, len, nr);
}

int read_partition(int p, uint8_t **buf, int *len) {
    int rv, offset, l;
    uint8_t *b;

    *buf = NULL;
    *len = -1;

    rv = flashrom_info(p, &offset, &l);
    if(rv) {
        printf("Partition %d: Offset: %d, length: %d, rv: %d\n", p, offset, l, rv);
        return -1;
    }

    if(!(b = malloc(l))) {
        printf("Couldn't allocate buffer!\n");
        return -1;
    }

    rv = flashrom_read(offset, b, l);
    if(rv < 0) {
        printf("Read flashrom returns %d\n", rv);
        free(b);
        return -1;
    }

    *buf = b;
    *len = l;

    return rv;
}

int erase_pso_keys(void) {
    uint8_t *buf;
    int len, nblks, rv, nr;

    rv = read_partition(FLASHROM_PT_BLOCK_1, &buf, &len);
    if(rv < 0) {
        printf("Error reading partition: %d\n", rv);
        return -1;
    }

    rv = remove_block(FLASHROM_B1_PSOKEYS, buf, len, &nr);
    if(rv < 0) {
        printf("Error removing blocks\n");
        return -1;
    }
    else if(nr == 0) {
        return 0;
    }

    /* Figure out how much of the flashrom we have to write... */
    nblks = rv;
    printf("Need to write first %d blocks (and bitmap)\n", nblks);

    rv = rewrite_partition(FLASHROM_PT_BLOCK_1, buf, len, nblks << 6);
    if(rv < 0) {
        return -1;
    }

    free(buf);
    return nr;
}

int erase_flashrom(void) {
    /* Only bother with these two, as most likely whatever they're trying to
       delete is in one of them. */
    if(erase_partition(FLASHROM_PT_SETTINGS))
        return -1;
    if(erase_partition(FLASHROM_PT_BLOCK_1))
        return -1;

    return 0;
}

static char cod(uint8_t c) {
    if(isprint(c))
        return (char)c;
    return '.';
}

int find_pso_keys(uint32_t *v1, uint32_t *v2) {
    uint8_t blk[64];
    int rv;
    uint32_t tmp;

    /* PSO Keys are stored in block 7 in the first block allocated bank. */
    rv = flashrom_get_block(FLASHROM_PT_BLOCK_1, FLASHROM_B1_PSOKEYS, blk);
    if(rv) {
        printf("Error finding PSO keys (%d)\n", rv);
        return -1;
    }

    printf("Block Magic: %c%c%c%c\n", cod(blk[2]), cod(blk[3]), cod(blk[4]),
           cod(blk[5]));

    /* Check the block to see if it looks sane... */
    if(blk[4] != (uint8_t)'1' || blk[5] != (uint8_t)'S') {
        printf("Block looks incorrect, trying anyway...\n");
    }

    tmp = blk[14] | (blk[15] << 8) | (blk[16] << 16) | (blk[17] << 24);
    printf("PSOv1 Key: %08" PRIX32 "\n", tmp);
    *v1 = tmp;

    tmp = blk[26] | (blk[27] << 8) | (blk[28] << 16) | (blk[29] << 24);
    printf("PSOv2 Key: %08" PRIX32 "\n", tmp);
    *v2 = tmp;

    return 0;
}
