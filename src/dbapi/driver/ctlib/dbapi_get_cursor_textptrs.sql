-- $Id$

-- Temporary procedures (loaded on demand) to work around not automatically
-- getting TEXTPTR data in TDS 7.2+ (needed for VARCHAR(MAX) et al.).
-- All yield a status result indicating the number of TEXTPTRs found, or
-- a negative number in the case of an error (also reported via RAISERROR).

-- Helper that tries to obtain a single textptr.
CREATE PROCEDURE [#dbapi_get_cursor_textptr_internal]
  @textptr   VARBINARY(16)  OUTPUT,
  @cursor_id sysname,
  @column    INT, -- 0-based for a specific column, negative to search all
  @position  INT,
  @type      INT,
  @column_id INT,
  @object_id INT,
  @dbid      INT,
  @dbname    sysname
AS
BEGIN
  DECLARE @status INT;
  DECLARE @sql    NVARCHAR(2000);
  DECLARE @params NVARCHAR(50);

  -- TYPE_ID('IMAGE'), ('TEXT'), and ('NTEXT') respectively.
  IF (@type != 34 AND @type != 35 AND @type != 99)
     OR (@column >= 0 AND @position != @column)
  BEGIN
    IF @position = @column
    BEGIN
      DECLARE @type_name sysname = ISNULL(TYPE_NAME(@type),
                                          N'no name available');
      RAISERROR('Column %d has unsupported type %d (%s)', 11, 1,
                @column, @type, @type_name);
    END;
    RETURN 0;
  END;

  SET @dbname = QUOTENAME(@dbname);

  DECLARE @table_name sysname = OBJECT_NAME(@object_id, @dbid);
  IF @table_name IS NULL
    RAISERROR('Unable to identify table with ID %d in %s', 11, 2,
              @object_id, @dbname);

  DECLARE @table_owner sysname = OBJECTPROPERTY(@object_id, N'OwnerID');
  SET @table_owner = ISNULL(QUOTENAME(@table_owner), N'');

  DECLARE @orig_name sysname = COL_NAME(@object_id, @column_id);
  IF @orig_name IS NULL -- COL_NAME may fail, at least for tempdb
  BEGIN
    SET @sql = N'SELECT @on = name FROM ' + @dbname
             + N'.[sys].[columns] WHERE object_id = @oid AND column_id = @cid';
    EXEC [sys].[sp_executesql] @sql, N'@on sysname OUTPUT, @oid INT, @cid INT',
      @on = @orig_name OUTPUT, @oid = @object_id, @cid = @column_id;
  END;
  IF @orig_name IS NULL
    RAISERROR('Unable to identify column name for %s.%s(%d).%d', 11, 3,
              @dbname, @table_name, @object_id, @column_id);
  SET @orig_name = QUOTENAME(@orig_name);

  DECLARE @fullname NVARCHAR(1000);
  IF SUBSTRING(@table_name, 1, 1) = '#'
    SET @fullname = @table_name;
  ELSE
    SET @fullname = @dbname + N'.' + @table_owner + N'.'
                  + QUOTENAME(@table_name);

  DECLARE @uuid UNIQUEIDENTIFIER = NEWID();
  DECLARE @uuid_type NVARCHAR(13);
  IF @type = 34 -- TYPE_ID('IMAGE')
    SET @uuid_type = N'VARBINARY(17)';
  ELSE
    SET @uuid_type = N'VARCHAR(37)';

  SET @sql = N'UPDATE ' + @fullname + N' SET ' + @orig_name
           + N' = @uuid WHERE CURRENT OF ' + QUOTENAME(@cursor_id);
  SET @params = N'@uuid ' + @uuid_type;
  EXEC @status = [sys].[sp_executesql] @sql, @params, @uuid = @uuid;
  IF @status != 0 OR @@ROWCOUNT != 1
    RAISERROR('Failed to mark precisely one datum.  Status: %d; count: %d',
              11, 4, @status, @@ROWCOUNT);

  SET @sql = N'SELECT @tp = TEXTPTR(' + @orig_name + N') FROM ' + @fullname
           + N' WHERE CAST(' + @orig_name + N' AS ' + @uuid_type
           + N') = @uuid';
  SET @params = N'@tp VARBINARY(16) OUTPUT, @uuid ' + @uuid_type;
  EXEC @status = [sys].[sp_executesql] @sql, @params,
    @tp = @textptr OUTPUT, @uuid = @uuid;
  IF @status != 0 OR @@ROWCOUNT != 1 OR @textptr IS NULL
    RAISERROR('Failed to identify text pointer.  Status: %d; count: %d',
              11, 5, @status, @@ROWCOUNT);

  RETURN 1;
END;
GO

-- Common logic between the two entry points.
CREATE PROCEDURE [#dbapi_get_cursor_textptr_or_ptrs]
  @textptr    VARBINARY(16) OUTPUT,
  @cursor_id  sysname,
  @cursor_src sysname = NULL,
  @column     INT
AS
BEGIN
  IF @cursor_src IS NULL
  BEGIN
    IF SUBSTRING(@cursor_id, 1, 1) = N'@'
      SET @cursor_src = N'variable';
    ELSE
      -- "API cursor" names are global (but explicit cursors might not be)
      SET @cursor_src = N'global';
  END;

  DECLARE @ret    INT = 0,
          @status INT,
          @cv     CURSOR;

  EXEC @status = [sys].[sp_describe_cursor_columns] @cursor_return=@cv OUTPUT,
    @cursor_source = @cursor_src, @cursor_identity = @cursor_id;

  IF @status = 0
  BEGIN
    BEGIN TRY
      BEGIN TRANSACTION DBAPIGetCurTPTxn;
      -- Use a formal save point, to which it will be possible to ROLLBACK
      -- without affecting any outer transactions.
      SAVE TRANSACTION DBAPIGetCurTPSavePoint;

      WHILE @status = 0
      BEGIN
        DECLARE @column_name sysname,
                @position    INT,
                @flags       INT,
                @size        INT,
                @type        SMALLINT,
                @prec        TINYINT,
                @scale       TINYINT,
                @order_pos   INT,
                @order_dir   VARCHAR,
                @hidden      SMALLINT,
                @column_id   INT,
                @object_id   INT,
                @dbid        INT,
                @dbname      sysname;

        FETCH NEXT FROM @cv INTO @column_name, @position, @flags,
          @size, @type, @prec, @scale, @order_pos, @order_dir,
          @hidden, @column_id, @object_id, @dbid, @dbname;
        SET @status = @@FETCH_STATUS;
        IF @status != 0
          BREAK;

        DECLARE @count INT;
        EXEC @count = [#dbapi_get_cursor_textptr_internal]
          @textptr = @textptr OUTPUT, @cursor_id = @cursor_id,
          @column = @column, @position = @position, @type = @type,
          @column_id = @column_id, @object_id = @object_id,
          @dbid = @dbid, @dbname = @dbname;
        -- Alternating tests yield row results only when actually wanted.
        IF @count > 0
          SET @ret = @ret + @count;
        IF @count < 0 OR @position = @column -- done with @cv
          BREAK;
        IF @count > 0
          SELECT @position AS [position], @textptr AS [textptr];
      END;

      ROLLBACK TRANSACTION DBAPIGetCurTPSavePoint;
      COMMIT TRANSACTION DBAPIGetCurTPTxn;
    END TRY
    BEGIN CATCH
      DECLARE @msg NVARCHAR(4000) = ERROR_MESSAGE(),
              @num INT            = ERROR_NUMBER(),
              @sev INT            = ERROR_SEVERITY();

      IF @num > 255
      BEGIN
        SET @msg = @msg + N'  Original message number: ' + STR(@num) + N'.';
        SET @num = 255;
      END;

      BEGIN TRY
        ROLLBACK TRANSACTION DBAPIGetCurTPSavePoint;
        COMMIT TRANSACTION DBAPIGetCurTPTxn;
      END TRY
      BEGIN CATCH
      END CATCH;
      RAISERROR('%s', @sev, @num, @msg);
      RETURN -@num;
    END CATCH;
  END;

  RETURN @ret;
END;
GO

-- Return a single cursor column's textptr, via an output variable.
CREATE PROCEDURE [#dbapi_get_cursor_textptr]
  @textptr    VARBINARY(16) OUTPUT,
  @cursor_id  sysname,
  @cursor_src sysname = NULL,
  @column     INT
AS
BEGIN
  IF @column < 0
  BEGIN
    RAISERROR('Negative column number %d not allowed', 11, 10, @column);
    RETURN -10;
  END;

  DECLARE @ret INT;

  EXEC @ret = [#dbapi_get_cursor_textptr_or_ptrs] @textptr = @textptr OUTPUT,
    @cursor_id = @cursor_id, @cursor_src = @cursor_src, @column = @column;
  IF @ret = 0
  BEGIN
    RAISERROR('Column with number %d not found', 11, 11, @column);
    RETURN -11;
  END;

  RETURN @ret;
END;
GO

-- Return all legacy blob columns' textptrs, one row result set each.
CREATE PROCEDURE [#dbapi_get_cursor_textptrs]
  @cursor_id  sysname,
  @cursor_src sysname = NULL
AS
BEGIN
  DECLARE @last_tp VARBINARY(16),
          @ret     INT;
  EXEC @ret = [#dbapi_get_cursor_textptr_or_ptrs] @textptr = @last_tp OUTPUT,
    @cursor_id = @cursor_id, @cursor_src = @cursor_src, @column = -1;
  RETURN @ret;
END;
GO
