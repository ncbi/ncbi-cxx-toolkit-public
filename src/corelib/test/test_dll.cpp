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
 * Author: Vladimir Ivanov
 *
 * File Description:
 *   Simple DLL to test class CDll (used into "test_ncbidll")
 *
 */

#include <ncbi_pch.hpp>
#include <string>
#include <iostream>
#include <ncbiconf.h>

using namespace std;

#if defined __cplusplus
extern "C" {
#endif

#if defined NCBI_OS_MSWIN
#   define DllExport __declspec( dllexport )
#elif defined NCBI_OS_UNIX
#   define DllExport
#endif

DllExport int DllVar_Counter = 0;

DllExport int Dll_Inc(int i)
{
    cout << "Dll_Inc(" << i << ")" << endl;
    DllVar_Counter += i;
    return DllVar_Counter;
}

DllExport int Dll_Add(int x, int y)
{
    cout << "Dll_Add(" << x << ", "<< y << ")" << endl;
    return x + y;
}

DllExport string* Dll_StrRepeat(const string& str, unsigned int count)
{
    cout << "Dll_StrRepeat(\"" << str << "\", " << count << ")" << endl;
    string* s = new string; 
    for (unsigned int i=0; i<count; i++) {
        *s += str;
    }
    return s;
}

#if defined __cplusplus
}
#endif


/*
 * ===========================================================================
 * $Log$
 * Revision 6.3  2004/05/14 13:59:51  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 6.2  2002/04/16 18:49:06  ivanov
 * Centralize threatment of assert() in tests.
 * Added #include <test/test_assert.h>. CVS log moved to end of file.
 *
 * Revision 6.1  2002/01/15 19:10:53  ivanov
 * Initial revision
 *
 * ===========================================================================
 */
