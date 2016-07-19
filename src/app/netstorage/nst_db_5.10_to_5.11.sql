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
-- File Description: NetStorage server DB SP to update from version 4 to 5
--


-- NB: before applying on a server you need to do two changes:
--     - change the DB name
--     - change the guard condition

-- Changes in the DB structure:
-- - none

-- Changes in stored procedures:
-- - GetObjectSizeAndLocator: newly introduced
-- - GetObjectFixedAttributes: improved - 1 scan in all cases




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
IF @sp_version != 10
BEGIN
    RAISERROR( 'Unexpected stored procedure version. Expected version is 10.', 11, 1 )
    SET NOEXEC ON
END
GO


-- Finally, the DB and SP versions are checked, so we can continue




CREATE PROCEDURE GetObjectSizeAndLocator
    @object_key         VARCHAR(289),
    @object_size        BIGINT OUT,
    @object_locator     VARCHAR(900) OUT
AS
BEGIN
    DECLARE @row_count      INT;
    DECLARE @error          INT;
    DECLARE @expiration     DATETIME;

    BEGIN TRANSACTION
    BEGIN TRY
        SELECT @expiration = tm_expiration, @object_size = size, @object_locator = object_loc
        FROM Objects WHERE object_key = @object_key;
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;
        COMMIT TRANSACTION;

        IF @error != 0
        BEGIN
            RETURN 1;           -- SQL execution error
        END
        IF @row_count = 0
        BEGIN
            RETURN -1;          -- object is not found
        END
        IF @expiration IS NOT NULL
        BEGIN
            IF @expiration < GETDATE()
            BEGIN
                RETURN -4;          -- object is expired
                                    -- -4 is to make it unified with the other SPs
            END
        END
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



ALTER TABLE Objects
ADD ttl BIGINT DEFAULT NULL;
GO



ALTER PROCEDURE GetObjectFixedAttributes
    @object_key         VARCHAR(289),
    @expiration         DATETIME OUT,
    @creation           DATETIME OUT,
    @obj_read           DATETIME OUT,
    @obj_write          DATETIME OUT,
    @attr_read          DATETIME OUT,
    @attr_write         DATETIME OUT,
    @read_cnt           BIGINT OUT,
    @write_cnt          BIGINT OUT,
    @client_name        VARCHAR(256) OUT,

    -- backward compatibility
    @user_namespace     VARCHAR(64) = NULL OUT,
    @user_name          VARCHAR(64) = NULL OUT,
    @obj_ttl            BIGINT = NULL OUT
AS
BEGIN
    DECLARE @object_id      BIGINT = NULL;
    DECLARE @cl_id          BIGINT = NULL;
    DECLARE @u_id           BIGINT = NULL;

    BEGIN TRANSACTION
    BEGIN TRY
        SELECT @object_id = object_id,
               @expiration = tm_expiration, @creation = tm_create,
               @obj_read = tm_read, @obj_write = tm_write,
               @attr_read = tm_attr_read, @attr_write = tm_attr_write,
               @read_cnt = read_count, @write_cnt = write_count,
               @cl_id = client_id, @u_id = user_id, @obj_ttl = ttl
               FROM Objects WHERE object_key = @object_key;
        IF @@ERROR != 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN 1;
        END
        IF @object_id IS NULL
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN -1;              -- object is not found
        END


        IF @cl_id IS NULL
        BEGIN
            SET @client_name = NULL;
        END
        ELSE
        BEGIN
            SELECT @client_name = name FROM Clients WHERE client_id = @cl_id;
        END

        IF @u_id IS NULL
        BEGIN
            SET @user_namespace = NULL;
            SET @user_name      = NULL;
        END
        ELSE
        BEGIN
            SELECT @user_namespace = name_space,
                   @user_name = name FROM Users WHERE user_id = @u_id;
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


-- Note: the procedure lets to reduce the expiration time
--       in the contrary to the prolonging functionality which never reduces
--       the expiration.
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
            DECLARE @ret_code   BIGINT;
            DECLARE @object_id  BIGINT;
            EXECUTE @ret_code = GetNextObjectID @object_id OUTPUT;

            IF @ret_code != 0
            BEGIN
                ROLLBACK TRANSACTION;
                RETURN 1;
            END

            INSERT INTO Objects (object_id, object_key, object_loc, client_id, tm_expiration, read_count, write_count, ttl)
            VALUES (@object_id, @object_key, @object_loc, @client_id, @expiration, 0, 0, @ttl);
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



ALTER PROCEDURE GetObjectExpiration
    @object_key     VARCHAR(289),
    @expiration     DATETIME OUT,

    -- backward compatibility

    @ttl            BIGINT = NULL OUT
AS
BEGIN
    DECLARE @row_count      INT;
    DECLARE @error          INT;

    -- To have a definitive value returned from the procedure even if
    -- there is no object
    SET @expiration = NULL;

    BEGIN TRANSACTION
    BEGIN TRY
        SELECT @expiration = tm_expiration,
               @ttl = ttl FROM Objects WHERE object_key = @object_key;
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;
        COMMIT TRANSACTION;

        IF @error != 0
        BEGIN
            RETURN 1;               -- SQL execution error
        END
        IF @row_count = 0
        BEGIN
            RETURN -1;              -- object is not found
        END
        IF @expiration IS NOT NULL
        BEGIN
            IF @expiration < GETDATE()
            BEGIN
                RETURN -4;          -- object is expired
                                    -- -4 is to make it unified with the other SPs
            END
        END
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
UPDATE Versions SET version = 11 WHERE name = 'sp_code'
SELECT @row_count = @@ROWCOUNT, @error = @@ERROR
IF @error != 0 OR @row_count = 0
BEGIN
    RAISERROR( 'Cannot update the stored procedure version in the Versions DB table', 11, 1 )
END
GO


-- Restore if it was changed
SET NOEXEC OFF
GO


