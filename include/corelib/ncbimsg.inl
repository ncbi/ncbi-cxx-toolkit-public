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
* Revision 1.5  1998/09/29 20:26:02  vakatov
* Redesigning #1
*
* ==========================================================================
*/


///////////////////////////////////////////////////////
//  CErr::

inline CErr::CErr(EErrSeverity sev, const char* message, bool flush)
 : m_Buffer(g_GetErrBuffer())
{
    m_Severity = sev;
    m_Active = false;
    if ( message )
        (*this) << message;
    if ( flush )
        (*this) << Flush;
}

inline CErr::~CErr(void) {
    err.m_Buffer.f_Detach(this);
}

inline EErrSeverity CErr::f_GetSeverity(void) {
    return m_Severity;
}

template<class X> CErr& CErr::operator << (X& x) {
    m_Buffer.f_Put(*this, x);
    return *this;
}

inline CErr& EndMess(CErr& err)  {
    err.m_Buffer.f_Flush(err);
    return err;
}

inline CErr& Info(CErr& err)  {
    err.m_Buffer.f_Severity(err);
    return err;
}
inline CErr& Warning(CErr& err)  {
    err.m_Buffer.f_Severity(err);
    return err;
}
inline CErr& Error(CErr& err)  {
    err.m_Buffer.f_Severity(err);
    return err;
}
inline CErr& Fatal(CErr& err)  {
    err.m_Buffer.f_Severity(err);
    return err;
}

inline CErr& Reset(CErr& err)  {
    err.m_Buffer.f_Reset(err);
    return err;
}




///////////////////////////////////////////////////////
//  CErrBuffer::

CErrBuffer::CErrBuffer(void) {
    m_Severity = sev;
    if ( message )
        m_Buffer << sev;
    if ( flush )
        f_Flush();
}

template<class X> CErrBuffer& CErrBuffer::operator << (X& x) {
    m_Buffer << x;
    return *this;
}

CErrBuffer& CErrBuffer::f_Clear(void) {
    VERIFY( !m_Buffer.rdbuf()->seekpos(0); );
};

CErrBuffer& CErrBuffer::f_Flush(void) {
    if ( m_Buffer.pcount() ) {
        VERIFY( f_FlushHook(m_Severity, m_Buffer.str(), m_Buffer.pcount()) );
        m_Buffer.freeze(false);
        f_Clear();
    }
    if (m_Severity == eE_Fatal)
        abort();
}

CErrBuffer& CErrBuffer::f_Severity(EErrSeverity sev) {
    f_Flush();
    m_Severity = sev;
}

#endif /* def NCBIERR__HPP  &&  ndef NCBIERR__INL */
