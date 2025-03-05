# Develop a PostgreSQL Extension for Data Movement to RocksDB

## Objective

Create a PostgreSQL extension that efficiently transfers data from files (via the COPY command) into RocksDB while meeting the specified requirements for metadata handling, column selection, and concurrent processing.

## Requirements

### Metadata Management

- Develop a User-Defined Function (UDF) to store table-specific metadata.
  Metadata will include information about which columns to process.
- Example: For a table example_table (id1 INT, id2 INT, rating INT), specify columns to process during UDF execution.
- Store this metadata somewhere (should be valid across restarts) for use during the COPY command to determine the table data should be stored in rocksdb.

### Data Processing During COPY Command

- The extension should read the stored metadata during the COPY operation.
  Only the columns specified in the metadata should be processed and written to RocksDB.

### RocksDB Key-Value Format

- Data must be stored in RocksDB using the following formats:
  ```
  Key: TableOid|col1Len|col1|col2Len|col2
  Value: TableOid|col3Len|col3
  ```
- Exactly two columns from the table will be used as the key, and one column will be used as the value.

### Supported Data Types

The extension must handle columns of type int16, int32, int64, and text.
Concurrent Operation

- Ensure the extension supports simultaneous operations across multiple processes and multiple tables.
- Handle concurrency effectively to prevent conflicts or data corruption.

### Performance Testing
- Compare the performance of the extension against an alternative approach using triggers.
- Implement triggers that insert each row into RocksDB upon data insertion into a PostgreSQL table.
- Measure and report the performance difference between the extension’s batch processing during the COPY command and the row-wise trigger-based approach.


### Deliverables

The PostgreSQL extension code with all required functionalities.
Documentation explaining:

- How to use the UDF for metadata storage.
- Steps to execute the COPY command with this extension.
- Test cases demonstrating the extension’s ability to handle various scenarios, including concurrent processes and multiple tables.
- Include a UDF to validate data integrity by comparing RocksDB data with the corresponding PostgreSQL table.
Example:
    - The UDF should take the table name as input, read data from RocksDB, and compare it with the PostgreSQL table data row by row.
    - This should be done as part of regression testing


## My Plan 

sql with and without trigger, with copy command and 1 bg worker to insert 
small - 10K
mideium - 100K
large - 1M rows

also with 100M


## Todo

- udf which takes in table_name, 3 column names, does appropriate checks and writes the metdata information into an sql table with length, type, col_name and table oid
- override processutility hook to write into a new file, which also checks for metadata information, and takes care of a situation where the metadata is not present by (first
	considering first 3 column names, if that's not possible, raise error) also handles null values
- after finish writing, signal the bg worker 
- the bg worker spawns a new thread to read the file also, from the metadata information from the queue element to write into rocksdb 
- insertion to rocksdb happens using batch processing, through multi threading, and in c programming by taking care of the datatype, length from metadata 
- also write an sql trigger instead of overriding process utility hook to insert key/value pairs directly into rocksdb 
- write a udf which takes in table name, gets the metadata information to check data integrity from rocksdb and postgresql table
- regression test for verifying features


Note: 
- create a new db when accessing from a new db
- implement a queue based mechanism for inter process communication 
- use csv format for storing key/value pairs, try out bin files as well
