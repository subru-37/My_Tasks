#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
// #include <murmur3.h>
#include "../murmur3/murmur3.h"
#include "bloomfilter.h"
#include <math.h>

BloomFilter* createBloomFilter(int n, double p){
    BloomFilter* bf = (BloomFilter*)malloc(sizeof(BloomFilter));

    bf->size = (int) ceil((-n * log(p)) / (log(2) * log(2)));
    bf->hash_count = (int) ceil((bf->size / (double)n) * log(2));
    bf->bit_array = (unsigned char *) calloc(bf->size, sizeof(unsigned char));
    bf->Put = Put;
    bf->Check = Check;
    return bf;
}

void Put(BloomFilter* bf, const void* str, size_t size){
    for(int i = 40; i<bf->hash_count + 40; i++){
        uint32_t hash;                /* Output for the hash */
        uint32_t seed = i*i;              /* Seed value for hash */
        MurmurHash3_x86_32(str, size, seed, &hash);
        bf->bit_array[(int)(hash % bf->size)] = 1;
        // printf("%d\n", (int)(hash % bf->size));
    }
}

int Check(BloomFilter* bf, const void* str, size_t size){
    int flag = 0;
    for(int i = 40; i<bf->hash_count + 40; i++){
        uint32_t hash;
        uint32_t seed = i*i;
        MurmurHash3_x86_32(str, size, seed, &hash);
        if(bf->bit_array[(int)(hash % bf->size)] == 0){
            flag = 1;
            break;
        }
    }
    return flag;
}

// int main(){
//     int n;
//     double p;
//     printf("Enter capacity of filter: ");
//     scanf("%d", &n);
//     printf("Enter False Positive rate in decimals: ");
//     scanf("%lf", &p);
//     getchar();
//     BloomFilter* myFilter = createBloomFilter(n,p);
//     printf("%d\n", myFilter->size);
//     char* item1 = "myStng1";
//     char* item2 = "myStng2";
//     char* item3 = "myStng3";
//     char* item4 = "mtrisqsng4";
//     myFilter->Put(myFilter,item1,sizeof(item1));
//     myFilter->Put(myFilter,item2,sizeof(item2));
//     myFilter->Put(myFilter,item3,sizeof(item3));
//     myFilter->Put(myFilter,item4,sizeof(item4));
//     int result1 = myFilter->Check(myFilter, item1, sizeof(item1));
//     int result2 = myFilter->Check(myFilter, item2, sizeof(item2));
//     int result3 = myFilter->Check(myFilter, item3, sizeof(item3));
//     int result4 = myFilter->Check(myFilter, item4, sizeof(item4));
//     printf("Item '%s' %s present\n", item1, result1 == 1 ? "is not\0" : "is\0");
//     printf("Item '%s' %s present\n", item2, result2 == 1 ? "is not\0" : "is\0");
//     printf("Item '%s' %s present\n", item3, result3 == 1 ? "is not\0" : "is\0");
//     printf("Item '%s' %s present\n", item4, result4 == 1 ? "is not\0" : "is\0");
//     free(myFilter);
//     return 0;
// }