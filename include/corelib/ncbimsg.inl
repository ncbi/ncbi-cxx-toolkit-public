#if defined(NCBIMSG__HPP)  &&  !defined(NCBIMSG__INL)
#define NCBIMSG__INL

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
*   NCBI Message Handling API for C++(using STL)
*
* --------------------------------------------------------------------------
* $Log$
* Revision 1.10  1998/10/05 23:03:45  vakatov
* *** empty log message ***
*
* Revision 1.9  1998/10/02 22:53:09  vakatov
* *** empty log message ***
*
* ==========================================================================
*/

/////////////////////////////////////////////////////////////////////////////
// WARNING -- all the above is for INTERNAL "ncbimsg" use only,
//            and any classes, typedefs and even "extern" functions and
//            variables declared in this file should not be used anywhere
//            but inside "ncbimsg.inl" and/or "ncbimsg.cpp"!!!
/////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////
// CMsgBuffer
// (can be accessed only by "CMessage" and created only by g_GetMsgBuffer())
//

class CMsgBuffer {
    friend class CMessage;
    friend CMsgBuffer& g_GetMsgBuffer(void);
private:
    const CMessage* m_Msg; // present user
    ostrstream  m_Stream;  // content of the current message

    CMsgBuffer(void);
    ~CMsgBuffer(void);

    // formatted output
    template<class X> void f_Put(const CMessage& msg, X& x);

    void f_Flush  (void);
    void f_Reset  (const CMessage& msg);  // reset content of current message
    void f_EndMess(const CMessage& msg);  // flush out current message

    // flush & detach the current user
    void f_Detach(const CMessage* msg);

    // (NOTE:  these two dont need to be protected by mutexes because it is not
    //  critical -- while not having a mutex around would save us a little
    //  performance)
    static EMsgSeverity sm_PostSeverity;
    static EMsgSeverity sm_DieSeverity;

    // call current message handler directly
    static void sf_MessageHandler(EMsgSeverity sev,
                                  const char*  message_buf,
                                  size_t       message_len);

    // Symbolic name for the severity levels(used by CMessage::f_SeverityName)
    static const char* sm_SeverityName[eM_Fatal+1];
};




///////////////////////////////////////////////////////
//  CMessage::

inline CMessage::CMessage(EMsgSeverity sev, const char* message, bool flush)
 : m_Buffer(g_GetMsgBuffer())
{
    m_Severity = sev;
    m_Active = false;
    if ( message )
        (*this) << message;
    if ( flush )
        (*this) << Flush;
}

inline CMessage::~CMessage(void) {
    m_Buffer.f_Detach(this);
}

template<class X> CMessage& CMessage::operator << (X& x) {
    m_Buffer.f_Put(*this, x);
    return *this;
}

inline CMessage& operator << (CMessage& (*f)(CMessage&)) {
    return f(*this);
}

inline EMsgSeverity CMessage::f_GetSeverity(void) {
    return m_Severity;
}


///////////////////////////////////////////////////////
//  CMessage:: manipulators

inline CMessage& Reset(CMessage& msg)  {
    msg.m_Buffer.f_Reset(msg);
    return msg;
}

inline CMessage& EndMess(CMessage& msg)  {
    msg.m_Buffer.f_EndMess(msg);
    return msg;
}

inline CMessage& Info(CMessage& msg)  {
    msg.m_Severity = eM_Info;
    return msg << EndMess;
}
inline CMessage& Warning(CMessage& msg)  {
    msg.m_Severity = eM_Warning;
    return msg << EndMess;
}
inline CMessage& Error(CMessage& msg)  {
    msg.m_Severity = eM_Error;
    return msg << EndMess;
}
inline CMessage& Fatal(CMessage& msg)  {
    msg.m_Severity = eM_Fatal;
    return msg << EndMess;
}



///////////////////////////////////////////////////////
//  CMsgBuffer::

inline CMsgBuffer::CMsgBuffer(void) {
    m_Msg = 0;
}

inline CMsgBuffer::~CMsgBuffer(void) {
    ASSERT( !m_Msg );
    ASSERT( !m_Stream.pcount() );
}

template<class X> void CMsgBuffer::f_Put(const CMessage& msg, X& x) {
    if (msg.f_GetSeverity() < s_MessagePostSeverity)
        return;

    if (m_Msg != &msg) {
        if ( m_Stream.pcount() )
            f_Flush();
        m_Msg = &msg;
    }
    m_Stream << x;
}

inline void CMsgBuffer::f_Flush(void) {
    if ( !m_Msg )
        return;

    EMsgSeverity sev = m_Msg->f_GetSeverity();
    if ( m_Stream.pcount() ) {
        const char* message = m_Stream.str();
        m_Stream.freeze(false);
        gs_MessageHandler(sev, message, m_Stream.pcount());
        f_Reset(m_Msg);
    }
    if (sev >= s_MessageDieSeverity)
        abort();
}

inline void CMsgBuffer::f_Reset(const CMessage& msg) {
    if (&msg == m_Msg)
        VERIFY( !m_Stream.rdbuf()->seekpos(0); );
}

inline void CMsgBuffer::f_EndMess(const CMessage& msg) {
    if (&msg == m_Msg)
        f_Flush();
}

inline void CMsgBuffer::f_Detach(const CMessage* msg) {
    if (msg == m_Msg) {
        f_Flush();
        m_Msg = 0;
    }
}


#endif /* def NCBIMSG__HPP  &&  ndef NCBIMSG__INL */
