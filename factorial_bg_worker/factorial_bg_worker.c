#include "postgres.h" // core psql headers
#include "fmgr.h" // definitions for postgresql function manager and function call interface
#include "postmaster/bgworker.h" // defines functions and data structures used for defining background workers
#include "storage/lwlock.h" // lightweight lock manager
#include "storage/shmem.h" // shared memory access functions and definitions
#include "ipc.h" // type definitions for shmem_startup_hook_type
#include "storage/proc.h" // data structures for each process's shared memory (latches and all)
#include "utils/wait_event.h" // definitions wait events 
#include "numeric.h" // return macros for numeric datatype

#define FACTORIAL_STRUCT "FactorialStruct"
#define FACTORIAL_LOCK_TRANCH "MyFactorialStructLock"

PG_MODULE_MAGIC; // why ?
// all libraries are dynamically loaded for extensions, so in order to ensure that
// the extension is not imported to an incompatible server, this macro is added in the begninning
// it allows psql to detect incompatibilities such as code compiled for a different version of psql 
// in a multiple-source-file module, the macro call should only appear once.
// the blocks are compared by memcmp and it contains returns PG_MODULE_MAGIC_DATA containing settings like
// PG_VERSION_NUM, FUNC_MAX_ARGS, NAMEDATALEN - identifier length

typedef enum FactorialStatus{
    FACT_FREE,
    FACT_PROCESSING,
    FACT_DONE
} FactorialStatus; // enum indicating bg worker status

typedef struct {
    LWLock *lock;
    int64 num;
    __uint128_t result;
    FactorialStatus status;
} FactorialSharedMemory; // struct for storing intermediate results

static FactorialSharedMemory *shared_memory = NULL; // global struct pointer to shared memory
static shmem_startup_hook_type prev_shmem_startup_hook = NULL; // hook for initializing shared memory

// function to compute factorial
static __uint128_t factorial(int64 input){
    __uint128_t result = 1;
    for (__uint128_t i = 1; i <= input; i++) {
        result *= i;
    }
    return result;
}
// declaration of bg process function
// PGDLLEXPORT void fact_bg_worker_main(Datum main_arg);
volatile sig_atomic_t got_sigterm = false; // what's atomic? 
// prevents different threads from updating the same variable simultaneously to prevent race-conditions
// atomic variables solves the synchronisation problems 
// volatile variables explicitly tell compilers not to optimize them and solves the visibility problem
// (when threads have a local cache and a shared cache, changes to the local cache are flushed to
// update the local cache of 2nd thread)
// when working with different threads

/* Signal handler for SIGTERM */
void SignalHandlerForShutdown(int signum) {
    got_sigterm = true;
    SetLatch(&MyProc->procLatch);  /* Wake up worker */ // immediately wakes up the process
}
// bg worker main function 
// PGDLLEXPORT check this out!!
void fact_bg_worker_main(Datum main_arg) {

    pqsignal(SIGTERM, SignalHandlerForShutdown); // process termination signal
    pqsignal(SIGINT, SignalHandlerForShutdown); // Interactive attention signal
    // check out signal handlers 
    // normally when all the child processes are getting executed, when the postmaster receives either of those above 
    // signals, it sends the same signals, to all the child processes
    // all the child processes have their own versions of signal handlers
    // wals ensure data consistency before exiting, backend processes cleans up, disconnect clients and shuts down
    // so background processes have to check for this signal periodically and resume its functioning, and 
    // gracefully shutdown on receiving appropriate signals
    
    // Registers SignalHandlerForShutdown() for SIGTERM and SIGINT signals.
    BackgroundWorkerUnblockSignals(); // aka it unblocks receiving of signals 
    // Ensures the worker can receive signals properly (since background workers run in a separate process).
    
    elog(LOG, "Factorial background worker started.");

    //Runs an infinite loop that keeps the worker alive until PostgreSQL sends a SIGTERM.
    while (!got_sigterm) {
        int rc;

        /* Efficiently sleeps until something important happens. */
        // function which puts the bg worker to sleep with conditions if true will wake up the process
        rc = WaitLatch(
            &MyProc->procLatch, // MyProc is a per process global variable and it has a member called procLatch
            // The latch (a synchronization primitive) that allows the process to sleep and wake up when necessary.
            WL_LATCH_SET | WL_TIMEOUT | WL_POSTMASTER_DEATH, // Conditions that wakes up the process
            500L,  /* 1-second timeout */
            PG_WAIT_EXTENSION // wait event
        );

        ResetLatch(&MyProc->procLatch);
        /* 
            Waits efficiently for an event (instead of looping and wasting CPU).
            WaitLatch() puts the worker to sleep until:
                - WL_LATCH_SET → The worker is explicitly woken up (e.g., SQL function triggers a job).
                - WL_TIMEOUT → The worker wakes up after 1 second even if nothing happens.
                - WL_POSTMASTER_DEATH → PostgreSQL is shutting down (the postmaster process is dead).
                - ResetLatch() This clears the latch after we wake up. If we don’t reset it, 
                    the latch will remain set, and WaitLatch() might not work properly the next time. 
        */
        /* Exit if PostgreSQL is shutting down */
        if (rc & WL_POSTMASTER_DEATH) {
            elog(LOG, "Postmaster has died, exiting.");
            /*
                If WL_POSTMASTER_DEATH is detected, PostgreSQL is shutting down.
                The worker logs a shutdown message and calls proc_exit(1); to exit safely.
            */
            proc_exit(1);
        }

        /* Acquire shared memory lock */
        // check out AddinShmemInitLock
        // create my own lock for this implementation
        if (shared_memory != NULL) {
            LWLockAcquire(shared_memory->lock, LW_EXCLUSIVE);
            if (shared_memory->status == FACT_FREE) {
                /* No new job, continue waiting */  
                LWLockRelease(shared_memory->lock);
                continue;
            } else if (shared_memory->status == FACT_PROCESSING) {
                /* Perform factorial computation */
                shared_memory->result = factorial(shared_memory->num);
                shared_memory->status = FACT_DONE;
            }
        }

        /* Release lock and loop again */
        LWLockRelease(shared_memory->lock);
    }

    elog(LOG, "Factorial background worker exiting.");
    proc_exit(0);
}

PG_FUNCTION_INFO_V1(compute_factorial);


Datum
compute_factorial(PG_FUNCTION_ARGS) {
    if (PG_ARGISNULL(0)) {
        elog(ERROR, "NULL input is not allowed");
        PG_RETURN_NULL();
    }
    int64 input = PG_GETARG_INT64(0);  // Get the input number from the SQL call
    // check for NULL input using existing api 
    if(shared_memory != NULL && input>=0){
        // Set the input number in shared memory and signal background worker to start processing
        if(input>20){ // change the hardcoded value
            elog(ERROR, "Integer overflow");
        }else{
            // replace AddinShmemInitLock with custom lock
            // set the input when it is free    
            LWLockAcquire(shared_memory->lock, LW_EXCLUSIVE);
            if(shared_memory->status == FACT_FREE){
                shared_memory->num = input;
                shared_memory->status = FACT_PROCESSING;  // Signal the worker to start processing
                LWLockRelease(shared_memory->lock);
            }else{
                while(shared_memory->status != FACT_FREE){
                    LWLockRelease(shared_memory->lock);
                    pg_usleep(1000000);
                    LWLockAcquire(shared_memory->lock, LW_EXCLUSIVE);
                }
                shared_memory->num = input;
                shared_memory->status = FACT_PROCESSING;
                LWLockRelease(shared_memory->lock);
            }
        
            // Wait for the worker to complete the computation (polling shared memory status)
            int attempts = 10; // 10 seconds max
            while (attempts-- > 0) {
                LWLockAcquire(shared_memory->lock, LW_EXCLUSIVE);
                if (shared_memory->status == FACT_DONE) {
                    __uint128_t result = shared_memory->result;
                    shared_memory->status = FACT_FREE;
                    LWLockRelease(shared_memory->lock);
                    // elog(LOG, "retrieved result: %lld and released lock", result);
                    attempts = 10;
                    
                    Numeric num_result;
                    num_result = int64_to_numeric(result); // Convert integer to NUMERIC

                    PG_RETURN_NUMERIC(num_result);
                    break;
                }
                LWLockRelease(shared_memory->lock);
                pg_usleep(1000000);
            }
            elog(ERROR, "Factorial computation timed out");
        }
    }else if(shared_memory != NULL && input<0){
        elog(ERROR, "Negative numbers are not allowed");
    } 
    // else if(input == NULL){
    //     elog(ERROR, "NUll input");
    // }
    else{
        elog(ERROR, "shared memory is null, null pointer dereference");
    }
}

/* Function to initialize shared memory */
static void factorial_shmem_startup(void) {
    bool found;

    /* Acquire the lock safely */
    LWLockAcquire(AddinShmemInitLock, LW_EXCLUSIVE);

    /* Initialize shared memory */
    shared_memory = (FactorialSharedMemory*)ShmemInitStruct(
        FACTORIAL_STRUCT, sizeof(FactorialSharedMemory), &found);

    if (!shared_memory) {
        elog(ERROR, "Failed to allocate shared memory");
    }
    
    if (!found) {
        shared_memory->num = 0;
        shared_memory->result = 0;
        shared_memory->status = FACT_FREE;
        LWLockPadded *tranche = GetNamedLWLockTranche(FACTORIAL_LOCK_TRANCH); 
        shared_memory->lock = &tranche[0].lock;
    }
    LWLockRelease(AddinShmemInitLock);

    /* Call previous startup hook (if any) */
    if (prev_shmem_startup_hook)
        prev_shmem_startup_hook();
}
// It ensures that only one process initializes the shared memory structures, avoiding race conditions 
// when multiple backends (connections) try to access the shared memory simultaneously during startup.

void _PG_init(void) {
    bool found;

    // /* Request shared memory */
    RequestAddinShmemSpace(sizeof(FactorialSharedMemory));
    RequestNamedLWLockTranche(FACTORIAL_LOCK_TRANCH, 1);
    prev_shmem_startup_hook = shmem_startup_hook;
    shmem_startup_hook = factorial_shmem_startup;

    /* Register background worker */
    BackgroundWorker worker;
    memset(&worker, 0, sizeof(BackgroundWorker));

    snprintf(worker.bgw_name,  BGW_MAXLEN,"factorial_worker");
    snprintf(worker.bgw_function_name,  BGW_MAXLEN,"fact_bg_worker_main");
    snprintf(worker.bgw_library_name,  BGW_MAXLEN,"factorial_bg_worker");
    
    worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
    // worker.bgw_start_time = BgWorkerStart_PostmasterStart;
    worker.bgw_restart_time = 10;
    worker.bgw_main_arg = Int32GetDatum(0);
    worker.bgw_flags = BGWORKER_SHMEM_ACCESS;

    RegisterBackgroundWorker(&worker);
}
