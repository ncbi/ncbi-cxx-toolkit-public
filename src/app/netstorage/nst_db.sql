USE [NETSTORAGE];
GO
-- Recommended settings during procedure creation:
SET ANSI_NULLS ON;
SET QUOTED_IDENTIFIER ON;
SET ANSI_PADDING ON;
SET ANSI_WARNINGS ON;
GO

-- Drop all the existing objects if so
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

