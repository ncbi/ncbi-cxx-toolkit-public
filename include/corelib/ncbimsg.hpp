#ifndef NCBIMSG__HPP
#define NCBIMSG__HPP

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
* Revision 1.8  1998/10/01 22:35:52  vakatov
* *** empty log message ***
*
* Revision 1.7  1998/10/01 21:36:03  vakatov
* Renamed everything to the "message handling"(rather than "error handling")
*
* ==========================================================================
*/

#include <iostream>
#include <strstream>
#include <string>

using namespace std;

namespace ncbi_msg {

    typedef enum {
        eM_Info = 0,
        eM_Warning,
        eM_Error,
        eM_Fatal   // guarantees to exit(or abort)
    } EMsgSeverity;

    class CMsgBuffer;  // forward declaration of an internal class


    //////////////////////////////////////////////////////////////////
    //
    class CMessage {
    public:
        CMessage(EMsgSeverity sev=eM_Error, const char* message=0,
                 bool flush=false);
        ~CMessage(void);

        EMsgSeverity f_GetSeverity(void) const;               // curr. severity
        template<class X> CMessage& operator << (X& x) const; // formatted out
        CMessage& operator << (CMessage& (*f)(CMessage&));    // manipulators

        // output manipulators for CMessage
        friend CMessage& Reset  (CMessage& msg); // reset content of curr.mess.
        friend CMessage& Endm   (CMessage& msg); // flush curr.mess., start new
        friend CMessage& Info   (CMessage& msg); /// these 4 manipulators first
        friend CMessage& Warning(CMessage& msg); /// do a flush;  then they set
        friend CMessage& Error  (CMessage& msg); /// severity for the next
        friend CMessage& Fatal  (CMessage& msg); /// message

    private:
        EMsgSeverity  m_Severity;  // severity level for the current message
        CMsgBuffer&   m_Buffer;    // this thread's error message buffer
    };


    // ATTENTION:  the following functions are application-wide, i.e they
    //             are not local for a particular thread
    //

    // Do not post messages which severity is less than "min_sev"
    // Return previous post-level
    extern EMsgSeverity g_SetMessagePostLevel(EMsgSeverity min_sev=eM_Error);

    // Abrupt the application if severity is >= "max_sev"
    // Return previous die-level
    extern EMsgSeverity g_SetMessageDieLevel(EMsgSeverity die_sev=eM_Fatal);

    // Write the error diagnostics to output stream "os"
    // (this uses the g_SetMessageHandler() functionality)
    // Return previous output stream, if any
    extern ostream* g_SetMessageStream(ostream* os);

    // Set new message handler("func"), data("data") and destructor("cleanup").
    // "func(..., data)" is to be called when any instance of "CMessageBuffer"
    // has a new error message completed and ready to post.
    // "cleanup(data)" will be called whenever this hook gets replaced and
    // on the program termination.
    // NOTE 1:  "message_buf" is in general NOT '\0'-terminated
    // NOTE 2:  "func()", "cleanup()" and "g_SetMessageHandler()" calls are
    //          MT-protected, so that they will not be called simultaneously
    //          from different threads
    typedef void (*FMessageHandler)(EMsgSeverity severity,
                                    const char* message_buf,
                                    size_t message_len,
                                    void* data);
    typedef void (*FMessageCleanup)(void* data);
    extern void g_SetMessageHandler(FMessageHandler func, void* data,
                                    FMessageCleanup cleanup);


    ///////////////////////////////////////////////////////
    // All inline function implementations and internal data
    // types, etc. are in this file
#include <ncbimsg.inl>

}  // namespace ncbi_msg

#endif  /* NCBIMSG__HPP */
