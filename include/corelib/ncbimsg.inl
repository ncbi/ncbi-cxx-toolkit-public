#if defined(NCBIERR__HPP)  &&  !defined(NCBIERR__INL)
#define NCBIERR__INL

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
* Revision 1.2  1998/09/25 19:35:39  vakatov
* *** empty log message ***
*
* Revision 1.1  1998/09/24 22:10:49  vakatov
* Initial revision
*
* ==========================================================================
*/


///////////////////////////////////////////////////////


template<class X> CError& CError::operator << (X& x) {
    m_Buffer << x;
    return *this;
}

CError& CError::f_Clear(void) {
    VERIFY( !m_Buffer.rdbuf()->seekpos(0); );
};

CError& CError::f_Flush(void) {
    VERIFY( f_FlushHook(m_Severity, m_Buffer.str(), m_Buffer.pcount()) );
    m_Buffer.freeze(false);
    f_Clear();
}


#endif /* def NCBIERR__HPP  &&  ndef NCBIERR__INL */
