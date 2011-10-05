#! /usr/bin/env python

# $Id$
# ===========================================================================
#
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
#
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
#
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
#
#  Please cite the author in any work or product based on this material.
#
# ===========================================================================
#
# Author:  Pavel Ivanov
#
#

from __future__ import with_statement
from python_ncbi_dbapi import *


def check(cond, msg):
    if not cond:
        raise Exception(msg)

def checkEqual(var1, var2):
    check(var1 == var2, "Values not equal: '" + str(var1) + "' !='" + str(var2) + "'")


conn = connect('ftds', 'MSSQL', 'MSDEV1', 'DBAPI_Sample', 'DBAPI_test', 'allowed')
cursor = conn.cursor()

cursor.execute('select qq = 57 + 33')
result = cursor.fetchone()
checkEqual(result[0], 90)


cursor.execute('select name, type from sysobjects')
checkEqual(len(cursor.fetchone()), 2)
checkEqual(len(cursor.fetchmany(1)), 1)
checkEqual(len(cursor.fetchmany(2)), 2)
checkEqual(len(cursor.fetchmany(3)), 3)
cursor.fetchall()


cursor.execute('select name, type from sysobjects where type = @type_par', {'@type_par':'S'})
cursor.fetchall()


cursor.execute("""
        CREATE TABLE #sale_stat (
                year INT NOT NULL,
                month VARCHAR(255) NOT NULL,
                stat INT NOT NULL
        )
""")

month_list = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December']
umonth_list = [u'January', u'February', u'March', u'April', u'May', u'June', u'July', u'August', u'September', u'October', u'November', u'December']
sql = "insert into #sale_stat(year, month, stat) values (@year, @month, @stat)"

# Check that the temporary table was successfully created and we can get data from it
cursor.execute("select * from #sale_stat")
checkEqual(len( cursor.fetchall() ), 0)
# Start transaction
cursor.execute('BEGIN TRANSACTION')
# Insert records
cursor.executemany(sql, [{'@year':year, '@month':month, '@stat':stat} for stat in range(1, 3) for year in range(2004, 2006) for month in month_list])
# Check how many records we have inserted
cursor.execute("select * from #sale_stat")
checkEqual(len( cursor.fetchall() ), 48)
# "Standard interface" rollback
conn.rollback();
# Check how many records left after "standard" ROLLBACK
# "Standard interface" rollback command is not supposed to affect current transaction.
cursor.execute("select * from #sale_stat")
checkEqual(len( cursor.fetchall() ), 48)
# Rollback transaction
cursor.execute('ROLLBACK TRANSACTION')
# Start transaction
cursor.execute('BEGIN TRANSACTION')
# Check how many records left after ROLLBACK
cursor.execute("select * from #sale_stat")
checkEqual(len( cursor.fetchall() ), 0)
# Insert records again
cursor.executemany(sql, [[year, month, stat] for stat in range(1, 3) for year in range(2004, 2006) for month in umonth_list])
# Check how many records we have inserted
cursor.execute("select * from #sale_stat")
checkEqual(len( cursor.fetchall() ), 48)
# Commit transaction
cursor.execute('COMMIT TRANSACTION')
# Check how many records left after COMMIT
cursor.execute("select * from #sale_stat")
checkEqual(len( cursor.fetchall() ), 48)

cursor.execute("select month from #sale_stat")
month = cursor.fetchone()[0]
check(isinstance(month, str), "Value of str should be regular string")
return_strs_as_unicode(True)
cursor.execute("select month from #sale_stat")
month = cursor.fetchone()[0]
check(isinstance(month, unicode), "Value of str should be unicode string")
return_strs_as_unicode(False)
cursor.execute("select month from #sale_stat")
month = cursor.fetchone()[0]
check(isinstance(month, str), "Value of str should be regular string")


cursor.callproc('sp_databases')
cursor.fetchall()
# Retrieve return status
checkEqual(cursor.get_proc_return_status(), 0)
# Call a stored procedure with a parameter.
cursor.callproc('sp_server_info', {'@attribute_id':1} )
cursor.fetchall()
# Retrieve return status
checkEqual(cursor.get_proc_return_status(), 0)
# Call a stored procedure with a parameter.
cursor.callproc('sp_server_info', [1] )
cursor.fetchall()
# Retrieve return status
checkEqual(cursor.get_proc_return_status(), 0)
# Call Stored Procedure using an "execute" method.
cursor.execute('sp_databases')
cursor.fetchall()
# Retrieve return status
checkEqual(cursor.get_proc_return_status(), 0)
# Call a stored procedure with a parameter.
cursor.execute('execute sp_server_info 1')
cursor.fetchall()
# Retrieve return status
checkEqual(cursor.get_proc_return_status(), 0)


# check output parameters
try:
    cursor.execute("drop procedure testing")
except:
    pass
cursor.execute("create procedure testing (@p1 int, @p2 int output) as begin\nset @p1 = 123\nset @p2 = 123\nend")
cursor.execute("grant execute on testing to DBAPI_test")
out = cursor.callproc('testing', [None, None])
if isinstance(out[1], str):
    raise Exception('Invalid data type of param2 (string)')
if isinstance(out[1], None.__class__):
    raise Exception('param2 was not returned')
if not isinstance(out[0], None.__class__):
    raise Exception('Invalid data type of param1: ' + repr(out[0].__class__))
out = cursor.callproc('testing', [456, None])
if isinstance(out[1], str):
    raise Exception('Invalid data type of param2 (string)')
if out[0] != 456:
    raise Exception('Invalid value of param1: ' + out[0])
cursor.execute("drop procedure testing")


#check procedure names
try:
    cursor.execute("drop procedure create_testing")
except:
    pass
cursor.execute("create procedure create_testing as begin\nselect 1, 2, 3\nend")
cursor.execute("grant execute on create_testing to DBAPI_test")
cursor.callproc('create_testing')
cursor.fetchall()[0]



conn = connect("ftds", "SYB", "PUBSEQ_OS", "master", "anyone", "allowed")
cursor = conn.cursor()
cursor.execute('id_get_accn_ver_by_gi 2')
cursor.fetchall()


conn = connect('ftds', 'MSSQL', 'MSDEV', 'DBAPI_ConnectionTest1', 'anyone', 'allowed')
cursor = conn.cursor()
cursor.execute("SELECT @@servername")
cursor.fetchall()


conn = connect('ftds', 'MSSQL', 'MSDEV', 'DBAPI_ConnectionTest2', 'anyone', 'allowed')
cursor = conn.cursor()
cursor.execute("SELECT @@servername")
cursor.fetchall()


conn = connect('ftds', 'MSSQL', 'DBAPI_MS_TEST', 'DBAPI_Sample', 'DBAPI_test', 'allowed')
with conn.cursor() as cursor:
    for row in cursor.execute("exec sp_spaceused"):
        rr = row

    cursor.nextset()
    for row in cursor:
        rr = row

try:
    cursor.execute("select * from sysobjects")
    raise Exception("DatabseError was not thrown")
except DatabaseError:
    pass

cursor = conn.cursor()
for row in cursor.execute("exec sp_spaceused"):
    rr = row
cursor = conn.cursor()
cursor.execute("select * from sysobjects")
cursor.fetchall()


print 'All tests completed successfully'
