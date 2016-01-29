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


-- Changes in stored procedures:
-- - 
-- The idea of the changes is to avoid extra Objects table scans





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
IF @sp_version != 4
BEGIN
    RAISERROR( 'Unexpected stored procedure version. Expected version is 4.', 11, 1 )
    SET NOEXEC ON
END
GO





-- Finally, the DB and SP versions are checked, so we can continue



ALTER PROCEDURE GetObjectExpiration
    @object_key     VARCHAR(256),
    @expiration     DATETIME OUT
AS
BEGIN
    BEGIN TRANSACTION
    BEGIN TRY
        DECLARE @row_count      INT
        DECLARE @error          INT

        SELECT @expiration = tm_expiration FROM Objects WHERE object_key = @object_key
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR
        COMMIT TRANSACTION

        IF @error != 0
        BEGIN
            RETURN 1
        END
        IF @row_count = 0
        BEGIN
            RETURN -1               -- object is not found
        END
        RETURN 0
    END TRY
    BEGIN CATCH
        ROLLBACK TRANSACTION

        DECLARE @error_message NVARCHAR(4000) = ERROR_MESSAGE()
        DECLARE @error_severity INT = ERROR_SEVERITY()
        DECLARE @error_state INT = ERROR_STATE()

        RAISERROR( @error_message, @error_severity, @error_state )
        RETURN 1
    END CATCH
END
GO



ALTER PROCEDURE SetObjectExpiration
    @object_key     VARCHAR(256),
    @expiration     DATETIME
AS
BEGIN
    BEGIN TRANSACTION
    BEGIN TRY
        DECLARE @row_count      INT
        DECLARE @error          INT

        UPDATE Objects SET tm_expiration = @expiration WHERE object_key = @object_key
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR

        IF @error != 0
        BEGIN
            ROLLBACK TRANSACTION
            RETURN 1
        END

        COMMIT TRANSACTION
        IF @row_count = 0
        BEGIN
            RETURN -1               -- object is not found
        END
        RETURN 0
    END TRY
    BEGIN CATCH
        ROLLBACK TRANSACTION

        DECLARE @error_message NVARCHAR(4000) = ERROR_MESSAGE()
        DECLARE @error_severity INT = ERROR_SEVERITY()
        DECLARE @error_state INT = ERROR_STATE()

        RAISERROR( @error_message, @error_severity, @error_state )
        RETURN 1
    END CATCH
END
GO




ALTER PROCEDURE AddAttribute
    @object_key     VARCHAR(256),
    @attr_name      VARCHAR(256),
    @attr_value     VARCHAR(1024),
    @client_id      BIGINT
AS
BEGIN
    DECLARE @object_id      BIGINT = NULL
    DECLARE @attr_id        BIGINT = NULL
    DECLARE @row_count      INT
    DECLARE @error          INT

    BEGIN TRANSACTION
    BEGIN TRY

        UPDATE Objects SET tm_attr_write = GETDATE(), @object_id = object_id WHERE object_key = @object_key
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR
        IF @error != 0
        BEGIN
            ROLLBACK TRANSACTION
            RETURN 1
        END

        IF @row_count = 0
        BEGIN
            COMMIT TRANSACTION
            RETURN -1               -- object is not found
        END

        -- Get the attribute ID
        SELECT @attr_id = attr_id FROM Attributes WHERE name = @attr_name
        IF @attr_id IS NULL
        BEGIN
            INSERT INTO Attributes (name) VALUES (@attr_name)
            SET @attr_id = SCOPE_IDENTITY()
        END

        -- Create or update the attribute
        UPDATE AttrValues SET value = @attr_value WHERE object_id = @object_id AND attr_id = @attr_id
        SET @row_count = @@ROWCOUNT
        IF @row_count = 0
            INSERT INTO AttrValues (object_id, attr_id, value) VALUES (@object_id, @attr_id, @attr_value)
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



ALTER PROCEDURE DelAttribute
    @object_key     VARCHAR(256),
    @attr_name      VARCHAR(256)
AS
BEGIN
    DECLARE @object_id      BIGINT = NULL
    DECLARE @attr_id        BIGINT = NULL
    DECLARE @row_count      INT
    DECLARE @error          INT

    BEGIN TRANSACTION
    BEGIN TRY

        UPDATE Objects SET tm_attr_write = GETDATE(), @object_id = object_id WHERE object_key = @object_key
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR
        IF @error != 0
        BEGIN
            ROLLBACK TRANSACTION
            RETURN 1
        END

        IF @row_count = 0
        BEGIN
            ROLLBACK TRANSACTION
            RETURN -1               -- object is not found
        END

        SELECT @attr_id = attr_id FROM Attributes WHERE name = @attr_name
        IF @@ERROR != 0
        BEGIN
            ROLLBACK TRANSACTION
            RETURN 1
        END
        IF @attr_id IS NULL
        BEGIN
            ROLLBACK TRANSACTION
            RETURN -2               -- attribute is not found
        END

        DELETE FROM AttrValues WHERE object_id = @object_id AND attr_id = @attr_id
        SET @row_count = @@ROWCOUNT
        IF @row_count = 0
        BEGIN
            ROLLBACK TRANSACTION
            RETURN -3           -- attribute value is not found
        END
    END TRY
    BEGIN CATCH
        ROLLBACK TRANSACTION
        DECLARE @error_message NVARCHAR(4000) = ERROR_MESSAGE()
        DECLARE @error_severity INT = ERROR_SEVERITY()
        DECLARE @error_state INT = ERROR_STATE()

        RAISERROR( @error_message, @error_severity, @error_state )
        RETURN 1
    END CATCH
    COMMIT TRANSACTION
    RETURN 0
END
GO






DECLARE @row_count  INT
DECLARE @error      INT
UPDATE Versions SET version = 5 WHERE name = 'sp_code'
SELECT @row_count = @@ROWCOUNT, @error = @@ERROR
IF @error != 0 OR @row_count = 0
BEGIN
    RAISERROR( 'Cannot update the stored procedure version in the Versions DB table', 11, 1 )
END
GO


-- Restore if it was changed
SET NOEXEC OFF
GO


