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
-- - table Objects::object_key type changed to VARCHAR(289) NOT NULL
-- - table Objects::object_loc type changed to VARCHAR(900) NOT NULL

-- Changes in stored procedures:





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
IF @sp_version != 7
BEGIN
    RAISERROR( 'Unexpected stored procedure version. Expected version is 7.', 11, 1 )
    SET NOEXEC ON
END
GO


-- AttrValues and Attributes tables need to be empty because of the indexes and
-- the changed data size
DECLARE @cnt BIGINT = 100;
SELECT @cnt = COUNT(*) FROM AttrValues;
IF @cnt != 0
BEGIN
    RAISERROR( 'The AttrValues table needs to be empty', 11, 1 )
    SET NOEXEC ON
END
GO

DECLARE @cnt BIGINT = 100;
SELECT @cnt = COUNT(*) FROM Attributes;
IF @cnt != 0
BEGIN
    RAISERROR( 'The Attributes table needs to be empty', 11, 1 )
    SET NOEXEC ON
END
GO






-- Finally, the DB and SP versions are checked, so we can continue


-- Extend the column length
ALTER TABLE Objects
ALTER COLUMN object_key VARCHAR(289) NOT NULL;
GO

-- Extend the column length
ALTER TABLE Objects
ALTER COLUMN object_loc VARCHAR(900) NOT NULL;
GO

-- Shorten the 'name' field to 64 characters
ALTER TABLE Attributes DROP CONSTRAINT IX_Attributes_name;
ALTER TABLE Attributes
ALTER COLUMN name VARCHAR(64) NOT NULL;
ALTER TABLE Attributes ADD CONSTRAINT
IX_Attributes_name UNIQUE NONCLUSTERED ( name );
GO


-- Need to drop the table because the data type of the 'value' columnis changed
-- dramatically. Dropping the table also drops the indexes
DROP TABLE AttrValues;
CREATE TABLE AttrValues
(
    object_id       BIGINT NOT NULL,
    attr_id         BIGINT NOT NULL,
    value           VARBINARY(900) NOT NULL
);

ALTER TABLE AttrValues ADD CONSTRAINT
PK_AttrValues PRIMARY KEY CLUSTERED ( object_id, attr_id );
GO

ALTER TABLE AttrValues ADD CONSTRAINT
FK_AttrValues_Object FOREIGN KEY ( object_id )
REFERENCES Objects ( object_id );
GO

ALTER TABLE AttrValues ADD CONSTRAINT
FK_AttrValues_Attr FOREIGN KEY ( attr_id )
REFERENCES Attributes ( attr_id );
GO



-- The tables have been changed. Now, all the stored procedures have to be updated

ALTER PROCEDURE AddAttribute
    @object_key             VARCHAR(289),
    @attr_name              VARCHAR(64),
    @attr_value             VARBINARY(900),
    @client_id              BIGINT,

    -- backward compatible

    @create_if_not_found    INT = 0,    -- to make it compatible with
                                        -- the previous implementation
    @object_expiration      DATETIME = NULL,
    @object_loc             VARCHAR(900) = NULL
AS
BEGIN
    DECLARE @object_id      BIGINT = NULL;
    DECLARE @attr_id        BIGINT = NULL;
    DECLARE @row_count      INT;
    DECLARE @error          INT;
    DECLARE @expiration     DATETIME;

    BEGIN TRANSACTION
    BEGIN TRY

        UPDATE Objects SET tm_attr_write = GETDATE(),
                           @object_id = object_id,
                           @expiration = tm_expiration
                       WHERE object_key = @object_key;
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;
        IF @error != 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN 1;
        END
        IF @row_count = 1 AND @expiration IS NOT NULL
        BEGIN
            IF @expiration < GETDATE()
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
            EXECUTE @ret_code = GetNextObjectID @object_id OUTPUT;

            IF @ret_code != 0
            BEGIN
                ROLLBACK TRANSACTION;
                RETURN 1;
            END

            INSERT INTO Objects (object_id, object_key, object_loc, client_id, tm_expiration, read_count, write_count, tm_attr_write)
            VALUES (@object_id, @object_key, @object_loc, @client_id, @object_expiration, 0, 0, GETDATE());
            SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;

            IF @error != 0 OR @row_count = 0
            BEGIN
                ROLLBACK TRANSACTION;
                RETURN 1;
            END
        END

        -- Get the attribute ID
        SELECT @attr_id = attr_id FROM Attributes WHERE name = @attr_name;
        IF @attr_id IS NULL
        BEGIN
            INSERT INTO Attributes (name) VALUES (@attr_name);
            SET @attr_id = SCOPE_IDENTITY();
        END

        -- Create or update the attribute
        UPDATE AttrValues SET value = @attr_value WHERE object_id = @object_id AND attr_id = @attr_id;
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;
        IF @error != 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN 1;
        END
        IF @row_count = 0
        BEGIN
            INSERT INTO AttrValues (object_id, attr_id, value) VALUES (@object_id, @attr_id, @attr_value);
            SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;
            IF @error != 0 OR @row_count != 1
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
    @size_was_null              INT = 0 OUT
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
            INSERT INTO Objects (object_id, object_key, object_loc, client_id, tm_create, size, tm_expiration, read_count, write_count)
            VALUES (@object_id, @object_key, @object_loc, @client_id, @object_create_tm, @object_size, @object_expiration, 0, 1)
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




ALTER PROCEDURE DelAttribute
    @object_key     VARCHAR(289),
    @attr_name      VARCHAR(64)
AS
BEGIN
    DECLARE @object_id      BIGINT = NULL;
    DECLARE @attr_id        BIGINT = NULL;
    DECLARE @row_count      INT;
    DECLARE @error          INT;
    DECLARE @expiration     DATETIME;

    BEGIN TRANSACTION
    BEGIN TRY

        UPDATE Objects SET tm_attr_write = GETDATE(),
                           @object_id = object_id,
                           @expiration = tm_expiration WHERE object_key = @object_key;
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
        IF @expiration IS NOT NULL
        BEGIN
            IF @expiration < GETDATE()
            BEGIN
                ROLLBACK TRANSACTION;
                RETURN -4;              -- object expired
            END
        END

        SELECT @attr_id = attr_id FROM Attributes WHERE name = @attr_name;
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;
        IF @error != 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN 1;
        END
        IF @row_count = 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN -2;              -- attribute is not found
        END

        DELETE FROM AttrValues WHERE object_id = @object_id AND attr_id = @attr_id
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;
        IF @error != 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN 1;
        END
        IF @row_count = 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN -3;              -- attribute value is not found
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




ALTER PROCEDURE DoesObjectExist
    @object_key     VARCHAR(289)
AS
BEGIN
    DECLARE @row_count      INT;
    DECLARE @error          INT;
    DECLARE @expiration     DATETIME;

    BEGIN TRANSACTION
    BEGIN TRY
        SELECT @expiration = tm_expiration FROM Objects WHERE object_key = @object_key;
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




ALTER PROCEDURE GetAttribute
    @object_key     VARCHAR(289),
    @attr_name      VARCHAR(64),
    @need_update    INT,
    @attr_value     VARBINARY(900) OUT
AS
BEGIN
    DECLARE @object_id      BIGINT = NULL;
    DECLARE @row_count      INT;
    DECLARE @error          INT;
    DECLARE @expiration     DATETIME;

    SET @attr_value = NULL;

    -- error handling is in two catch blocks and the SQL Studio complains
    -- if the variables defined twice.
    DECLARE @error_message      NVARCHAR(4000);
    DECLARE @error_severity     INT;
    DECLARE @error_state        INT;


    BEGIN TRANSACTION
    BEGIN TRY
        SELECT @object_id = object_id, @expiration = tm_expiration FROM Objects WHERE object_key = @object_key;
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
            END
        END
    END TRY
    BEGIN CATCH
        ROLLBACK TRANSACTION;
        SET @error_message = ERROR_MESSAGE();
        SET @error_severity = ERROR_SEVERITY();
        SET @error_state = ERROR_STATE();

        RAISERROR( @error_message, @error_severity, @error_state );
        RETURN 1;
    END CATCH


    -- Separate transaction to avoid keeping lock on the Objects table
    DECLARE @attr_id        BIGINT = NULL;

    BEGIN TRANSACTION
    BEGIN TRY
        SELECT @attr_id = attr_id FROM Attributes WHERE name = @attr_name;
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;
        IF @error != 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN 1;
        END
        IF @row_count = 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN -2;              -- attribute is not found
        END

        SELECT @attr_value = value FROM AttrValues WHERE object_id = @object_id AND attr_id = @attr_id;
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;
        IF @error != 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN 1;
        END
        IF @row_count = 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN -3;              -- attribute value is not found
        END

        IF @need_update != 0
        BEGIN
            -- Update attribute timestamp for the existing object
            UPDATE Objects SET tm_attr_read = GETDATE() WHERE object_id = @object_id;
            SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;

            IF @error != 0 OR @row_count = 0
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
        SET @error_message = ERROR_MESSAGE();
        SET @error_severity = ERROR_SEVERITY();
        SET @error_state = ERROR_STATE();

        RAISERROR( @error_message, @error_severity, @error_state );
        RETURN 1;
    END CATCH
END
GO





ALTER PROCEDURE GetAttributeNames
    @object_key     VARCHAR(289)
AS
BEGIN
    DECLARE @object_id      BIGINT = NULL;
    DECLARE @row_count      INT;
    DECLARE @error          INT;
    DECLARE @expiration     DATETIME;

    -- error handling is in two catch blocks and the SQL Studio complains
    -- if the variables defined twice.
    DECLARE @error_message      NVARCHAR(4000);
    DECLARE @error_severity     INT;
    DECLARE @error_state        INT;

    BEGIN TRANSACTION
    BEGIN TRY
        SELECT @object_id = object_id, @expiration = tm_expiration FROM Objects WHERE object_key = @object_key;
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
    END TRY
    BEGIN CATCH
        ROLLBACK TRANSACTION;
        SET @error_message = ERROR_MESSAGE();
        SET @error_severity = ERROR_SEVERITY();
        SET @error_state = ERROR_STATE();

        RAISERROR( @error_message, @error_severity, @error_state );
        RETURN 1;
    END CATCH

    -- Separate transaction to avoid keeping lock on the Objects table
    BEGIN TRANSACTION
    BEGIN TRY
        -- This is the output recordset!
        SELECT name FROM Attributes AS a, AttrValues AS b
                    WHERE a.attr_id = b.attr_id AND b.object_id = @object_id;
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
        SET @error_message = ERROR_MESSAGE();
        SET @error_severity = ERROR_SEVERITY();
        SET @error_state = ERROR_STATE();

        RAISERROR( @error_message, @error_severity, @error_state );
        RETURN 1;
    END CATCH
END
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
        SELECT @client_id = client_id FROM Clients WHERE name = @client_name;
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



ALTER PROCEDURE GetObjectExpiration
    @object_key     VARCHAR(289),
    @expiration     DATETIME OUT
AS
BEGIN
    DECLARE @row_count      INT;
    DECLARE @error          INT;

    -- To have a definitive value returned from the procedure even if
    -- there is no object
    SET @expiration = NULL;

    BEGIN TRANSACTION
    BEGIN TRY
        SELECT @expiration = tm_expiration FROM Objects WHERE object_key = @object_key;
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




ALTER PROCEDURE GetObjectFixedAttributes
    @object_key     VARCHAR(289),
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
        SELECT @client_name = name FROM Clients WHERE client_id = @cl_id;
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




ALTER PROCEDURE GetObjectSize
    @object_key     VARCHAR(289),
    @object_size    BIGINT OUT
AS
BEGIN
    DECLARE @row_count      INT;
    DECLARE @error          INT;
    DECLARE @expiration     DATETIME;

    BEGIN TRANSACTION
    BEGIN TRY
        SELECT @expiration = tm_expiration, @object_size = size FROM Objects WHERE object_key = @object_key;
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





ALTER PROCEDURE RemoveObject
    @object_key     VARCHAR(289)
AS
BEGIN
    DECLARE @object_id      BIGINT = NULL;
    DECLARE @expiration     DATETIME;

    BEGIN TRANSACTION
    BEGIN TRY
        SELECT @object_id = object_id, @expiration = tm_expiration FROM Objects WHERE object_key = @object_key;
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

        IF @expiration IS NOT NULL
        BEGIN
            IF @expiration < GETDATE()
            BEGIN
                ROLLBACK TRANSACTION;
                RETURN -4;          -- object is expired
                                    -- -4 is to make it unified with the other SPs
            END
        END

        DELETE FROM AttrValues WHERE object_id = @object_id;
        DELETE FROM Objects WHERE object_id = @object_id;

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
ALTER PROCEDURE SetObjectExpiration
    @object_key             VARCHAR(289),
    @expiration             DATETIME,

    -- backward compatible

    @create_if_not_found    INT = 0,    -- to make it compatible with
                                        -- the previous implementation
    @object_loc             VARCHAR(900) = NULL,
    @client_id              BIGINT = NULL,
    @object_size            BIGINT = NULL OUT
AS
BEGIN
    BEGIN TRANSACTION
    BEGIN TRY
        DECLARE @row_count          INT;
        DECLARE @error              INT;
        DECLARE @old_expiration     DATETIME;


        -- Do not reduce the expiration
        -- Note: T-SQL lets to save the old value of a column
        UPDATE Objects SET @old_expiration = tm_expiration,
                           @object_size = size,
                           tm_expiration = @expiration
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

            INSERT INTO Objects (object_id, object_key, object_loc, client_id, tm_expiration, read_count, write_count)
            VALUES (@object_id, @object_key, @object_loc, @client_id, @expiration, 0, 0);
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
            INSERT INTO Objects (object_id, object_key, object_loc, client_id, tm_read, tm_expiration, size, read_count, write_count)
            VALUES (@object_id, @object_key, @object_loc, @client_id, @current_time, @object_exp_if_not_found, @object_size, 1, 1);
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




ALTER PROCEDURE UpdateObjectOnRead
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

        IF @row_count = 1       -- The object has been found and updated
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

            COMMIT TRANSACTION;
            RETURN 0;
        END

        -- Here: the update did not change a record because it does not exist
        -- So, commit the transaction. The record will be created in another SP.
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
    EXECUTE @ret_code = UpdateObjectOnRead_IfNotExists @object_key, @object_loc,
                                                       @object_size, @client_id,
                                                       @object_exp_if_found,
                                                       @object_exp_if_not_found,
                                                       @current_time, @size_was_null;

    RETURN @ret_code;
END
GO




ALTER PROCEDURE UpdateObjectOnRelocate_IfNotExists
    @object_key                 VARCHAR(289),
    @object_loc                 VARCHAR(900),
    @client_id                  BIGINT,

    -- make it compatible with the previous version of the server
    @current_time               DATETIME = NULL,
    @object_exp_if_found        DATETIME = NULL,
    @object_exp_if_not_found    DATETIME = NULL

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
        DECLARE @row_count  INT;
        DECLARE @error      INT;

        IF @current_time IS NULL
        BEGIN
            -- old version of the server
            UPDATE Objects SET object_loc = @object_loc WHERE object_key = @object_key;
            SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;
        END
        ELSE
        BEGIN
            -- new version of the server
            UPDATE Objects SET object_loc = @object_loc,
                               tm_expiration = @object_exp_if_found
                           WHERE object_key = @object_key;
            SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;
        END

        IF @error != 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN 1;
        END

        IF @row_count = 0       -- object is not found; create it
        BEGIN
            IF @current_time IS NULL
            BEGIN
                -- old version of the server
                INSERT INTO Objects (object_id, object_key, object_loc, client_id)
                VALUES (@object_id, @object_key, @object_loc, @client_id);
                SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;
            END
            ELSE
            BEGIN
                -- new version of the server
                INSERT INTO Objects (object_id, object_key, object_loc, client_id, tm_expiration)
                VALUES (@object_id, @object_key, @object_loc, @client_id, @object_exp_if_not_found);
                SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;
            END

            IF @error != 0 OR @row_count = 0
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

        DECLARE @error_message  NVARCHAR(4000) = ERROR_MESSAGE();
        DECLARE @error_severity INT = ERROR_SEVERITY();
        DECLARE @error_state    INT = ERROR_STATE();

        RAISERROR( @error_message, @error_severity, @error_state );
        RETURN 1;
    END CATCH
END
GO



ALTER PROCEDURE UpdateObjectOnRelocate
    @object_key                 VARCHAR(289),
    @object_loc                 VARCHAR(900),
    @client_id                  BIGINT,

    -- make it compatible with the previous version of the server
    @current_time               DATETIME = NULL,
    @object_exp_if_found        DATETIME = NULL,
    @object_exp_if_not_found    DATETIME = NULL
AS
BEGIN
    BEGIN TRANSACTION
    BEGIN TRY
        DECLARE @row_count  INT;
        DECLARE @error      INT;

        IF @current_time IS NULL
        BEGIN
            -- Old version of the server
            UPDATE Objects SET object_loc = @object_loc
                           WHERE object_key = @object_key;
            SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;
        END
        ELSE
        BEGIN
            -- New version of the server
            UPDATE Objects SET object_loc = @object_loc,
                               tm_expiration = @object_exp_if_found
                           WHERE object_key = @object_key;
            SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;
        END

        IF @error != 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN 1;
        END

        IF @row_count = 1       -- The object has been found and updated
        BEGIN
            COMMIT TRANSACTION;
            RETURN 0;
        END

        -- the update did not change a record because it does not exist
        -- finish the transaction and go ahead
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

    -- Create the record
    DECLARE @ret_code   BIGINT;
    EXECUTE @ret_code = UpdateObjectOnRelocate_IfNotExists @object_key,
                                                           @object_loc,
                                                           @client_id,
                                                           @current_time,
                                                           @object_exp_if_found,
                                                           @object_exp_if_not_found;
    RETURN @ret_code;
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
            INSERT INTO Objects (object_id, object_key, object_loc, client_id, tm_write, tm_expiration, size, read_count, write_count)
            VALUES (@object_id, @object_key, @object_loc, @client_id, @current_time, @object_exp_if_not_found, @object_size, 0, 1);
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
                           size = @object_size
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
                                                        @size_was_null;

    RETURN @ret_code;
END
GO




ALTER PROCEDURE UpdateObjectSizeIfNULL
    @object_key     VARCHAR(289),
    @object_size    BIGINT OUT
AS
BEGIN
    DECLARE @row_count      INT;
    DECLARE @error          INT;
    DECLARE @expiration     DATETIME;
    DECLARE @updated_size   BIGINT;

    BEGIN TRANSACTION
    BEGIN TRY


        UPDATE Objects SET @expiration = tm_expiration,
                           @updated_size = size = ( CASE
                                                        WHEN size is NULL THEN @object_size
                                                        ELSE size
                                                    END )
                       WHERE object_key = @object_key;
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;
        IF @error != 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN 1;           -- SQL execution error
        END
        IF @row_count = 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN -1;          -- object is not found
        END
        IF @expiration IS NOT NULL
        BEGIN
            IF @expiration < GETDATE()
            BEGIN
                ROLLBACK TRANSACTION;
                RETURN -4;          -- object is expired
                                    -- -4 is to make it unified with the other SPs
            END
        END

        -- returning back the updated object size which could have been changed
        -- since it was initially requested
        SET @object_size = @updated_size
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





-- Newly introduced procedure for the internal usage. It is not supposed to be
-- executed from the C++ code
ALTER PROCEDURE UpdateUserKeyObjectOnWrite_IfNotExists
    @object_key                 VARCHAR(289),
    @object_loc                 VARCHAR(900),
    @object_size                BIGINT,
    @client_id                  BIGINT,
    @object_exp_if_found        DATETIME,
    @object_exp_if_not_found    DATETIME,
    @current_time               DATETIME
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
        DECLARE @row_count      INT;
        DECLARE @error          INT;

        -- See the more detailed description in the UpdateUserKeyObjectOnWrite() procedure

        -- Need to try to update once again because the initial transaction was
        -- commited and so someone else could have been able to insert a record
        -- into the Objects table while the next object ID was requested
        UPDATE Objects SET size = @object_size,
                           tm_write = @current_time,
                           write_count = ( CASE
                                                WHEN tm_expiration IS NOT NULL AND tm_expiration < @current_time THEN 1
                                                ELSE write_count + 1
                                           END ),
                           read_count = ( CASE
                                                WHEN tm_expiration IS NOT NULL AND tm_expiration < @current_time THEN 0
                                                ELSE read_count
                                          END ),
                           tm_expiration = ( CASE
                                                WHEN tm_expiration IS NOT NULL AND tm_expiration < @current_time THEN @object_exp_if_not_found
                                                WHEN tm_expiration IS NOT NULL AND tm_expiration >= @current_time THEN @object_exp_if_found
                                                ELSE tm_expiration
                                             END )
                       WHERE object_key = @object_key;
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;

        IF @error != 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN 1;
        END

        IF @row_count = 0       -- object is not found; create it
        BEGIN
            -- For the user key objects creation time is set as well
            INSERT INTO Objects (object_id, object_key, object_loc, client_id, tm_create, tm_write, tm_expiration, size, read_count, write_count)
            VALUES (@object_id, @object_key, @object_loc, @client_id, @current_time, @current_time, @object_exp_if_not_found, @object_size, 0, 1);
            SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;

            IF @error != 0 OR @row_count = 0
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

        DECLARE @error_message  NVARCHAR(4000) = ERROR_MESSAGE();
        DECLARE @error_severity INT = ERROR_SEVERITY();
        DECLARE @error_state    INT = ERROR_STATE();

        RAISERROR( @error_message, @error_severity, @error_state );
        RETURN 1;
    END CATCH
END
GO




ALTER PROCEDURE UpdateUserKeyObjectOnWrite
    @object_key                 VARCHAR(289),
    @object_loc                 VARCHAR(900),
    @object_size                BIGINT,
    @client_id                  BIGINT,
    @object_exp_if_found        DATETIME,
    @object_exp_if_not_found    DATETIME,
    @current_time               DATETIME
AS
BEGIN
    BEGIN TRANSACTION
    BEGIN TRY
        DECLARE @row_count      INT;
        DECLARE @error          INT;

        -- If the record exists then the fields to update (and their values) depend
        -- on the expiration value. The UPDATE statement below includes all these cases
        -- in the CASE ... WHEN ... END clauses. This is done to improve the performance.
        -- The initial version of the procedure did the same via 3 scans: UPDATE/SELECT/UPDATE
        -- Now it is (less clear though) just one scan in the case of the existing records.
        UPDATE Objects SET size = @object_size,
                           tm_write = @current_time,
                           write_count = ( CASE
                                                WHEN tm_expiration IS NOT NULL AND tm_expiration < @current_time THEN 1
                                                ELSE write_count + 1
                                           END ),
                           read_count = ( CASE
                                                WHEN tm_expiration IS NOT NULL AND tm_expiration < @current_time THEN 0
                                                ELSE read_count
                                          END ),
                           tm_expiration = ( CASE
                                                WHEN tm_expiration IS NOT NULL AND tm_expiration < @current_time THEN @object_exp_if_not_found
                                                WHEN tm_expiration IS NOT NULL AND tm_expiration >= @current_time THEN @object_exp_if_found
                                                ELSE tm_expiration
                                             END )
                       WHERE object_key = @object_key;
        SELECT @row_count = @@ROWCOUNT, @error = @@ERROR;

        IF @error != 0
        BEGIN
            ROLLBACK TRANSACTION;
            RETURN 1;
        END

        IF @row_count = 1       -- The object has been found and updated
        BEGIN
            COMMIT TRANSACTION;
            RETURN 0;
        END
    END TRY
    BEGIN CATCH
        ROLLBACK TRANSACTION;

        DECLARE @error_message  NVARCHAR(4000) = ERROR_MESSAGE();
        DECLARE @error_severity INT = ERROR_SEVERITY();
        DECLARE @error_state    INT = ERROR_STATE();

        RAISERROR( @error_message, @error_severity, @error_state );
        RETURN 1;
    END CATCH

    -- Here: the update did not change a record because it does not exist
    -- So, commit the transaction and create the record
    COMMIT TRANSACTION;

    DECLARE @ret_code   BIGINT;
    EXECUTE @ret_code = UpdateUserKeyObjectOnWrite_IfNotExists @object_key, @object_loc, @object_size, @client_id, @object_exp_if_found, @object_exp_if_not_found, @current_time;

    RETURN @ret_code;
END
GO





DECLARE @row_count  INT
DECLARE @error      INT
UPDATE Versions SET version = 8 WHERE name = 'sp_code'
SELECT @row_count = @@ROWCOUNT, @error = @@ERROR
IF @error != 0 OR @row_count = 0
BEGIN
    RAISERROR( 'Cannot update the stored procedure version in the Versions DB table', 11, 1 )
END
GO

DECLARE @row_count  INT
DECLARE @error      INT
UPDATE Versions SET version = 3 WHERE name = 'db_structure'
SELECT @row_count = @@ROWCOUNT, @error = @@ERROR
IF @error != 0 OR @row_count = 0
BEGIN
    RAISERROR( 'Cannot update the DB structuree version in the Versions DB table', 11, 1 )
END
GO


-- Restore if it was changed
SET NOEXEC OFF
GO

