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


-- Changes in stored procedures:





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
IF @db_version != 4
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
IF @sp_version != 9
BEGIN
    RAISERROR( 'Unexpected stored procedure version. Expected version is 8.', 11, 1 )
    SET NOEXEC ON
END
GO


-- Finally, the DB and SP versions are checked, so we can continue



ALTER TABLE Clients
DROP CONSTRAINT IX_Clients_name_and_namespace;
GO
ALTER TABLE Clients
DROP CONSTRAINT DF__Clients__name_sp__45400413;
GO
ALTER TABLE Clients
DROP COLUMN name_space;
GO
ALTER TABLE Clients ADD CONSTRAINT
IX_Clients_name UNIQUE NONCLUSTERED ( name );
GO



ALTER PROCEDURE CreateClient
    @client_name        VARCHAR(256),
    @client_id          BIGINT OUT
AS
BEGIN
    BEGIN TRANSACTION
    BEGIN TRY
        SET @client_id = NULL;
        SELECT @client_id = client_id FROM Clients WHERE name = @client_name;
        IF @client_id IS NULL
        BEGIN
            INSERT INTO Clients (name) VALUES (@client_name);
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


ALTER PROCEDURE GetClients
AS
BEGIN
    BEGIN TRANSACTION
    BEGIN TRY

        -- This is the output recordset!
        SELECT name FROM Clients;
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


ALTER TABLE Objects
ALTER COLUMN client_id BIGINT NOT NULL;
GO



-- Newly introduced table
CREATE TABLE Users
(
    user_id     BIGINT NOT NULL IDENTITY (1, 1),
    name        VARCHAR(64) NOT NULL,
    name_space  VARCHAR(64) NOT NULL
)
GO

ALTER TABLE Users ADD CONSTRAINT
IX_Users_name_and_namespace UNIQUE NONCLUSTERED ( name, name_space );
GO


CREATE PROCEDURE CreateUser
    @user_name          VARCHAR(64),
    @user_namespace     VARCHAR(64),
    @user_id            BIGINT OUT
AS
BEGIN
    BEGIN TRANSACTION
    BEGIN TRY
        SET @user_id = NULL;
        SELECT @user_id = user_id FROM Users
                                  WHERE name = @user_name AND
                                        name_space = @user_namespace;
        IF @user_id IS NULL
        BEGIN
            INSERT INTO Users (name, name_space) VALUES (@user_name,
                                                         @user_namespace);
            SET @user_id = SCOPE_IDENTITY();
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
ADD user_id BIGINT DEFAULT NULL;
GO
ALTER TABLE Objects ADD CONSTRAINT
FK_Objects_User FOREIGN KEY ( user_id )
REFERENCES Users ( user_id );
GO



-- if @limit is NULL then there will be no limit on how many records are
-- included into the result set
ALTER PROCEDURE GetClientObjects
    @client_name        VARCHAR(256),
    @limit              BIGINT = NULL,
    @total_object_cnt   BIGINT OUT
AS
BEGIN
    DECLARE @client_id      BIGINT = NULL;

    BEGIN TRANSACTION
    BEGIN TRY

        -- Get the client ID
        SELECT @client_id = client_id FROM Clients
                                      WHERE name = @client_name;
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
    @user_name          VARCHAR(64) = NULL OUT
AS
BEGIN
    DECLARE @object_id      BIGINT = NULL;
    DECLARE @cl_id          BIGINT = NULL;
    DECLARE @u_id           BIGINT = NULL;

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
               @cl_id = client_id, @u_id = user_id
               FROM Objects WHERE object_key = @object_key;
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


DROP PROCEDURE UpdateClientIDForObject;
GO

CREATE PROCEDURE UpdateUserIDForObject
    @object_key         VARCHAR(289),
    @u_id               BIGINT
AS
BEGIN
    BEGIN TRANSACTION
    BEGIN TRY
        DECLARE @row_count          INT;
        DECLARE @error              INT;

        UPDATE Objects SET user_id = @u_id WHERE object_key = @object_key;
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




ALTER PROCEDURE UpdateObjectOnRead_IfNotExists
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
    -- GetNextObjectID() Must not be within a transaction;
    -- The SP has its own transaction
    DECLARE @object_id  BIGINT;
    DECLARE @ret_code   BIGINT;
    EXECUTE @ret_code = GetNextObjectID @object_id OUTPUT;

    IF @ret_code != 0
    BEGIN
        RETURN 1;
    END

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

        IF @row_count = 0       -- object is not found; create it
        BEGIN
            INSERT INTO Objects (object_id, object_key, object_loc, client_id, tm_read, tm_expiration, size, read_count, write_count, user_id)
            VALUES (@object_id, @object_key, @object_loc, @client_id, @current_time, @object_exp_if_not_found, @object_size, 1, 1, NULL);
            SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;

            IF @error != 0 OR @row_count = 0
            BEGIN
                ROLLBACK TRANSACTION;
                RETURN 1;
            END

            -- The record was created with non-null size
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



ALTER PROCEDURE CreateObjectWithClientID
    @object_id          BIGINT,
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
            INSERT INTO Objects (object_id, object_key, object_loc, client_id, tm_create, size, tm_expiration, read_count, write_count, user_id)
            VALUES (@object_id, @object_key, @object_loc, @client_id, @object_create_tm, @object_size, @object_expiration, 0, 1, @user_id)
            SELECT @row_count = @@ROWCOUNT, @error = @@ERROR

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



ALTER PROCEDURE UpdateObjectOnWrite_IfNotExists
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
    -- GetNextObjectID() Must not be within a transaction;
    -- The SP has its own transaction
    DECLARE @object_id  BIGINT;
    DECLARE @ret_code   BIGINT;
    EXECUTE @ret_code = GetNextObjectID @object_id OUTPUT;

    IF @ret_code != 0
    BEGIN
        RETURN 1;
    END

    BEGIN TRANSACTION
    BEGIN TRY
        DECLARE @row_count          INT;
        DECLARE @error              INT;
        DECLARE @old_object_size    BIGINT;

        -- Need to try to update once again because the initial transaction was
        -- commited and so someone else could have been able to insert a record
        -- into the Objects table while the next object ID was requested
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

        IF @row_count = 0       -- object is not found; create it
        BEGIN
            INSERT INTO Objects (object_id, object_key, object_loc, client_id, tm_write, tm_expiration, size, read_count, write_count, user_id)
            VALUES (@object_id, @object_key, @object_loc, @client_id, @current_time, @object_exp_if_not_found, @object_size, 0, 1, @user_id);
            SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;

            IF @error != 0 OR @row_count = 0
            BEGIN
                ROLLBACK TRANSACTION;
                RETURN 1;
            END

            -- The record was created with non-null size
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
        -- So, commit the transaction and create the record in a separate SP
        COMMIT TRANSACTION;
    END TRY
    BEGIN CATCH
        ROLLBACK TRANSACTION;

        DECLARE @error_message  NVARCHAR(4000) = ERROR_MESSAGE();
        DECLARE @error_severity INT = ERROR_SEVERITY();
        DECLARE @error_state    INT = ERROR_STATE();

        RAISERROR( @error_message, @error_severity, @error_state );
        RETURN 1;
    END CATCH


    DECLARE @ret_code   BIGINT;
    EXECUTE @ret_code = UpdateObjectOnWrite_IfNotExists @object_key,
                                                        @object_loc,
                                                        @object_size,
                                                        @client_id,
                                                        @object_exp_if_found,
                                                        @object_exp_if_not_found,
                                                        @current_time,
                                                        @user_id,
                                                        @size_was_null;

    RETURN @ret_code;
END
GO








DECLARE @row_count  INT
DECLARE @error      INT
UPDATE Versions SET version = 10 WHERE name = 'sp_code'
SELECT @row_count = @@ROWCOUNT, @error = @@ERROR
IF @error != 0 OR @row_count = 0
BEGIN
    RAISERROR( 'Cannot update the stored procedure version in the Versions DB table', 11, 1 )
END
GO

DECLARE @row_count  INT
DECLARE @error      INT
UPDATE Versions SET version = 5 WHERE name = 'db_structure'
SELECT @row_count = @@ROWCOUNT, @error = @@ERROR
IF @error != 0 OR @row_count = 0
BEGIN
    RAISERROR( 'Cannot update the DB structuree version in the Versions DB table', 11, 1 )
END
GO


-- Restore if it was changed
SET NOEXEC OFF
GO

