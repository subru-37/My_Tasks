DROP TYPE IF EXISTS metadata_result CASCADE;

CREATE TYPE metadata_result AS (
    table_name TEXT,
    key TEXT,
    value TEXT
);

CREATE OR REPLACE FUNCTION metadata_handler(table_name text , column1 text , column2 text , column3 text ) 
    RETURNS metadata_result AS
    'MODULE_PATHNAME', 'metadata_handler'
LANGUAGE C STRICT;
-- STRICT

