#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "hashmap.h"
#include <stddef.h>

unsigned long secondaryHash(const void* key, size_t size) {
    const unsigned char* data = (const unsigned char*)key;
    unsigned long hash = 0;

    for (size_t i = 0; i < size; i++) {
        hash = (hash * 33) ^ data[i]; // XOR-based hash
    }

    return hash | 1; // Ensure the hash is odd to make it coprime with typical table sizes
}


unsigned long djb2Hash(const void* key, size_t size) {
    const unsigned char* data = (const unsigned char*)key;
    unsigned long hash = 5381;

    for (size_t i = 0; i < size; i++) {
        hash = ((hash << 5) + hash) + data[i];
    }
    return hash;
}

// Hash Wrapper for Pointers
unsigned long hashPointer(const void* ptr, size_t size) {
    return djb2Hash(ptr, size);
}


int handleCollision(HashMap* map, void* key, int size){
    int i = 0;
    int hash1 = map->hashPointer(key, size);
    int index = hash1 % map->bucketSize;
    while (map->buckets[index] != NULL && map->size <= map->bucketSize) {
        if (map->buckets[index]->key == key) {
            return index; 
        }
        i++;
        index = (hash1 + i * secondaryHash(key, size)) % map->bucketSize; 
        // size++;
    }
    //overflow
    if(map->size >= map->bucketSize){
        printf("Error: HashMap is full\n");
        return -1;
    }else{
        return index;
    }
}



// Function to create a new HashMap with a user-defined size
HashMap* createHashMap(int bucketSize) {
    HashMap* map = (HashMap*)malloc(sizeof(HashMap));
    if(!map){
        printf("Failed to allocate memory! for map\n");
        return NULL;
    }
    map->buckets = (myHashMapNode**)malloc(bucketSize * sizeof(myHashMapNode*));
    if(!(map->buckets)){
        printf("Failed to allocate memory! for map buckets\n");
        return NULL;
    }
    for (int i = 0; i < bucketSize; i++) {
        map->buckets[i] = NULL;  // Initialize all buckets to NULL
    }
    
    map->bucketSize = bucketSize;
    map->Put = Put;  // Assign Put function
    map->Get = Get;  // Assign Get function
    map->Remove = Remove;
    map->DestroyHashMap = DestroyHashMap;
    map->CreateIterator = CreateIterator;
    map->hashPointer = hashPointer;
    map->handleCollision = handleCollision;
    
    return map;
}

void DestroyHashMap(HashMap* map){
    for (int i = 0; i < map->bucketSize; i++) {
        if (map->buckets[i] != NULL) {
            free(map->buckets[i]);
        }
    }
    free(map->buckets);
    map->bucketSize = 0;
    free(map);
    return;
}   

void Put(HashMap* map, void* key, void* valuePtr, size_t size) { 
    unsigned long hash = map->hashPointer(key, size);  // Get initial index
    // printf("Hash: %d\n", index);
    int i = 0;

    myHashMapNode* newNode = (myHashMapNode*)malloc(sizeof(myHashMapNode));
    if(!newNode){
        printf("Failed to allocate memory! for newNode\n");
        return;
    }
    newNode->key = key;
    newNode->valuePtr = valuePtr;
    int index = map->handleCollision(map, key, size);
    // printf("Index: %d\n", index);
    if(index != -1){
        map->buckets[index] = newNode;
        // printf("Stored Index: %d\n",index);
        map->size++;
    }
}

void* Get(HashMap* map, void* key, size_t size){
    unsigned long hash = map->hashPointer(key, size);  // Get initial index
    int i = 0;
    int index = map->handleCollision(map, key, size);
    if(index != -1){
        if(map->buckets[index] != NULL){
            return map->buckets[index]->valuePtr;
        }else{
            return NULL;
        }
    }else{
        return NULL;
    }
}

HashMapIterator* CreateIterator(HashMap* map) {
    HashMapIterator* itr = (HashMapIterator*)malloc(sizeof(HashMapIterator));
    if (!itr) {
        printf("Failed to allocate memory for iterator.\n");
        return NULL;
    }

    itr->map = map;
    itr->index = 0;
    itr->currentNode = NULL;
    while (itr->index < map->bucketSize) {
        if (map->buckets[itr->index] != NULL) {
            itr->currentNode = map->buckets[itr->index];
            break;
        }
        itr->index++;
    }
    itr->HasNext = HasNext;
    itr->Next = Next;
    return itr;
}


myHashMapNode* Next(HashMapIterator* itr){
    // If we have a valid currentNode, return it and move to the next
    if (itr->currentNode != NULL) {
        myHashMapNode* node = itr->currentNode;
        itr->currentNode = NULL;  // Move to next node (in next call)
        // printf("Found!: %d\n", itr->index);
        return node;
    }

}


int HasNext(HashMapIterator* itr){
    while(itr->currentNode == NULL && itr->index < itr->map->bucketSize){
        itr->index++;
        if(itr->index < itr->map->bucketSize){
            itr->currentNode = itr->map->buckets[itr->index];
        }
        // printf("Index: %d\n",itr->index);
    }
    return (itr->currentNode != NULL);
}


myHashMapNode* Remove(HashMap* map, void* key, size_t size){
    myHashMapNode* removedKey = NULL;
    unsigned long hash = map->hashPointer(key, size);  // Get initial index
    int i = 0;
    int flag = 0;
    int index = map->handleCollision(map, key, size);
    // printf("Index: %d\n", index);
    if(index != -1){
        removedKey = map->buckets[index];
        map->buckets[index] = NULL;
        flag = 1;
    }
    if(flag == 0){
        // printf("Element not found");
        return NULL;
    }else{
        map->size--;
        return removedKey;
    }
}

