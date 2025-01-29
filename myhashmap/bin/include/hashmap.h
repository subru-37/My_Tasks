#ifndef HASHMAP_H
#define HASHMAP_H


#ifdef __cplusplus
extern "C" {
#endif

#define HASHMAP_SIZE 5
#define A 0.6180339887
#include <stddef.h>

typedef struct { 
    void* key;
    void* valuePtr;
} myHashMapNode;

typedef struct HashMap {
    int bucketSize;
    myHashMapNode** buckets;
    int size;
    void (*Put)(struct HashMap* map, void* key, void* valuePtr, size_t size);  // Function pointer for Put
    void* (*Get)(struct HashMap* map, void* key, size_t size);  // Function pointer for Get
    myHashMapNode* (*Remove)(struct HashMap* map, void* key, size_t size);
    void (*DestroyHashMap)(struct HashMap* map);
    struct HashMapIterator* (*CreateIterator)(struct HashMap* map);
    unsigned long (*hashPointer)(const void* ptr, size_t size);
    int (*handleCollision)(struct HashMap* map, void* key, int index);
} HashMap;

typedef struct HashMapIterator {
    HashMap* map;
    int index;
    myHashMapNode* currentNode;
    // void* (*CreateIterator)(HashMap* map);
    myHashMapNode* (*Next)(struct HashMapIterator* iterator);
    int (*HasNext)(struct HashMapIterator* iterator);
} HashMapIterator;


// Function prototypes
// int hashFunction(int key, int bucketSize);
unsigned long hashPointer(const void* ptr, size_t size);
int handleCollision(HashMap* map, void* key, int index);
void Put(HashMap* map, void* key, void* valuePtr, size_t size);
void* Get(HashMap* map, void* key, size_t size);
HashMapIterator* CreateIterator(HashMap* map);
myHashMapNode* Next(HashMapIterator* iterator);
int HasNext(HashMapIterator* iterator);
myHashMapNode* Remove(HashMap* map, void* key, size_t size);
HashMap* createHashMap(int bucketSize);
void DestroyHashMap(HashMap* map);

#ifdef __cplusplus
}
#endif
#endif // HASHMAP_H