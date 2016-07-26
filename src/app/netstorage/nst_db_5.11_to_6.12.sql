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
-- File Description: NetStorage server DB SP to update from version 5.11 to 6.12
--


-- NB: before applying on a server you need to do two changes:
--     - change the DB name
--     - change the guard condition

-- Changes in the DB structure:
-- - dropped table ObjectIdGen
-- - created sequence ObjectIdGenerator

-- Changes in stored procedures:
-- - GetNextObjectID: use a generator instead of a table
-- - A bunch of stored procedures is dropped due to there is no need to separate
--   transactions for the ObjectIdGen table:
--   - UpdateObjectOnRead_IfNotExists
--   - UpdateObjectOnRelocate_IfNotExists
--   - UpdateObjectOnWrite_IfNotExists
--   - UpdateUserKeyObjectOnWrite_IfNotExists
-- - Correspondigly a few stored procedures are simplified:
--   - UpdateObjectOnRead
--   - UpdateObjectOnRelocate
--   - UpdateObjectOnWrite
--   - UpdateUserKeyObjectOnWrite
-- - AddAttribute: direct use of a generator
-- - SetObjectExpiration: direct use of a generator





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
IF @db_version != 5
BEGIN
    RAISERROR( 'Unexpected database structure version. Expected version is 5.', 11, 1 )
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
IF @sp_version != 11
BEGIN
    RAISERROR( 'Unexpected stored procedure version. Expected version is 11.', 11, 1 )
    SET NOEXEC ON
END
GO


-- Finally, the DB and SP versions are checked, so we can continue




CREATE SEQUENCE ObjectIdGenerator
    AS BIGINT
    START WITH  40000000
    CYCLE;
GO



ALTER PROCEDURE GetNextObjectID
    @next_id     BIGINT OUT,
    @count       BIGINT = 1
AS
BEGIN
    IF @count = 1
    BEGIN
        SET @next_id = NEXT VALUE FOR ObjectIdGenerator;
    END
    ELSE
    BEGIN
        DECLARE @next_id_as_variant     SQL_VARIANT;

        EXECUTE sp_sequence_get_range @sequence_name = N'ObjectIdGenerator',
                                      @range_size = @count,
                                      @range_first_value = @next_id_as_variant OUTPUT;
        SET @next_id = CONVERT(BIGINT, @next_id_as_variant);
    END
    RETURN 0;
END
GO



DROP TABLE ObjectIdGen;
GO





ALTER PROCEDURE UpdateObjectOnRead
    @object_key                 VARCHAR(289),
    @object_loc                 VARCHAR(900),
    @object_size                BIGINT,
    @client_id                  BIGINT,
    @object_exp_if_found        DATETIME,
    @object_exp_if_not_found    DATETIME,
    @current_time               DATETIME,

    -- make it backward compatible
    @size_was_null              INT = 0 OUT
AS
BEGIN
    BEGIN TRANSACTION
    BEGIN TRY
        DECLARE @row_count          INT;
        DECLARE @error              INT;
        DECLARE @old_object_size    BIGINT;

        UPDATE Objects SET tm_read = @current_time,
                           tm_expiration = @object_exp_if_found,
                           read_count = read_count + 1,
                           @old_object_size = size,
                           size = @object_size
                           WHERE object_key = @object_key;
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;

        IF @error != 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN 1;
        END

        IF @row_count = 1       -- The object has been found and updated
        BEGIN
            -- the record existed so the size could have been not null
            IF @old_object_size IS NULL
            BEGIN
                SET @size_was_null = 1;
            END
            ELSE
            BEGIN
                SET @size_was_null = 0;
            END

            COMMIT TRANSACTION;
            RETURN 0;
        END

        -- Here: the update did not change a record because it does not exist
        INSERT INTO Objects (object_id, object_key, object_loc,
                             client_id, tm_read, tm_expiration,
                             size, read_count, write_count,
                             user_id)
        VALUES (NEXT VALUE FOR ObjectIdGenerator, @object_key, @object_loc,
                @client_id, @current_time, @object_exp_if_not_found,
                @object_size, 1, 1,
                NULL);
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;

        IF @error != 0 OR @row_count = 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN 1;
        END

        -- The record was created with non-null size
        SET @size_was_null = 1;

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


DROP PROCEDURE UpdateObjectOnRead_IfNotExists;
GO




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
                             client_id, tm_expiration)
        VALUES (NEXT VALUE FOR ObjectIdGenerator, @object_key, @object_loc,
                @client_id, @object_exp_if_not_found);
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


DROP PROCEDURE UpdateObjectOnRelocate_IfNotExists;
GO



ALTER PROCEDURE UpdateObjectOnWrite
    @object_key                 VARCHAR(289),
    @object_loc                 VARCHAR(900),
    @object_size                BIGINT,
    @client_id                  BIGINT,
    @object_exp_if_found        DATETIME,
    @object_exp_if_not_found    DATETIME,
    @current_time               DATETIME,

    -- make it backward compatible
    @user_id                    BIGINT = NULL,
    @size_was_null              INT = 0 OUT
AS
BEGIN
    BEGIN TRANSACTION
    BEGIN TRY

        DECLARE @row_count          INT;
        DECLARE @error              INT;
        DECLARE @old_object_size    BIGINT;

        UPDATE Objects SET tm_write = @current_time,
                           tm_expiration = @object_exp_if_found,
                           write_count = write_count + 1,
                           @old_object_size = size,
                           size = @object_size,
                           object_loc = @object_loc
                       WHERE object_key = @object_key;
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;

        IF @error != 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN 1;
        END

        IF @row_count = 1   -- The object has been found and updated
        BEGIN
            IF @old_object_size IS NULL
            BEGIN
                SET @size_was_null = 1;
            END
            ELSE
            BEGIN
                SET @size_was_null = 0;
            END

            COMMIT TRANSACTION;
            RETURN 0;
        END

        -- Here: the update did not change a record because it does not exist

        INSERT INTO Objects (object_id, object_key, object_loc,
                             client_id, tm_write, tm_expiration,
                             size, read_count, write_count,
                             user_id)
        VALUES (NEXT VALUE FOR ObjectIdGenerator, @object_key, @object_loc,
                @client_id, @current_time, @object_exp_if_not_found,
                @object_size, 0, 1,
                @user_id);
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;

        IF @error != 0 OR @row_count = 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN 1;
        END

        -- The record was created with non-null size
        SET @size_was_null = 1;

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


DROP PROCEDURE UpdateObjectOnWrite_IfNotExists;
GO



ALTER PROCEDURE UpdateUserKeyObjectOnWrite
    @object_key                 VARCHAR(289),
    @object_loc                 VARCHAR(900),
    @object_size                BIGINT,
    @client_id                  BIGINT,
    @object_exp_if_found        DATETIME,
    @object_exp_if_not_found    DATETIME,
    @current_time               DATETIME
AS
BEGIN
    BEGIN TRANSACTION
    BEGIN TRY
        DECLARE @row_count      INT;
        DECLARE @error          INT;

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
                       WHERE object_key = @object_key;
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;

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

        -- Here: the update did not change a record because it does not exist

        -- For the user key objects creation time is set as well
        INSERT INTO Objects (object_id, object_key, object_loc,
                             client_id, tm_create, tm_write,
                             tm_expiration, size, read_count,
                             write_count)
        VALUES (NEXT VALUE FOR ObjectIdGenerator, @object_key, @object_loc,
                @client_id, @current_time, @current_time,
                @object_exp_if_not_found, @object_size, 0,
                1);
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


DROP PROCEDURE UpdateUserKeyObjectOnWrite_IfNotExists;
GO




ALTER PROCEDURE AddAttribute
    @object_key             VARCHAR(289),
    @attr_name              VARCHAR(64),
    @attr_value             VARBINARY(900),
    @client_id              BIGINT,

    -- backward compatible

    @create_if_not_found    INT = 0,    -- to make it compatible with
                                        -- the previous implementation
    @object_expiration      DATETIME = NULL,
    @object_loc             VARCHAR(900) = NULL
AS
BEGIN
    DECLARE @object_id      BIGINT = NULL;
    DECLARE @attr_id        BIGINT = NULL;
    DECLARE @row_count      INT;
    DECLARE @error          INT;
    DECLARE @expiration     DATETIME;

    BEGIN TRANSACTION
    BEGIN TRY

        UPDATE Objects SET tm_attr_write = GETDATE(),
                           @object_id = object_id,
                           @expiration = tm_expiration
                       WHERE object_key = @object_key;
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;
        IF @error != 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN 1;
        END
        IF @row_count = 1 AND @expiration IS NOT NULL
        BEGIN
            IF @expiration < GETDATE()
            BEGIN
                ROLLBACK TRANSACTION;
                RETURN -4;          -- object is expired
                                    -- -4 is to make it unified with the other SPs
            END
        END
        IF @row_count = 0 AND @create_if_not_found = 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN -1;              -- object is not found
        END


        IF @row_count = 0
        BEGIN
            -- need to create an object
            SET @object_id = NEXT VALUE FOR ObjectIdGenerator;
            INSERT INTO Objects (object_id, object_key, object_loc,
                                 client_id, tm_expiration, read_count,
                                 write_count, tm_attr_write)
            VALUES (@object_id, @object_key, @object_loc,
                    @client_id, @object_expiration, 0,
                    0, GETDATE());
            SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;

            IF @error != 0 OR @row_count = 0
            BEGIN
                ROLLBACK TRANSACTION;
                RETURN 1;
            END
        END

        -- Get the attribute ID
        SELECT @attr_id = attr_id FROM Attributes WHERE name = @attr_name;
        IF @attr_id IS NULL
        BEGIN
            INSERT INTO Attributes (name) VALUES (@attr_name);
            SET @attr_id = SCOPE_IDENTITY();
        END

        -- Create or update the attribute
        UPDATE AttrValues SET value = @attr_value WHERE object_id = @object_id AND attr_id = @attr_id;
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;
        IF @error != 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN 1;
        END
        IF @row_count = 0
        BEGIN
            INSERT INTO AttrValues (object_id, attr_id, value) VALUES (@object_id, @attr_id, @attr_value);
            SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;
            IF @error != 0 OR @row_count != 1
            BEGIN
                ROLLBACK TRANSACTION;
                RETURN 1;
            END
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



ALTER PROCEDURE SetObjectExpiration
    @object_key             VARCHAR(289),
    @expiration             DATETIME,

    -- backward compatible

    @create_if_not_found    INT = 0,    -- to make it compatible with
                                        -- the previous implementation
    @object_loc             VARCHAR(900) = NULL,
    @client_id              BIGINT = NULL,
    @object_size            BIGINT = NULL OUT,
    @ttl                    BIGINT = NULL
AS
BEGIN
    BEGIN TRANSACTION
    BEGIN TRY
        DECLARE @row_count          INT;
        DECLARE @error              INT;
        DECLARE @old_expiration     DATETIME;


        -- Do not reduce the expiration
        -- Note: T-SQL lets to save the old value of a column
        UPDATE Objects SET @old_expiration = tm_expiration,
                           @object_size = size,
                           tm_expiration = @expiration,
                           ttl = ( CASE
                                        WHEN @ttl is NULL THEN ttl
                                        ELSE @ttl
                                   END )
                       WHERE object_key = @object_key;
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;
        IF @error != 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN 1;
        END

        IF @row_count = 1 AND @old_expiration IS NOT NULL
        BEGIN
            IF @old_expiration < GETDATE()
            BEGIN
                ROLLBACK TRANSACTION;
                RETURN -4;          -- object is expired
                                    -- -4 is to make it unified with the other SPs
            END
        END
        IF @row_count = 0 AND @create_if_not_found = 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN -1;              -- object is not found
        END


        IF @row_count = 0
        BEGIN
            -- need to create an object
            INSERT INTO Objects (object_id, object_key, object_loc,
                                 client_id, tm_expiration, read_count,
                                 write_count, ttl)
            VALUES (NEXT VALUE FOR ObjectIdGenerator, @object_key, @object_loc,
                    @client_id, @expiration, 0,
                    0, @ttl);
            SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;

            IF @error != 0 OR @row_count = 0
            BEGIN
                ROLLBACK TRANSACTION;
                RETURN 1;
            END

            -- object size is unknown here
            SET @object_size = NULL;

        END

        COMMIT TRANSACTION;
        RETURN 0;
    END TRY
    BEGIN CATCH
        ROLLBACK TRANSACTION;

        DECLARE @error_message NVARCHAR(4000) = ERROR_MESSAGE();
        DECLARE @error_severity INT = ERROR_SEVERITY();
        DECLARE @error_state INT = ERROR_STATE();

        RAISERROR( @error_message, @error_severity, @error_state );
        RETURN 1;
    END CATCH
END
GO






DECLARE @row_count  INT
DECLARE @error      INT
UPDATE Versions SET version = 12 WHERE name = 'sp_code'
SELECT @row_count = @@ROWCOUNT, @error = @@ERROR
IF @error != 0 OR @row_count = 0
BEGIN
    RAISERROR( 'Cannot update the stored procedure version in the Versions DB table', 11, 1 )
END
GO

DECLARE @row_count  INT
DECLARE @error      INT
UPDATE Versions SET version = 6 WHERE name = 'db_structure'
SELECT @row_count = @@ROWCOUNT, @error = @@ERROR
IF @error != 0 OR @row_count = 0
BEGIN
    RAISERROR( 'Cannot update the DB structure version in the Versions DB table', 11, 1 )
END
GO


-- Restore if it was changed
SET NOEXEC OFF
GO


