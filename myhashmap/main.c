#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <hashmap.h>

long unsigned int myHashPointer(const void* ptr, size_t size){
    printf("Hello World\n");
    int* myIndex = (int*)ptr;
    return *myIndex % 10;
    return 0;
}

void testHashMap() {
    int bucketSize = HASHMAP_SIZE;
    HashMap* map = createHashMap(10);

    printf("Testing Put method...\n");
    int value1 = 42, value2 = 84, value3 = 126, value4 = 909;
    int key1 = 25, key2 = 98, key3 = 100, key4 = 12;
    // map->hashPointer = myHashPointer;
    map->Put(map, &key1, &value1, sizeof(key1));
    map->Put(map, &key2, &value2, sizeof(key2));
    map->Put(map, &key3, &value3, sizeof(key3));
    map->Put(map, &key4, &value4, sizeof(key4));
    printf("Put completed.\n\n");

    printf("Testing Get method...\n");
    int* result1 = (int*)map->Get(map, &key1, sizeof(key1));
    int* result2 = (int*)map->Get(map, &key2, sizeof(key2));
    int* result3 = (int*)map->Get(map, &key3, sizeof(key3));
    int* result4 = (int*)map->Get(map, &key4, sizeof(key4));
    // printf("%d\n",result1);    
    printf("Key: %d, Value: %d\n", key1, result1 ? *result1 : -1);
    printf("Key: %d, Value: %d\n", key2, result2 ? *result2 : -1);
    printf("Key: %d, Value: %d\n", key3, result3 ? *result3 : -1);
    printf("Key: %d, Value: %d\n", key4, result4 ? *result4 : -1);
    printf("\n");

    printf("Testing Remove method...\n");
    myHashMapNode* removedNode = map->Remove(map, &key1, sizeof(key1));
    if (removedNode) {
        printf("Removed Key: %d, Value: %d\n", *(int*)removedNode->key, *(int*)removedNode->valuePtr);
        free(removedNode); // Free the removed node
    }
    printf("\n");

    printf("Verifying removed element...\n");
    int* removedResult = (int*)map->Get(map, &key1, sizeof(key1));
    printf("Key: %d, Value: %s\n", key1,removedResult ? "Found" : "Not Found");
    printf("\n");

    printf("Testing Iterator...\n");
    struct HashMapIterator* iterator = map->CreateIterator(map);
    myHashMapNode* node ;
        // printf("Iterator Key: %d, Value: %d\n", *(int*)node->key, *(int*)node->valuePtr);

    while (iterator->HasNext(iterator)) {
        node = iterator->Next(iterator);
        printf("Iterator Key: %d, Value: %d\n", *(int*)node->key, *(int*)node->valuePtr);
        // break;
    }
    free(iterator); // Free the iterator
    printf("Iterator Destroyed\n");

    printf("Destroying HashMap...\n");
    map->DestroyHashMap(map);
    printf("HashMap destroyed.\n");
}

int main() {
    printf("Starting HashMap tests...\n");
    testHashMap();
    printf("All tests completed.\n");
    return 0;
}