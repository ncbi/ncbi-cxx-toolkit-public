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
*   NCBI C++ diagnostic API
*
* --------------------------------------------------------------------------
* $Log$
* Revision 1.7  1998/11/06 22:42:41  vakatov
* Introduced BEGIN_, END_ and USING_ NCBI_SCOPE macros to put NCBI C++
* API to namespace "ncbi::" and to use it by default, respectively
* Introduced THROWS_NONE and THROWS(x) macros for the exception
* specifications
* Other fixes and rearrangements throughout the most of "corelib" code
*
* Revision 1.6  1998/11/03 22:57:51  vakatov
* Use #define'd manipulators(like "NcbiFlush" instead of "flush") to
* make it compile and work with new(templated) version of C++ streams
*
* Revision 1.5  1998/11/03 22:30:20  vakatov
* cosmetics...
*
* Revision 1.4  1998/11/03 22:28:35  vakatov
* Renamed Die/Post...Severity() to ...Level()
*
* Revision 1.3  1998/11/03 20:51:26  vakatov
* Adaptation for the SunPro compiler glitchs(see conf. #NO_INCLASS_TMPL)
*
* Revision 1.2  1998/10/30 20:08:37  vakatov
* Fixes to (first-time) compile and test-run on MSVS++
*
* Revision 1.1  1998/10/27 23:04:56  vakatov
* Initial revision
*
* ==========================================================================
*/

#include <ncbidiag.hpp>

// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE


//## DECLARE_MUTEX(s_Mutex);  // protective mutex


EDiagSev CDiagBuffer::sm_PostSeverity = eDiag_Error;
EDiagSev CDiagBuffer::sm_DieSeverity  = eDiag_Fatal;

const char* CDiagBuffer::SeverityName[eDiag_Trace+1] = {
    "Info", "Warning", "Error", "Fatal", "Trace"
};

FDiagHandler CDiagBuffer::sm_HandlerFunc    = 0;
void*        CDiagBuffer::sm_HandlerData    = 0;
FDiagCleanup CDiagBuffer::sm_HandlerCleanup = 0;


void CDiagBuffer::DiagHandler(EDiagSev    sev,
                              const char* message_buf,
                              size_t      message_len)
{
    if ( CDiagBuffer::sm_HandlerFunc ) {
        //## MUTEX_LOCK(s_Mutex);
        if ( CDiagBuffer::sm_HandlerFunc )
            CDiagBuffer::sm_HandlerFunc(sev, message_buf, message_len,
                                        CDiagBuffer::sm_HandlerData);
        //## MUTEX_UNLOCK(s_Mutex);
    }
}


extern EDiagSev SetDiagPostLevel(EDiagSev post_sev)
{
    //## MUTEX_LOCK(s_Mutex);
    EDiagSev sev = CDiagBuffer::sm_PostSeverity;
    CDiagBuffer::sm_PostSeverity = post_sev;
    //## MUTEX_UNLOCK(s_Mutex);
    return sev;
}


extern EDiagSev SetDiagDieLevel(EDiagSev die_sev)
{
    //## MUTEX_LOCK(s_Mutex);
    EDiagSev sev = CDiagBuffer::sm_DieSeverity;
    CDiagBuffer::sm_DieSeverity = die_sev;
    //## MUTEX_UNLOCK(s_Mutex);
    return sev;
}


extern void SetDiagHandler(FDiagHandler func, void* data,
                           FDiagCleanup cleanup)
{
    //## MUTEX_LOCK(s_Mutex);
    if ( CDiagBuffer::sm_HandlerCleanup )
        CDiagBuffer::sm_HandlerCleanup(CDiagBuffer::sm_HandlerData);
    CDiagBuffer::sm_HandlerFunc    = func;
    CDiagBuffer::sm_HandlerData    = data;
    CDiagBuffer::sm_HandlerCleanup = cleanup;
    //## MUTEX_UNLOCK(s_Mutex);
}


//## static void s_TlsCleanup(TTls TLS, void* old_value)
//## {
//##     delete (CDiagBuffer *)old_value;
//## }

extern CDiagBuffer& GetDiagBuffer(void)
{
    //## TLS_DECLARE(s_DiagBufferTls);
    //## CDiagBuffer* msg_buf = TLS_GET_VALUE(s_DiagBufferTls);
    //## if ( !msg_buf ) {
    //##    msg_buf = new CDiagBuffer;
    //##    TLS_SET_VALUE(s_DiagBufferTls, msg_buf, s_TlsCleanup);
    //## }
    //## return *msg_buf;

    static CDiagBuffer s_DiagBuffer;
    return s_DiagBuffer;
}



/////////////////////////////////////////////////////////////////////////////
// A common-use case:  direct all messages to an output stream
//

struct SToStream_Data {
    CNcbiOstream *os;
    bool          quick_flush;
};

static void s_ToStream_Handler(EDiagSev    sev,
                               const char* message_buf,
                               size_t      message_len,
                               void*       data)
{ // (it is MT-protected -- by the "CDiagBuffer::DiagHandler()")
    SToStream_Data* x_data = (SToStream_Data*) data;
    if ( !x_data )
        return;

    CNcbiOstream& os = *x_data->os;
    os << CNcbiDiag::SeverityName(sev) << ": ";
    os.write(message_buf, message_len);
    os << NcbiEndl;
    if ( x_data->quick_flush )
        os << NcbiFlush;
}

static void s_ToStream_Cleanup(void* data)
{
    if ( data )
        delete (SToStream_Data*)data;
}


extern void SetDiagStream(CNcbiOstream* os, bool quick_flush)
{
    if ( !os ) {
        SetDiagHandler(0, 0, 0);
        return;
    }

    SToStream_Data* data = new SToStream_Data;
    data->os          = os;
    data->quick_flush = quick_flush;
    SetDiagHandler(s_ToStream_Handler, data, s_ToStream_Cleanup);
}


// (END_NCBI_SCOPE must be preceeded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE
