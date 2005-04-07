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
# File Name: sample3.py
#
# Author: Sergey Sikorskiy
#
# Description: a NCBI DBAPI Python extension module usage example.
# (using SQL parameters)
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

# 4) Execute a SQL statement with a parameter.
# Parameter values should be passed as a mapping.
cursor.execute('select name, type from sysobjects where type = @type_par', {'@type_par':'S'})

# 5) Fetch all records using 'fetchall()'
print "Fetch all records using 'fetchall()'"
print cursor.fetchall()

# ===========================================================================
#
# $Log$
# Revision 1.3  2005/04/07 16:50:12  ssikorsk
# Added '#! /usr/bin/env python' to each sample
#
# Revision 1.2  2005/02/08 18:50:14  ssikorsk
# Adapted to the "simple mode" interface
#
# Revision 1.1  2005/01/21 22:15:28  ssikorsk
# Added: python samples for the NCBI DBAPI extension module.
#
# ===========================================================================
