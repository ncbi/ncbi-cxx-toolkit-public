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
-- File Description: NetStorage server DB SP to update from version 3 to 4
--


-- NB: before applying on a server you need to do two changes:
--     - change the DB name
--     - change the guard condition


-- Changes in the data:
-- - The Versions table sp_code record updated with a description
-- Changes in stored procedures:
-- - UpdateObjectOnWrite
-- - UpdateUserKeyObjectOnWrite
-- - UpdateObjectOnRead
-- - UpdateObjectOnRelocate
-- The idea of the changes is to avoid having the GetNextObjectID in the same
-- transaction as the main table update





-- NB: Change the DB name when applying
USE [NETSTORAGE];
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
IF @db_version != 2
BEGIN
    RAISERROR( 'Unexpected database structure version. Expected version is 2.', 11, 1 )
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
IF @sp_version != 3
BEGIN
    RAISERROR( 'Unexpected stored procedure version. Expected version is 3.', 11, 1 )
    SET NOEXEC ON
END
GO





-- Finally, the DB and SP versions are checked, so we can continue



-- Newly introduced procedure for the internal usage. It is not supposed to be
-- executed from the C++ code
CREATE PROCEDURE UpdateObjectOnWrite_IfNotExists
    @object_key                 VARCHAR(256),
    @object_loc                 VARCHAR(256),
    @object_size                BIGINT,
    @client_id                  BIGINT,
    @object_exp_if_found        DATETIME,
    @object_exp_if_not_found    DATETIME,
    @current_time               DATETIME
AS
BEGIN
    -- GetNextObjectID() Must not be within a transaction;
    -- The SP has its own transaction
    DECLARE @object_id  BIGINT
    DECLARE @ret_code   BIGINT
    EXECUTE @ret_code = GetNextObjectID @object_id OUTPUT

    IF @ret_code != 0
    BEGIN
        RETURN 1
    END

    BEGIN TRANSACTION
    BEGIN TRY
        DECLARE @row_count  INT
        DECLARE @error      INT

        -- Need to try to update once again because the initial transaction was
        -- commited and so someone else could have been able to insert a record
        -- into the Objects table while the next object ID was requested
        UPDATE Objects SET size = @object_size, tm_write = @current_time,
                           tm_expiration = @object_exp_if_found,
                           write_count = write_count + 1 WHERE object_key = @object_key
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR

        IF @error != 0
        BEGIN
            ROLLBACK TRANSACTION
            RETURN 1
        END

        IF @row_count = 0       -- object is not found; create it
        BEGIN
            INSERT INTO Objects (object_id, object_key, object_loc, client_id, tm_write, tm_expiration, size, read_count, write_count)
            VALUES (@object_id, @object_key, @object_loc, @client_id, @current_time, @object_exp_if_not_found, @object_size, 0, 1)
            SELECT @row_count = @@ROWCOUNT, @error = @@ERROR

            IF @error != 0 OR @row_count = 0
            BEGIN
                ROLLBACK TRANSACTION
                RETURN 1
            END
        END
    END TRY
    BEGIN CATCH
        ROLLBACK TRANSACTION

        DECLARE @error_message  NVARCHAR(4000) = ERROR_MESSAGE()
        DECLARE @error_severity INT = ERROR_SEVERITY()
        DECLARE @error_state    INT = ERROR_STATE()

        RAISERROR( @error_message, @error_severity, @error_state )
        RETURN 1
    END CATCH

    COMMIT TRANSACTION
    RETURN 0
END
GO



ALTER PROCEDURE UpdateObjectOnWrite
    @object_key                 VARCHAR(256),
    @object_loc                 VARCHAR(256),
    @object_size                BIGINT,
    @client_id                  BIGINT,
    @object_exp_if_found        DATETIME,
    @object_exp_if_not_found    DATETIME,
    @current_time               DATETIME
AS
BEGIN
    BEGIN TRANSACTION
    BEGIN TRY

        DECLARE @row_count  INT
        DECLARE @error      INT

        UPDATE Objects SET size = @object_size, tm_write = @current_time,
                           tm_expiration = @object_exp_if_found,
                           write_count = write_count + 1 WHERE object_key = @object_key
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR

        IF @error != 0
        BEGIN
            ROLLBACK TRANSACTION
            RETURN 1
        END

        IF @row_count = 1   -- The object has been found and updated
        BEGIN
            COMMIT TRANSACTION
            RETURN 0
        END
    END TRY
    BEGIN CATCH
        ROLLBACK TRANSACTION

        DECLARE @error_message  NVARCHAR(4000) = ERROR_MESSAGE()
        DECLARE @error_severity INT = ERROR_SEVERITY()
        DECLARE @error_state    INT = ERROR_STATE()

        RAISERROR( @error_message, @error_severity, @error_state )
        RETURN 1
    END CATCH

    -- Here: the update did not change a record because it does not exist
    -- So, commit the transaction and create the record
    COMMIT TRANSACTION

    DECLARE @ret_code   BIGINT
    EXECUTE @ret_code = UpdateObjectOnWrite_IfNotExists @object_key, @object_loc, @object_size, @client_id, @object_exp_if_found, @object_exp_if_not_found, @current_time

    RETURN @ret_code
END
GO




-- Newly introduced procedure for the internal usage. It is not supposed to be
-- executed from the C++ code
CREATE PROCEDURE UpdateUserKeyObjectOnWrite_IfNotExists
    @object_key                 VARCHAR(256),
    @object_loc                 VARCHAR(256),
    @object_size                BIGINT,
    @client_id                  BIGINT,
    @object_exp_if_found        DATETIME,
    @object_exp_if_not_found    DATETIME,
    @current_time               DATETIME
AS
BEGIN
    -- GetNextObjectID() Must not be within a transaction;
    -- The SP has its own transaction
    DECLARE @object_id  BIGINT
    DECLARE @ret_code   BIGINT
    EXECUTE @ret_code = GetNextObjectID @object_id OUTPUT

    IF @ret_code != 0
    BEGIN
        RETURN 1
    END

    BEGIN TRANSACTION
    BEGIN TRY
        DECLARE @row_count      INT
        DECLARE @error          INT

        -- See the more detailed description in the UpdateUserKeyObjectOnWrite() procedure

        -- Need to try to update once again because the initial transaction was
        -- commited and so someone else could have been able to insert a record
        -- into the Objects table while the next object ID was requested
        UPDATE Objects SET size = @object_size,
                           tm_write = @current_time,
                           write_count = ( CASE
                                                WHEN tm_expiration IS NOT NULL AND tm_expiration < @current_time THEN 1
                                                ELSE write_count + 1
                                           END ),
                           read_count = ( CASE
                                                WHEN tm_expiration IS NOT NULL AND tm_expiration < @current_time THEN 0
                                                ELSE read_count
                                          END ),
                           tm_expiration = ( CASE
                                                WHEN tm_expiration IS NOT NULL AND tm_expiration < @current_time THEN @object_exp_if_not_found
                                                WHEN tm_expiration IS NOT NULL AND tm_expiration >= @current_time THEN @object_exp_if_found
                                                ELSE tm_expiration
                                             END )
                       WHERE object_key = @object_key
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR

        IF @error != 0
        BEGIN
            ROLLBACK TRANSACTION
            RETURN 1
        END

        IF @row_count = 0       -- object is not found; create it
        BEGIN
            -- For the user key objects creation time is set as well
            INSERT INTO Objects (object_id, object_key, object_loc, client_id, tm_create, tm_write, tm_expiration, size, read_count, write_count)
            VALUES (@object_id, @object_key, @object_loc, @client_id, @current_time, @current_time, @object_exp_if_not_found, @object_size, 0, 1)
            SELECT @row_count = @@ROWCOUNT, @error = @@ERROR

            IF @error != 0 OR @row_count = 0
            BEGIN
                ROLLBACK TRANSACTION
                RETURN 1
            END
        END
    END TRY
    BEGIN CATCH
        ROLLBACK TRANSACTION

        DECLARE @error_message  NVARCHAR(4000) = ERROR_MESSAGE()
        DECLARE @error_severity INT = ERROR_SEVERITY()
        DECLARE @error_state    INT = ERROR_STATE()

        RAISERROR( @error_message, @error_severity, @error_state )
        RETURN 1
    END CATCH

    COMMIT TRANSACTION
    RETURN 0
END
GO


ALTER PROCEDURE UpdateUserKeyObjectOnWrite
    @object_key                 VARCHAR(256),
    @object_loc                 VARCHAR(256),
    @object_size                BIGINT,
    @client_id                  BIGINT,
    @object_exp_if_found        DATETIME,
    @object_exp_if_not_found    DATETIME,
    @current_time               DATETIME
AS
BEGIN
    BEGIN TRANSACTION
    BEGIN TRY
        DECLARE @row_count      INT
        DECLARE @error          INT

        -- If the record exists then the fields to update (and their values) depend
        -- on the expiration value. The UPDATE statement below includes all these cases
        -- in the CASE ... WHEN ... END clauses. This is done to improve the performance.
        -- The initial version of the procedure did the same via 3 scans: UPDATE/SELECT/UPDATE
        -- Now it is (less clear though) just one scan in the case of the existing records.
        UPDATE Objects SET size = @object_size,
                           tm_write = @current_time,
                           write_count = ( CASE
                                                WHEN tm_expiration IS NOT NULL AND tm_expiration < @current_time THEN 1
                                                ELSE write_count + 1
                                           END ),
                           read_count = ( CASE
                                                WHEN tm_expiration IS NOT NULL AND tm_expiration < @current_time THEN 0
                                                ELSE read_count
                                          END ),
                           tm_expiration = ( CASE
                                                WHEN tm_expiration IS NOT NULL AND tm_expiration < @current_time THEN @object_exp_if_not_found
                                                WHEN tm_expiration IS NOT NULL AND tm_expiration >= @current_time THEN @object_exp_if_found
                                                ELSE tm_expiration
                                             END )
                       WHERE object_key = @object_key
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR

        IF @error != 0
        BEGIN
            ROLLBACK TRANSACTION
            RETURN 1
        END

        IF @row_count = 1       -- The object has been found and updated
        BEGIN
            COMMIT TRANSACTION
            RETURN 0
        END
    END TRY
    BEGIN CATCH
        ROLLBACK TRANSACTION

        DECLARE @error_message  NVARCHAR(4000) = ERROR_MESSAGE()
        DECLARE @error_severity INT = ERROR_SEVERITY()
        DECLARE @error_state    INT = ERROR_STATE()

        RAISERROR( @error_message, @error_severity, @error_state )
        RETURN 1
    END CATCH

    -- Here: the update did not change a record because it does not exist
    -- So, commit the transaction and create the record
    COMMIT TRANSACTION

    DECLARE @ret_code   BIGINT
    EXECUTE @ret_code = UpdateUserKeyObjectOnWrite_IfNotExists @object_key, @object_loc, @object_size, @client_id, @object_exp_if_found, @object_exp_if_not_found, @current_time

    RETURN @ret_code
END
GO



-- Newly introduced procedure for the internal usage. It is not supposed to be
-- executed from the C++ code
CREATE PROCEDURE UpdateObjectOnRead_IfNotExists
    @object_key                 VARCHAR(256),
    @object_loc                 VARCHAR(256),
    @object_size                BIGINT,
    @client_id                  BIGINT,
    @object_exp_if_found        DATETIME,
    @object_exp_if_not_found    DATETIME,
    @current_time               DATETIME
AS
BEGIN
    -- GetNextObjectID() Must not be within a transaction;
    -- The SP has its own transaction
    DECLARE @object_id  BIGINT
    DECLARE @ret_code   BIGINT
    EXECUTE @ret_code = GetNextObjectID @object_id OUTPUT

    IF @ret_code != 0
    BEGIN
        RETURN 1
    END

    BEGIN TRANSACTION
    BEGIN TRY
        DECLARE @row_count  INT
        DECLARE @error      INT

        UPDATE Objects SET tm_read = @current_time,
                           tm_expiration = @object_exp_if_found,
                           read_count = read_count + 1 WHERE object_key = @object_key
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR

        IF @error != 0
        BEGIN
            ROLLBACK TRANSACTION
            RETURN 1
        END

        IF @row_count = 0       -- object is not found; create it
        BEGIN
            INSERT INTO Objects (object_id, object_key, object_loc, client_id, tm_read, tm_expiration, size, read_count, write_count)
            VALUES (@object_id, @object_key, @object_loc, @client_id, @current_time, @object_exp_if_not_found, @object_size, 1, 1)
            SELECT @row_count = @@ROWCOUNT, @error = @@ERROR

            IF @error != 0 OR @row_count = 0
            BEGIN
                ROLLBACK TRANSACTION
                RETURN 1
            END
        END
    END TRY
    BEGIN CATCH
        ROLLBACK TRANSACTION

        DECLARE @error_message  NVARCHAR(4000) = ERROR_MESSAGE()
        DECLARE @error_severity INT = ERROR_SEVERITY()
        DECLARE @error_state    INT = ERROR_STATE()

        RAISERROR( @error_message, @error_severity, @error_state )
        RETURN 1
    END CATCH

    COMMIT TRANSACTION
    RETURN 0
END
GO



ALTER PROCEDURE UpdateObjectOnRead
    @object_key                 VARCHAR(256),
    @object_loc                 VARCHAR(256),
    @object_size                BIGINT,
    @client_id                  BIGINT,
    @object_exp_if_found        DATETIME,
    @object_exp_if_not_found    DATETIME,
    @current_time               DATETIME
AS
BEGIN
    BEGIN TRANSACTION
    BEGIN TRY
        DECLARE @row_count  INT
        DECLARE @error      INT

        UPDATE Objects SET tm_read = @current_time,
                           tm_expiration = @object_exp_if_found,
                           read_count = read_count + 1 WHERE object_key = @object_key
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR

        IF @error != 0
        BEGIN
            ROLLBACK TRANSACTION
            RETURN 1
        END

        IF @row_count = 1       -- The object has been found and updated
        BEGIN
            COMMIT TRANSACTION
            RETURN 0
        END
    END TRY
    BEGIN CATCH
        ROLLBACK TRANSACTION

        DECLARE @error_message  NVARCHAR(4000) = ERROR_MESSAGE()
        DECLARE @error_severity INT = ERROR_SEVERITY()
        DECLARE @error_state    INT = ERROR_STATE()

        RAISERROR( @error_message, @error_severity, @error_state )
        RETURN 1
    END CATCH

    -- Here: the update did not change a record because it does not exist
    -- So, commit the transaction and create the record
    COMMIT TRANSACTION

    DECLARE @ret_code   BIGINT
    EXECUTE @ret_code = UpdateObjectOnRead_IfNotExists @object_key, @object_loc, @object_size, @client_id, @object_exp_if_found, @object_exp_if_not_found, @current_time

    RETURN @ret_code
END
GO



-- Newly introduced procedure for the internal usage. It is not supposed to be
-- executed from the C++ code
CREATE PROCEDURE UpdateObjectOnRelocate_IfNotExists
    @object_key     VARCHAR(256),
    @object_loc     VARCHAR(256),
    @client_id      BIGINT
AS
BEGIN
    -- GetNextObjectID() Must not be within a transaction;
    -- The SP has its own transaction
    DECLARE @object_id  BIGINT
    DECLARE @ret_code   BIGINT
    EXECUTE @ret_code = GetNextObjectID @object_id OUTPUT

    IF @ret_code != 0
    BEGIN
        RETURN 1
    END

    BEGIN TRANSACTION
    BEGIN TRY
        DECLARE @row_count  INT
        DECLARE @error      INT

        UPDATE Objects SET object_loc = @object_loc WHERE object_key = @object_key
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR

        IF @error != 0
        BEGIN
            ROLLBACK TRANSACTION
            RETURN 1
        END

        IF @row_count = 0       -- object is not found; create it
        BEGIN
            INSERT INTO Objects (object_id, object_key, object_loc, client_id, tm_write)
            VALUES (@object_id, @object_key, @object_loc, @client_id, GETDATE())
            SELECT @row_count = @@ROWCOUNT, @error = @@ERROR

            IF @error != 0 OR @row_count = 0
            BEGIN
                ROLLBACK TRANSACTION
                RETURN 1
            END
        END
    END TRY
    BEGIN CATCH
        ROLLBACK TRANSACTION

        DECLARE @error_message  NVARCHAR(4000) = ERROR_MESSAGE()
        DECLARE @error_severity INT = ERROR_SEVERITY()
        DECLARE @error_state    INT = ERROR_STATE()

        RAISERROR( @error_message, @error_severity, @error_state )
        RETURN 1
    END CATCH

    COMMIT TRANSACTION
    RETURN 0
END
GO


ALTER PROCEDURE UpdateObjectOnRelocate
    @object_key     VARCHAR(256),
    @object_loc     VARCHAR(256),
    @client_id      BIGINT
AS
BEGIN
    BEGIN TRANSACTION
    BEGIN TRY
        DECLARE @row_count  INT
        DECLARE @error      INT

        UPDATE Objects SET object_loc = @object_loc WHERE object_key = @object_key
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR

        IF @error != 0
        BEGIN
            ROLLBACK TRANSACTION
            RETURN 1
        END

        IF @row_count = 1       -- The object has been found and updated
        BEGIN
            COMMIT TRANSACTION
            RETURN 0
        END
    END TRY
    BEGIN CATCH
        ROLLBACK TRANSACTION

        DECLARE @error_message  NVARCHAR(4000) = ERROR_MESSAGE()
        DECLARE @error_severity INT = ERROR_SEVERITY()
        DECLARE @error_state    INT = ERROR_STATE()

        RAISERROR( @error_message, @error_severity, @error_state )
        RETURN 1
    END CATCH

    -- Here: the update did not change a record because it does not exist
    -- So, commit the transaction and create the record
    COMMIT TRANSACTION

    DECLARE @ret_code   BIGINT
    EXECUTE @ret_code = UpdateObjectOnRelocate_IfNotExists @object_key, @object_loc, @client_id

    RETURN @ret_code
END
GO





-- The database structure has not changed.
-- However a new component has been introduced: the sp_code
DECLARE @row_count  INT
DECLARE @error      INT
UPDATE Versions SET description = 'Stored procedures version' WHERE name = 'sp_code'
SELECT @row_count = @@ROWCOUNT, @error = @@ERROR
IF @error != 0 OR @row_count = 0
BEGIN
    RAISERROR( 'Cannot update the stored procedure version record description in the Versions DB table', 11, 1 )
END
GO


DECLARE @row_count  INT
DECLARE @error      INT
UPDATE Versions SET version = 4 WHERE name = 'sp_code'
SELECT @row_count = @@ROWCOUNT, @error = @@ERROR
IF @error != 0 OR @row_count = 0
BEGIN
    RAISERROR( 'Cannot update the stored procedure version in the Versions DB table', 11, 1 )
END
GO


-- Restore if it was changed
SET NOEXEC OFF
GO


