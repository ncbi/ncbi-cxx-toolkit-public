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
-- - table Clients extended with the client_type field VARCHAR(64) NOT NULL
-- - table Objects now has client_id nullable


-- Changes in stored procedures:
-- - CreateClient: added @client_namespace





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
IF 1 = 0
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
IF @db_version != 3
BEGIN
    RAISERROR( 'Unexpected database structure version. Expected version is 3.', 11, 1 )
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
IF @sp_version != 8
BEGIN
    RAISERROR( 'Unexpected stored procedure version. Expected version is 8.', 11, 1 )
    SET NOEXEC ON
END
GO


-- Finally, the DB and SP versions are checked, so we can continue


ALTER TABLE Clients
ADD name_space VARCHAR(32) NOT NULL DEFAULT '';
GO
ALTER TABLE Clients
DROP CONSTRAINT IX_Clients_name;
GO
ALTER TABLE Clients ADD CONSTRAINT
IX_Clients_name_and_namespace UNIQUE NONCLUSTERED ( name, name_space );
GO



ALTER PROCEDURE CreateClient
    @client_name        VARCHAR(256),
    @client_id          BIGINT OUT,

    -- backward compatibility

    @client_namespace   VARCHAR(32) = ''    -- default namespace is an empty string
AS
BEGIN
    BEGIN TRANSACTION
    BEGIN TRY
        SET @client_id = NULL;
        SELECT @client_id = client_id FROM Clients
                                      WHERE name = @client_name AND
                                            name_space = @client_namespace;
        IF @client_id IS NULL
        BEGIN
            INSERT INTO Clients (name, name_space) VALUES (@client_name,
                                                           @client_namespace);
            SET @client_id = SCOPE_IDENTITY();
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



ALTER TABLE Objects
ALTER COLUMN client_id BIGINT NULL;
GO



ALTER PROCEDURE GetClients
AS
BEGIN
    BEGIN TRANSACTION
    BEGIN TRY
        -- This is the output recordset!
        SELECT name_space, name FROM Clients;
        IF @@ERROR != 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN 1;
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
    @client_namespace   VARCHAR(32) = NULL OUT
AS
BEGIN
    DECLARE @object_id      BIGINT = NULL;
    DECLARE @cl_id          BIGINT = NULL;

    BEGIN TRANSACTION
    BEGIN TRY
        SELECT @object_id = object_id FROM Objects WHERE object_key = @object_key;
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

        SELECT @expiration = tm_expiration, @creation = tm_create,
               @obj_read = tm_read, @obj_write = tm_write,
               @attr_read = tm_attr_read, @attr_write = tm_attr_write,
               @read_cnt = read_count, @write_cnt = write_count,
               @cl_id = client_id
               FROM Objects WHERE object_key = @object_key;
        IF @cl_id IS NULL
        BEGIN
            SET @client_namespace = NULL;
            SET @client_name = NULL;
        END
        ELSE
        BEGIN
            SELECT @client_namespace = name_space,
                   @client_name = name FROM Clients WHERE client_id = @cl_id;
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



CREATE PROCEDURE UpdateClientIDForObject
    @object_key         VARCHAR(289),
    @cl_id              BIGINT
AS
BEGIN
    BEGIN TRANSACTION
    BEGIN TRY
        DECLARE @row_count          INT;
        DECLARE @error              INT;

        UPDATE Objects SET client_id = @cl_id WHERE object_key = @object_key;
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;
        IF @error != 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN 1;
        END

        IF @row_count = 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN -1;              -- object is not found
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



-- if @limit is NULL then there will be no limit on how many records are
-- included into the result set
ALTER PROCEDURE GetClientObjects
    @client_name        VARCHAR(256),
    @client_namespace   VARCHAR(32) = '',
    @limit              BIGINT = NULL,
    @total_object_cnt   BIGINT OUT
AS
BEGIN
    DECLARE @client_id      BIGINT = NULL;

    BEGIN TRANSACTION
    BEGIN TRY

        -- Get the client ID
        SELECT @client_id = client_id FROM Clients
                                      WHERE name = @client_name AND name_space = @client_namespace;
        IF @@ERROR != 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN 1;
        END
        IF @client_id IS NULL
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN -1;              -- client is not found
        END

        DECLARE @current_time   DATETIME = GETDATE();
        SELECT @total_object_cnt = COUNT(*) FROM Objects WHERE client_id = @client_id AND
                                                               (tm_expiration IS NULL OR tm_expiration >= @current_time);
        IF @@ERROR != 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN 1;
        END

        -- This is the output recordset!
        IF @limit IS NULL
        BEGIN
            SELECT object_loc FROM Objects WHERE client_id = @client_id AND
                                                 (tm_expiration IS NULL OR tm_expiration >= @current_time) ORDER BY object_id ASC;
            IF @@ERROR != 0
            BEGIN
                ROLLBACK TRANSACTION;
                RETURN 1;
            END
        END
        ELSE
        BEGIN
            SELECT TOP(@limit) object_loc FROM Objects WHERE client_id = @client_id AND
                                                             (tm_expiration IS NULL OR tm_expiration >= @current_time) ORDER BY object_id ASC;
            IF @@ERROR != 0
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
UPDATE Versions SET version = 9 WHERE name = 'sp_code'
SELECT @row_count = @@ROWCOUNT, @error = @@ERROR
IF @error != 0 OR @row_count = 0
BEGIN
    RAISERROR( 'Cannot update the stored procedure version in the Versions DB table', 11, 1 )
END
GO

DECLARE @row_count  INT
DECLARE @error      INT
UPDATE Versions SET version = 4 WHERE name = 'db_structure'
SELECT @row_count = @@ROWCOUNT, @error = @@ERROR
IF @error != 0 OR @row_count = 0
BEGIN
    RAISERROR( 'Cannot update the DB structuree version in the Versions DB table', 11, 1 )
END
GO


-- Restore if it was changed
SET NOEXEC OFF
GO

