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
# File Name: sample8.py
#
# Author: Sergey Sikorskiy
#
# Description: a basic NCBI DBAPI Python extension module usage pattern
# ( operations with a database using the "simple mode" )
#
# ===========================================================================

# 1) Import NCBI DBAPI Python extension module
import python_ncbi_dbapi

# 2) Connect to a database
# Parameters: connect(driver_name, db_type, server_name, db_name, user_name, user_pswd, use_std_interface)
# driver_name: ctlib, dblib, ftds, odbc, mysql, msdblib
# db_type (case insensitive): SYBASE, MSSQL, MYSQL
# server_name: database server name
# db_name: default database name
# use_std_interface: an optional parameter (default value is "False")

# Open a connection to a database using the "simple mode" ( default behavior )
conn = python_ncbi_dbapi.connect('ftds', 'MSSQL', 'MS_DEV1', 'DBAPI_Sample', 'anyone', 'allowed')

# 3) Allocate a cursor
cursor = conn.cursor()

# Create a temporary table
cursor.execute("""
        CREATE TABLE #sale_stat (
                year INT NOT NULL,
                month VARCHAR(255) NOT NULL,
                stat INT NOT NULL
        )
""")

month_list = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December']
sql = "insert into #sale_stat(year, month, stat) values (@year, @month, @stat)"

# Check that the temporary table was successfully created and we can get data from it
cursor.execute("select * from #sale_stat")
print "Empty table contains", len( cursor.fetchall() ), "records"

# Start transaction
cursor.execute('BEGIN TRANSACTION')

# Insert records
cursor.executemany(sql, [{'@year':year, '@month':month, '@stat':stat} for stat in range(1, 3) for year in range(2004, 2006) for month in month_list])

# Check how many records we have inserted
cursor.execute("select * from #sale_stat")
print "We have inserted", len( cursor.fetchall() ), "records"

# "Standard interface" rollback
conn.rollback();

# Check how many records left after "standard" ROLLBACK
# "Standard interface" rollback command is not supposed to affect current transaction.
cursor.execute("select * from #sale_stat")
print "After a 'standard' rollback command the table contains", len( cursor.fetchall() ), "records"

# Rollback transaction
cursor.execute('ROLLBACK TRANSACTION')
# Start transaction
cursor.execute('BEGIN TRANSACTION')

# Check how many records left after ROLLBACK
cursor.execute("select * from #sale_stat")
print "After a 'manual' rollback command the table contains", len( cursor.fetchall() ), "records"

# Insert records again
cursor.executemany(sql, [{'@year':year, '@month':month, '@stat':stat} for stat in range(1, 3) for year in range(2004, 2006) for month in month_list])

# Check how many records we have inserted
cursor.execute("select * from #sale_stat")
print "We have inserted", len( cursor.fetchall() ), "records"

# Commit transaction
cursor.execute('COMMIT TRANSACTION')

# Check how many records left after COMMIT
cursor.execute("select * from #sale_stat")
print "After a 'manual' commit command the table contains", len( cursor.fetchall() ), "records"

# ===========================================================================
#
# $Log$
# Revision 1.2  2005/02/10 16:52:42  ssikorsk
# Added demonstration of difference between 'standard' and 'manual' transaction processing in the 'simple' mode.
#
# Revision 1.1  2005/02/08 21:16:48  ssikorsk
# New Python code example
#
# ===========================================================================
