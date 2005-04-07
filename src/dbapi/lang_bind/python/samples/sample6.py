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
# File Name: sample6.py
#
# Author: Sergey Sikorskiy
#
# Description: a NCBI DBAPI Python extension module usage example.
# (Populate a database with data using "executemany" and List Comprehensions)
#
# ===========================================================================

# 1) Import NCBI DBAPI Python extension module
import python_ncbi_dbapi

# The shared connection object
conn = None

# All code gets the connection object via this function
def getCon():
    global conn
    return conn

# Create the schema and make sure we're not accessing an old, incompatible schema
def CreateSchema():
        # Allocate a cursor
        cu = getCon().cursor()

        # Execute a SQL statement.
        cu.execute("SELECT name from sysobjects WHERE name = 'sale_stat' AND type = 'U'")

        if len(cu.fetchall()) > 0:
                # Drop the table
                cu.execute("DROP TABLE sale_stat")

        # Create new table.
        cu.execute("""
                CREATE TABLE sale_stat (
                        year INT NOT NULL,
                        month VARCHAR(255) NOT NULL,
                        stat INT NOT NULL
                )
        """)
        getCon().commit()

def GetSaleStat():
        # Allocate a cursor
        cu = getCon().cursor()

        # Execute a SQL statement.
        cu.execute("select * from sale_stat")

        # Fetch all records using 'fetchall()'
        print cu.fetchall()

def CreateSaleStat():
        month_list = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December']
        # Allocate a cursor
        cu = getCon().cursor()

        sql = "insert into sale_stat(year, month, stat) values (@year, @month, @stat)"

        # Execute a SQL statement with many parameters simultaneously.
        cu.executemany(sql, [{'@year':year, '@month':month, '@stat':stat} for stat in range(1, 3) for year in range(2004, 2006) for month in month_list])

        # Commit transaction
        getCon().commit()

def main():
        global conn

        # Connect to a database
        # Set an optional parameter "use_std_interface" to "True"
        conn = python_ncbi_dbapi.connect('ftds', 'MSSQL', 'MS_DEV1', 'DBAPI_Sample', 'anyone', 'allowed', True)

        CreateSchema()

        CreateSaleStat()

        GetSaleStat()

if __name__ == "__main__":
        main()

# ===========================================================================
#
# $Log$
# Revision 1.3  2005/04/07 16:50:12  ssikorsk
# Added '#! /usr/bin/env python' to each sample
#
# Revision 1.2  2005/02/08 18:50:14  ssikorsk
# Adapted to the "simple mode" interface
#
# Revision 1.1  2005/01/27 22:32:32  ssikorsk
# New Python DBAPI example.
# Populate a database with data using "executemany"
# and List Comprehensions.
#
# ===========================================================================
