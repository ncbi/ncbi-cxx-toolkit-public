IF OBJECT_ID('testDecimal') IS NOT NULL DROP PROC testDecimal
go
CREATE PROCEDURE testDecimal
  @idecimal NUMERIC(20,10)
AS
BEGIN
	SELECT CAST(@idecimal*2 AS NUMERIC(20,10))
END
go
IF OBJECT_ID('testDecimal') IS NOT NULL DROP PROC testDecimal
go

