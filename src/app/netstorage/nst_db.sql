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
IF 1 = 0
BEGIN
    IF (EXISTS (SELECT * FROM sys.database_principals WHERE name = 'netstorage_read')) OR
       (EXISTS (SELECT * FROM sys.database_principals WHERE name = 'netstorage_write')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'UpdateObjectOnReadByKey')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'UpdateObjectOnReadByLoc')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'UpdateObjectOnWriteByKey')) OR
       (EXISTS (SELECT * FROM sysobjects WHERE name = 'UpdateObjectOnWriteByLoc')) OR
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
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'UpdateObjectOnReadByKey')
    DROP PROCEDURE UpdateObjectOnReadByKey
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'UpdateObjectOnReadByLoc')
    DROP PROCEDURE UpdateObjectOnReadByLoc
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'UpdateObjectOnWriteByKey')
    DROP PROCEDURE UpdateObjectOnWriteByKey
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'UpdateObjectOnWriteByLoc')
    DROP PROCEDURE UpdateObjectOnWriteByLoc
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
    object_key      VARCHAR(256) NOT NULL,       -- to underlying storage
    object_loc      VARCHAR(256) NOT NULL,       -- full key
    client_id       BIGINT NOT NULL,
    tm_create       DATETIME NOT NULL,
    tm_write        DATETIME NULL,
    tm_read         DATETIME NULL,
    size            BIGINT NOT NULL
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



-- Returns 0 if everything is fine
CREATE PROCEDURE GetNextObjectID
    @next_id_    BIGINT OUT
AS
BEGIN
    SET NOCOUNT ON
    UPDATE ObjectIdGen SET @next_id_ = next_object_id = next_object_id + 1
    IF @@ERROR = 0
        RETURN 0
    RETURN 1
END
GO



-- Returns 0 if everything is fine
CREATE PROCEDURE CreateClient
    @client_name    VARCHAR(256),
    @client_id_     BIGINT OUT
AS
BEGIN
    DECLARE @ret_val INT
    DECLARE @error_message NVARCHAR(4000)
    DECLARE @error_severity INT
    DECLARE @error_state INT
    DECLARE @error_number INT

    SET NOCOUNT ON

    -- Initialize return values
    SET @ret_val = 0
    SET @client_id_ = NULL


    -- Handle the Clients table
    IF ((@client_name IS NOT NULL) AND (@client_name != ''))
    BEGIN
        IF NOT EXISTS (SELECT client_id FROM Clients WHERE name = @client_name)
        BEGIN
            BEGIN TRY
                INSERT INTO Clients (name) VALUES (@client_name)
            END TRY
            BEGIN CATCH
                SET @error_message = ERROR_MESSAGE()
                SET @error_severity = ERROR_SEVERITY()
                SET @error_state = ERROR_STATE()
                SET @error_number = ERROR_NUMBER()

                IF @error_number != 2627   -- Already exists
                BEGIN
                    RAISERROR( @error_message, @error_severity, @error_state )
                    SET @ret_val = 1
                END
            END CATCH
        END
        SELECT @client_id_ = client_id FROM Clients WHERE name = @client_name
    END

    RETURN @ret_val
END
GO


-- Returns 0 if everything is fine
CREATE PROCEDURE CreateObject
    @object_id      BIGINT,
    @object_key     VARCHAR(256),
    @object_loc     VARCHAR(256),
    @object_size    BIGINT,
    @client_name    VARCHAR(256)
AS
BEGIN
    SET NOCOUNT ON

    INSERT INTO Objects (object_id, object_key, object_loc, client_id, tm_create, size)
    VALUES (@object_id,
            @object_key,
            @object_loc,
            (SELECT client_id FROM Clients WHERE name = @client_name),
            GETDATE(),
            @object_size)
    IF @@ERROR = 0
        RETURN 0
    RETURN 1
END
GO


-- Returns 0 if everything is fine
CREATE PROCEDURE CreateObjectWithClientID
    @object_id      BIGINT,
    @object_key     VARCHAR(256),
    @object_loc     VARCHAR(256),
    @object_size    BIGINT,
    @client_id      BIGINT
AS
BEGIN
    SET NOCOUNT ON

    INSERT INTO Objects (object_id, object_key, object_loc, client_id, tm_create, size)
    VALUES (@object_id, @object_key, @object_loc, @client_id, GETDATE(), @object_size)
    IF @@ERROR = 0
        RETURN 0
    RETURN 1
END
GO



-- Returns 0 if everything is fine
CREATE PROCEDURE UpdateObjectOnWriteByKey
    @object_key     VARCHAR(256),
    @object_size    BIGINT
AS
BEGIN
    SET NOCOUNT ON

    UPDATE Objects SET size = @object_size, tm_write = GETDATE() WHERE object_key = @object_key
    IF @@ERROR = 0
        RETURN 0
    RETURN 1
END
GO


-- Returns 0 if everything is fine
CREATE PROCEDURE UpdateObjectOnWriteByLoc
    @object_loc     VARCHAR(256),
    @object_size    BIGINT
AS
BEGIN
    SET NOCOUNT ON

    UPDATE Objects SET size = @object_size, tm_write = GETDATE() WHERE object_loc = @object_loc
    IF @@ERROR = 0
        RETURN 0
    RETURN 1
END
GO


-- Returns 0 if everything is fine
CREATE PROCEDURE UpdateObjectOnReadByKey
    @object_key     VARCHAR(256)
AS
BEGIN
    SET NOCOUNT ON

    UPDATE Objects SET tm_read = GETDATE() WHERE object_key = @object_key
    IF @@ERROR = 0
        RETURN 0
    RETURN 1
END
GO


-- Returns 0 if everything is fine
CREATE PROCEDURE UpdateObjectOnReadByLoc
    @object_loc     VARCHAR(256)
AS
BEGIN
    SET NOCOUNT ON

    UPDATE Objects SET tm_read = GETDATE() WHERE object_loc = @object_loc
    IF @@ERROR = 0
        RETURN 0
    RETURN 1
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

