#ifndef NCBIDIAG__HPP
#define NCBIDIAG__HPP

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
* Revision 1.10  1999/03/15 16:08:09  vakatov
* Fixed "{...}" macros to "do {...} while(0)" lest it to mess up with "if"s
*
* Revision 1.9  1999/03/12 18:04:06  vakatov
* Added ERR_POST macro to perform a plain "standard" error posting
*
* Revision 1.8  1998/12/30 21:52:16  vakatov
* Fixed for the new SunPro 5.0 beta compiler that does not allow friend
* templates and member(in-class) templates
*
* Revision 1.7  1998/12/28 17:56:27  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.6  1998/11/06 22:42:37  vakatov
* Introduced BEGIN_, END_ and USING_ NCBI_SCOPE macros to put NCBI C++
* API to namespace "ncbi::" and to use it by default, respectively
* Introduced THROWS_NONE and THROWS(x) macros for the exception
* specifications
* Other fixes and rearrangements throughout the most of "corelib" code
*
* Revision 1.5  1998/11/04 23:46:35  vakatov
* Fixed the "ncbidbg/diag" header circular dependencies
*
* Revision 1.4  1998/11/03 20:51:24  vakatov
* Adaptation for the SunPro compiler glitchs(see conf. #NO_INCLASS_TMPL)
*
* Revision 1.3  1998/10/30 20:08:20  vakatov
* Fixes to (first-time) compile and test-run on MSVS++
*
* Revision 1.2  1998/10/27 23:06:58  vakatov
* Use NCBI C++ interface to iostream's
*
* Revision 1.1  1998/10/23 23:22:10  vakatov
* Initial revision
*
* ==========================================================================
*/

#include <corelib/ncbistre.hpp>
#include <stdlib.h>

// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE


class CDiagBuffer;  // (fwd-declaration of internal class)

// Severity level for the posted diagnostics
typedef enum {
    eDiag_Info = 0,
    eDiag_Warning,
    eDiag_Error,
    eDiag_Fatal,   // guarantees to exit(or abort)
    eDiag_Trace
} EDiagSev;

// Auxiliary macro for a "standard" error posting
#define ERR_POST(message)  do { \
    NCBI_NS_NCBI::CNcbiDiag _diag_(NCBI_NS_NCBI::eDiag_Error); \
    _diag_ << message; \
} while(0)


//////////////////////////////////////////////////////////////////
// The diagnostics class

class CNcbiDiag {
public:
    CNcbiDiag(EDiagSev    sev     = eDiag_Error,
              const char* message = 0,
              bool        flush   = false);
    ~CNcbiDiag(void);

    EDiagSev GetSeverity(void) const;                     // current severity

#if !defined(NO_INCLASS_TMPL)
    // formatted output
    template<class X> CNcbiDiag& operator <<(const X& x) {
        m_Buffer.Put(*this, x);
        return *this;
    }
#endif

    CNcbiDiag& operator <<(CNcbiDiag& (*f)(CNcbiDiag&)) { // manipulators
        return f(*this);
    }

    // output manipulators for CNcbiDiag
    friend CNcbiDiag& Reset  (CNcbiDiag& diag); // reset content of curr.mess.
    friend CNcbiDiag& Endm   (CNcbiDiag& diag); // flush curr.mess., start new

    friend CNcbiDiag& Info   (CNcbiDiag& diag); /// these 5 manipulators:
    friend CNcbiDiag& Warning(CNcbiDiag& diag); /// first do a flush;
    friend CNcbiDiag& Error  (CNcbiDiag& diag); /// then they set a
    friend CNcbiDiag& Fatal  (CNcbiDiag& diag); /// severity for the next
    friend CNcbiDiag& Trace  (CNcbiDiag& diag); /// diagnostic message

    // a common symbolic name for the severity levels
    static const char* SeverityName(EDiagSev sev);

#if !defined(NO_INCLASS_TMPL)
private:
#endif

    EDiagSev     m_Severity;  // severity level for the current message
    CDiagBuffer& m_Buffer;    // this thread's error message buffer
    // prohibit assignment
    CNcbiDiag& operator =(const CNcbiDiag&) { return *this; }
};

#if defined(NO_INCLASS_TMPL)
// formatted output
template<class X> CNcbiDiag& operator <<(CNcbiDiag& diag, const X& x);
#endif


/////////////////////////////////////////////////////////////////////////////
// ATTENTION:  the following functions are application-wide, i.e they
//             are not local for a particular thread
/////////////////////////////////////////////////////////////////////////////


// Do not post messages which severity is less than "min_sev"
// Return previous post-level
extern EDiagSev SetDiagPostLevel(EDiagSev post_sev=eDiag_Error);

// Abrupt the application if severity is >= "max_sev"
// Return previous die-level
extern EDiagSev SetDiagDieLevel(EDiagSev die_sev=eDiag_Fatal);

// Write the error diagnostics to output stream "os"
// (this uses the SetDiagHandler() functionality)
// If "quick_flush" is "true" then do stream "flush()" after every message
extern void SetDiagStream(CNcbiOstream* os, bool quick_flush=true);

// Set new message handler("func"), data("data") and destructor("cleanup").
// "func(..., data)" is to be called when any instance of "CNcbiDiagBuffer"
// has a new error message completed and ready to post.
// "cleanup(data)" will be called whenever this hook gets replaced and
// on the program termination.
// NOTE 1:  "message_buf" is in general NOT '\0'-terminated
// NOTE 2:  "func()", "cleanup()" and "g_SetDiagHandler()" calls are
//          MT-protected, so that they would never be called simultaneously
//          from different threads
typedef void (*FDiagHandler)(EDiagSev    sev,
                             const char* message_buf,
                             size_t      message_len,
                             void*       data);
typedef void (*FDiagCleanup)(void* data);
extern void SetDiagHandler(FDiagHandler func,
                           void*        data,
                           FDiagCleanup cleanup);


///////////////////////////////////////////////////////
// All inline function implementations and internal data
// types, etc. are in this file
#include <corelib/ncbidiag.inl>


// (END_NCBI_SCOPE must be preceeded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE

#endif  /* NCBIDIAG__HPP */
