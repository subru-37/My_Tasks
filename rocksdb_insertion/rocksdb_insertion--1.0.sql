DROP TYPE IF EXISTS metadata_result CASCADE;

CREATE TYPE metadata_result AS (
    table_name TEXT,
    key TEXT,
    value TEXT
);
CREATE TABLE IF NOT EXISTS metadata (
        id UUID DEFAULT gen_random_uuid() PRIMARY KEY NOT NULL,
        table_oid BIGINT NOT NULL,
        table_name TEXT NOT NULL,
        column_name TEXT NOT NULL,
        data_type TEXT NOT NULL, 
        key_or_value TEXT NOT NULL, UNIQUE(table_name, column_name));
        
CREATE OR REPLACE FUNCTION metadata_handler(table_name text , column1 text , column2 text , column3 text ) 
    RETURNS metadata_result AS
    'MODULE_PATHNAME', 'metadata_handler'
LANGUAGE C STRICT;
-- STRICT

