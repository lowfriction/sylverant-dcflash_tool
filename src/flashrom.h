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

#ifndef FLASHROM_H
#define FLASHROM_H

#include <stdint.h>
#include <dc/flashrom.h>

#define FLASHROM_B1_PSOKEYS 0x0007

int erase_partition(int p);
int find_pso_keys(uint32_t *v1, uint32_t *v2);
int read_partition(int p, uint8_t **buf, int *len);
int remove_blocks(uint16_t bn[], int bnc, uint8_t *buf, int len, int *nr);
int remove_block(uint16_t b, uint8_t *buf, int len, int *nr);
int erase_flashrom(void);
int erase_pso_keys(void);

#endif /* !FLASHROM_H */
