#if defined(NCBIDIAG__HPP)  &&  !defined(NCBIDIAG__INL)
#define NCBIDIAG__INL

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
*   NCBI C++ diagnostic API
*
* --------------------------------------------------------------------------
* $Log$
* Revision 1.1  1998/10/24 00:25:43  vakatov
* Initial revision(incomplete transition from "ncbimsg.inl")
*
* ==========================================================================
*/

#include <sstream>

/////////////////////////////////////////////////////////////////////////////
// WARNING -- all the beneath is for INTERNAL "ncbidiag" use only,
//            and any classes, typedefs and even "extern" functions and
//            variables declared in this file should not be used anywhere
//            but inside "ncbidiag.inl" and/or "ncbidiag.cpp"!!!
/////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////
// CDiagBuffer
// (can be accessed only by "CDiag" and created only by GetDiagBuffer())
//

class CDiagBuffer {
    friend class CDiag;
    friend CDiagBuffer& GetDiagBuffer(void);
private:
    const CDiag*   m_Diag;    // present user
    ostringstream  m_Stream;  // storage for the diagnostic message

    CDiagBuffer(void);
    ~CDiagBuffer(void);

    // formatted output
    template<class X> void Put(const CDiag& diag, X& x);

    void Flush  (void);
    void Reset  (const CDiag& diag);  // reset content of the diag. message
    void EndMess(const CDiag& diag);  // output current diag. message

    // flush & detach the current user
    void Detach(const CDiag* diag);

    // (NOTE:  these two dont need to be protected by mutexes because it is not
    //  critical -- while not having a mutex around would save us a little
    //  performance)
    static EDiagSev sm_PostSeverity;
    static EDiagSev sm_DieSeverity;

    // call the current diagnostics handler directly
    static void DiagHandler(EDiagSev    sev,
                            const char* message_buf,
                            size_t      message_len);

    // Symbolic name for the severity levels(used by CDiag::SeverityName)
    static const char* sm_SeverityName[eM_Fatal+1];
};




///////////////////////////////////////////////////////
//  CDiag::

inline CDiag::CDiag(EDiagSev sev, const char* message, bool flush)
 : m_Buffer(GetDiagBuffer())
{
    m_Severity = sev;
    m_Active = false;
    if ( message )
        (*this) << message;
    if ( flush )
        (*this) << Flush;
}

inline CDiag::~CDiag(void) {
    m_Buffer.Detach(this);
}

template<class X> CDiag& CDiag::operator << (X& x) {
    m_Buffer.Put(*this, x);
    return *this;
}

inline CDiag& operator << (CDiag& (*f)(CDiag&)) {
    return f(*this);
}

inline EDiagSev CDiag::GetSeverity(void) {
    return m_Severity;
}


///////////////////////////////////////////////////////
//  CDiag:: manipulators

inline CDiag& Reset(CDiag& diag)  {
    diag.m_Buffer.Reset(diag);
    return diag;
}

inline CDiag& EndMess(CDiag& diag)  {
    diag.m_Buffer.EndMess(diag);
    return diag;
}

inline CDiag& Info(CDiag& diag)  {
    diag.m_Severity = eM_Info;
    return diag << EndMess;
}
inline CDiag& Warning(CDiag& diag)  {
    diag.m_Severity = eM_Warning;
    return diag << EndMess;
}
inline CDiag& Error(CDiag& diag)  {
    diag.m_Severity = eM_Error;
    return diag << EndMess;
}
inline CDiag& Fatal(CDiag& diag)  {
    diag.m_Severity = eM_Fatal;
    return diag << EndMess;
}
inline CDiag& Trace(CDiag& diag)  {
    diag.m_Severity = eM_Trace;
    return diag << EndMess;
}



///////////////////////////////////////////////////////
//  CDiagBuffer::

inline CDiagBuffer::CDiagBuffer(void) {
    m_Diag = 0;
}

inline CDiagBuffer::~CDiagBuffer(void) {
    _ASSERT( !m_Diag );
    _ASSERT( !m_Stream.pcount() );
}

template<class X> void CDiagBuffer::Put(const CDiag& diag, X& x) {
    if (diag.GetSeverity() < sm_PostSeverity)
        return;

    if (m_Diag != &diag) {
        Flush();
        m_Diag = &diag;
    }
    m_Stream << x;
}

inline void CDiagBuffer::Flush(void) {
    if ( !m_Diag )
        return;

    EDiagSev sev = m_Diag->GetSeverity();
    if ( m_Stream.pcount() ) {
        const char* message = m_Stream.str();
        m_Stream.freeze(false);
        DiagHandler(sev, message, m_Stream.pcount());
        Reset(m_Diag);
    }
    if (sev == sm_DieSeverity)
        abort();
}

inline void CDiagBuffer::Reset(const CDiag& diag) {
    if (&diag == m_Diag)
        _VERIFY( !m_Stream.rdbuf()->seekpos(0); );
}

inline void CDiagBuffer::EndMess(const CDiag& diag) {
    if (&diag == m_Diag)
        Flush();
}

inline void CDiagBuffer::Detach(const CDiag* diag) {
    if (diag == m_Diag) {
        Flush();
        m_Diag = 0;
    }
}


#endif /* def NCBIDIAG__HPP  &&  ndef NCBIDIAG__INL */
