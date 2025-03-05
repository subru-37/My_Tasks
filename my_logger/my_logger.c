#include "postgres.h"
#include "commands/event_trigger.h"
#include "tcop/utility.h"
#include "executor/executor.h"

PG_MODULE_MAGIC;

// documendation + basics
// ProcessUtility, SharedMemory hook, planner and executor hooks
// Functions for factorial, 
// Postgres background workers
// New extension: background worker -> factorial function i/o -> client
// control and sql files
// add regression tests for the new extension

// Hooks
static ProcessUtility_hook_type prev_ProcessUtility = NULL;
static ExecutorStart_hook_type prev_ExecutorStart = NULL;

// Function to log DDL queries
void UtilityFnLogger(PlannedStmt *pstmt, const char *queryString, bool readOnlyTree,
                 ProcessUtilityContext context, ParamListInfo params,
                 QueryEnvironment *queryEnv, DestReceiver *dest, QueryCompletion *qc)
{
    char myEnvironment[50] = "";
    switch(context){
        case 0:
            strncpy(myEnvironment, "Top Level", sizeof(myEnvironment) - 1);
            myEnvironment[sizeof(myEnvironment) - 1] = '\0';
            break;
        case 1:
            strncpy(myEnvironment, "Complete Query - Atomic", sizeof(myEnvironment) - 1);
            myEnvironment[sizeof(myEnvironment) - 1] = '\0';
            break;
        case 2: 
            strncpy(myEnvironment, "Complete Query - Non Atomic", sizeof(myEnvironment) - 1);
            myEnvironment[sizeof(myEnvironment) - 1] = '\0';
            break;
        case 3:
            strncpy(myEnvironment, "SubCommand", sizeof(myEnvironment) - 1);
            myEnvironment[sizeof(myEnvironment) - 1] = '\0';
            break;
        default: 
            strncpy(myEnvironment, "Unknown", sizeof(myEnvironment) - 1);
            myEnvironment[sizeof(myEnvironment) - 1] = '\0';
            break;

            
    }

    elog(LOG, "Environment: %s, Executing Utility Function: %s", myEnvironment, queryString);
    
    if (prev_ProcessUtility)
        prev_ProcessUtility(pstmt, queryString, readOnlyTree, context, params, queryEnv, dest, qc);
    else
        standard_ProcessUtility(pstmt, queryString, readOnlyTree, context, params, queryEnv, dest, qc);
}

// Function to log DML queries
void DMLlogger(QueryDesc *queryDesc, int eflags)
{
    elog(LOG, "Executing DML Query: %s", queryDesc->sourceText);

    if (prev_ExecutorStart)
        prev_ExecutorStart(queryDesc, eflags);
    else
        standard_ExecutorStart(queryDesc, eflags);
}

// Initialize hooks
void _PG_init(void)
{
    prev_ProcessUtility = ProcessUtility_hook;
    ProcessUtility_hook = UtilityFnLogger;

    prev_ExecutorStart = ExecutorStart_hook;
    ExecutorStart_hook = DMLlogger;
}

// Cleanup hooks on unload
void _PG_fini(void)
{
    if (ProcessUtility_hook == UtilityFnLogger)
        ProcessUtility_hook = prev_ProcessUtility;

    if (ExecutorStart_hook == DMLlogger)
        ExecutorStart_hook = prev_ExecutorStart;
}
