#ifndef NCBIDIAG__HPP
#define NCBIDIAG__HPP

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
* Revision 1.28  2001/07/25 19:12:41  lavr
* Added date/time stamp for message logging
*
* Revision 1.27  2001/06/14 03:37:28  vakatov
* For the ErrCode manipulator, use CNcbiDiag:: method rather than a friend
*
* Revision 1.26  2001/06/13 23:19:36  vakatov
* Revamped previous revision (prefix and error codes)
*
* Revision 1.25  2001/06/13 20:51:52  ivanov
* + PushDiagPostPrefix(), PopPushDiagPostPrefix() - stack post prefix messages.
* + ERR_POST_EX, LOG_POST_EX - macros for posting with error codes.
* + ErrCode(code[,subcode]) - CNcbiDiag error code manipulator.
* + eDPF_ErrCode, eDPF_ErrSubCode - new post flags.
*
* Revision 1.24  2001/05/17 14:50:58  lavr
* Typos corrected
*
* Revision 1.23  2000/11/29 16:58:23  vakatov
* Added LOG_POST() macro -- to print the posted message only
*
* Revision 1.22  2000/10/24 19:54:44  vakatov
* Diagnostics to go to CERR by default (was -- disabled by default)
*
* Revision 1.21  2000/04/18 19:50:10  vakatov
* Get rid of the old-fashioned C-like "typedef enum {...} E;"
*
* Revision 1.20  2000/04/04 22:31:57  vakatov
* SetDiagTrace() -- auto-set basing on the application
* environment and/or registry
*
* Revision 1.19  2000/03/10 14:17:40  vasilche
* Added missing namespace specifier to macro.
*
* Revision 1.18  2000/02/18 16:54:02  vakatov
* + eDiag_Critical
*
* Revision 1.17  2000/01/20 16:52:29  vakatov
* SDiagMessage::Write() to replace SDiagMessage::Compose()
* + operator<<(CNcbiOstream& os, const SDiagMessage& mess)
* + IsSetDiagHandler(), IsDiagStream()
*
* Revision 1.16  1999/12/29 22:30:22  vakatov
* Use "exit()" rather than "abort()" in non-#_DEBUG mode
*
* Revision 1.15  1999/12/28 18:55:24  vasilche
* Reduced size of compiled object files:
* 1. avoid inline or implicit virtual methods (especially destructors).
* 2. avoid std::string's methods usage in inline methods.
* 3. avoid string literals ("xxx") in inline methods.
*
* Revision 1.14  1999/12/27 19:44:15  vakatov
* Fixes for R1.13:
* ERR_POST() -- use eDPF_Default rather than eDPF_Trace;  forcibly flush
* ("<< Endm") the diag. stream. Get rid of the eDPF_CopyFilename, always
* make a copy of the file name.
*
* Revision 1.13  1999/09/27 16:23:20  vasilche
* Changed implementation of debugging macros (_TRACE, _THROW*, _ASSERT etc),
* so that they will be much easier for compilers to eat.
*
* Revision 1.12  1999/05/04 00:03:06  vakatov
* Removed the redundant severity arg from macro ERR_POST()
*
* Revision 1.11  1999/04/30 19:20:57  vakatov
* Added more details and more control on the diagnostics
* See #ERR_POST, EDiagPostFlag, and ***DiagPostFlag()
*
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
* ==========================================================================
*/

#include <corelib/ncbistre.hpp>
#include <list>


BEGIN_NCBI_SCOPE


// Auxiliary macros for a "standard" error posting
#define ERR_POST(message) \
    ( NCBI_NS_NCBI::CNcbiDiag(__FILE__, __LINE__) << message << NCBI_NS_NCBI::Endm )

#define LOG_POST(message) \
    ( NCBI_NS_NCBI::CNcbiDiag(eDiag_Error, eDPF_Log) << message << NCBI_NS_NCBI::Endm )

// ...with error codes
#define ERR_POST_EX(err_code, err_subcode, message) \
    ( NCBI_NS_NCBI::CNcbiDiag(__FILE__, __LINE__) << NCBI_NS_NCBI::ErrCode(err_code, err_subcode) << message << NCBI_NS_NCBI::Endm )

#define LOG_POST_EX(err_code, err_subcode, message) \
    ( NCBI_NS_NCBI::CNcbiDiag(eDiag_Error, eDPF_Log) << NCBI_NS_NCBI::ErrCode(err_code, err_subcode) << message << NCBI_NS_NCBI::Endm )


// Severity level for the posted diagnostics
enum EDiagSev {
    eDiag_Info = 0,
    eDiag_Warning,
    eDiag_Error,
    eDiag_Critical,
    eDiag_Fatal,   // guarantees to exit(or abort)
    eDiag_Trace
};


// Which parts of the diagnostic context should be posted, and which are not...
// The generic appearance of the posted message is as follows:
//   "<file>", line <line>: <severity>: (<err_code>.<err_subcode>)
//    [<prefix1>::<prefix2>::<prefixN>] <message>
//
// e.g. if all flags are set, and prefix string is set to "My prefix", and
// ERR_POST(eDiag_Warning, "Take care!"):
//   "/home/iam/myfile.cpp", line 33: Warning: (2.11) [aa::bb::cc] Take care!
// See also SDiagMessage::Compose().
enum EDiagPostFlag {
    eDPF_File         = 0x1,  // set by default #if _DEBUG;  else -- not set
    eDPF_LongFilename = 0x2,  // set by default #if _DEBUG;  else -- not set
    eDPF_Line         = 0x4,  // set by default #if _DEBUG;  else -- not set
    eDPF_Prefix       = 0x8,  // set by default (always)
    eDPF_Severity     = 0x10, // set by default (always)
    eDPF_ErrCode      = 0x20, // set by default (always)
    eDPF_ErrSubCode   = 0x40, // set by default (always)
    eDPF_DateTime     = 0x80, // set by default (always)

    // set all flags
    eDPF_All          = 0x7FFF,
    // set all flags for using with __FILE__ and __LINE__
    eDPF_Trace        = 0x9f,
    // print the posted message only;  without severity, location, prefix, etc.
    eDPF_Log          = 0x0,
    // ignore all other flags, use global flags
    eDPF_Default      = 0x8000
};


// Forward declaration of some classes
class CDiagBuffer;
class ErrCode;


//////////////////////////////////////////////////////////////////
// The diagnostics class

class CNcbiDiag
{
public:
    CNcbiDiag(EDiagSev     sev        = eDiag_Error,
              unsigned int post_flags = eDPF_Default);
    CNcbiDiag(const char*  file, size_t line,
              EDiagSev     sev        = eDiag_Error,
              unsigned int post_flags = eDPF_Default);
    ~CNcbiDiag(void);

#if !defined(NO_INCLASS_TMPL)
    // formatted output
    template<class X> CNcbiDiag& operator<< (const X& x) {
        m_Buffer.Put(*this, x);
        return *this;
    }
#endif

    // manipulator to set error code(s), like:  CNcbiDiag() << ErrCode(5,3);
    CNcbiDiag& operator<< (const ErrCode& err_code);

    // other (function-based) manipulators
    CNcbiDiag& operator<< (CNcbiDiag& (*f)(CNcbiDiag&)) {
        return f(*this);
    }

    // output manipulators for CNcbiDiag
    friend CNcbiDiag& Reset  (CNcbiDiag& diag); // reset content of curr.mess.
    friend CNcbiDiag& Endm   (CNcbiDiag& diag); // flush curr.mess., start new

    friend CNcbiDiag& Info    (CNcbiDiag& diag); /// these 5 manipulators:
    friend CNcbiDiag& Warning (CNcbiDiag& diag); /// first do a flush;
    friend CNcbiDiag& Error   (CNcbiDiag& diag); /// then
    friend CNcbiDiag& Critical(CNcbiDiag& diag); /// set a
    friend CNcbiDiag& Fatal   (CNcbiDiag& diag); /// severity for the next
    friend CNcbiDiag& Trace   (CNcbiDiag& diag); /// diagnostic message

    // get a common symbolic name for the severity levels
    static const char* SeverityName(EDiagSev sev);

    // specify file name and line number to post
    // they are active for this message only, and they will be reset to
    // zero after this message is posted
    CNcbiDiag& SetFile(const char* file);
    CNcbiDiag& SetLine(size_t line);

    CNcbiDiag& SetErrorCode(int code = 0, int subcode = 0);

    // get severity, file , line and error code of the current message
    EDiagSev     GetSeverity    (void) const;
    const char*  GetFile        (void) const;
    size_t       GetLine        (void) const;
    int          GetErrorCode   (void) const;
    int          GetErrorSubCode(void) const;
    unsigned int GetPostFlags   (void) const;

#if !defined(NO_INCLASS_TMPL)
private:
#endif

    EDiagSev     m_Severity;   // severity level of the current message
    char         m_File[256];  // file name
    size_t       m_Line;       // line #
    int          m_ErrCode;    // error code 
    int          m_ErrSubCode; // error subcode
    CDiagBuffer& m_Buffer;     // this thread's error message buffer
    unsigned int m_PostFlags;  // bitwise OR of "EDiagPostFlag"

    // prohibit assignment
    CNcbiDiag(const CNcbiDiag&);
    CNcbiDiag& operator=(const CNcbiDiag&);
};

#if defined(NO_INCLASS_TMPL)
// formatted output
template<class X>
inline
CNcbiDiag& operator <<(CNcbiDiag& diag, const X& x);
#endif


/////////////////////////////////////////////////////////////////////////////
// ATTENTION:  the following functions are application-wide, i.e they
//             are not local for a particular thread
/////////////////////////////////////////////////////////////////////////////


// Return "true" if the specified "flag" is set in global "flags"(to be posted)
// If eDPF_Default is set in "flags" then use the current global flags as
// specified by SetDiagPostFlag()/UnsetDiagPostFlag()
inline bool IsSetDiagPostFlag(EDiagPostFlag flag,
                              unsigned int  flags = eDPF_Default);

// Set/unset the specified flag (globally)
extern void SetDiagPostFlag(EDiagPostFlag flag);
extern void UnsetDiagPostFlag(EDiagPostFlag flag);

// Specify a string to prefix all subsequent error postings with
extern void SetDiagPostPrefix(const char* prefix);

// Push/pop a string to/from the list of message prefixes
extern void PushDiagPostPrefix(const char* prefix);
extern void PopDiagPostPrefix(void);

// Do not post messages which severity is less than "min_sev"
// Return previous post-level
extern EDiagSev SetDiagPostLevel(EDiagSev post_sev = eDiag_Error);

// Abrupt the application if severity is >= "max_sev"
// Return previous die-level
extern EDiagSev SetDiagDieLevel(EDiagSev die_sev = eDiag_Fatal);

// Disable/enable posting of "eDiag_Trace" messages.
// By default, these messages are disabled unless:
//   1)  environment variable $DIAG_TRACE is set (to any value), or
//   2)  registry value of DIAG_TRACE, section DEBUG is set (to any value)
#define DIAG_TRACE "DIAG_TRACE"
enum EDiagTrace {
    eDT_Default = 0,  // restores the default tracing context
    eDT_Disable,      // ignore messages of severity "eDiag_Trace"
    eDT_Enable        // enable messages of severity "eDiag_Trace"
};
extern void SetDiagTrace(EDiagTrace how, EDiagTrace dflt = eDT_Default);

// Set new message handler("func"), data("data") and destructor("cleanup").
// The "func(..., data)" to be called when any instance of "CNcbiDiagBuffer"
// has a new diagnostic message completed and ready to post.
// "cleanup(data)" will be called whenever this hook gets replaced and
// on the program termination.
// NOTE 1:  "func()", "cleanup()" and "g_SetDiagHandler()" calls are
//          MT-protected, so that they would never be called simultaneously
//          from different threads.
// NOTE 2:  By default, the errors will be written to standard error stream.
struct SDiagMessage {
    SDiagMessage(EDiagSev severity, const char* buf, size_t len, void* data,
                 const char* file = 0, size_t line = 0,
                 unsigned int flags = eDPF_Default, const char* prefix = 0,
                 int err_code = 0, int err_subcode = 0);

    EDiagSev     m_Severity;
    const char*  m_Buffer;  // not guaranteed to be '\0'-terminated!
    size_t       m_BufferLen;
    void*        m_Data;
    const char*  m_File;
    size_t       m_Line;
    int          m_ErrCode;
    int          m_ErrSubCode;
    unsigned int m_Flags;   // bitwise OR of "EDiagPostFlag"
    const char*  m_Prefix;

    // Compose a message string in the standard format(see also "flags"):
    //    "<file>", line <line>: <severity>: [<prefix>] <message> EOL
    // and put it to string "str", or write to an output stream "os".
    void          Write(string& str) const;
    CNcbiOstream& Write(CNcbiOstream& os) const;
};

inline CNcbiOstream& operator<<(CNcbiOstream& os, const SDiagMessage& mess) {
    return mess.Write(os);
}


typedef void (*FDiagHandler)(const SDiagMessage& mess);

typedef void (*FDiagCleanup)(void* data);

extern void SetDiagHandler(FDiagHandler func,
                           void*        data,
                           FDiagCleanup cleanup);

// Return TRUE if user has ever set (or unset) diag. handler
extern bool IsSetDiagHandler(void);


// Write the error diagnostics to output stream "os"
// (this uses the SetDiagHandler() functionality)
extern void SetDiagStream
(CNcbiOstream* os,
 bool          quick_flush  = true,  // do stream flush after every message
 FDiagCleanup  cleanup      = 0,     // call "cleanup(cleanup_data)" if diag.
 void*         cleanup_data = 0      // stream is changed (see SetDiagHandler)
 );

// Return TRUE if "os" is the current diag. stream
extern bool IsDiagStream(const CNcbiOstream* os);



///////////////////////////////////////////////////////
// All inline function implementations and internal data
// types, etc. are in this file
#include <corelib/ncbidiag.inl>


END_NCBI_SCOPE

#endif  /* NCBIDIAG__HPP */
