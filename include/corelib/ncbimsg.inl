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
* Revision 1.6  1998/09/29 21:45:56  vakatov
* *** empty log message ***
*
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
    m_Buffer.f_Detach(this);
}

inline EErrSeverity CErr::f_GetSeverity(void) {
    return m_Severity;
}

template<class X> CErr& CErr::operator << (X& x) {
    m_Buffer.f_Put(*this, x);
    return *this;
}

inline CErr& operator << (CErr& (*f)(CErr&)) {
    return f(*this);
}

inline CErr& Reset(CErr& err)  {
    err.m_Buffer.f_Reset(err);
    return err;
}

inline CErr& EndMess(CErr& err)  {
    err.m_Buffer.f_EndMess(err);
    return err;
}

inline CErr& Info(CErr& err)  {
    err.m_Severity = eE_Info;
    return err << EndMess;
}
inline CErr& Warning(CErr& err)  {
    err.m_Severity = eE_Warning;
    return err << EndMess;
}
inline CErr& Error(CErr& err)  {
    err.m_Severity = eE_Error;
    return err << EndMess;
}
inline CErr& Fatal(CErr& err)  {
    err.m_Severity = eE_Fatal;
    return err << EndMess;
}



///////////////////////////////////////////////////////
//  CErrBuffer::

inline CErrBuffer::CErrBuffer(void) {
    m_Err = 0;
}

inline CErrBuffer::~CErrBuffer(void) {
    ASSERT( !m_Err );
    ASSERT( !m_Stream.pcount() );
}

template<class X> void CErrBuffer::f_Put(const CErr& err, X& x) {
    if (m_Err != &err) {
        if ( m_Stream.pcount() )
            f_Flush();
        m_Err = &err;
    }
    m_Stream << x;
}

inline void CErrBuffer::f_Flush(void) {
    if ( !m_Err )
        return;

    EErrSeverity sev = m_Err->f_GetSeverity();
    if ( m_Stream.pcount() ) {
        VERIFY( f_FlushHook(sev, m_Stream.str(), m_Stream.pcount()) );
        m_Stream.freeze(false);
        f_Reset(m_Err);
    }
    if (sev == eE_Fatal)
        abort();
}

inline void CErrBuffer::f_Reset(const CErr& err) {
    if (&err == m_Err)
        VERIFY( !m_Stream.rdbuf()->seekpos(0); );
}

inline void CErrBuffer::f_EndMess(const CErr& err) {
    if (&err == m_Err)
        f_Flush();
}


#endif /* def NCBIERR__HPP  &&  ndef NCBIERR__INL */
