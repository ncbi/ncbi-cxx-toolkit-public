#ifndef NCBIERR__HPP
#define NCBIERR__HPP

/*  $RCSfile$  $Revision$  $Date$
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
*   NCBI Error Handling API for C++(using STL)
*
* --------------------------------------------------------------------------
* $Log$
* Revision 1.1  1998/09/24 22:10:48  vakatov
* Initial revision
*
* ==========================================================================
*/

#include <iostream>
#include <strstream>
#include <string>

using namespace std;

namespace ncbi_err {

    typedef enum {
        eE_Info = 0,
        eE_Warning,
        eE_Error,

        eE_Fatal   /* guarantees to exit(or abort) */ 
    } EErrSeverity;


    class CError {
    public:
        CError(void);
        CError(EErrSeverity sev, const char* message=0, bool flush=true);

        template<class X> CError& operator << (X& x);

    private:
        ostrstream m_Buffer;
        
        
    };


    ///////////////////////////////////////////////////////
    // All inline function implementations are in this file
#include <ncbierr.inl>

}  // namespace ncbi_err

#endif  /* NCBIERR__HPP */
