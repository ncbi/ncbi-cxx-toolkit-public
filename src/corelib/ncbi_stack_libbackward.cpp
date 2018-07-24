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
 * Authors:  Anton Perkov
 *
 */

#include <ncbi_pch.hpp>
#include <ncbiconf.h>

#if defined(HAVE_LIBDW)
#  define BACKWARD_HAS_DW 1
#elif defined(HAVE_LIBUNWIND)
#  define BACKWARD_HAS_UNWIND 1
#endif

#include <backward.hpp>
#if defined(HAVE_CXA_DEMANGLE)
#  include <cxxabi.h>
#endif
#include <stdio.h>

BEGIN_NCBI_SCOPE

#define CAN_HONOR_SIGNAL_HANDLING_CONFIGURATION 1

#if defined(USE_LIBBACKWARD_SIG_HANDLING)
#  define TRACE_SIGNALS_BY_DEFAULT true
#else
#  define TRACE_SIGNALS_BY_DEFAULT false
#endif

NCBI_PARAM_DECL(bool, Debug, Trace_Fatal_Signals);
NCBI_PARAM_DEF_EX(bool, Debug, Trace_Fatal_Signals, TRACE_SIGNALS_BY_DEFAULT,
                  0, DEBUG_TRACE_FATAL_SIGNALS);
typedef NCBI_PARAM_TYPE(Debug, Trace_Fatal_Signals) TTraceFatalSignals;

unique_ptr<backward::SignalHandling> s_SignalHandler;


// Call this function to get a backtrace.
class CStackTraceImpl
{
public:
    CStackTraceImpl(void);
    ~CStackTraceImpl(void);

    void Expand(CStackTrace::TStack& stack);
    static void s_HonorSignalHandlingConfiguration(void);

private:
    typedef backward::StackTrace TStack;

    TStack m_Stack;
};


CStackTraceImpl::CStackTraceImpl(void)
{
    m_Stack.load_here();
}


CStackTraceImpl::~CStackTraceImpl(void)
{
}


void CStackTraceImpl::Expand(CStackTrace::TStack& stack)
{
    backward::TraceResolver resolver;
    resolver.load_stacktrace(m_Stack);
    for (size_t trace_idx = 0; trace_idx < m_Stack.size(); ++trace_idx) {
        CStackTrace::SStackFrameInfo info;
        const backward::ResolvedTrace& trace
            = resolver.resolve(m_Stack[trace_idx]);
        info.module = trace.object_filename;
        info.addr = trace.addr;

        if (!trace.source.filename.size()) {
            /*os << "   Object \""
               << trace.object_filename
               << ", at "
               << trace.addr
               << ", in "
               << trace.object_function
               << "\n";*/
        }

        for (size_t inliner_idx = 0; inliner_idx < trace.inliners.size(); ++inliner_idx) {
            const backward::ResolvedTrace::SourceLoc& inliner_loc = trace.inliners[inliner_idx];
            CStackTrace::SStackFrameInfo info2;
            info2.file = inliner_loc.filename;
            info2.line = inliner_loc.line;
            info2.func = inliner_loc.function;
            stack.push_back(move(info2));
        }

        if (trace.source.filename.size()) {
            info.file = trace.source.filename;
            info.line = trace.source.line;
            //info.func = trace.source.function;
            info.func = trace.object_function;
            info.addr = trace.addr;
        }
        stack.push_back(move(info));
    }
}

void CStackTraceImpl::s_HonorSignalHandlingConfiguration(void)
{
    if (TTraceFatalSignals::GetDefault()) {
        s_SignalHandler.reset(new backward::SignalHandling);
    }
}

END_NCBI_SCOPE
