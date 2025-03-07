#define _GNU_SOURCE
#include "postgres.h" // core psql headers
#include "fmgr.h" // definitions for postgresql function manager and function call interface
#include "executor/spi.h"
#include "utils/elog.h"
#include "utils/builtins.h" // text_to_cstring
#include "funcapi.h"
#include "ipc.h" // type definitions for shmem_startup_hook_type
#include "tcop/utility.h" // process utility hook types
#include "commands/defrem.h" // for defgetBoolea and similar functions which gives the values of the options
#include "stdlib.h"
#include <uuid/uuid.h>
#include <time.h>
#include "storage/proc.h" // data structures for each process's shared memory (latches and all)
#include "storage/lwlock.h"
#include "string.h"

#define QUERY_STRING_LENGTH 1024
#define QUEUE_LOCK_TRANCHE "MyQueueLock"
#define QUEUE_STRUCT "QueueStruct"
#define MAX_QUEUE_SIZE 100
#define MAX_LINE_LENGTH 4096  // Maximum length for a line in CSV
#define TOKEN_LENGTH 256

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

// sample struct where the data is stored after a select statement
/*
 * typedef struct SPITupleTable {
 *   // Public members 
 *   TupleDesc tupdesc; // tuple descriptor 
 *   HeapTuple *vals; // array of tuples
 *   uint64 numvals; // number of valid tuples
 * 
 *   // Private members, not intended for external callers 
 *   uint64 alloced; // allocated length of vals array
 *   MemoryContext tuptabcxt; // memory context of result table
 *   slist_node next; // link for internal bookkeeping
 *   SubTransactionId subid; // subxact in which tuptable was created
 * } SPITupleTable;
 */


// structures and enums for various purposes
typedef enum column_type{
    UNDEFINED,
    KEY1,
    KEY2,
    VALUE
}column_type;

typedef struct column_list {
    struct column_list* next; // pointer to the next element in linked list
    char column_name[NAMEDATALEN]; // char array with longest sequence possible for a column name in postgresql
    char data_type[NAMEDATALEN]; // datatype of the column
    column_type type; // column type 'key1/key2/value'
    char table_name[NAMEDATALEN]; // table name for reference
}column_list;

// typedef enum copy_type {
//     COPY_FROM,
//     COPY_TO
// }copy_type;

typedef struct kv_data{
    // char* key1, key2, value;
    char key1[TOKEN_LENGTH];
    char key2[TOKEN_LENGTH];
    char value[TOKEN_LENGTH];
    struct kv_data* next;
}kv_data;

// Define copy_properties structure
typedef struct {
    char table_name[NAMEDATALEN]; // table name for reference
    char csv_location[1024]; // location of csv file
    char delimiter[NAMEDATALEN]; // type of delimiter inside the csv file
    bool has_header; // whether the csv file includes header information
    int next; // index of next element in the queue
    char key1_column[NAMEDATALEN]; // 0 indexed column number indicating key1 from csv file 
    char key2_column[NAMEDATALEN]; // 0 indexed column number indicating key2 from csv file 
    char value_column[NAMEDATALEN]; // 0 indexed column number indicating value from csv file 
} copy_properties;

// queue structure
typedef struct {
    copy_properties queue[MAX_QUEUE_SIZE];
    int head, tail, free_index;  
    LWLock *lock;
} Queue;

// global variables and function pointers
static Queue *shared_queue = NULL; // global struct pointer to shared memory
static shmem_startup_hook_type prev_shmem_startup_hook = NULL; // hook for initializing shared memory
static ProcessUtility_hook_type prev_process_utility_hook = NULL;

void initQueue(Queue* q) {
    // q->front = q->rear = NULL;
    q->head = -1;
    q->tail = -1;
    q->free_index = 0;
}

// Check if the queue is empty
int isEmpty(Queue* q) {
    return q->head == -1;
}

// Enqueue operation
void enqueue(copy_properties* node) {    
    LWLockAcquire(shared_queue->lock, LW_EXCLUSIVE);
    if (shared_queue->free_index >= MAX_QUEUE_SIZE){
        elog(NOTICE, "Queue is full, waiting");
        return;
    }
    int new_node_index = shared_queue->free_index++;
    // Fill node data
    snprintf(shared_queue->queue[new_node_index].table_name, NAMEDATALEN, "%s", node->table_name);
    snprintf(shared_queue->queue[new_node_index].csv_location, 1024, "%s", node->csv_location);
    snprintf(shared_queue->queue[new_node_index].delimiter, NAMEDATALEN, "%s", node->delimiter);
    shared_queue->queue[new_node_index].has_header = node->has_header;
    shared_queue->queue[new_node_index].next = -1;
    // printf("Inserted: %d\n", value);

    // insertion of new queue
    if (shared_queue->tail != -1) {
        shared_queue->queue[shared_queue->tail].next = new_node_index;
    }
    shared_queue->tail = new_node_index;

    //if the queue is empty
    if (shared_queue->head == -1) {
        shared_queue->head = new_node_index;
    }
    LWLockRelease(shared_queue->lock);
}

// Dequeue operation
void dequeue() {
    LWLockAcquire(shared_queue->lock, LW_EXCLUSIVE);
    if (shared_queue->head == -1) {
        elog(LOG, "Queue is empty!\n");
        return;
    }
    int remove_index = shared_queue->head;
    shared_queue->head = shared_queue->queue[remove_index].next;

    if (shared_queue->head == -1) {
        shared_queue->tail = -1;  // Queue is empty now
    }
    LWLockRelease(shared_queue->lock);
}

/// @brief Function to insert in the begining of the linked list
/// @param name The name of colum 
/// @param data_type Data type of the column 
/// @param type Type of column which includes KEY1, KEY2, VALUE
/// @param table_name Name of table
/// @param HEAD pointer to the first element in the linked list
/// @return returns the updated linked list after insertion
column_list* add_column(char* name, char* data_type, column_type type, char* table_name){
    column_list* node = (column_list*)palloc(sizeof(column_list));
    strncpy(node->column_name, name, NAMEDATALEN);
    strncpy(node->data_type, data_type, NAMEDATALEN);
    strncpy(node->table_name, table_name, NAMEDATALEN);
    node->type = type;
    node->next = NULL;
    // node->next = HEAD;
    // HEAD = node;
    return node;
}

/// @brief Removes all column elements from the linked list
/// @param HEAD pointer to the first element of the linked list
/// @return NULL to update the local pointer to indicate its empty
column_list* remove_all_columns(column_list* HEAD ){
    column_list* temp = HEAD;
    column_list* next = temp;
    while(temp!=NULL){
        next = temp->next;
        pfree(temp);
        temp = next;
    }
    return NULL;
}

/// @brief helper function which checks whether the given columns are present in the linked list (and to check if there's repetition in the column)
/// @param table_name name of the table 
/// @param key_column_1 key1 column name from the format KEY1|KEY2 : VALUE
/// @param key_column_2 key2 column name from the format KEY1|KEY2 : VALUE
/// @param value_column_1 value of column name from the format KEY1|KEY2 : VALUE
/// @param HEAD pointer to the first element of the linked list of columns
/// @return true or false - whether it passed all the checks or not
bool search_columns(char* table_name, char* key_column_1, char* key_column_2, char* value_column_1, column_list* HEAD ){
    column_list* temp = HEAD;
    bool flag1 = false;
    bool flag2 = false;
    bool flag3 = false;
    while(temp!=NULL){
        if(strncmp(key_column_1, temp->column_name, NAMEDATALEN) == 0){
            flag1 = true;
            temp->type = KEY1;
        }
        if(strncmp(key_column_2, temp->column_name, NAMEDATALEN) == 0){
            flag2 = true;
            temp->type = KEY2;
        }
        if(strncmp(value_column_1, temp->column_name, NAMEDATALEN) == 0){
            flag3 = true;
            temp->type = VALUE;
        }
        temp = temp->next;
    }
    if(flag1 && flag2 && flag3){
        if(strncmp(key_column_1, key_column_2, NAMEDATALEN) !=0 && 
            strncmp(key_column_1, value_column_1, NAMEDATALEN) !=0 &&
            strncmp(key_column_2, value_column_1, NAMEDATALEN) !=0){
                return true;
        }else{
            elog(ERROR, "Columns are not unique");
            return false;
        }
    }else{
        if(!flag1){
            elog(LOG, "Input: %s", key_column_1);
            elog(ERROR, "%s not found in %s", key_column_1, table_name);
        }
        if(!flag2){
            elog(ERROR, "%s not found in %s", key_column_2, table_name);
        }
        if(!flag3){
            elog(ERROR, "%s not found in %s", value_column_1, table_name);
        }
        return false;
    }
}

/// @brief creation of linked list of the different columns of the table type `column_properties`  
/// @param table_name name of table from which the columns are extracted
/// @param key_column_1 key1 column name from the format KEY1|KEY2 : VALUE
/// @param key_column_2 key2 column name from the format KEY1|KEY2 : VALUE
/// @param value_column_1 value of column name from the format KEY1|KEY2 : VALUE
/// @param validity boolean pointer indicating whether all checks were passed by `search_columns()` 
/// @param len length of linked list
/// @param showerrors a false value would not stop the execution when a column repeats or whether it is not found
/// @return pointer to the first element of the linked list (HEAD)
column_list* valid_columns(char* table_name, char* key_column_1, char* key_column_2, char* value_column_1, bool *validity, int* len, bool showerrors){
    column_list* HEAD = NULL;
    column_list* TAIL = NULL;
    char current_query[QUERY_STRING_LENGTH];
    int result;
    snprintf(current_query, QUERY_STRING_LENGTH, "SELECT a.attname AS column_name, t.typname AS data_type \
        FROM pg_class c\
        JOIN pg_attribute a ON a.attrelid = c.oid\
        JOIN pg_type t ON a.atttypid = t.oid\
        WHERE c.relname = '%s' AND a.attnum > 0", table_name);
    result = SPI_execute(current_query, true, 0);
    if(result != SPI_OK_SELECT){
        elog(ERROR, "Unable to check current details of %s: %d", table_name, result);
        *validity = false;
    }
    if(SPI_processed <=0){
        elog(ERROR, "Table '%s' not found", table_name);
        *validity = false;
    }
    bool isnull = false;
    uint64 i = 0;
    while(i<SPI_processed){
        Datum column_name_bin = SPI_getbinval(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, 1, &isnull);
        Datum column_type_bin = SPI_getbinval(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, 2, &isnull);
        char *name = DatumGetCString(column_name_bin);
        char *data_type = DatumGetCString(column_type_bin);
        if(TAIL == NULL && HEAD == NULL){
            HEAD = add_column(name, data_type, UNDEFINED, table_name);
            TAIL = HEAD;
        }else{
            column_list* new_node = add_column(name, data_type, UNDEFINED, table_name);
            TAIL->next = new_node;
            TAIL = new_node;
        }
        elog(LOG, "column detected: %s", TAIL->column_name);
        i++;
    }
    *len = i;
    if(showerrors){
        if(!search_columns(table_name, key_column_1, key_column_2, value_column_1, HEAD) && showerrors){
            *validity = false;
        }else{
            *validity = true;
        }
    }
    return HEAD;
}

/// @brief SQL function definition which inserts metadata information to the `metadata` relation
/// @param  NIL handled by postgres
/// @return `NULL` if insertion fails, `metadata_result` type if insertion is successful - back to client
Datum metadata_handler(PG_FUNCTION_ARGS){
    // declaring and initializing required variables
    int connection_status;
    char current_query[QUERY_STRING_LENGTH];
    int result;
    if (PG_ARGISNULL(0) || PG_ARGISNULL(1) || PG_ARGISNULL(2) || PG_ARGISNULL(3)) {
        elog(ERROR, "NULL input is not allowed");
        PG_RETURN_NULL();
    }
    // column_metadata* input_data = (column_metadata*)palloc(sizeof(column_metadata));
    text *input_text0 = PG_GETARG_TEXT_P(0);
    char table_name[NAMEDATALEN],key_column_1[NAMEDATALEN], key_column_2[NAMEDATALEN], value_column_1[NAMEDATALEN];
    strncpy(table_name, text_to_cstring(input_text0), NAMEDATALEN);
    text *input_text1 = PG_GETARG_TEXT_P(1);
    strncpy(key_column_1, text_to_cstring(input_text1), NAMEDATALEN);
    text *input_text2 = PG_GETARG_TEXT_P(2);
    strncpy(key_column_2, text_to_cstring(input_text2), NAMEDATALEN);
    text *input_text3 = PG_GETARG_TEXT_P(3);
    strncpy(value_column_1, text_to_cstring(input_text3), NAMEDATALEN);

    TupleDesc tupdesc;
    HeapTuple tuple;
    Datum values[3];
    bool nulls[3] = {false, false, false};

    if ((connection_status = SPI_connect()) != SPI_OK_CONNECT) {
        elog(ERROR, "SPI_connect failed: error code %d", connection_status);
        PG_RETURN_NULL();
    }
    elog(LOG, "SPI Successfully Connected");
    bool validity = false;
    int len;
    column_list* HEAD = valid_columns(table_name, key_column_1, key_column_2, value_column_1, &validity, &len, true);
    if(!(validity)){
        PG_RETURN_NULL();
    }
    snprintf(current_query, QUERY_STRING_LENGTH, "SELECT * FROM metadata WHERE table_name = '%s'", table_name);
    result = SPI_execute(current_query, true, 0);
    if(result == SPI_OK_SELECT){
        int proc = SPI_processed;
        if(proc>0){
            column_list* temp = HEAD;
            bool error = false;
            while(temp!=NULL){
                if(strncmp(temp->column_name, key_column_1, NAMEDATALEN) == 0){
                    snprintf(current_query, QUERY_STRING_LENGTH, "UPDATE metadata SET column_name = '%s' WHERE key_or_value = 'KEY1'\
                        AND table_name = '%s'", key_column_1, table_name);
                    result = SPI_execute(current_query, false, 0);
                    if(SPI_processed<=0 || result!= SPI_OK_UPDATE){
                        elog(ERROR, "Error updating metadata info for KEY1 in table '%s'", table_name);
                        error = true;
                        break;
                    }
                    snprintf(current_query, QUERY_STRING_LENGTH, "UPDATE metadata SET data_type = '%s' WHERE key_or_value = 'KEY1'\
                        AND table_name = '%s'",temp->data_type, table_name);
                    result = SPI_execute(current_query, false, 0);
                    if(SPI_processed<=0 || result!= SPI_OK_UPDATE){
                        elog(ERROR, "Error updating metadata info for KEY1 in table '%s'", table_name);
                        error = true;
                        break;
                    }
                }
                if(strncmp(temp->column_name, key_column_2, NAMEDATALEN) == 0){
                    snprintf(current_query, QUERY_STRING_LENGTH, "UPDATE metadata SET column_name = '%s' WHERE key_or_value = 'KEY2'\
                        AND table_name = '%s'", key_column_2, table_name);
                    result = SPI_execute(current_query, false, 0);
                    if(SPI_processed<=0 || result!= SPI_OK_UPDATE){
                        elog(ERROR, "Error updating metadata info for KEY2 in table '%s'", table_name);
                        error = true;
                        break;
                    }
                    snprintf(current_query, QUERY_STRING_LENGTH, "UPDATE metadata SET data_type = '%s' WHERE key_or_value = 'KEY2'\
                        AND table_name = '%s'",temp->data_type, table_name);
                    result = SPI_execute(current_query, false, 0);
                    if(SPI_processed<=0 || result!= SPI_OK_UPDATE){
                        elog(ERROR, "Error updating metadata info for KEY2 in table '%s'", table_name);
                        error = true;
                        break;
                    }
                }
                if(strncmp(temp->column_name, value_column_1, NAMEDATALEN) == 0){
                    snprintf(current_query, QUERY_STRING_LENGTH, "UPDATE metadata SET column_name = '%s' WHERE key_or_value = 'VALUE'\
                        AND table_name = '%s'", value_column_1, table_name);
                    result = SPI_execute(current_query, false, 0);
                    if(SPI_processed<=0 || result!= SPI_OK_UPDATE){
                        elog(ERROR, "Error updating metadata info for VALUE in table '%s'", table_name);
                        error = true;
                        break;
                    }
                    snprintf(current_query, QUERY_STRING_LENGTH, "UPDATE metadata SET data_type = '%s' WHERE key_or_value = 'VALUE'\
                        AND table_name = '%s'",temp->data_type, table_name);
                    result = SPI_execute(current_query, false, 0);
                    if(SPI_processed<=0 || result!= SPI_OK_UPDATE){
                        elog(ERROR, "Error updating metadata info for VALUE in table '%s'", table_name);
                        error = true;
                        break;
                    }
                }
                temp = temp->next;
            }
            if(!error){
                /* Clean up SPI connection */
                if ((connection_status = SPI_finish()) == SPI_OK_FINISH) {
                    elog(LOG, "SPI Successfully disconnected");
                } else {
                    elog(ERROR, "Error disconnecting : error code %d", connection_status);
                }

                /* Return the concatenated text */
                int key_size = VARSIZE_ANY_EXHDR(input_text1) + VARSIZE_ANY_EXHDR(input_text2) + 2;
                text *key_combined = (text *) palloc0(VARHDRSZ + key_size);
                SET_VARSIZE(key_combined, VARHDRSZ + key_size);
                snprintf(VARDATA(key_combined), key_size, "%s|%s", text_to_cstring(input_text1), text_to_cstring(input_text2));

                if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
                    elog(ERROR, "Type function must be declared to return a composite type");

                /* Set values */
                values[0] = PointerGetDatum(input_text0);  // Table name
                values[1] = PointerGetDatum(key_combined);  // Key1|Key2
                values[2] = PointerGetDatum(input_text3);  // Value

                /* Build the tuple */
                tuple = heap_form_tuple(tupdesc, values, nulls);
                elog(NOTICE, "Metadata successfully updated");
                PG_RETURN_DATUM(HeapTupleGetDatum(tuple));

            }
        }else{
            bool isnull = false;

            // get current schema
            snprintf(current_query, QUERY_STRING_LENGTH, "SELECT table_schema FROM information_schema.tables WHERE table_name = '%s'", table_name);
            result = SPI_execute(current_query, true, 0);
            if(SPI_processed<=0 || (result != SPI_OK_SELECT)){
                elog(ERROR, "Can't access current schema");
                PG_RETURN_NULL();
            }
            Datum schema_bin = SPI_getbinval(SPI_tuptable->vals[0], SPI_tuptable->tupdesc,1,&isnull);
            char* schema = DatumGetCString(schema_bin);

            // get table oid
            snprintf(current_query, QUERY_STRING_LENGTH, "SELECT oid FROM pg_class WHERE relname = '%s'\
                AND relnamespace = '%s'::regnamespace", table_name, schema);
            result = SPI_execute(current_query, true, 0);
            if(SPI_processed<=0 || result!= SPI_OK_SELECT){
                elog(ERROR, "Can't get table OID");
                PG_RETURN_NULL();
            }
            Datum table_oid_bin = SPI_getbinval(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1, &isnull);
            int table_oid = DatumGetInt32(table_oid_bin);
            column_list* temp = HEAD;
            bool error = false;
            while(temp!=NULL){
                if(strncmp(temp->column_name, key_column_1, NAMEDATALEN) == 0){
                    snprintf(current_query, QUERY_STRING_LENGTH, "INSERT INTO metadata(table_oid, table_name, column_name,\
                        data_type, key_or_value) VALUES(%d, '%s', '%s', '%s', 'KEY1')", 
                        table_oid, table_name, key_column_1, temp->data_type);
                    result = SPI_execute(current_query, false, 0);
                    if(SPI_processed<=0 || result!= SPI_OK_INSERT){
                        elog(ERROR, "Error inserting metadata info for '%s' in table '%s'", key_column_1, table_name);
                        error = true;
                        break;
                    }
                }
                if(strncmp(temp->column_name, key_column_2, NAMEDATALEN) == 0){
                    snprintf(current_query, QUERY_STRING_LENGTH, "INSERT INTO metadata(table_oid, table_name, column_name,\
                        data_type, key_or_value) VALUES(%d, '%s', '%s', '%s', 'KEY2')", 
                        table_oid, table_name, key_column_2, temp->data_type);
                    result = SPI_execute(current_query, false, 0);
                    if(SPI_processed<=0 || result!= SPI_OK_INSERT){
                        elog(ERROR, "Error inserting metadata info for '%s' in table '%s'", key_column_2, table_name);
                        error = true;
                        break;
                    }
                }
                if(strncmp(temp->column_name, value_column_1, NAMEDATALEN) == 0){
                    snprintf(current_query, QUERY_STRING_LENGTH, "INSERT INTO metadata(table_oid, table_name, column_name,\
                        data_type, key_or_value) VALUES(%d, '%s', '%s', '%s', 'VALUE')", 
                        table_oid, table_name, value_column_1, temp->data_type);
                    result = SPI_execute(current_query, false, 0);
                    if(SPI_processed<=0 || result!= SPI_OK_INSERT){
                        elog(ERROR, "Error inserting metadata info for '%s' in table '%s'", value_column_1, table_name);
                        error = true;
                        break;
                    }
                }
                temp = temp->next;
            }
            if(!error){
                /* Clean up SPI connection */
                if ((connection_status = SPI_finish()) == SPI_OK_FINISH) {
                    elog(LOG, "SPI Successfully disconnected");
                } else {
                    elog(ERROR, "Error disconnecting : error code %d", connection_status);
                }
                int key_size = VARSIZE_ANY_EXHDR(input_text1) + VARSIZE_ANY_EXHDR(input_text2) + 2;
                text *key_combined = (text *) palloc0(VARHDRSZ + key_size);
                SET_VARSIZE(key_combined, VARHDRSZ + key_size);
                snprintf(VARDATA(key_combined), key_size, "%s|%s", text_to_cstring(input_text1), text_to_cstring(input_text2));

                if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
                    elog(ERROR, "Type function must be declared to return a composite type");

                /* Set values */
                values[0] = PointerGetDatum(input_text0);  // Table name
                values[1] = PointerGetDatum(key_combined);  // Key1|Key2
                values[2] = PointerGetDatum(input_text3);  // Value

                /* Build the tuple */
                tuple = heap_form_tuple(tupdesc, values, nulls);
                /* Return the concatenated text */
                elog(NOTICE, "Metadata successfully inserted");
                PG_RETURN_DATUM(HeapTupleGetDatum(tuple));
            }
        }
    }else{
        elog(ERROR, "Select query of metadata table error: %d", result);
    }    

    if((connection_status = SPI_finish()) == SPI_OK_FINISH){
        elog(LOG, "SPI Successfully disconnected");
    }else{
        elog(ERROR, "Error disconnecting : error code %d", connection_status);
    }
    PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(metadata_handler);

/// @brief generates a 32 bit uuid string
/// @param uuid pointer to the string to write uuid content
void generate_uuid(char *uuid) {
    uuid_t binuuid;
    uuid_generate_random(binuuid);

#ifdef capitaluuid
    uuid_unparse_upper(binuuid, uuid);
#elif lowercaseuuid
    uuid_unparse_lower(binuuid, uuid);
#else
    uuid_unparse(binuuid, uuid);
#endif
}

/// @brief Generation of `count` random numbers from 1 to `n`
/// @param n upper limit of random number generation
/// @param count number of random numbers
/// @param result result array containing n unique random numbers
void generate_unique_random_numbers(int n, int count, int result[]) {
    if (count > n) {
        elog(ERROR, "Error: Cannot generate more unique numbers than the range. Need atleast 3 columns.\n");
        return;
    }

    int used[n + 1]; // Array to track used numbers
    for (int i = 0; i <= n; i++) used[i] = 0; // Initialize all to -1

    int generated = 0;
    while (generated < count) {
        int num = (rand() % n); // Generate a number between 1 and n

        if (!used[num]) { // Ensure uniqueness
            result[generated++] = num;
            used[num] = 1;
        }
    }
}

// Definition of strip_newline
void strip_newline(char *token) {
    size_t len = strlen(token);
    if (len > 0 && token[len - 1] == '\n') {
        token[len - 1] = '\0';
    }
}


void read_write_csv(const char *read_filename, const char* write_filename, copy_properties* properties, column_list* HEAD){
    FILE *read_file = fopen(read_filename, "r");
    FILE *write_file = fopen(write_filename, "w");

    if (write_file == NULL) {
        elog(ERROR, "Error opening file, check permission for '%s'", write_filename);
        return;
    }
    if(read_file == NULL){
        elog(ERROR, "Error opening file, check permission for '%s'", read_filename);
        return;
    }

    char line[MAX_LINE_LENGTH]; // Buffer to store each line
    // int row = 0;
    int key1_column_no, key2_column_no, value_column_no;
    column_list* temp = HEAD;
    int i = 0;
    while(temp != NULL && i<3){
        if(strncmp(temp->column_name, properties->key1_column, NAMEDATALEN) == 0){
            key1_column_no = i;
        }
        else if(strncmp(temp->column_name, properties->key2_column, NAMEDATALEN) == 0){
            key2_column_no = i;
        }
        else if(strncmp(temp->column_name, properties->value_column, NAMEDATALEN) == 0){
            value_column_no = i;
        }
        temp = temp->next;
        i++;
    }
    
    while (fgets(line, MAX_LINE_LENGTH, read_file)) {
        char *token;
        char *rest = line;
        char key1[TOKEN_LENGTH] = {0};
        char key2[TOKEN_LENGTH] = {0};
        char value[TOKEN_LENGTH] = {0};
        // int row = 0;
        int i = 0;
        while ((token = strsep(&rest, properties->delimiter)) != NULL) {
            if (*token == '\0') continue;  // Skip empty tokens
            strip_newline(token);
            if(i == key1_column_no){
                strncpy(key1, token, TOKEN_LENGTH);
            }else if(i == key2_column_no){
                strncpy(key2, token, TOKEN_LENGTH);
            }else if(i == value_column_no){
                strncpy(value, token, TOKEN_LENGTH);
            }
            i++;
        }
        fprintf(write_file, "%s,%s,%s\n", key1, key2, value);
    }
    fclose(write_file);
    fclose(read_file);
}

/// @brief Write the key/value data to a csv file according to metadata information in the relation `metadata`
/// @param properties pointer to struct containing all the parsed data regarding the copy query for  key/value insertion
/// @return true/false for a successful write operation
bool write_to_file(copy_properties* properties){
    int connection_status;
    int result;
    char current_query[QUERY_STRING_LENGTH];
    if ((connection_status = SPI_connect()) != SPI_OK_CONNECT) {
        elog(ERROR, "SPI_connect failed: error code %d", connection_status);
        return false;
    }
    // collecting table data, validity is not important here
    char* temp1 = "temp1";
    char* temp2 = "temp2";
    char* temp3 = "temp3";
    bool valid;
    int len;
    column_list* HEAD = valid_columns(properties->table_name, temp1, temp2, temp3, &valid, &len, false);
    snprintf(current_query, QUERY_STRING_LENGTH, "SELECT * FROM metadata WHERE table_name = '%s' ORDER BY key_or_value", properties->table_name);
    
    result = SPI_execute(current_query, false, 0);
    // column_list* selected_head = NULL;
    char key_column_1[NAMEDATALEN], key_column_2[NAMEDATALEN], value_column_1[NAMEDATALEN];
    
    if(result != SPI_OK_SELECT || SPI_processed<=0){
        // metadata not found or errored case
        elog(WARNING_CLIENT_ONLY, "Metadata not found for table: %s", properties->table_name);
        elog(WARNING_CLIENT_ONLY, "--Taking key value pairs from random 3 columns for rocksdb--");

        int random_columns[3];
        generate_unique_random_numbers(len, 3, random_columns);
        generate_unique_random_numbers(len, 3, random_columns);
        generate_unique_random_numbers(len, 3, random_columns);
        
        //collecting schema information for oid
        bool isnull;
        snprintf(current_query, QUERY_STRING_LENGTH, "SELECT table_schema FROM information_schema.tables WHERE table_name = '%s'", properties->table_name);
        result = SPI_execute(current_query, true, 0);
        if(SPI_processed<=0 || (result != SPI_OK_SELECT)){
            elog(ERROR, "Can't access current schema");
            // PG_RETURN_NULL();
            return false;
        }
        Datum schema_bin = SPI_getbinval(SPI_tuptable->vals[0], SPI_tuptable->tupdesc,1,&isnull);
        char* schema = DatumGetCString(schema_bin);

        //collecting oid from schema information
        snprintf(current_query, QUERY_STRING_LENGTH, "SELECT oid FROM pg_class WHERE relname = '%s'\
            AND relnamespace = '%s'::regnamespace", properties->table_name, schema);
        result = SPI_execute(current_query, true, 0);
        if(SPI_processed<=0 || result!= SPI_OK_SELECT){
            elog(ERROR, "Can't get table OID");
            return false;
        }
        Datum table_oid_bin = SPI_getbinval(SPI_tuptable->vals[0], SPI_tuptable->tupdesc, 1, &isnull);
        int table_oid = DatumGetInt32(table_oid_bin);
        
        // start iteration to save metadata information
        column_list* temp = HEAD;
        bool error = false;
        int i = 0;
        while(temp!=NULL){
            if(i == random_columns[0]){
                strncpy(key_column_1, temp->column_name, NAMEDATALEN);
                temp->type = KEY1;
                snprintf(current_query, QUERY_STRING_LENGTH, "INSERT INTO metadata(table_oid, table_name, column_name,\
                    data_type, key_or_value) VALUES(%d, '%s', '%s', '%s', 'KEY1')", 
                    table_oid, properties->table_name, temp->column_name, temp->data_type);
                result = SPI_execute(current_query, false, 0);
                if(SPI_processed<=0 || result!= SPI_OK_INSERT){
                    elog(ERROR, "Error inserting metadata info for '%s' in table '%s'", temp->column_name, properties->table_name);
                    error = true;
                    break;
                }
                strncpy(properties->key1_column, temp->column_name, NAMEDATALEN);
            }
            else if(i == random_columns[1]){
                strncpy(key_column_2, temp->column_name, NAMEDATALEN);
                temp->type = KEY2;
                snprintf(current_query, QUERY_STRING_LENGTH, "INSERT INTO metadata(table_oid, table_name, column_name,\
                    data_type, key_or_value) VALUES(%d, '%s', '%s', '%s', 'KEY2')", 
                    table_oid, properties->table_name, temp->column_name, temp->data_type);
                result = SPI_execute(current_query, false, 0);
                if(SPI_processed<=0 || result!= SPI_OK_INSERT){
                    elog(ERROR, "Error inserting metadata info for '%s' in table '%s'", temp->column_name, properties->table_name);
                    error = true;
                    break;
                }
                strncpy(properties->key2_column, temp->column_name, NAMEDATALEN);
            }
            else if(i == random_columns[2]){
                strncpy(value_column_1, temp->column_name, NAMEDATALEN);
                temp->type = VALUE;
                snprintf(current_query, QUERY_STRING_LENGTH, "INSERT INTO metadata(table_oid, table_name, column_name,\
                    data_type, key_or_value) VALUES(%d, '%s', '%s', '%s', 'VALUE')", 
                    table_oid, properties->table_name, temp->column_name, temp->data_type);
                result = SPI_execute(current_query, false, 0);
                if(SPI_processed<=0 || result!= SPI_OK_INSERT){
                    elog(ERROR, "Error inserting metadata info for '%s' in table '%s'", temp->column_name, properties->table_name);
                    error = true;
                    break;
                }
                strncpy(properties->value_column, temp->column_name, NAMEDATALEN);
            }
            i++;
            temp = temp->next;
        }

        if(error){
            elog(NOTICE, "Try again!!");
        }        
        elog(NOTICE, "Choosen columns: %s|%s for KEY and %s for VALUE", properties->key1_column, properties->key2_column, properties->value_column);

    }else{
        uint64 i = 0; 
        while(i<SPI_processed){
            char *table_name = SPI_getvalue(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, 3);
            char *column_name = SPI_getvalue(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, 4);
            char *data_type = SPI_getvalue(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, 5);
            char *type = SPI_getvalue(SPI_tuptable->vals[i], SPI_tuptable->tupdesc, 6);

            if (!table_name || !column_name || !data_type || !type) {
                elog(LOG, "NULL value encountered, skipping row %lu", i);
                continue;
            }

            if(strcmp(type, "KEY1") == 0){
                strncpy(key_column_1, column_name, NAMEDATALEN);
                strncpy(properties->key1_column, column_name, NAMEDATALEN);
            }else if(strcmp(type, "KEY2") == 0){
                strncpy(key_column_2, column_name, NAMEDATALEN);
                strncpy(properties->key2_column, column_name, NAMEDATALEN);
            }else if(strcmp(type, "VALUE") == 0){
                strncpy(value_column_1, column_name, NAMEDATALEN);
                strncpy(properties->value_column, column_name, NAMEDATALEN);
            }
            i++;
        }
        elog(NOTICE, "Choosen columns: %s|%s for KEY and %s for VALUE", properties->key1_column, properties->key2_column, properties->value_column);
    }
    // FILE *file;
    // char buffer[4096]; // one line buffer
    char uuid[UUID_STR_LEN];
    generate_uuid(uuid);  // Generate a new UUID
    char filename[UUID_STR_LEN + 9];  // ".csv" needs 4 extra bytes
    snprintf(filename, sizeof(filename), "/tmp/%s.csv", uuid);

    elog(NOTICE, "Generated file: %s\n", filename);
    read_write_csv(properties->csv_location, filename, properties, HEAD);
    elog(NOTICE, "Successfuly written key/value pair info");
    if((connection_status = SPI_finish()) == SPI_OK_FINISH){
        elog(LOG, "SPI Successfully disconnected");
    }else{
        elog(ERROR, "Error disconnecting : error code %d", connection_status);
    }
    // if (file == NULL) {
    //     elog(ERROR, "Error opening file for writing");
    //     return false;
    // }
    return true;
}

/// @brief contains the definition for `ProcessUtility_hook` which checks whether it is a copy from operation, extract the parsed data to form the `copy_properties` structure and calls `write_to_file` for write operation 
/// @param pstmt 
/// @param queryString 
/// @param readOnlyTree 
/// @param context 
/// @param params 
/// @param queryEnv 
/// @param dest 
/// @param qc 
void file_writer_for_rocksdb(PlannedStmt *pstmt, const char *queryString, bool readOnlyTree,
                 ProcessUtilityContext context, ParamListInfo params,
                 QueryEnvironment *queryEnv, DestReceiver *dest, QueryCompletion *qc){

    // int connection_status;
    // char current_query[QUERY_STRING_LENGTH];

    if(pstmt->utilityStmt->type == T_CopyStmt){
        // elog(NOTICE, "Copy Statement executed");
        CopyStmt *stmt = (CopyStmt *)pstmt->utilityStmt;
        
        /* Check if COPY FROM (not COPY TO) */
        if (!stmt->is_from) {
            elog(INFO, "Ignoring COPY TO command.");
            if (prev_process_utility_hook)
                prev_process_utility_hook(pstmt, queryString, readOnlyTree, context, params, queryEnv, dest, qc);
        }else{
            copy_properties* properties = (copy_properties*)palloc(sizeof(copy_properties));
            strcpy(properties->table_name, stmt->relation->relname);
            strcpy(properties->csv_location, stmt->filename);
            elog(NOTICE, "File Location: %s", properties->csv_location);
            ListCell *option;
            foreach(option, stmt->options) {
                DefElem *defel = (DefElem *) lfirst(option);
                // elog(NOTICE, "%s", defel->defname);
                if(strcmp(defel->defname, "header") == 0){
                    properties->has_header = defGetBoolean(defel);
                    elog(NOTICE, "Has Headers: %s", properties->has_header ? "True" : "False");
                }
                if(strcmp(defel->defname, "delimiter") == 0){
                    strncpy(properties->delimiter, defGetString(defel), NAMEDATALEN);
                    elog(NOTICE, "Delimiter: %s", properties->delimiter);
                }

            }
            bool status = write_to_file(properties);
            if(!status){
                elog(NOTICE, "Cancel the operation and try again!");
            }
        }
    }
    if(prev_process_utility_hook){
        prev_process_utility_hook(pstmt, queryString, readOnlyTree, context, params, queryEnv, dest, qc);
    }else{
        standard_ProcessUtility(pstmt, queryString, readOnlyTree, context, params, queryEnv, dest, qc);
    }
}

/// @brief provides function definition for `shmem_startup_hook()` which initializes the queue of structrues for `rocksdb insertor()` to parse and dequeue in shared memory 
static void queue_shmem_startup(){
    // RequestAddinShmemSpace(sizeof(Queue));
    // RequestNamedLWLockTranche(QUEUE_LOCK_TRANCHE, 1);
    bool found;
    LWLockAcquire(AddinShmemInitLock, LW_EXCLUSIVE);
    shared_queue = (Queue*)ShmemInitStruct(QUEUE_STRUCT, sizeof(Queue), &found);
    if (!shared_queue) {
        elog(ERROR, "Failed to allocate shared memory");
    }
    if (!found) {
        initQueue(shared_queue);
    }
    LWLockRelease(AddinShmemInitLock);
    if (prev_shmem_startup_hook)
        prev_shmem_startup_hook();
}

void _PG_init(void){

    prev_process_utility_hook = ProcessUtility_hook;
    ProcessUtility_hook = file_writer_for_rocksdb;

    prev_shmem_startup_hook = shmem_startup_hook;
    shmem_startup_hook = queue_shmem_startup;
}

void _PG_fini(void){
    if(ProcessUtility_hook == file_writer_for_rocksdb){
        ProcessUtility_hook = prev_process_utility_hook;
    }
}
