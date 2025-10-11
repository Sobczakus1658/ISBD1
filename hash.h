#ifndef HASH_H
#define HASH_H

#include <stdint.h>
#include <stddef.h>

#define  CRC_START_64_ECMA  0x0000000000000000ull
#define  CRC_START_64_WE    0xFFFFFFFFFFFFFFFFull

uint64_t ul_update_crc64( uint64_t crc, unsigned char c);
uint64_t ul_crc64_update( char *input_str, size_t num_bytes, uint64_t crc);

#endif /*HASH_H*/