#ifndef CORELIB___NCBI_STACK__HPP
#define CORELIB___NCBI_STACK__HPP

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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <corelib/ncbistre.hpp>
#include <list>
#include <memory>

BEGIN_NCBI_SCOPE


// Forward declaration - internal class for holding stack trace data.
class CStackTraceImpl;


class NCBI_XNCBI_EXPORT CStackTrace
{
public:
    /// Structure for holding stack trace data
    struct SStackFrameInfo
    {
        string func;
        string file;
        string module;
        size_t offs;
        size_t line;

        SStackFrameInfo()
            : offs(0), line(0) {}
    };
    typedef list<SStackFrameInfo> TStack;

    /// Get and store current stack trace. When printing the stack trace
    /// to a stream, each line is prepended with "prefix".
    CStackTrace(const string& prefix = "");
    ~CStackTrace(void);

    // Copy - required by some compilers for operator<<()
    CStackTrace(const CStackTrace& stack_trace);
    CStackTrace& operator=(const CStackTrace& stack_trace);

    /// Check if stack trace information is available
    bool Empty(void) const
        {
            x_ExpandStackTrace();
            return m_Stack.empty();
        }

    /// Get the stack trace data
    const TStack& GetStack(void) const
        {
            x_ExpandStackTrace(); return m_Stack;
        }

    /// Get current prefix
    const string& GetPrefix(void) const { return m_Prefix; }
    /// Set new prefix
    void SetPrefix(const string& prefix) const { m_Prefix = prefix; }
    /// Write stack trace to the stream, prepend each line with the prefix.
    void Write(CNcbiOstream& os) const;

private:
    // Convert internal stack trace data (collected addresses)
    // to the list of SStackFrameInfo.
    void x_ExpandStackTrace(void) const;

    mutable auto_ptr<CStackTraceImpl> m_Impl;

    mutable TStack m_Stack;
    mutable string m_Prefix;
};


/// Output stack trace
inline
CNcbiOstream& operator<<(CNcbiOstream& os, const CStackTrace& stack_trace)
{
    stack_trace.Write(os);
    return os;
}


END_NCBI_SCOPE

#endif  // CORELIB___NCBI_STACK__HPP
