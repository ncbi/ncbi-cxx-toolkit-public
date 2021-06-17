--  $Id$
-- ===========================================================================
--
--                            PUBLIC DOMAIN NOTICE
--               National Center for Biotechnology Information
--
--  This software/database is a "United States Government Work" under the
--  terms of the United States Copyright Act.  It was written as part of
--  the author's official duties as a United States Government employee and
--  thus cannot be copyrighted.  This software/database is freely available
--  to the public for use. The National Library of Medicine and the U.S.
--  Government have not placed any restriction on its use or reproduction.
--
--  Although all reasonable efforts have been taken to ensure the accuracy
--  and reliability of the software and data, the NLM and the U.S.
--  Government do not and cannot warrant the performance or results that
--  may be obtained by using this software or data. The NLM and the U.S.
--  Government disclaim all warranties, express or implied, including
--  warranties of performance, merchantability or fitness for any particular
--  purpose.
--
--  Please cite the author in any work or product based on this material.
--
-- ===========================================================================
--
-- Authors:  Sergey Satskiy
--
-- File Description: NetStorage server DB SP to update from version 6.14 to
--                   6.15
--


-- NB: before applying on a server you need to do two changes:
--     - change the DB name
--     - change the guard condition

-- Changes in the DB structure:
-- - None

-- Changes in stored procedures:
-- - UpdateObjectOnRelocate:
--   bug fix: Cannot insert the value NULL into column 'read_count'





-- NB: Change the DB name when applying
USE [NETSTORAGE_DEV];
GO
-- Recommended settings during procedure creation:
SET ANSI_NULLS ON;
SET QUOTED_IDENTIFIER ON;
SET ANSI_PADDING ON;
SET ANSI_WARNINGS ON;
GO


-- NB: change it manually when applying
IF 1 = 1
BEGIN
    RAISERROR( 'Fix the condition manually before running the update script', 11, 1 )
    SET NOEXEC ON
END



DECLARE @db_version BIGINT = NULL
SELECT @db_version = version FROM Versions WHERE name = 'db_structure'
IF @@ERROR != 0
BEGIN
    RAISERROR( 'Error retrieving the database structure version', 11, 1 )
    SET NOEXEC ON
END
IF @db_version IS NULL
BEGIN
    RAISERROR( 'Database structure version is not found in the Versions table', 11, 1 )
    SET NOEXEC ON
END
IF @db_version != 6
BEGIN
    RAISERROR( 'Unexpected database structure version. Expected version is 6.', 11, 1 )
    SET NOEXEC ON
END
GO


DECLARE @sp_version BIGINT = NULL
SELECT @sp_version = version FROM Versions WHERE name = 'sp_code'
IF @@ERROR != 0
BEGIN
    RAISERROR( 'Error retrieving the stored procedures version', 11, 1 )
    SET NOEXEC ON
END
IF @sp_version IS NULL
BEGIN
    RAISERROR( 'Stored procedures version is not found in the Versions table', 11, 1 )
    SET NOEXEC ON
END
IF @sp_version != 14
BEGIN
    RAISERROR( 'Unexpected stored procedure version. Expected version is 14.', 11, 1 )
    SET NOEXEC ON
END
GO


-- Finally, the DB and SP versions are checked, so we can continue


ALTER PROCEDURE UpdateObjectOnRelocate
    @object_key                 VARCHAR(289),
    @object_loc                 VARCHAR(900),
    @client_id                  BIGINT,

    -- make it compatible with the previous version of the server
    @current_time               DATETIME = NULL,
    @object_exp_if_found        DATETIME = NULL,
    @object_exp_if_not_found    DATETIME = NULL
AS
BEGIN
    BEGIN TRANSACTION
    BEGIN TRY
        DECLARE @row_count  INT;
        DECLARE @error      INT;

        IF @current_time IS NULL
        BEGIN
            -- Old version of the server
            UPDATE Objects SET object_loc = @object_loc
                           WHERE object_key = @object_key;
            SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;
        END
        ELSE
        BEGIN
            -- New version of the server
            UPDATE Objects SET object_loc = @object_loc,
                               tm_expiration = @object_exp_if_found
                           WHERE object_key = @object_key;
            SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;
        END

        IF @error != 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN 1;
        END

        IF @row_count = 1       -- The object has been found and updated
        BEGIN
            COMMIT TRANSACTION;
            RETURN 0;
        END

        -- the update did not change a record because it does not exist
        INSERT INTO Objects (object_id, object_key, object_loc,
                             client_id, tm_expiration,
                             read_count, write_count)
        VALUES (NEXT VALUE FOR ObjectIdGenerator, @object_key, @object_loc,
                @client_id, @object_exp_if_not_found, 0, 0);
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;

        IF @error != 0 OR @row_count = 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN 1;
        END

        COMMIT TRANSACTION;
        RETURN 0;
    END TRY
    BEGIN CATCH
        ROLLBACK TRANSACTION;

        DECLARE @error_message  NVARCHAR(4000) = ERROR_MESSAGE();
        DECLARE @error_severity INT = ERROR_SEVERITY();
        DECLARE @error_state    INT = ERROR_STATE();

        RAISERROR( @error_message, @error_severity, @error_state );
        RETURN 1;
    END CATCH
END
GO




DECLARE @row_count  INT
DECLARE @error      INT
UPDATE Versions SET version = 15 WHERE name = 'sp_code'
SELECT @row_count = @@ROWCOUNT, @error = @@ERROR
IF @error != 0 OR @row_count = 0
BEGIN
    RAISERROR( 'Cannot update the stored procedure version in the Versions DB table', 11, 1 )
END
GO


-- Restore if it was changed
SET NOEXEC OFF
GO


