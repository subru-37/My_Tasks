

#ifndef _SBBF_
#define _SBBF_


#include "filter/block.h"
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif


//-----------------------------------------------------------------------------

typedef struct SplitBlockBloomFilter {
    libfilter_block *bit_array;  // Bit array to store filter data
    unsigned int size;                  // Size of the bit array (m)
    long int hash_count;             // Number of hash functions (k)
    void (*Insert)(struct SplitBlockBloomFilter* bf, const void* str, size_t size);
    int (*CheckKey)(struct SplitBlockBloomFilter* bf, const void* str, size_t size);
} SplitBlockBloomFilter;

SplitBlockBloomFilter* createSplitBlockBloomFilter(long int n, double p);
void Insert(SplitBlockBloomFilter* bf, const void* str, size_t size);
int CheckKey(SplitBlockBloomFilter* bf, const void* str, size_t size);


//-----------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif

#endif // _SBBF_
