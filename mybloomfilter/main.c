#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include "murmur3.h"
#include "bloomfilter.h"
#include <math.h>
#include "filter/block.h"
#include <assert.h>
#include "sbbf.h"
#include <time.h>

int main() {
    int n;
    double p;
    printf("Enter capacity of filter: ");
    scanf("%d", &n);
    getchar();
    printf("Enter False Positive rate in decimals: ");
    scanf("%lf", &p);
    getchar();

    printf("\nTesting Normal Bloom Filter\n");
    char* item1 = "myStng1";
    char* item2 = "myStng2";
    char* item3 = "myStng3";
    char* item4 = "mtrisqsng4";
    char* item5 = "testItem5";
    char* item6 = "example6";
    char* item7 = "randomString7";

    BloomFilter* myFilter = createBloomFilter(n, p);
    printf("Size of Bloom Filter: %d\n", myFilter->size);

    clock_t start = clock();

    myFilter->Put(myFilter, item1, sizeof(item1));
    myFilter->Put(myFilter, item2, sizeof(item2));
    myFilter->Put(myFilter, item3, sizeof(item3));
    myFilter->Put(myFilter, item4, sizeof(item4));
    myFilter->Put(myFilter, item5, sizeof(item5));

    int result1 = myFilter->Check(myFilter, item1, sizeof(item1));
    int result2 = myFilter->Check(myFilter, item2, sizeof(item2));
    int result3 = myFilter->Check(myFilter, item3, sizeof(item3));
    int result4 = myFilter->Check(myFilter, item4, sizeof(item4));
    int result5 = myFilter->Check(myFilter, item5, sizeof(item5));
    int result6 = myFilter->Check(myFilter, item6, sizeof(item6));
    int result7 = myFilter->Check(myFilter, item7, sizeof(item7));

    clock_t end = clock();

    printf("Item '%s' %s present\n", item1, result1 == 1 ? "is not" : "is");
    printf("Item '%s' %s present\n", item2, result2 == 1 ? "is not" : "is");
    printf("Item '%s' %s present\n", item3, result3 == 1 ? "is not" : "is");
    printf("Item '%s' %s present\n", item4, result4 == 1 ? "is not" : "is");
    printf("Item '%s' %s present\n", item5, result5 == 1 ? "is not" : "is");
    printf("Item '%s' %s present\n", item6, result6 == 1 ? "is not" : "is");
    printf("Item '%s' %s present\n", item7, result7 == 1 ? "is not" : "is");

    double cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;  // Convert to seconds
    printf("Time taken: %f seconds\n", cpu_time_used);

    free(myFilter->bit_array);
    free(myFilter);

    printf("\nTesting Split Block Bloom Filter\n");

    SplitBlockBloomFilter* sbbf = createSplitBlockBloomFilter(n, p);
    printf("Size of Split Block Bloom Filter: %d\n", sbbf->size);

    start = clock();

    sbbf->Insert(sbbf, item1, sizeof(item1));
    sbbf->Insert(sbbf, item2, sizeof(item2));
    sbbf->Insert(sbbf, item3, sizeof(item3));
    sbbf->Insert(sbbf, item4, sizeof(item4));
    sbbf->Insert(sbbf, item5, sizeof(item5));

    int r1 = sbbf->CheckKey(sbbf, item1, sizeof(item1));
    int r2 = sbbf->CheckKey(sbbf, item2, sizeof(item2));
    int r3 = sbbf->CheckKey(sbbf, item3, sizeof(item3));
    int r4 = sbbf->CheckKey(sbbf, item4, sizeof(item4));
    int r5 = sbbf->CheckKey(sbbf, item5, sizeof(item5));
    int r6 = sbbf->CheckKey(sbbf, item6, sizeof(item6));
    int r7 = sbbf->CheckKey(sbbf, item7, sizeof(item7));

    end = clock();

    printf("Item '%s' %s present\n", item1, r1 == 1 ? "is not" : "is");
    printf("Item '%s' %s present\n", item2, r2 == 1 ? "is not" : "is");
    printf("Item '%s' %s present\n", item3, r3 == 1 ? "is not" : "is");
    printf("Item '%s' %s present\n", item4, r4 == 1 ? "is not" : "is");
    printf("Item '%s' %s present\n", item5, r5 == 1 ? "is not" : "is");
    printf("Item '%s' %s present\n", item6, r6 == 1 ? "is not" : "is");
    printf("Item '%s' %s present\n", item7, r7 == 1 ? "is not" : "is");

    cpu_time_used = ((double)(end - start)) / CLOCKS_PER_SEC;  // Convert to seconds
    printf("Time taken: %f seconds\n", cpu_time_used);

    libfilter_block_destruct(sbbf->bit_array);
    free(sbbf->bit_array);
    free(sbbf);

    return 0;
}
