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
IF 1 = 1
BEGIN
    IF (EXISTS (SELECT * FROM sys.database_principals WHERE name = 'netstorage_read')) OR
       (EXISTS (SELECT * FROM sys.database_principals WHERE name = 'netstorage_write')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'GetAttributeNames')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'GetAttribute')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'DelAttribute')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'AddAttribute')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'RemoveObject')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'SetObjectExpiration')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'GetObjectExpiration')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'GetObjectFixedAttributes')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'UpdateObjectOnRelocate')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'UpdateObjectOnRead')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'UpdateObjectOnWrite')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'UpdateUserKeyObjectOnWrite')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'CreateObject')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'CreateObjectWithClientID')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'CreateClient')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'ObjectIdGen')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'GetNextObjectID')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'AttrValues')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'Objects')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'Clients')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'Versions')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'Attributes')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'GetStatInfo')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'GetClientObjects')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'GetClients'))
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
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'GetAttributeNames')
    DROP PROCEDURE GetAttributeNames
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'GetAttribute')
    DROP PROCEDURE GetAttribute
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'DelAttribute')
    DROP PROCEDURE DelAttribute
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'AddAttribute')
    DROP PROCEDURE AddAttribute
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'RemoveObject')
    DROP PROCEDURE RemoveObject
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'SetObjectExpiration')
    DROP PROCEDURE SetObjectExpiration
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'GetObjectExpiration')
    DROP PROCEDURE GetObjectExpiration
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'GetObjectFixedAttributes')
    DROP PROCEDURE GetObjectFixedAttributes
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'UpdateObjectOnRelocate')
    DROP PROCEDURE UpdateObjectOnRelocate
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'UpdateObjectOnRead')
    DROP PROCEDURE UpdateObjectOnRead
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'UpdateObjectOnWrite')
    DROP PROCEDURE UpdateObjectOnWrite
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'UpdateUserKeyObjectOnWrite')
    DROP PROCEDURE UpdateUserKeyObjectOnWrite
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'CreateObject')
    DROP PROCEDURE CreateObject
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'CreateObjectWithClientID')
    DROP PROCEDURE CreateObjectWithClientID
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'GetNextObjectID')
    DROP PROCEDURE GetNextObjectID
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'CreateClient')
    DROP PROCEDURE CreateClient
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'GetStatInfo')
    DROP PROCEDURE GetStatInfo
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'GetClientObjects')
    DROP PROCEDURE GetClientObjects
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'GetClients')
    DROP PROCEDURE GetClients
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'AttrValues')
    DROP TABLE AttrValues
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'Objects')
    DROP TABLE Objects
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'Clients')
    DROP TABLE Clients
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'Versions')
    DROP TABLE Versions
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'Attributes')
    DROP TABLE Attributes
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'ObjectIdGen')
    DROP TABLE ObjectIdGen
GO


-- Create versions table which can hold versions of various components
-- For the time being it stores only one record with the DB structure version
-- Later on other components could be added.
CREATE TABLE Versions
(
    version_id      BIGINT NOT NULL IDENTITY (1, 1),
    name            VARCHAR(256) NOT NULL,
    version         INT NOT NULL,
    description     VARCHAR(512) NULL
)
GO

ALTER TABLE Versions ADD CONSTRAINT
PK_Versions PRIMARY KEY CLUSTERED ( version_id )
GO

ALTER TABLE Versions ADD CONSTRAINT
IX_Versions_name UNIQUE NONCLUSTERED ( name )

-- The initial DB structure version is 1
INSERT INTO Versions ( name, version, description )
VALUES ( 'db_structure', 1, 'Database structure' )
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
    tm_expiration   DATETIME NULL,
    size            BIGINT NULL,                -- might be NULL because meta info could be
                                                -- switched on after the object was created or read
    read_count      BIGINT NOT NULL,            -- number of times the object was read except of
                                                -- when monitoring metadata option was used
    write_count     BIGINT NOT NULL             -- number of times the object was written
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


CREATE PROCEDURE CreateObjectWithClientID
    @object_id          BIGINT,
    @object_key         VARCHAR(256),
    @object_create_tm   DATETIME,
    @object_loc         VARCHAR(256),
    @object_size        BIGINT,
    @client_id          BIGINT,
    @object_expiration  DATETIME
AS
BEGIN
    BEGIN TRY
        DECLARE @row_count  INT
        DECLARE @error      INT

        INSERT INTO Objects (object_id, object_key, object_loc, client_id, tm_create, size, tm_expiration, read_count, write_count)
        VALUES (@object_id, @object_key, @object_loc, @client_id, @object_create_tm, @object_size, @object_expiration, 0, 1)
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


-- The procedure is triggered for the objects which are written with the user
-- keys. For such objects there is no preceding CREATE command so a record
-- may or may not exist in the Objects table.
-- If the record does not exist it should be created.
-- If it exists then special steps must be done depending on the expiration_tm
-- value. If the time is expired then it should be reset (set to NULL). If not
-- expired then no changes should appear.
CREATE PROCEDURE UpdateUserKeyObjectOnWrite
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

        UPDATE Objects SET size = @object_size, tm_write = @current_time,
                           write_count = write_count + 1 WHERE object_key = @object_key
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
        ELSE                    -- object is found; tm_write updated; see if expiration should be updated too
        BEGIN
            DECLARE @expiration     DATETIME

            SELECT @expiration = tm_expiration FROM Objects WHERE object_key = @object_key
            IF @expiration IS NOT NULL
            BEGIN
                IF @expiration < @current_time
                BEGIN
                    -- the object is expired, so the expiration must be reset
                    UPDATE Objects SET tm_expiration = @object_exp_if_not_found, read_count = 0, write_count = 1 WHERE object_key = @object_key
                    SELECT @error = @@ERROR

                    IF @error != 0
                    BEGIN
                        ROLLBACK TRANSACTION
                        RETURN 1
                    END
                END
                ELSE
                BEGIN
                    -- the object is not expired; expiration might need to be updated though
                    UPDATE Objects SET tm_expiration = @object_exp_if_found WHERE object_key = @object_key
                    SELECT @error = @@ERROR

                    IF @error != 0
                    BEGIN
                        ROLLBACK TRANSACTION
                        RETURN 1
                    END
                END
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


CREATE PROCEDURE GetObjectFixedAttributes
    @object_key     VARCHAR(256),
    @expiration     DATETIME OUT,
    @creation       DATETIME OUT,
    @obj_read       DATETIME OUT,
    @obj_write      DATETIME OUT,
    @attr_read      DATETIME OUT,
    @attr_write     DATETIME OUT,
    @read_cnt       BIGINT OUT,
    @write_cnt      BIGINT OUT,
    @client_name    VARCHAR(256) OUT
AS
BEGIN
    DECLARE @object_id      BIGINT = NULL
    DECLARE @cl_id          BIGINT = NULL

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

        SELECT @expiration = tm_expiration, @creation = tm_create,
               @obj_read = tm_read, @obj_write = tm_write,
               @attr_read = tm_attr_read, @attr_write = tm_attr_write,
               @read_cnt = read_count, @write_cnt = write_count,
               @cl_id = client_id
               FROM Objects WHERE object_key = @object_key
        SELECT @client_name = name FROM Clients WHERE client_id = @cl_id
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


CREATE PROCEDURE GetObjectExpiration
    @object_key     VARCHAR(256),
    @expiration     DATETIME OUT
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

        SELECT @expiration = tm_expiration FROM Objects WHERE object_key = @object_key
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


CREATE PROCEDURE SetObjectExpiration
    @object_key     VARCHAR(256),
    @expiration     DATETIME
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

        UPDATE Objects SET tm_expiration = @expiration WHERE object_key = @object_key
        IF @@ERROR != 0
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


CREATE PROCEDURE AddAttribute
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

        -- Get the object ID
        SELECT @object_id = object_id FROM Objects WHERE object_key = @object_key
        IF @@ERROR != 0
        BEGIN
            ROLLBACK TRANSACTION
            RETURN 1
        END
        IF @object_id IS NULL       -- object is not found; create it
        BEGIN
            ROLLBACK TRANSACTION
            return -1
        END

        -- Update attribute timestamp for the existing object
        UPDATE Objects SET tm_attr_write = GETDATE() WHERE object_key = @object_key
        IF @@ERROR != 0
        BEGIN
            ROLLBACK TRANSACTION
            RETURN 1
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


CREATE PROCEDURE GetAttributeNames
    @object_key     VARCHAR(256)
AS
BEGIN
    DECLARE @object_id      BIGINT = NULL

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

        -- This is the output recordset!
        SELECT name FROM Attributes AS a, AttrValues AS b
                    WHERE a.attr_id = b.attr_id AND b.object_id = @object_id
        IF @@ERROR != 0
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


CREATE PROCEDURE GetClients
AS
BEGIN
    BEGIN TRANSACTION
    BEGIN TRY

        -- This is the output recordset!
        SELECT name FROM Clients
        IF @@ERROR != 0
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


-- if @limit is NULL then there will be no limit on how many records are
-- included into the result set
CREATE PROCEDURE GetClientObjects
    @client_name        VARCHAR(256),
    @limit              BIGINT = NULL,
    @total_object_cnt   BIGINT OUT
AS
BEGIN
    DECLARE @client_id      BIGINT = NULL

    BEGIN TRANSACTION
    BEGIN TRY

        -- Get the client ID
        SELECT @client_id = client_id FROM Clients WHERE name = @client_name
        IF @@ERROR != 0
        BEGIN
            ROLLBACK TRANSACTION
            RETURN 1
        END
        IF @client_id IS NULL
        BEGIN
            ROLLBACK TRANSACTION
            RETURN -1               -- client is not found
        END

        DECLARE @current_time   DATETIME = GETDATE()
        SELECT @total_object_cnt = COUNT(*) FROM Objects WHERE client_id = @client_id AND
                                                               (tm_expiration IS NULL OR tm_expiration >= @current_time)
        IF @@ERROR != 0
        BEGIN
            ROLLBACK TRANSACTION
            RETURN 1
        END

        -- This is the output recordset!
        IF @limit IS NULL
        BEGIN
            SELECT object_loc FROM Objects WHERE client_id = @client_id AND
                                                 (tm_expiration IS NULL OR tm_expiration >= @current_time) ORDER BY object_id ASC
            IF @@ERROR != 0
            BEGIN
                ROLLBACK TRANSACTION
                RETURN 1
            END
        END
        ELSE
        BEGIN
            SELECT TOP(@limit) object_loc FROM Objects WHERE client_id = @client_id AND
                                                             (tm_expiration IS NULL OR tm_expiration >= @current_time) ORDER BY object_id ASC
            IF @@ERROR != 0
            BEGIN
                ROLLBACK TRANSACTION
                RETURN 1
            END
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
    @need_update    INT,
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

        IF @need_update != 0
        BEGIN
            -- Update attribute timestamp for the existing object
            UPDATE Objects SET tm_attr_read = GETDATE() WHERE object_id = @object_id
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


CREATE PROCEDURE GetStatInfo
AS
BEGIN
    BEGIN TRANSACTION
        SELECT count(*) AS TotalObjectCount FROM Objects
        SELECT count(*) AS ExpiredObjectCount FROM Objects WHERE tm_expiration IS NOT NULL AND tm_expiration < GETDATE()
        SELECT count(*) AS ClientCount FROM Clients
        SELECT count(*) AS AttributeCount FROM Attributes
        SELECT count(*) AS AttributeValueCount FROM AttrValues
        SELECT version as DBStructureVersion FROM Versions WHERE name = 'db_structure'
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

