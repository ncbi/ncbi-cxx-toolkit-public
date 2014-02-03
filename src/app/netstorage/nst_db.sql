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

-- Drop all the existing objects if so
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'CreateObjectWithIDs')
    DROP PROCEDURE CreateObjectWithIDs
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'CreateObject')
    DROP PROCEDURE CreateObject
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'CreateClientOwnerGroup')
    DROP PROCEDURE CreateClientOwnerGroup
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'AttrValues')
    DROP TABLE AttrValues
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'Objects')
    DROP TABLE Objects
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'Owners')
    DROP TABLE Owners
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'Groups')
    DROP TABLE Groups
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'Clients')
    DROP TABLE Clients
IF EXISTS (SELECT * FROM sysobjects WHERE name = 'Attributes')
    DROP TABLE Attributes
GO




-- Create the Owners table
CREATE TABLE Owners
(
    owner_id        bigint NOT NULL IDENTITY (1, 1),
    name            varchar(128) NOT NULL,
    description     varchar(512) NULL
)
GO

ALTER TABLE Owners ADD CONSTRAINT
PK_Owners PRIMARY KEY CLUSTERED ( owner_id )
GO

ALTER TABLE Owners ADD CONSTRAINT
IX_Owners_name UNIQUE NONCLUSTERED ( name )
GO


-- Create the Groups table
CREATE TABLE Groups
(
    group_id        bigint NOT NULL IDENTITY (1, 1),
    name            varchar(128) NOT NULL,
    description     varchar(512) NULL
)
GO

ALTER TABLE Groups ADD CONSTRAINT
PK_Groups PRIMARY KEY CLUSTERED ( group_id )
GO

ALTER TABLE Groups ADD CONSTRAINT
IX_Groups_name UNIQUE NONCLUSTERED ( name )
GO


-- Create the Clients table
CREATE TABLE Clients
(
    client_id       bigint NOT NULL IDENTITY (1, 1),
    name            varchar(256) NOT NULL,
    description     varchar(512) NULL
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
    attr_id         bigint NOT NULL IDENTITY (1, 1),
    name            varchar(256) NOT NULL,
    description     varchar(512) NULL
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
    object_id       bigint NOT NULL IDENTITY (1, 1),
    name            varchar(256) NOT NULL,
    owner_id        bigint NULL,
    group_id        bigint NULL,
    client_id       bigint NOT NULL,
    tm_create       datetime NOT NULL,
    tm_write        datetime NULL,
    tm_read         datetime NULL,
    size            bigint NOT NULL
)
GO

ALTER TABLE Objects ADD CONSTRAINT
PK_Objects PRIMARY KEY CLUSTERED ( object_id )
GO

ALTER TABLE Objects ADD CONSTRAINT
IX_Objects_name UNIQUE NONCLUSTERED ( name )
GO

ALTER TABLE Objects ADD CONSTRAINT
FK_Objects_Owner FOREIGN KEY ( owner_id )
REFERENCES Owners ( owner_id )
GO

ALTER TABLE Objects ADD CONSTRAINT
FK_Objects_Group FOREIGN KEY ( group_id )
REFERENCES Groups ( group_id )
GO

ALTER TABLE Objects ADD CONSTRAINT
FK_Objects_Client FOREIGN KEY ( client_id )
REFERENCES Clients ( client_id )
GO




-- Create the AttrValues table
CREATE TABLE AttrValues
(
    object_id       bigint NOT NULL,
    attr_id         bigint NOT NULL,
    value           varchar(1024) NOT NULL
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




-- Stored procedures
CREATE PROCEDURE CreateClientOwnerGroup
    @client_name    varchar(256),
    @owner_name     varchar(128) = NULL,
    @group_name     varchar(128) = NULL,

    @client_id_     bigint OUT,
    @owner_id_      bigint OUT,
    @group_id_      bigint OUT
AS
BEGIN
    IF ((@client_name IS NOT NULL) AND (@client_name != ''))
    BEGIN
        IF NOT EXISTS (SELECT client_id FROM Clients WHERE name = @client_name)
            INSERT INTO Clients (name) VALUES (@client_name)
        SELECT @client_id_ = client_id FROM Clients WHERE name = @client_name
    END
    ELSE
        SET @client_id_ = NULL

    IF ((@owner_name IS NOT NULL) AND (@owner_name != ''))
    BEGIN
        IF NOT EXISTS (SELECT owner_id FROM Owners WHERE name = @owner_name)
            INSERT INTO Owners (name) VALUES (@owner_name)
        SELECT @owner_id_ = owner_id FROM Owners WHERE name = @owner_name
    END
    ELSE
        SET @owner_id_ = NULL

    IF ((@group_name IS NOT NULL) AND (@group_name != ''))
    BEGIN
        IF NOT EXISTS (SELECT group_id FROM Groups WHERE name = @group_name)
            INSERT INTO Groups (name) VALUES (@group_name)
        SELECT @group_id_ = group_id FROM Groups WHERE name = @group_name
    END
        SET @group_id_ = NULL
END
GO


CREATE PROCEDURE CreateObject
    @object_name    varchar(256),
    @object_size    bigint,
    @client_name    varchar(256),
    @owner_name     varchar(128) = NULL,
    @group_name     varchar(128) = NULL
AS
BEGIN
    INSERT INTO Objects (name, owner_id, group_id, client_id, tm_create, size)
    VALUES (@object_name,
            (SELECT owner_id FROM Owners WHERE name = @owner_name),
            (SELECT group_id FROM Groups WHERE name = @group_name),
            (SELECT client_id FROM Clients WHERE name = @client_name),
            GETDATE(),
            @object_size)
END
GO

CREATE PROCEDURE CreateObjectWithIDs
    @object_name    varchar(256),
    @object_size    bigint,
    @client_id      bigint,
    @owner_id       bigint = NULL,
    @group_id       bigint = NULL
AS
BEGIN
    INSERT INTO Objects (name, owner_id, group_id, client_id, tm_create, size)
    VALUES (@object_name, @owner_id, @group_id, @client_id, GETDATE(), @object_size)
END
GO

