/*  $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
* Author: Sergey Sikorskiy
*
* File Description:
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include "../pythonpp/pythonpp_emb.hpp"

int
main(int argc, char *argv[])
{
    ncbi::pythonpp::CEngine engine(argc, argv);

    // load python module manualy ...
    /*
    PyObject* pName;
    PyObject* pModule;

    pName = PyString_FromString("python_ncbi_dbapi");
    pModule = PyImport_Import(pName);
    Py_DECREF(pName);

    if (pModule != NULL)
    {
    } else {
        PyErr_Print();
        fprintf(stderr, "Failed to load python_ncbi_dbapi\n");
        Py_Finalize();
        return 1;
    }
    */

    try {
        // Execute some Python statements (in module __main__)

        /*
        engine.ExecuteStr("import python_ncbi_dbapi\n");

        engine.ExecuteStr("connection = python_ncbi_dbapi.connect('ftds', 'ms_sql', 'MS_DEV1', 'DBAPI_Sample', 'anyone', 'allowed')\n");
        engine.ExecuteStr("connection2 = python_ncbi_dbapi.connect('ftds', 'ms_sql', 'MS_DEV2', '', 'anyone', 'allowed')\n");

        engine.ExecuteStr("connection.commit()\n");
        engine.ExecuteStr("connection.rollback()\n");

        engine.ExecuteStr("connection2.commit()\n");
        engine.ExecuteStr("connection2.rollback()\n");

        engine.ExecuteStr("cursor = connection.cursor()\n");
        engine.ExecuteStr("cursor2 = connection2.cursor()\n");

        engine.ExecuteStr("date_val = python_ncbi_dbapi.Date(1, 1, 1)\n");
        engine.ExecuteStr("time_val = python_ncbi_dbapi.Time(1, 1, 1)\n");
        engine.ExecuteStr("timestamp_val = python_ncbi_dbapi.Timestamp(1, 1, 1, 1, 1, 1)\n");
        engine.ExecuteStr("binary_val = python_ncbi_dbapi.Binary('Binary test')\n");

        engine.ExecuteStr("cursor.execute('select qq = 57 + 33')\n");
        engine.ExecuteStr("cursor.fetchone()\n");
        engine.ExecuteStr("cursor.execute('select qq = 57.55 + 0.0033')\n");
        engine.ExecuteStr("cursor.fetchone()\n");
        engine.ExecuteStr("cursor.execute('select qq = GETDATE()')\n");
        engine.ExecuteStr("cursor.fetchone()\n");
        engine.ExecuteStr("cursor.execute('select name, type from sysobjects')\n");
        engine.ExecuteStr("cursor.fetchone()\n");
        engine.ExecuteStr("cursor.execute('select name, type from sysobjects where type = @type_par', {'type_par':'S'})\n");
        engine.ExecuteStr("cursor.fetchone()\n");
        engine.ExecuteStr("cursor.executemany('select name, type from sysobjects where type = @type_par', [{'type_par':'S'}, {'type_par':'D'}])\n");
        engine.ExecuteStr("cursor.fetchmany()\n");
        engine.ExecuteStr("cursor.fetchall()\n");
        engine.ExecuteStr("cursor.nextset()\n");
        engine.ExecuteStr("cursor.setinputsizes()\n");
        engine.ExecuteStr("cursor.setoutputsize()\n");
        // engine.ExecuteStr("cursor.callproc('host_name')\n");

        engine.ExecuteStr("cursor2.execute('select qq = 57 + 33')\n");
        engine.ExecuteStr("print cursor2.fetchone()\n");
        // engine.ExecuteStr("cursor2.executemany('select qq = 57 + 33', [])\n");
        // engine.ExecuteStr("cursor2.fetchmany()\n");
        // engine.ExecuteStr("cursor2.callproc('host_name()')\n");

        engine.ExecuteStr("cursor.close()\n");
        engine.ExecuteStr("cursor2.close()\n");

        engine.ExecuteStr("connection.close()\n");
        engine.ExecuteStr("connection2.close()\n");
        */

        /**/
        engine.ExecuteStr("import python_ncbi_dbapi\n");
        engine.ExecuteStr("connection = python_ncbi_dbapi.connect('ftds', 'ms_sql', 'MS_DEV1', 'DBAPI_Sample', 'anyone', 'allowed')\n");
        engine.ExecuteStr("cursor = connection.cursor()\n");
        engine.ExecuteStr("cursor.execute('select qq = 57 + 33')\n");
        engine.ExecuteStr("cursor.fetchone()\n");
        /**/

        /*
        engine.ExecuteStr("import python_ncbi_dbapi\n");
        engine.ExecuteStr("connection = python_ncbi_dbapi.connect('ftds', 'ms_sql', 'MS_DEV1', 'DBAPI_Sample', 'anyone', 'allowed')\n");
        engine.ExecuteStr("cursor = connection.cursor()\n");
        engine.ExecuteStr("cursor.executemany('select qq = 57 + 33', [])\n");
        engine.ExecuteStr("cursor.fetchone()\n");
        */

        // engine.ExecuteFile("E:\\home\\nih\\c++\\src\\dbapi\\lang_bind\\python\\samples\\sample5.py");
        // engine.ExecuteFile("E:\\home\\nih\\c++\\src\\dbapi\\lang_bind\\python\\tests\\python_ncbi_dbapi_test.py");

    } catch(...)
    {
	    return 1;
    }

	return 0;
}

/* ===========================================================================
*
* $Log$
* Revision 1.2  2005/01/27 18:50:03  ssikorsk
* Fixed: a bug with transactions
* Added: python 'transaction' object
*
* Revision 1.1  2005/01/18 19:26:08  ssikorsk
* Initial version of a Python DBAPI module
*
*
* ===========================================================================
*/
