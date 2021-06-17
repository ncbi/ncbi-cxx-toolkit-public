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
-- File Description: NetStorage server DB SP to update from version 6.13 to
--                   6.14
--


-- NB: before applying on a server you need to do two changes:
--     - change the DB name
--     - change the guard condition

-- Changes in the DB structure:
-- - None

-- Changes in stored procedures:
-- - CreateObjectWithClientID_v2: 





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
IF @sp_version != 13
BEGIN
    RAISERROR( 'Unexpected stored procedure version. Expected version is 13.', 11, 1 )
    SET NOEXEC ON
END
GO


-- Finally, the DB and SP versions are checked, so we can continue



CREATE PROCEDURE CreateObjectWithClientID_v2
    @object_key         VARCHAR(289),
    @object_create_tm   DATETIME,
    @object_loc         VARCHAR(900),
    @object_size        BIGINT,
    @client_id          BIGINT,
    @object_expiration  DATETIME,

    -- make it backward compatible
    @user_id            BIGINT = NULL,
    @size_was_null      INT = 0 OUT
AS
BEGIN
    DECLARE @row_count          INT;
    DECLARE @error              INT;
    DECLARE @old_object_size    BIGINT;

    BEGIN TRANSACTION
    BEGIN TRY

        -- Try update first
        UPDATE Objects SET tm_create = @object_create_tm,
                           @old_object_size = size,
                           size = @object_size
                       WHERE object_key = @object_key;
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;

        IF @error != 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN 1;
        END

        IF @row_count = 0       -- object is not found; create it
        BEGIN
            INSERT INTO Objects (object_id, object_key, object_loc,
                                 client_id, tm_create, size,
                                 tm_expiration, read_count, write_count,
                                 user_id)
            VALUES (NEXT VALUE FOR ObjectIdGenerator, @object_key, @object_loc,
                    @client_id, @object_create_tm, @object_size,
                    @object_expiration, 0, 1, @user_id);
            SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;

            IF @error != 0 OR @row_count = 0
            BEGIN
                ROLLBACK TRANSACTION;
                RETURN 1;
            END

            -- the object was created with non-null size
            SET @size_was_null = 1;
        END
        ELSE
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
UPDATE Versions SET version = 14 WHERE name = 'sp_code'
SELECT @row_count = @@ROWCOUNT, @error = @@ERROR
IF @error != 0 OR @row_count = 0
BEGIN
    RAISERROR( 'Cannot update the stored procedure version in the Versions DB table', 11, 1 )
END
GO


-- Restore if it was changed
SET NOEXEC OFF
GO


