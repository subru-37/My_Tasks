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

#define QUERY_STRING_LENGTH 1024

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

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

typedef struct column_list {
    struct column_list* next;
    char column_name[NAMEDATALEN]; // char array with longest sequence possible for a column name in postgresql
    char data_type[NAMEDATALEN]; // datatype of the column
}column_list;

// static shmem_startup_hook_type prev_shmem_startup_hook = NULL; // hook for initializing shared memory
static ProcessUtility_hook_type prev_process_utility_hook = NULL;

typedef enum copy_type {
    COPY_FROM,
    COPY_TO
}copy_type;

typedef struct copy_properties{
    char table_name[NAMEDATALEN];
    char csv_location[1024];
    char delimiter[NAMEDATALEN];
    bool has_header;
}copy_properties;

column_list* add_column(char* name, char* type,column_list* HEAD ){
    column_list* node = (column_list*)palloc(sizeof(column_list));
    strncpy(node->column_name, name, NAMEDATALEN);
    strncpy(node->data_type, type, NAMEDATALEN);
    node->next = HEAD;
    HEAD = node;
    return node;
}

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

bool search_columns(char* table_name, char* key_column_1, char* key_column_2, char* value_column_1, column_list* HEAD ){
    column_list* temp = HEAD;
    bool flag1 = false;
    bool flag2 = false;
    bool flag3 = false;
    while(temp!=NULL){
        if(strncmp(key_column_1, temp->column_name, NAMEDATALEN) == 0){
            flag1 = true;
        }
        if(strncmp(key_column_2, temp->column_name, NAMEDATALEN) == 0){
            flag2 = true;
        }
        if(strncmp(value_column_1, temp->column_name, NAMEDATALEN) == 0){
            flag3 = true;
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

column_list* valid_columns(char* current_query, char* table_name, char* key_column_1, char* key_column_2, char* value_column_1, bool *validity, int* len, bool showerrors){
    column_list* HEAD = NULL;
    int result;
    snprintf(current_query, QUERY_STRING_LENGTH, "SELECT a.attname AS column_name, t.typname AS data_type \
        FROM pg_class c\
        JOIN pg_attribute a ON a.attrelid = c.oid\
        JOIN pg_type t ON a.atttypid = t.oid\
        WHERE c.relname = '%s' AND a.attnum > 0", table_name);
    result = SPI_execute(current_query, true, 0);
    if(result != SPI_OK_SELECT){
        elog(ERROR, "Unable to check current details of table_name");
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
        char *type = DatumGetCString(column_type_bin);
        HEAD = add_column(name, type, HEAD);
        elog(LOG, "column detected: %s", HEAD->column_name);
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


Datum metadata_handler(PG_FUNCTION_ARGS){
    // declaring and initializing required variables
    int connection_status;
    char current_query[QUERY_STRING_LENGTH];
    snprintf(current_query, QUERY_STRING_LENGTH, "CREATE TABLE IF NOT EXISTS metadata (\
        id UUID DEFAULT gen_random_uuid() PRIMARY KEY NOT NULL,\
        table_oid BIGINT NOT NULL,\
        table_name TEXT NOT NULL,\
        column_name TEXT NOT NULL,\
        data_type TEXT NOT NULL, \
        key_or_value TEXT NOT NULL, UNIQUE(table_name, column_name))");
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
    result = SPI_execute(current_query, false, 0);
    if(result != SPI_OK_UTILITY){
        elog(ERROR, "Metadata table creation error: %d", result);
        PG_RETURN_NULL();
    }
    elog(LOG, "Verified creation og metadata table");
    bool validity = false;
    int len;
    column_list* HEAD = valid_columns(current_query, table_name, key_column_1, key_column_2, value_column_1, &validity, &len, true);
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

// void generate_uuid(char *uuid) {
//     uuid_t binuuid;
//     uuid_generate_random(binuuid);

// #ifdef capitaluuid
//     uuid_unparse_upper(binuuid, uuid);
// #elif lowercaseuuid
//     uuid_unparse_lower(binuuid, uuid);
// #else
//     uuid_unparse(binuuid, uuid);
// #endif
// }
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
// file writer 
bool write_to_file(copy_properties* properties){
    int connection_status;
    int result;
    char current_query[QUERY_STRING_LENGTH];
    snprintf(current_query, QUERY_STRING_LENGTH, "SELECT * FROM metadata WHERE table_name = '%s'", properties->table_name);
    if ((connection_status = SPI_connect()) != SPI_OK_CONNECT) {
        elog(ERROR, "SPI_connect failed: error code %d", connection_status);
        return false;
    }
    result = SPI_execute(current_query, false, 0);
    if(result != SPI_OK_SELECT || SPI_processed<=0){
        // metadata not found or errored case
        elog(WARNING_CLIENT_ONLY, "Metadata not found for table: %s", properties->table_name);
        elog(WARNING_CLIENT_ONLY, "--Taking key value pairs from random 3 columns for rocksdb--");
        
        
        // collecting table data, validity is not important here
        char* temp1 = "temp1";
        char* temp2 = "temp2";
        char* temp3 = "temp3";
        bool valid;
        int len;
        column_list* HEAD = valid_columns(current_query, properties->table_name, temp1, temp2, temp3, &valid, &len, false);
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
        char key_column_1[NAMEDATALEN], key_column_2[NAMEDATALEN], value_column_1[NAMEDATALEN];
        elog(NOTICE, "Column numbers are %d, %d, %d, length: %d", random_columns[0], random_columns[1], random_columns[2], len);
        SPI_execute("BEGIN;", false, 0);
        while(temp!=NULL){
            if(i == random_columns[0]){
                strncpy(key_column_1, temp->column_name, NAMEDATALEN);
                snprintf(current_query, QUERY_STRING_LENGTH, "INSERT INTO metadata(table_oid, table_name, column_name,\
                    data_type, key_or_value) VALUES(%d, '%s', '%s', '%s', 'KEY1')", 
                    table_oid, properties->table_name, temp->column_name, temp->data_type);
                result = SPI_execute(current_query, false, 0);
                if(SPI_processed<=0 || result!= SPI_OK_INSERT){
                    elog(ERROR, "Error inserting metadata info for '%s' in table '%s'", temp->column_name, properties->table_name);
                    error = true;
                    break;
                }
            }
            if(i == random_columns[1]){
                strncpy(key_column_2, temp->column_name, NAMEDATALEN);
                snprintf(current_query, QUERY_STRING_LENGTH, "INSERT INTO metadata(table_oid, table_name, column_name,\
                    data_type, key_or_value) VALUES(%d, '%s', '%s', '%s', 'KEY2')", 
                    table_oid, properties->table_name, temp->column_name, temp->data_type);
                result = SPI_execute(current_query, false, 0);
                if(SPI_processed<=0 || result!= SPI_OK_INSERT){
                    elog(ERROR, "Error inserting metadata info for '%s' in table '%s'", temp->column_name, properties->table_name);
                    error = true;
                    break;
                }
            }
            if(i == random_columns[2]){
                strncpy(value_column_1, temp->column_name, NAMEDATALEN);
                snprintf(current_query, QUERY_STRING_LENGTH, "INSERT INTO metadata(table_oid, table_name, column_name,\
                    data_type, key_or_value) VALUES(%d, '%s', '%s', '%s', 'VALUE')", 
                    table_oid, properties->table_name, temp->column_name, temp->data_type);
                result = SPI_execute(current_query, false, 0);
                if(SPI_processed<=0 || result!= SPI_OK_INSERT){
                    elog(ERROR, "Error inserting metadata info for '%s' in table '%s'", temp->column_name, properties->table_name);
                    error = true;
                    break;
                }
            }
            i++;
            temp = temp->next;
        }
        if(error){
            elog(NOTICE, "Try again!!");
        }else{
            SPI_execute("COMMIT;", false, 0);
        }
        
        elog(NOTICE, "Choosen column numbers: %s|%s for KEY and %s for VALUE", key_column_1, key_column_2, value_column_1);
    }
    // FILE *file;
    // char buffer[4096]; // one line buffer
    // char uuid[UUID_STR_LEN];
    // generate_uuid(uuid);  // Generate a new UUID
    // char filename[UUID_STR_LEN + 4];  // ".csv" needs 4 extra bytes
    // snprintf(filename, sizeof(filename), "%s.csv", uuid);

    // elog(NOTICE, "Generated file: %s\n", filename);
    // FILE *file = fopen(filename, "w");
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

void _PG_init(void){
    prev_process_utility_hook = ProcessUtility_hook;
    ProcessUtility_hook = file_writer_for_rocksdb;

}

void _PG_fini(void){
    if(ProcessUtility_hook == file_writer_for_rocksdb){
        ProcessUtility_hook = prev_process_utility_hook;
    }
}
