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
-- - GetStatInfo: better performance




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
IF @sp_version != 5
BEGIN
    RAISERROR( 'Unexpected stored procedure version. Expected version is 5.', 11, 1 )
    SET NOEXEC ON
END
GO




-- Finally, the DB and SP versions are checked, so we can continue

ALTER PROCEDURE GetStatInfo
AS
BEGIN
    BEGIN TRANSACTION
        -- https://msdn.microsoft.com/en-us/library/ms187737.aspx shows a faster way
        -- of retrieving the number of rows in a table. However this way requires
        -- additional permissions to the user which would be nice to avoid to grant.
        -- So, as suggested in DBH-8384 another way is used instead of
        -- plain count(*)
        SELECT rows AS TotalObjectCount FROM sysindexes WHERE id = OBJECT_ID('Objects') AND indid = 1
        SELECT count(*) AS ExpiredObjectCount FROM Objects WHERE tm_expiration IS NOT NULL AND tm_expiration < GETDATE()
        SELECT rows AS ClientCount FROM sysindexes WHERE id = OBJECT_ID('Clients') AND indid = 1
        SELECT rows AS AttributeCount FROM sysindexes WHERE id = OBJECT_ID('Attributes') AND indid = 1
        SELECT rows AS AttributeValueCount FROM sysindexes WHERE id = OBJECT_ID('AttrValues') AND indid = 1
        SELECT version AS DBStructureVersion FROM Versions WHERE name = 'db_structure'
        SELECT version AS SPCodeVersion FROM Versions WHERE name = 'sp_code'
    COMMIT TRANSACTION
    RETURN 0
END
GO






DECLARE @row_count  INT
DECLARE @error      INT
UPDATE Versions SET version = 6 WHERE name = 'sp_code'
SELECT @row_count = @@ROWCOUNT, @error = @@ERROR
IF @error != 0 OR @row_count = 0
BEGIN
    RAISERROR( 'Cannot update the stored procedure version in the Versions DB table', 11, 1 )
END
GO


-- Restore if it was changed
SET NOEXEC OFF
GO


