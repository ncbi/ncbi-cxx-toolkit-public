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

#ifdef _MSC_VER
// disable warning C4005: macro redefinition.
#pragma warning(disable: 4005)
#endif

#include <ncbi_pch.hpp>

#include <Python.h>

void execute(char* cmd)
{
    if (PyRun_SimpleString(cmd) == -1) {
	    throw 0;
    }
}

int
main(int argc, char *argv[])
{
	/* Pass argv[0] to the Python interpreter */
	Py_SetProgramName(argv[0]);

	/* Initialize the Python interpreter.  Required. */
	Py_Initialize();

	/* Define sys.argv.  It is up to the application if you
	   want this; you can also let it undefined (since the Python
	   code is generally not a main program it has no business
	   touching sys.argv...) */
	PySys_SetArgv(argc, argv);

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
        execute("import python_ncbi_dbapi\n");

        execute("connection = python_ncbi_dbapi.connect('ftds', 'ms_sql', 'MS_DEV1', 'DBAPI_Sample', 'anyone', 'allowed')\n");
        execute("connection2 = python_ncbi_dbapi.connect('ftds', 'ms_sql', 'MS_DEV2', '', 'anyone', 'allowed')\n");

        execute("connection.commit()\n");
        execute("connection.rollback()\n");

        execute("connection2.commit()\n");
        execute("connection2.rollback()\n");

        execute("cursor = connection.cursor()\n");
        execute("cursor2 = connection2.cursor()\n");

        execute("date_val = python_ncbi_dbapi.Date(1, 1, 1)\n");
        execute("time_val = python_ncbi_dbapi.Time(1, 1, 1)\n");
        execute("timestamp_val = python_ncbi_dbapi.Timestamp(1, 1, 1, 1, 1, 1)\n");
        execute("binary_val = python_ncbi_dbapi.Binary('Binary test')\n");

        execute("cursor.execute('select qq = 57 + 33')\n");
        execute("cursor.fetchone()\n");
        execute("cursor.execute('select qq = 57.55 + 0.0033')\n");
        execute("cursor.fetchone()\n");
        execute("cursor.execute('select qq = GETDATE()')\n");
        execute("cursor.fetchone()\n");
        execute("cursor.execute('select name, type from sysobjects')\n");
        execute("cursor.fetchone()\n");
        execute("cursor.execute('select name, type from sysobjects where type = @type_par', {'type_par':'S'})\n");
        execute("cursor.fetchone()\n");
        execute("cursor.executemany('select name, type from sysobjects where type = @type_par', [{'type_par':'S'}, {'type_par':'D'}])\n");
        execute("cursor.fetchmany()\n");
        execute("cursor.fetchall()\n");
        execute("cursor.nextset()\n");
        execute("cursor.setinputsizes()\n");
        execute("cursor.setoutputsize()\n");
        execute("cursor.callproc('host_name()')\n");

        execute("cursor2.execute('select qq = 57 + 33')\n");
        execute("print cursor2.fetchone()\n");
        execute("cursor2.executemany('select qq = 57 + 33', [])\n");
        execute("cursor2.fetchmany()\n");
        execute("cursor2.fetchall()\n");
        execute("cursor2.nextset()\n");
        execute("cursor2.setinputsizes()\n");
        execute("cursor2.setoutputsize()\n");
        execute("cursor2.callproc('host_name()')\n");

        execute("cursor.close()\n");
        execute("cursor2.close()\n");

        execute("connection.close()\n");
        execute("connection2.close()\n");

    } catch(...)
    {
	    // Exit, cleaning up the interpreter
        Py_Finalize();
	    return 1;
    }

	// Exit, cleaning up the interpreter
    Py_Finalize();
	return 0;
}

/* ===========================================================================
*
* $Log$
* Revision 1.1  2005/01/18 19:26:08  ssikorsk
* Initial version of a Python DBAPI module
*
*
* ===========================================================================
*/
