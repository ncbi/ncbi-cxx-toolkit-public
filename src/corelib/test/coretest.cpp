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
* Author:  Denis Vakatov
*
* File Description:
*   TEST for:  NCBI C++ core API
*
* --------------------------------------------------------------------------
* $Log$
* Revision 1.1  1998/10/30 19:57:54  vakatov
* Initial revision
*
* ==========================================================================
*/

#include <ncbidiag.hpp>


/////////////////////////////////
// Diagnostics
//
extern void TestDiag(void)
{
    CNcbiDiag diag;
    double d = 123.45;

    diag << "[Unset Diag Stream]  Diagnostics double = " << d;
    _TRACE( "[Unset Diag Stream]  Trace double = " << d );

    SetDiagStream(&cerr);
    diag << "[Set Diag Stream(cerr)]  Diagnostics double = " << d;
    _TRACE( "[Set Diag Stream(cerr)]  Trace double = " << d );
}


/////////////////////////////////
// MAIN
//
extern int main(void)
{
    TestDiag();

    return 0;
}

