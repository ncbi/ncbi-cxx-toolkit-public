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
# File Name: sample1.py
#
# Author: Sergey Sikorskiy
#
# Description: a basic NCBI DBAPI Python extension module usage pattern
# (import, connect, cursor, execute, fetchone)
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
conn = python_ncbi_dbapi.connect('ftds', 'MSSQL', 'MS_DEV1', 'DBAPI_Sample', 'anyone', 'allowed')

# 3) Allocate a cursor
cursor = conn.cursor()

# 4) Execute a SQL statement
cursor.execute('select qq = 57 + 33')

# 5) Get a result
result = cursor.fetchone()

print result

# ===========================================================================
#
# $Log$
# Revision 1.2  2005/02/08 18:50:14  ssikorsk
# Adapted to the "simple mode" interface
#
# Revision 1.1  2005/01/21 22:15:28  ssikorsk
# Added: python samples for the NCBI DBAPI extension module.
#
# ===========================================================================
