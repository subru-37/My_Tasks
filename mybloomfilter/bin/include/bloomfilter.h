#ifndef _BLOOMFILTER_H_
#define _BLOOMFILTER_H_


#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
#include <stdint.h>
#include <stddef.h>
typedef struct BloomFilter{
    unsigned char *bit_array;  // Bit array to store filter data
    int size;                  // Size of the bit array (m)
    int hash_count;             // Number of hash functions (k)
    void (*Put)(struct BloomFilter* bf, const void* str, size_t size);
    int (*Check)(struct BloomFilter* bf, const void* str, size_t size);
} BloomFilter;

BloomFilter* createBloomFilter(int n, double p);
void Put(BloomFilter* bf, const void* str, size_t size);
int Check(BloomFilter* bf, const void* str, size_t size);


//-----------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif

#endif // _BLOOMFILTER_H_