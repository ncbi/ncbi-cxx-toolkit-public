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
 * Authors:  Andrei Gourianov, Denis Vakatov
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbi_toolkit.hpp>


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
// CNcbiToolkitImpl_LogMessage

class CNcbiToolkitImpl_LogMessage : public CNcbiToolkit_LogMessage
{
public:
    CNcbiToolkitImpl_LogMessage(const SDiagMessage& msg);
    virtual ~CNcbiToolkitImpl_LogMessage(void);
};

CNcbiToolkitImpl_LogMessage::CNcbiToolkitImpl_LogMessage
(const SDiagMessage& msg)
    : CNcbiToolkit_LogMessage(msg)
{
}

CNcbiToolkitImpl_LogMessage::~CNcbiToolkitImpl_LogMessage(void)
{
}



/////////////////////////////////////////////////////////////////////////////
// CNcbiToolkit_LogMessage

CNcbiToolkit_LogMessage::CNcbiToolkit_LogMessage(const SDiagMessage& msg)
    : m_Msg(msg)
{
}

CNcbiToolkit_LogMessage::~CNcbiToolkit_LogMessage(void)
{
}

CNcbiToolkit_LogMessage::operator string(void) const
{
    CNcbiOstrstream str_os;
    str_os << m_Msg;
    return CNcbiOstrstreamToString(str_os);
}

const SDiagMessage& CNcbiToolkit_LogMessage::GetNativeToolkitMessage(void)
    const
{
    return m_Msg;
}

CNcbiToolkit_LogMessage::ESeverity CNcbiToolkit_LogMessage::Severity(void) const
{
    ESeverity sev = eFatal;
    switch (m_Msg.m_Severity) {
    default:                                      break;
    case eDiag_Info:      sev = eInfo;     break;
    case eDiag_Warning:   sev = eWarning;  break;
    case eDiag_Error:     sev = eError;    break;
    case eDiag_Critical:  sev = eCritical; break;
    case eDiag_Fatal:     sev = eFatal;    break;
    case eDiag_Trace:     sev = eTrace;    break;
    }
    return sev;
}

int CNcbiToolkit_LogMessage::ErrCode(void) const
{
    return m_Msg.m_ErrCode;
}

int CNcbiToolkit_LogMessage::ErrSubCode(void) const
{
    return m_Msg.m_ErrSubCode;
}

string CNcbiToolkit_LogMessage::Message(void) const
{
    if ( !m_Msg.m_Buffer ) {
        return kEmptyStr;
    }
    return string(m_Msg.m_Buffer, m_Msg.m_BufferLen);
}

string CNcbiToolkit_LogMessage::File(void) const
{
    return m_Msg.m_File;
}

size_t CNcbiToolkit_LogMessage::Line(void) const
{
    return m_Msg.m_Line;
}



/////////////////////////////////////////////////////////////////////////////
// CNcbiToolkitImpl_Application

class CNcbiToolkitImpl_Application : public CNcbiApplication
{
public:
    CNcbiToolkitImpl_Application(void)
    {
        DisableArgDescriptions();
    }
    virtual int Run(void)
    {
        return 0;
    }
};

/////////////////////////////////////////////////////////////////////////////
// CNcbiApplication factory

CNcbiApplication* DefaultFactory(void)
{
    return new CNcbiToolkitImpl_Application();
}

typedef CNcbiApplication* ( *FNcbiApplicationFactory)(void);
static FNcbiApplicationFactory s_fAppFactory = DefaultFactory;



/////////////////////////////////////////////////////////////////////////////
// CNcbiToolkitImpl_DiagHandler

class CNcbiToolkitImpl_DiagHandler : public CDiagHandler
{
public:
    CNcbiToolkitImpl_DiagHandler(INcbiToolkit_LogHandler* log_handler);
    virtual ~CNcbiToolkitImpl_DiagHandler(void);
    virtual void Post(const SDiagMessage& msg);
private:
    INcbiToolkit_LogHandler* m_LogHandler;
};

CNcbiToolkitImpl_DiagHandler::CNcbiToolkitImpl_DiagHandler
(INcbiToolkit_LogHandler* log_handler)
    : m_LogHandler(log_handler)
{
    SetDiagHandler(this, false/*eNoOwnership*/);
}

CNcbiToolkitImpl_DiagHandler::~CNcbiToolkitImpl_DiagHandler(void)
{
}

void CNcbiToolkitImpl_DiagHandler::Post(const SDiagMessage& msg)
{
    CNcbiToolkitImpl_LogMessage toolkit_msg(msg);
    m_LogHandler->Post(toolkit_msg);
}


/////////////////////////////////////////////////////////////////////////////
// CNcbiToolkit (singleton)

class CNcbiToolkit
{
public:
    CNcbiToolkit(int                  argc,
                 const TXChar* const* argv,
                 const TXChar* const* envp = NULL,
                       INcbiToolkit_LogHandler* log_handler = NULL);
    ~CNcbiToolkit(void);

private:
    auto_ptr<CNcbiApplication>              m_App;
    auto_ptr<CNcbiToolkitImpl_DiagHandler>  m_DiagHandler;
};


CNcbiToolkit::CNcbiToolkit(int                  argc,
                           const TXChar* const* argv,
                           const TXChar* const* envp,
                                 INcbiToolkit_LogHandler* log_handler)
{
    if (log_handler) {
        m_DiagHandler.reset(new CNcbiToolkitImpl_DiagHandler(log_handler));
    }
    if (s_fAppFactory) {
        m_App.reset( s_fAppFactory() );
        m_App->AppMain(argc, argv, envp,
                       m_DiagHandler.get() ? eDS_User : eDS_Default);
    }
}


CNcbiToolkit::~CNcbiToolkit( void )
{
    if ( m_DiagHandler.get() ) {
        SetDiagHandler(NULL, false/*eNoOwnership*/);
    }
}


/////////////////////////////////////////////////////////////////////////////
// Toolkit initialization

static const CNcbiToolkit* s_NcbiToolkit = NULL;

const CNcbiToolkit* kNcbiToolkit_Finalized = (const CNcbiToolkit*)(-1L);

DEFINE_STATIC_FAST_MUTEX(s_NcbiToolkit_Mtx);

void NcbiToolkit_Init
(int                            argc,
 const TXChar* const*           argv,
 const TXChar* const*           envp,
       INcbiToolkit_LogHandler* log_handler)
{
    CFastMutexGuard mtx_guard(s_NcbiToolkit_Mtx);

    if (s_NcbiToolkit != NULL)
        throw runtime_error( "NcbiToolkit should be initialized only once");

    s_NcbiToolkit = new CNcbiToolkit(argc, argv, envp, log_handler);
}


void NcbiToolkit_Fini(void)
{
    CFastMutexGuard mtx_guard(s_NcbiToolkit_Mtx);

    if (s_NcbiToolkit != NULL  &&  s_NcbiToolkit != kNcbiToolkit_Finalized) {
        delete s_NcbiToolkit;
        s_NcbiToolkit = kNcbiToolkit_Finalized;
    }
}

void NcbiToolkit_RegisterNcbiApplicationFactory(FNcbiApplicationFactory f)
{
    CFastMutexGuard mtx_guard(s_NcbiToolkit_Mtx);
    s_fAppFactory = f;
}

END_NCBI_SCOPE
