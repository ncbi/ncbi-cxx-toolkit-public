USE [DBAPI_Sample];
GO
-- Recommended settings during procedure creation:
SET ANSI_NULLS ON;
SET QUOTED_IDENTIFIER ON;
GO

/*
 *  The "if proc doesn't exist create fake proc endif, alter proc" approach
 *  below has an advantage over the "drop, create" approach.  Specifically,
 *  when the proc already exists, its properties (creation date, permissions,
 *  etc) are not lost.  However, you may need to drop and re-create if either
 *  ANSI_NULLS or QUOTED_IDENTIFIER is not set correctly because those can
 *  only be set during the stored procedure creation.
 */
IF OBJECT_ID('[dbapi_simple_sproc]', 'P') IS NULL
    EXEC('CREATE PROCEDURE [dbapi_simple_sproc] AS RAISERROR(''Incomplete.'', 11, 1);');
GO
ALTER PROCEDURE [dbapi_simple_sproc]
    @max_id INT,
    @max_fl FLOAT,
    @num_rows INT OUTPUT
AS
BEGIN
    /*
     *  This stored procedure is referred to by the NCBI C++ Toolkit
     *  documentation and sample programs.  Please don't drop or alter it.
     */

    -- Recommended settings during procedure execution:
    SET ANSI_PADDING ON;
    SET ANSI_WARNINGS ON;
    SET ARITHABORT ON;
    SET CONCAT_NULL_YIELDS_NULL ON;
    SET NUMERIC_ROUNDABORT OFF;

    -- Select data per passed-in parameters.
    SELECT int_val, fl_val
        FROM SelectSample
        WHERE int_val <= @max_id AND fl_val <= @max_fl;

    -- Assign an output parameter.
    SET @num_rows = @@ROWCOUNT;

    -- Select some more data.
    SELECT 'abc' AS col1, 123 AS col2;
    SELECT @max_id AS max_id, @max_fl AS max_fl, @num_rows AS num_rows;

    -- Generate a simple message and a warning.
    PRINT 'Print something.';
    RAISERROR('Generate a warning.', 1, 1);

    -- Generate an exception.
    RAISERROR('Generate an exception.', 13, 17);

    -- Caller may not retrieve this if the exception is caught and handled.
    SELECT 'def' AS col1, 456 AS col2;

    RETURN 0;
END
GO
