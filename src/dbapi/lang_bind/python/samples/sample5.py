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
# File Name: sample5.py
#
# Author: Sergey Sikorskiy
#
# Description: a NCBI DBAPI Python extension module usage example.
# (execute a SQL query with many parameters simultaneously ("executemany"))
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
        cu.execute("SELECT name from sysobjects WHERE name = 'customers' AND type = 'U'")

        if len(cu.fetchall()) > 0:
                # Drop the table
                cu.execute("DROP TABLE customers")

        # Create new table.
        cu.execute("""
                CREATE TABLE customers (
                        cust_name VARCHAR(255) NOT NULL
                )
        """)
        getCon().commit()

def GetCustomers():
        # Allocate a cursor
        cu = getCon().cursor()

        # Execute a SQL statement.
        cu.execute("select * from customers")

        # Fetch all records using 'fetchall()'
        print cu.fetchall()

def DeleteCustomers():
        # Allocate a cursor
        cu = getCon().cursor()

        # Execute a SQL statement.
        cu.execute("delete from customers")

        # Commit a transaction
        getCon().commit()

def CreateCustomers():
        # Allocate a cursor
        cu = getCon().cursor()

        sql = "insert into customers(cust_name) values (@name)"

        # Insert customers with invalid names.
        # Execute a SQL statement with many parameters simultaneously.
        cu.executemany(sql, [{'@name':'1111'}, {'@name':'2222'}, {'@name':'3333'}])

        # Rollback transaction
        getCon().rollback()

        # Execute a SQL statement with many parameters simultaneously.
        cu.executemany(sql, [{'@name':'Jane'}, {'@name':'Doe'}, {'@name':'Scott'}])

        # Commit transaction
        getCon().commit()

def main():
        global conn

        # Connect to a database
        # Set an optional parameter "use_std_interface" to "True"
        conn = python_ncbi_dbapi.connect('ftds', 'MSSQL', 'MS_DEV1', 'DBAPI_Sample', 'anyone', 'allowed', True)

        CreateSchema()

        CreateCustomers()

        GetCustomers()

        # Delete the customer, and all her orders.
        DeleteCustomers()


if __name__ == "__main__":
        main()

# ===========================================================================
#
# $Log$
# Revision 1.3  2005/02/08 18:50:14  ssikorsk
# Adapted to the "simple mode" interface
#
# Revision 1.2  2005/01/27 21:14:29  ssikorsk
# Fixed: python samples (DDL + transaction)
#
# Revision 1.1  2005/01/21 22:15:28  ssikorsk
# Added: python samples for the NCBI DBAPI extension module.
#
# ===========================================================================
