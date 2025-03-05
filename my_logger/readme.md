## Working of Copy Command

After the query is parsed and tokenized, utility function does not have a planning stage per se, it doesn't build a plan tree. 
Instead, when a Utility function is observed, `ProcessUtility` function is called to handle Utility functions. Here, `standard_ProcessUtility` function
is called to route it to the correct Utility function. 

Utility functions consist of DDL commands, DCL commands and some special operations like VACCUM, ANALYZE etc.
They do not have a planner stage because: 
- They do not require index scans, joins, or cost-based optimizations.
- They often operate at the schema or system level, not on tuples returned from an execution plan. 

Some utility commands like EXPLAIN ANALYZE or CREATE MATERIALIZED VIEW may internally invoke the planner, but the utility function itself does not have a planning phase.

### DoCopy (Initial Processing)

DoCopy executes the SQL COPY statement. This function first checks whether the COPY operation is reading from a file or writing to a file.
In `DoCopy` function, here are the key arguments:

- `pstate`: Parser state structure containing the parsing context
- `stmt`: The parsed COPY statement structure
- `stmt_location`: Position of the statement in the query string (starting at beginning)
- `stmt_len`: Length of the COPY statement
- `processed`: Pointer to store number of rows processed

### BeginCopyFrom

Setup to read tuples from a file for COPY FROM.

Key Arguments:
 * 'rel': Used as a template for the tuples
 * 'whereClause': WHERE clause from the COPY FROM command
 * 'filename': Name of server-local file to read, NULL for STDIN
 * 'is_program': true if 'filename' is program to execute
 * 'data_source_cb': callback that provides the input data
 * 'attnamelist': List of char *, columns to include. NIL selects all cols.
 * 'options': List of DefElem. See copy_opt_item in gram.y for selections.
 
Returns a CopyFromState, to be passed to NextCopyFrom and related functions.

Local Variables (though many are optimized out):

- cstate: Will hold the CopyStateData structure for managing the copy operation
- tupDesc: Will store the tuple descriptor for the target table
- num_phys_attrs: Number of physical attributes in the table
- in_functions: Array of input functions for each column
- typioparams: Type-specific parameters for input functions
- defmap and defexprs: For handling default values
- progress_cols and progress_vals: For progress reporting

Functions: 

- Initializes the infrastructure for the COPY operation
- Allocates and sets up the CopyStateData structure (cstate)
- Validates input parameters (file permissions, table access rights)
- Sets up type conversion functions for each column
- Prepares default value handling
- Basically handles all the preparation work before actual data reading begins
- Returns the initialized cstate structure

### CopyFrom

This function will:

- Set up the execution state
- Initialize batch insertion buffers
- Process the CSV file row by row (through internal functions)
- Handle any triggers on the target table
- Manage error cases and report progress

Key Arguments:

- cstate: The CopyStateData structure containing all the copy operation state, including file handle, options, and format information

Important Local Variables:

- resultRelInfo: Information about the target relation (table)
- estate: Execution state for the operation
- multiInsertInfo: Structure for batch insertion optimization with fields:
    - multiInsertBuffers: 0x202adf40 (buffer for batching rows)
    - bufferedTuples: Current count of buffered rows
    - bufferedBytes: Size of buffered data
    - mycid: 627337184 (Command ID for this operation)

- errcallback: Error handling structure with:
    - previous: (previous error handler)
    - callback: (error callback function)
    - arg: (argument passed to callback)

### CopyReadLine

`CopyReadLine` parses the data in 'input_buf', one line at a time.
    It is responsible for finding the next newline marker, taking quote and
    escape characters into account according to the COPY options.  The line
    is copied into 'line_buf', with quotes and escape characters still
    intact.

`CopyReadLine`, operates at a higher level and is specifically designed for processing structured text input. It:
- Uses `CopyGetData` underneath to get raw data. It calls this function inside a do-While loop
- Understands line endings (both Unix and Windows style)
- Handles quoted fields in CSV format
- Manages escape sequences
- Recognizes field delimiters
- Builds complete logical lines even if they span multiple physical lines
- Maintains state about partially processed lines

- Handles the CSV format specifics
- Processes quoted fields and escape characters
- May need multiple calls to CopyGetData to build a complete logical line
- Returns fully processed lines to CopyFrom

This file contains routines to parse the text, CSV and binary input
formats.  The main entry point is NextCopyFrom(), which parses the
next input line and returns it as Datums.

In text/CSV mode, the parsing happens in multiple stages:

[data source] --> raw_buf --> input_buf --> line_buf --> attribute_buf
            1.          2.            3.           4.

1. CopyLoadRawBuf() reads raw data from the input file or client, and
places it into 'raw_buf'.

2. CopyConvertBuf() calls the encoding conversion function to convert
the data in 'raw_buf' from client to server encoding, placing the
converted result in 'input_buf'.

3. CopyReadLine() parses the data in 'input_buf', one line at a time.
It is responsible for finding the next newline marker, taking quote and
escape characters into account according to the COPY options.  The line
is copied into 'line_buf', with quotes and escape characters still
intact.

4. CopyReadAttributesText/CSV() function takes the input line from
'line_buf', and splits it into fields, unescaping the data as required.
The fields are stored in 'attribute_buf', and 'raw_fields' array holds
pointers to each field.




### CopyGetData

CopyGetData is a lower-level function that handles the raw reading of data from the input source. Think of it as the foundation layer that deals with the basic I/O operations. It:

- Reads raw bytes from the input source (file or program)
- Manages the input buffer
- Handles end-of-file conditions
- Deals with different input encodings
- Can read arbitrary amounts of data without assuming any structure
- Is encoding-aware and handles character set conversions

### EndCopyFrom

- Ensures all remaining data is written
- Closes the input file
- Cleans up allocated resources
- Reports final statistics

After the copy from process, various DML operations like index updation, constrain checks, are run which triggers a log.

### BeginCopyTo

This is the initialization phase of a COPY TO command that's exporting data from the order_items table to a CSV file.

Parser State (pstate):
The parser state structure (0x58a2f963c420) shows this is a direct table copy rather than a query-based copy since:
- p_rtable exists but p_joinexprs is NULL
- p_is_insert is false (as expected for COPY TO)
- No aggregates or window functions are involved (p_hasAggs and p_hasWindowFuncs are false)


Target Setup:
- rel (0x7c21118e52c0): Points to the relation descriptor for order_items
- raw_query is NULL: Confirms this is a simple table copy, not a query
- queryRelId is 16401: The OID of the order_items table
- filename points to "/home/subru/backup/myorderitems1.csv"
- is_program is false: We're writing to a file, not a program
- attnamelist is NULL: No specific column list provided, so all columns will be copied

Progress Tracking:
The progress_cols and progress_vals arrays are being set up to track:
- Number of tuples processed
- Amount of data written


### DoCopyTo

The CopyStateData structure (cstate) reveals how PostgreSQL has configured this COPY TO operation. Let's break it down piece by piece:
File and Encoding Configuration:

- The operation is writing to a file (copy_dest = COPY_FILE) at "/home/subru/backup/myorderitems2.csv"
- Character encoding handling is active (need_transcoding = true) with encoding ID 6
The system will need to handle character set conversion during the copy

CSV Formatting Options:

- CSV mode is enabled (csv_mode = true)
- Headers will be included (header_line = true)
- The delimiter is set to "," (delim)
- Double quotes (") are used for quoting (quote) and escaping (escape)
- No special NULL handling is configured (null_print is empty)
- No selective quoting is enforced (force_quote_all = false)

Memory Management:

A dedicated copy context (copycontext) has been allocated at 0x58a2f95f68f0
The rowcontext for processing individual rows will be set up when needed
The bytes_processed counter is initialized to 0

Query Configuration:

- This is a direct table copy (queryDesc = 0x0, no query descriptor)
- The target relation (rel) is set to 0x7c21118e52c0
- An attribute number list (attnumlist) is prepared at 0x58a2f95f6a10
- No WHERE clause filtering (whereClause = 0x0)

When DoCopyTo executes, it will:

- Prepare the output buffer system
- Write the header row (since header_line = true)
- Begin scanning the source table
- Convert each row's values to their text representation
- Apply CSV formatting rules (quoting, escaping)
- Write the formatted data to the output file
- Keep track of bytes processed for progress reporting

The character encoding handling (need_transcoding = true) means PostgreSQL will convert data from the database's internal encoding to the file encoding (file_encoding = 6) during output. This ensures proper character representation in the CSV file.

### EndCopyTo



## ProcessUtility_hook
```c
/*
    This hook is called inside PortalUtility when the hook is defined else goes to standard_ProcessUtility
    PortalUtility if called inside PortalRunUtility which executes a utility statement inside a portal 
    A portal is an abstraction that represents the execution state of a running or runnable query. (type of portal depends on the Portal run strategy).

 * standard_ProcessUtility itself deals only with utility commands for
 * which we do not provide event trigger support.  Commands that do have
 * such support are passed down to ProcessUtilitySlow, which contains the
 * necessary infrastructure for such triggers.
 *
 * This division is not just for performance: it's critical that the
 * event trigger code not be invoked when doing START TRANSACTION for
 * example, because we might need to refresh the event trigger cache,
 * which requires being in a valid transaction.
 */
```

### Function Arguments: 

1.  ```c
    PlannedStmt * pstmt //PlannedStmt wrapper for the utility statement
    /* ----------------
    *		PlannedStmt node
    *
    * The output of the planner is a Plan tree headed by a PlannedStmt node.
    * PlannedStmt holds the "one time" information needed by the executor.
    *
    * For simplicity in APIs, we also wrap utility statements in PlannedStmt
    * nodes; in such cases, commandType == CMD_UTILITY, the statement itself
    * is in the utilityStmt field, and the rest of the struct is mostly dummy.
    * (We do use canSetTag, stmt_location, stmt_len, and possibly queryId.)
    * ----------------
    */
    ```

2. ```c
    const char * queryString 
    /* original source text of command, may be passed multiple times when processing a query string containing multiple semicolon-separated statements. pstmt->stmt_location and pstmt->stmt_len indicates the substring containing the current statement.
    */
   ```
3. ```c
    ProcessUtilityContext context // identifies source of statement (toplevel client command, non-toplevel client command, subcommand of a larger utility command)
    typedef enum{
        PROCESS_UTILITY_TOPLEVEL,	/* toplevel interactive command */
        PROCESS_UTILITY_QUERY,		/* a complete query, but not toplevel */
        PROCESS_UTILITY_QUERY_NONATOMIC,	/* a complete query, nonatomic
                                            * execution context */
        PROCESS_UTILITY_SUBCOMMAND	/* a portion of a query */
    } ProcessUtilityContext;
    ```
4. ```c
   ParamListInfo params // parameters of an execution.
   ```
5. ```c
   QueryEnvironment * queryEnv // execution environment, optional, can be NULL.
   ```
6. ```c
   DestReceiver * dest // results receiver.
   /* ----------------
    *		DestReceiver is a base type for destination-specific local state.
    *		In the simplest cases, there is no state info, just the function
    *		pointers that the executor must call.
    *
    * Note: the receiveSlot routine must be passed a slot containing a TupleDesc
    * identical to the one given to the rStartup routine.  It returns bool where
    * a "true" value means "continue processing" and a "false" value means
    * "stop early, just as if we'd reached the end of the scan".
    * ----------------
    */
   ```

## planner_hook

```c
/*
    called under planner() function which calls standard_planner() if planner_hook() is not defined
*/
```

### Function Arguments:
```c
Query * parse // parsed query text.
/*****************************************************************************
 *	Query Tree
    *****************************************************************************/

/*
    * Query -
    *	  Parse analysis turns all statements into a Query tree
    *	  for further processing by the rewriter and planner.
    *
    *	  Utility statements (i.e. non-optimizable statements) have the
    *	  utilityStmt field set, and the rest of the Query is mostly dummy.
    *
    *	  Planning converts a Query tree into a Plan tree headed by a PlannedStmt
    *	  node --- the Query structure is not used by the executor.
    */

const char * query_string // original query text.
int cursorOptions
ParamListInfo boundParams
```

Returns a pointer to struct of type `PlannedStmt`

```c
/* ----------------
 *		PlannedStmt node
 *
 * The output of the planner is a Plan tree headed by a PlannedStmt node.
 * PlannedStmt holds the "one time" information needed by the executor.
 *
 * For simplicity in APIs, we also wrap utility statements in PlannedStmt
 * nodes; in such cases, commandType == CMD_UTILITY, the statement itself
 * is in the utilityStmt field, and the rest of the struct is mostly dummy.
 * (We do use canSetTag, stmt_location, stmt_len, and possibly queryId.)
 * ----------------
 */
```

## ExecutorRun_hook

```c
/* ----------------------------------------------------------------
 *		ExecutorRun
 *
 *		This is the main routine of the executor module. It accepts
 *		the query descriptor from the traffic cop and executes the
 *		query plan.
 *
 *		ExecutorStart must have been called already. which initializes all the structures required for storing intermediate data during the execution
 *
 *		If direction is NoMovementScanDirection then nothing is done
 *		except to start up/shut down the destination.  Otherwise,
 *		we retrieve up to 'count' tuples in the specified direction.
 *
 *		Note: count = 0 is interpreted as no portal limit, i.e., run to
 *		completion.  Also note that the count limit is only applied to
 *		retrieved tuples, not for instance to those inserted/updated/deleted
 *		by a ModifyTable plan node.
 *
 *		There is no return value, but output tuples (if any) are sent to
 *		the destination receiver specified in the QueryDesc; and the number
 *		of tuples processed at the top level can be found in
 *		estate->es_processed.
 *
 *		We provide a function hook variable that lets loadable plugins
 *		get control when ExecutorRun is called.  Such a plugin would
 *		normally call standard_ExecutorRun().
 *
 * ----------------------------------------------------------------
 */
 ```

Called at any plan execution, after `ExecutorStart`.

Replaces `standard_ExecutorRun()`

```c
QueryDesc * queryDesc // query descriptor from the traffic cop.
/* ----------------
 *		query descriptor:
 *
 *	a QueryDesc encapsulates everything that the executor
 *	needs to execute the query.
 *
 *	For the convenience of SQL-language functions, we also support QueryDescs
 *	containing utility statements; these must not be passed to the executor
 *	however.
 * ---------------------
 */

ScanDirection direction // if value is NoMovementScanDirection then nothing is done except to start up/shut down the destination.

typedef enum ScanDirection
{
	BackwardScanDirection = -1,
	NoMovementScanDirection = 0,
	ForwardScanDirection = 1
} ScanDirection;

uint64 count // count = 0 is interpreted as no portal limit, i.e., run to completion. Also note that the count limit is only applied to retrieved tuples, not for instance to those inserted/updated/deleted by a ModifyTable plan node.
// unsigned long int uint64
bool execute_once //     becomes equal to true after first execution.
```

## Shared Memory & LWLocks

Shared memory in PostgreSQL is a memory segment accessible by all PostgreSQL backend processes. It is primarily used for caching, communication, and coordination between processes. PostgreSQL relies on shared memory for key operations such as:

- Buffer Cache: Storing frequently accessed disk pages.
- Locks and LWLocks: Synchronization mechanisms for concurrent access.
- WAL Buffers: Temporary storage for write-ahead logging (WAL).
- Background Workers Communication: Allows different processes to exchange information.

### LWLocks (Lightweight Locks) in PostgreSQL
In PostgreSQL, LWLocks (Lightweight Locks) are synchronization primitives used to protect shared memory structures from concurrent access by multiple processes. They provide a balance between performance and safety, ensuring efficient access control without introducing significant overhead.

Add-ins can reserve LWLocks and an allocation of shared memory on server startup. The add-in's shared library must be preloaded by specifying it in shared_preload_libraries. Shared memory is reserved by calling the below code from your `_PG_init()` function.

```c
void RequestAddinShmemSpace(int size)
/*
 * RequestAddinShmemSpace
 *		Request that extra shmem space be allocated for use by
 *		a loadable module.
 *
 * This is only useful if called from the _PG_init hook of a library that
 * is loaded into the postmaster via shared_preload_libraries.  Once
 * shared memory has been allocated, calls will be ignored.  (We could
 * raise an error, but it seems better to make it a no-op, so that
 * libraries containing such calls can be reloaded if needed.)
 */

/*
 * ShmemInitStruct -- Create/attach to a structure in shared memory.
 *
 *		This is called during initialization to find or allocate
 *		a data structure in shared memory.  If no other process
 *		has created the structure, this routine allocates space
 *		for it.  If it exists already, a pointer to the existing
 *		structure is returned.
 *
 *	Returns: pointer to the object.  *foundPtr is set true if the object was
 *		already in the shmem index (hence, already initialized).
 *
 *	Note: before Postgres 9.0, this function returned NULL for some failure
 *	cases.  Now, it always throws error instead, so callers need not check
 *	for NULL.
 */
```

LWLocks are reserved by calling the below code from `_PG_init()`. 
```c
void RequestNamedLWLockTranche(const char *tranche_name, int num_lwlocks)
/*
 * RequestNamedLWLockTranche
 *		Request that extra LWLocks be allocated during postmaster
 *		startup.
 *
 * This is only useful for extensions if called from the _PG_init hook
 * of a library that is loaded into the postmaster via
 * shared_preload_libraries.  Once shared memory has been allocated, calls
 * will be ignored.  (We could raise an error, but it seems better to make
 * it a no-op, so that libraries containing such calls can be reloaded if
 * needed.)
 *
 * The tranche name will be user-visible as a wait event name, so try to
 * use a name that fits the style for those.
 */
```

This will ensure that an array of num_lwlocks LWLocks is available under the name tranche_name. Use `GetNamedLWLockTranche()` to get a pointer to this array.

🔹 Types of LWLocks
LWLocks can be taken in two modes:

1. Shared Mode (LW_SHARED): Multiple readers can hold the lock simultaneously.
2. Exclusive Mode (LW_EXCLUSIVE): Only one process can hold the lock at a time.

_📌 Example: If multiple processes need to read a shared memory structure, they use a shared lock. If a process needs to modify it, it acquires an exclusive lock._


For example: 

```c
static int *my_shared_counter = NULL;
#define FACTORIAL_RESULT_SHMEM "FactorialResult"
#define FACTORIAL_COUNT_SHMEM "FactorialCount"
void _PG_init(void)
{
    bool found_result, found_count;
    // Reserve shared memory space before PostgreSQL starts
    RequestAddinShmemSpace(sizeof(int) * 2);
    // Acquire the lock before initializing multiple shared memory variables
    LWLockAcquire(AddinShmemInitLock, LW_EXCLUSIVE);
    
    factorial_result = ShmemInitStruct(FACTORIAL_RESULT_SHMEM, sizeof(int), &found_result);
    if (!found_result)
    {
        *factorial_result = 1; // Default initialization
    }

    // Allocate shared memory for factorial computation count
    factorial_count = ShmemInitStruct(FACTORIAL_COUNT_SHMEM, sizeof(int), &found_count);
    if (!found_count)
    {
        *factorial_count = 0; // Initialize count to zero
    }

    // Release lock after allocation
    LWLockRelease(AddinShmemInitLock);
}
```

PostgreSQL doesn't allow allocation of shared memory in runtime so `RequestAddinShmemSpace` is called to reserve the SHmem before startup so that it can properly allocated. 

- `ShmemInitStruct` called during extension load, protected with `AddinShmemInitLock` which allocates and initializes shared memory.
- `RequestAddinShmemSpace(size)` called before PostgreSQL starts (inside _PG_init()) which reserves shared memory space.

## Background Workers

### **Parameters/Properties of a PostgreSQL Background Worker**
When defining a **background worker** in PostgreSQL, you configure its behavior using the `BackgroundWorker` struct, which contains various **parameters/properties**. 

---

### **Key Properties of Background Workers**
Below is a breakdown of the important properties:

| **Property**           | **Type**        | **Description** |
|------------------------|----------------|----------------|
| `bgw_name`            | `char[BGW_MAXLEN]` | Name of the background worker (shown in logs and `pg_stat_activity`). |
| `bgw_type`            | `char[BGW_MAXLEN]` | Type of worker (optional, can be used for categorization). |
| `bgw_flags`           | `int` | Flags defining permissions (e.g., shared memory access). |
| `bgw_start_time`      | `BgWorkerStartTime` | Determines when the worker starts (see options below). |
| `bgw_restart_time`    | `int` | Specifies the delay (in seconds) before restarting after a crash (-1 = never restart). |
| `bgw_main`            | `bgworker_main_type` | Pointer to the function that runs the worker's main loop. |
| `bgw_main_arg`        | `Datum` | A user-defined argument passed to the worker function. |
| `bgw_notify_pid`      | `pid_t` | PID of the PostgreSQL backend process that started the worker (used for notification). |

---

### **1️⃣ `bgw_name`**
- Specifies the **display name** of the worker (useful for debugging/logging).
- Shown in logs and system views like `pg_stat_activity`.
```c
snprintf(worker.bgw_name, BGW_MAXLEN, "Custom Background Worker");
```

---

### **2️⃣ `bgw_type`**
- (Optional) A category label for the worker.
- Helps distinguish between different types of background workers.

```c
snprintf(worker.bgw_type, BGW_MAXLEN, "MyWorkerType");
```

---

### **3️⃣ `bgw_flags`**
Defines permissions for the background worker. Possible values:

| **Flag**                 | **Meaning** |
|--------------------------|------------|
| `BGWORKER_SHMEM_ACCESS`  | Allows the worker to access shared memory. |
| `BGWORKER_BACKEND_DATABASE_CONNECTION` | Allows the worker to connect to a database. |

Example:
```c
worker.bgw_flags = BGWORKER_SHMEM_ACCESS | BGWORKER_BACKEND_DATABASE_CONNECTION;
```

---

### **4️⃣ `bgw_start_time`**
Determines **when** the worker starts relative to the PostgreSQL server startup phase.

| **Option**                      | **When the worker starts** |
|---------------------------------|----------------------------|
| `BgWorkerStart_PostmasterStart` | Immediately after the postmaster starts. |
| `BgWorkerStart_ConsistentState` | After reaching a consistent state during crash recovery. |
| `BgWorkerStart_RecoveryFinished` | After recovery is finished and database is available. |

Example:
```c
worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
```

---

### **5️⃣ `bgw_restart_time`**
- Specifies the **restart delay** in seconds after the worker **crashes**.
- Use `-1` to prevent automatic restarts.

| **Value** | **Behavior** |
|----------|-------------|
| `0`       | Restart immediately. |
| `10`      | Restart after 10 seconds. |
| `-1`      | Never restart automatically. |

Example:
```c
worker.bgw_restart_time = 10;
```

---

### **6️⃣ `bgw_main`**
- The **entry point function** for the background worker.
- This function runs in a loop and performs the worker’s task.

```c
worker.bgw_main = custom_worker_main;
```

---

### **7️⃣ `bgw_main_arg`**
- A user-defined **argument** passed to the worker.
- Useful for passing configuration or worker-specific data.

```c
worker.bgw_main_arg = Int32GetDatum(42);  // Passes an integer
```

---

### **8️⃣ `bgw_notify_pid`**
- Stores the **process ID (PID)** of the backend process that started the worker.
- If `0`, no notification is sent.

Example:
```c
worker.bgw_notify_pid = MyProcPid;  // Notify the main backend process
```

---

## **Example: Defining a Background Worker with All Parameters**
```c
void _PG_init(void)
{
    BackgroundWorker worker;

    memset(&worker, 0, sizeof(BackgroundWorker));
    worker.bgw_flags = BGWORKER_SHMEM_ACCESS | BGWORKER_BACKEND_DATABASE_CONNECTION;
    worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
    worker.bgw_restart_time = 10;
    snprintf(worker.bgw_name, BGW_MAXLEN, "Factorial Worker");
    snprintf(worker.bgw_type, BGW_MAXLEN, "Computation");
    worker.bgw_main = factorial_worker_main;
    worker.bgw_main_arg = Int32GetDatum(100);
    worker.bgw_notify_pid = 0;

    RegisterBackgroundWorker(&worker);
}
```

---

## **Summary**
| **Property**           | **Purpose** |
|------------------------|------------|
| `bgw_name`            | Name of the worker (shown in logs). |
| `bgw_type`            | Category of the worker (optional). |
| `bgw_flags`           | Defines access permissions (e.g., shared memory). |
| `bgw_start_time`      | Specifies when the worker starts. |
| `bgw_restart_time`    | Defines restart behavior after crashes. |
| `bgw_main`            | Function that runs the worker. |
| `bgw_main_arg`        | User-defined argument for the worker. |
| `bgw_notify_pid`      | Backend process ID for notifications. |

---

### 🚀 **Why Are These Properties Useful?**
- **Better Control**: Configure worker startup and restart behavior.
- **Enhanced Debugging**: Set meaningful names for tracking worker activity.
- **Increased Flexibility**: Pass arguments to customize worker behavior.
- **Improved Performance**: Use background workers for **heavy** or **asynchronous tasks**.
