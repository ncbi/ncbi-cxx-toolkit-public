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
# File Name:  $Id$
#
# Author: Sergey Sikorskiy
#
# ===========================================================================

import python_ncbi_dbapi
#
connection = python_ncbi_dbapi.connect('ftds', 'ms_sql', 'MS_DEV1', 'DBAPI_Sample', 'anyone', 'allowed')
connection2 = python_ncbi_dbapi.connect('ftds', 'ms_sql', 'MS_DEV2', '', 'anyone', 'allowed')
#
connection.commit()
connection.rollback()
#
connection2.commit()
connection2.rollback()
#
cursor = connection.cursor()
cursor2 = connection2.cursor()
#
print "testing first cursor object"
cursor.execute('select qq = 57 + 33')
print cursor.fetchone()
cursor.execute('select qq = 57.55 + 0.0033')
print cursor.fetchone()
cursor.execute('select qq = GETDATE()')
print cursor.fetchone()
cursor.execute('select name, type from sysobjects')
print cursor.fetchone()
cursor.execute('select name, type from sysobjects where type = @type_par', {'type_par':'S'})
print cursor.fetchall()
cursor.executemany('select name, type from sysobjects where type = @type_par', [{'type_par':'S'}, {'type_par':'D'}])
print cursor.fetchmany(5)
cursor.nextset()
cursor.setinputsizes()
cursor.setoutputsize()
# cursor.callproc('host_name()')
#
print "testing second cursor object"
cursor2.execute('select qq = 57 + 33')
print cursor2.fetchone()
cursor.execute('select qq = 57.55 + 0.0033')
print cursor.fetchone()
cursor.execute('select qq = GETDATE()')
print cursor.fetchone()
cursor.execute('select name, type from sysobjects')
print cursor.fetchone()
cursor2.executemany('select qq = 57 + 33', [])
cursor2.fetchmany()
cursor2.fetchall()
cursor2.nextset()
cursor2.setinputsizes()
cursor2.setoutputsize()
# cursor2.callproc('host_name()')
#
cursor.close()
cursor2.close()
#
connection.close()
connection2.close()

# ===========================================================================
#
# $Log$
# Revision 1.2  2005/01/27 18:50:03  ssikorsk
# Fixed: a bug with transactions
# Added: python 'transaction' object
#
# Revision 1.1  2005/01/18 19:26:08  ssikorsk
# Initial version of a Python DBAPI module
#
#
# ===========================================================================
