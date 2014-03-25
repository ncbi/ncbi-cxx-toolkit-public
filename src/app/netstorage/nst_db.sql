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
-- File Description: NetStorage server DB creation script
--



USE [NETSTORAGE];
GO
-- Recommended settings during procedure creation:
SET ANSI_NULLS ON;
SET QUOTED_IDENTIFIER ON;
SET ANSI_PADDING ON;
SET ANSI_WARNINGS ON;
GO


-- Modify the condition below when ready for the deployment
-- IF 1 = 0 => development
-- IF 1 = 1 => production
IF 1 = 0
BEGIN
    IF (EXISTS (SELECT * FROM sys.database_principals WHERE name = 'netstorage_read')) OR
       (EXISTS (SELECT * FROM sys.database_principals WHERE name = 'netstorage_write')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'GetAttribute')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'DelAttribute')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'AddAttribute')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'RemoveObject')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'UpdateObjectOnRelocate')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'UpdateObjectOnRead')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'UpdateObjectOnWrite')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'CreateObject')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'CreateObjectWithClientID')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'CreateClient')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'ObjectIdGen')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'GetNextObjectID')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'AttrValues')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'Objects')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'Clients')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'Attributes'))
    BEGIN
        RAISERROR( 'Do not run the script on existing database', 11, 1 )
        SET NOEXEC ON
    END
END



-- Drop all the existing objects if so
IF EXISTS (SELECT * FROM sys.database_principals WHERE name = 'netstorage_read')
    DROP USER netstorage_read
IF EXISTS (SELECT * FROM sys.database_principals WHERE name = 'netstorage_write')
    DROP USER netstorage_write
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'GetAttribute')
    DROP PROCEDURE GetAttribute
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'DelAttribute')
    DROP PROCEDURE DelAttribute
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'AddAttribute')
    DROP PROCEDURE AddAttribute
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'RemoveObject')
    DROP PROCEDURE RemoveObject
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'UpdateObjectOnRelocate')
    DROP PROCEDURE UpdateObjectOnRelocate
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'UpdateObjectOnRead')
    DROP PROCEDURE UpdateObjectOnRead
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'UpdateObjectOnWrite')
    DROP PROCEDURE UpdateObjectOnWrite
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'CreateObject')
    DROP PROCEDURE CreateObject
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'CreateObjectWithClientID')
    DROP PROCEDURE CreateObjectWithClientID
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'GetNextObjectID')
    DROP PROCEDURE GetNextObjectID
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'CreateClient')
    DROP PROCEDURE CreateClient
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'AttrValues')
    DROP TABLE AttrValues
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'Objects')
    DROP TABLE Objects
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'Clients')
    DROP TABLE Clients
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'Attributes')
    DROP TABLE Attributes
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'ObjectIdGen')
    DROP TABLE ObjectIdGen
GO



-- Create the Clients table
CREATE TABLE Clients
(
    client_id       BIGINT NOT NULL IDENTITY (1, 1),
    name            VARCHAR(256) NOT NULL,
    description     VARCHAR(512) NULL
)
GO

ALTER TABLE Clients ADD CONSTRAINT
PK_Clients PRIMARY KEY CLUSTERED ( client_id )
GO

ALTER TABLE Clients ADD CONSTRAINT
IX_Clients_name UNIQUE NONCLUSTERED ( name )
GO

-- Create the Attributes table
CREATE TABLE Attributes
(
    attr_id         BIGINT NOT NULL IDENTITY (1, 1),
    name            VARCHAR(256) NOT NULL,
    description     VARCHAR(512) NULL
)
GO

ALTER TABLE Attributes ADD CONSTRAINT
PK_Attributes PRIMARY KEY CLUSTERED ( attr_id )
GO

ALTER TABLE Attributes ADD CONSTRAINT
IX_Attributes_name UNIQUE NONCLUSTERED ( name )
GO


-- Create the Objects table
CREATE TABLE Objects
(
    object_id       BIGINT NOT NULL,
    object_key      VARCHAR(256) NOT NULL,      -- to underlying storage
    object_loc      VARCHAR(256) NOT NULL,      -- full key
    client_id       BIGINT NOT NULL,
    tm_create       DATETIME NULL,              -- might be NULL because meta info could be
                                                -- switched on after the object is created
    tm_write        DATETIME NULL,
    tm_read         DATETIME NULL,
    tm_attr_write   DATETIME NULL,
    tm_attr_read    DATETIME NULL,
    size            BIGINT NULL                 -- might be NULL because meta info could be
                                                -- switched on after the object was created or read
)
GO

ALTER TABLE Objects ADD CONSTRAINT
PK_Objects PRIMARY KEY CLUSTERED ( object_id )
GO

ALTER TABLE Objects ADD CONSTRAINT
IX_Objects_object_key UNIQUE NONCLUSTERED ( object_key )
GO

ALTER TABLE Objects ADD CONSTRAINT
IX_Objects_object_loc UNIQUE NONCLUSTERED ( object_loc )
GO

ALTER TABLE Objects ADD CONSTRAINT
FK_Objects_Client FOREIGN KEY ( client_id )
REFERENCES Clients ( client_id )
GO




-- Create the AttrValues table
CREATE TABLE AttrValues
(
    object_id       BIGINT NOT NULL,
    attr_id         BIGINT NOT NULL,
    value           VARCHAR(1024) NOT NULL
)

ALTER TABLE AttrValues ADD CONSTRAINT
PK_AttrValues PRIMARY KEY CLUSTERED ( object_id, attr_id )
GO

ALTER TABLE AttrValues ADD CONSTRAINT
FK_AttrValues_Object FOREIGN KEY ( object_id )
REFERENCES Objects ( object_id )
GO

ALTER TABLE AttrValues ADD CONSTRAINT
FK_AttrValues_Attr FOREIGN KEY ( attr_id )
REFERENCES Attributes ( attr_id )
GO


-- Create the ObjectIdGen table
CREATE TABLE ObjectIdGen
(
    next_object_id      BIGINT NOT NULL
)
GO
INSERT INTO ObjectIdGen ( next_object_id ) VALUES ( 1 )
GO



--
-- Common procedures design:
-- - procedures may raise an exception and always have return value
-- - return value 0 means everything is OK
-- - return value > 0 means there was an SQL server error
-- - return value < 0 means there was a logical error (e.g. not found object)
--
-- Therefore C++ code should do both catch exceptions and check return code
--




CREATE PROCEDURE GetNextObjectID
    @next_id     BIGINT OUT
AS
BEGIN
    DECLARE @row_count  INT
    DECLARE @error      INT

    SET @next_id = NULL
    UPDATE ObjectIdGen SET @next_id = next_object_id = next_object_id + 1
    SELECT @row_count = @@ROWCOUNT, @error = @@ERROR

    IF @error != 0 OR @row_count = 0 OR @next_id IS NULL
        RETURN 1
    RETURN 0
END
GO


CREATE PROCEDURE CreateClient
    @client_name        VARCHAR(256),
    @client_id          BIGINT OUT
AS
BEGIN
    BEGIN TRANSACTION
    BEGIN TRY
        SET @client_id = NULL
        SELECT @client_id = client_id FROM Clients WHERE name = @client_name
        IF @client_id IS NULL
        BEGIN
            INSERT INTO Clients (name) VALUES (@client_name)
            SET @client_id = SCOPE_IDENTITY()
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


CREATE PROCEDURE CreateObject
    @object_id      BIGINT,
    @object_key     VARCHAR(256),
    @object_loc     VARCHAR(256),
    @object_size    BIGINT,
    @client_name    VARCHAR(256)
AS
BEGIN
    BEGIN TRY
        DECLARE @row_count  INT
        DECLARE @error      INT

        INSERT INTO Objects (object_id, object_key, object_loc, client_id, tm_create, size)
        VALUES (@object_id,
                @object_key,
                @object_loc,
                (SELECT client_id FROM Clients WHERE name = @client_name),
                GETDATE(),
                @object_size)
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR

        IF @error != 0 OR @row_count = 0
            RETURN 1
        RETURN 0
    END TRY
    BEGIN CATCH
        DECLARE @error_message  NVARCHAR(4000) = ERROR_MESSAGE()
        DECLARE @error_severity INT = ERROR_SEVERITY()
        DECLARE @error_state    INT = ERROR_STATE()

        RAISERROR( @error_message, @error_severity, @error_state )
        RETURN 1
    END CATCH
END
GO


CREATE PROCEDURE CreateObjectWithClientID
    @object_id      BIGINT,
    @object_key     VARCHAR(256),
    @object_loc     VARCHAR(256),
    @object_size    BIGINT,
    @client_id      BIGINT
AS
BEGIN
    BEGIN TRY
        DECLARE @row_count  INT
        DECLARE @error      INT

        INSERT INTO Objects (object_id, object_key, object_loc, client_id, tm_create, size)
        VALUES (@object_id, @object_key, @object_loc, @client_id, GETDATE(), @object_size)
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR

        IF @error != 0 OR @row_count = 0
            RETURN 1
        RETURN 0
    END TRY
    BEGIN CATCH
        DECLARE @error_message  NVARCHAR(4000) = ERROR_MESSAGE()
        DECLARE @error_severity INT = ERROR_SEVERITY()
        DECLARE @error_state    INT = ERROR_STATE()

        RAISERROR( @error_message, @error_severity, @error_state )
        RETURN 1
    END CATCH
END
GO



CREATE PROCEDURE UpdateObjectOnWrite
    @object_key     VARCHAR(256),
    @object_loc     VARCHAR(256),
    @object_size    BIGINT,
    @client_id      BIGINT
AS
BEGIN
    BEGIN TRANSACTION
    BEGIN TRY

        DECLARE @row_count  INT
        DECLARE @error      INT

        UPDATE Objects SET size = @object_size, tm_write = GETDATE() WHERE object_key = @object_key
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR

        IF @error != 0
        BEGIN
            ROLLBACK TRANSACTION
            RETURN 1
        END

        IF @row_count = 0       -- object is not found; create it
        BEGIN
            DECLARE @object_id  BIGINT
            DECLARE @ret_code   BIGINT
            EXECUTE @ret_code = GetNextObjectID @object_id OUTPUT

            IF @ret_code != 0
            BEGIN
                ROLLBACK TRANSACTION
                RETURN 1
            END

            INSERT INTO Objects (object_id, object_key, object_loc, client_id, tm_write, size)
            VALUES (@object_id, @object_key, @object_loc, @client_id, GETDATE(), @object_size)
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


CREATE PROCEDURE UpdateObjectOnRead
    @object_key     VARCHAR(256),
    @object_loc     VARCHAR(256),
    @object_size    BIGINT,
    @client_id      BIGINT
AS
BEGIN
    BEGIN TRANSACTION
    BEGIN TRY

        DECLARE @row_count  INT
        DECLARE @error      INT

        UPDATE Objects SET tm_read = GETDATE() WHERE object_key = @object_key
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR

        IF @error != 0
        BEGIN
            ROLLBACK TRANSACTION
            RETURN 1
        END

        IF @row_count = 0       -- object is not found; create it
        BEGIN
            DECLARE @object_id  BIGINT
            DECLARE @ret_code   BIGINT
            EXECUTE @ret_code = GetNextObjectID @object_id OUTPUT

            IF @ret_code != 0
            BEGIN
                ROLLBACK TRANSACTION
                RETURN 1
            END

            INSERT INTO Objects (object_id, object_key, object_loc, client_id, tm_read, size)
            VALUES (@object_id, @object_key, @object_loc, @client_id, GETDATE(), @object_size)
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


CREATE PROCEDURE UpdateObjectOnRelocate
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

        IF @row_count = 0       -- object is not found; create it
        BEGIN
            DECLARE @object_id  BIGINT
            DECLARE @ret_code   BIGINT
            EXECUTE @ret_code = GetNextObjectID @object_id OUTPUT

            IF @ret_code != 0
            BEGIN
                ROLLBACK TRANSACTION
                RETURN 1
            END

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



CREATE PROCEDURE RemoveObject
    @object_key     VARCHAR(256)
AS
BEGIN
    DECLARE @object_id      BIGINT = NULL

    BEGIN TRANSACTION
    BEGIN TRY
        SELECT @object_id = object_id FROM Objects WHERE object_key = @object_key
        IF @@ERROR != 0
        BEGIN
            ROLLBACK TRANSACTION
            RETURN 1
        END
        IF @object_id IS NULL
        BEGIN
            ROLLBACK TRANSACTION
            RETURN -1               -- object is not found
        END

        DELETE FROM AttrValues WHERE object_id = @object_id
        DELETE FROM Objects WHERE object_id = @object_id
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


CREATE PROCEDURE AddAttribute
    @object_key     VARCHAR(256),
    @object_loc     VARCHAR(256),
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

        -- Get the object ID
        SELECT @object_id = object_id FROM Objects WHERE object_key = @object_key
        IF @@ERROR != 0
        BEGIN
            ROLLBACK TRANSACTION
            RETURN 1
        END
        IF @object_id IS NULL       -- object is not found; create it
        BEGIN
            DECLARE @ret_code   BIGINT
            EXECUTE @ret_code = GetNextObjectID @object_id OUTPUT

            IF @ret_code != 0
            BEGIN
                ROLLBACK TRANSACTION
                RETURN 1
            END

            INSERT INTO Objects (object_id, object_key, object_loc, client_id, tm_attr_write)
            VALUES (@object_id, @object_key, @object_loc, @client_id, GETDATE())
            SELECT @row_count = @@ROWCOUNT, @error = @@ERROR

            IF @error != 0 OR @row_count = 0
            BEGIN
                ROLLBACK TRANSACTION
                RETURN 1
            END
        END
        ELSE
        BEGIN
            -- Update attribute timestamp for the existing object
            UPDATE Objects SET tm_attr_write = GETDATE() WHERE object_key = @object_key
            IF @@ERROR != 0
            BEGIN
                ROLLBACK TRANSACTION
                RETURN 1
            END
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


CREATE PROCEDURE DelAttribute
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

        -- Get the object ID
        SELECT @object_id = object_id FROM Objects WHERE object_key = @object_key
        IF @@ERROR != 0
        BEGIN
            ROLLBACK TRANSACTION
            RETURN 1
        END
        IF @object_id IS NULL
        BEGIN
            ROLLBACK TRANSACTION
            RETURN -1               -- object is not found
        END

        UPDATE Objects SET tm_attr_write = GETDATE() WHERE object_id = @object_id
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR

        IF @error != 0 OR @row_count = 0
        BEGIN
            ROLLBACK TRANSACTION
            RETURN 1
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


CREATE PROCEDURE GetAttribute
    @object_key     VARCHAR(256),
    @attr_name      VARCHAR(256),
    @attr_value     VARCHAR(1024) OUT
AS
BEGIN
    DECLARE @object_id      BIGINT = NULL
    DECLARE @attr_id        BIGINT = NULL
    DECLARE @row_count      INT
    DECLARE @error          INT

    SET @attr_value = NULL

    BEGIN TRANSACTION
    BEGIN TRY

        -- Get the object ID
        SELECT @object_id = object_id FROM Objects WHERE object_key = @object_key
        IF @@ERROR != 0
        BEGIN
            ROLLBACK TRANSACTION
            RETURN 1
        END
        IF @object_id IS NULL
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

        SELECT @attr_value = value FROM AttrValues WHERE object_id = @object_id AND attr_id = @attr_id
        IF @@ERROR != 0
        BEGIN
            ROLLBACK TRANSACTION
            RETURN 1
        END
        IF @attr_value IS NULL
        BEGIN
            ROLLBACK TRANSACTION
            RETURN -3               -- attribute value is not found
        END

        -- Update attribute timestamp for the existing object
        UPDATE Objects SET tm_attr_read = GETDATE() WHERE object_id = @object_id
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR

        IF @error != 0 OR @row_count = 0
        BEGIN
            ROLLBACK TRANSACTION
            RETURN 1
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



-- Users
-- It is expected that two logins have already been created on the DB server:
-- netstorage_read
-- netstorage_write
CREATE USER netstorage_read FOR LOGIN netstorage_read
CREATE USER netstorage_write FOR LOGIN netstorage_write
EXEC sp_addrolemember 'db_datareader', 'netstorage_read'
EXEC sp_addrolemember 'db_datawriter', 'netstorage_write'
EXEC sp_addrolemember 'db_datareader', 'netstorage_write'
GO

-- Grants execution permissions to all the stored procedures
GRANT EXECUTE TO netstorage_write
GO


-- Restore if it was changed
SET NOEXEC OFF
GO

