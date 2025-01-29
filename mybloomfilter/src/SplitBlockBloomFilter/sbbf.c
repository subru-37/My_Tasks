#include "filter/block.h"
#include <assert.h>
#include "sbbf.h"
#include <stdlib.h>
#include "murmur3.h"
#include <stdio.h>


SplitBlockBloomFilter* createSplitBlockBloomFilter(long int ndv, double fpp){
    uint64_t bytes = libfilter_block_bytes_needed(ndv, fpp);
    // libfilter_block filter;
    libfilter_block* filter = (libfilter_block*)malloc(sizeof(libfilter_block));
    if(!libfilter_block_init(bytes, filter)){
        SplitBlockBloomFilter* bf = (SplitBlockBloomFilter*)malloc(sizeof(SplitBlockBloomFilter));
        if(bf == NULL){
            printf("Memory Not allocated!\n");
            return NULL;
        }
        bf->bit_array = filter;
        bf->size = bytes * 8;
        bf->Insert = Insert;
        bf->CheckKey = CheckKey;
        return bf;
    }else{
        printf("Memory Not allocated!\n");
        return NULL;
    }
}

void Insert(SplitBlockBloomFilter* bf, const void* str, size_t size){
    uint64_t hash = 0;
    uint32_t seed = 0xfeedba;
    MurmurHash3_x86_32(str, size, seed, &hash);
    libfilter_block_add_hash(hash, bf->bit_array);
}

int CheckKey(SplitBlockBloomFilter* bf, const void* str, size_t size){
    uint64_t hash = 0;
    uint32_t seed = 0xfeedba;
    MurmurHash3_x86_32(str, size, seed, &hash);
    if(libfilter_block_find_hash(hash, bf->bit_array)){
        // printf("Element has been found\n");
        return 0;
    }else {
        // printf("Element not found\n");
        return 1;
    }
}
