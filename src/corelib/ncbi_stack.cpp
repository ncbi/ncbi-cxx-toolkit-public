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

#include <ncbi_pch.hpp>
#include <corelib/ncbi_stack.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_param.hpp>

#ifdef NCBI_OS_SOLARIS
#  include <sys/ucontext.h> // for additional test below
#endif

#if defined NCBI_OS_MSWIN
#  if NCBI_PLATFORM_BITS == 64
#    include "ncbi_stack_win64.cpp"
#  else
#    include "ncbi_stack_win32.cpp"
#  endif
#elif defined NCBI_OS_SOLARIS  &&  defined(GETUSTACK)
#  include "ncbi_stack_solaris.cpp"
#elif defined NCBI_OS_LINUX
#  include "ncbi_stack_linux.cpp"
#else
#  include "ncbi_stack_default.cpp"
#endif


BEGIN_NCBI_SCOPE


string CStackTrace::SStackFrameInfo::AsString(void) const
{
    return module + " " + file + ":" + NStr::UInt8ToString(line) + " " + func +
        " offset=0x" + NStr::UInt8ToString(offs, 0, 16);
}


CStackTrace::CStackTrace(const string& prefix)
    : m_Impl(new CStackTraceImpl),
      m_Prefix(prefix)
{
}


CStackTrace::~CStackTrace(void)
{
}


CStackTrace::CStackTrace(const CStackTrace& stack_trace)
{
    *this = stack_trace;
}


CStackTrace& CStackTrace::operator=(const CStackTrace& stack_trace)
{
    if (&stack_trace != this) {
        const TStack& stack = stack_trace.GetStack();
        m_Stack.clear();
        m_Stack.insert(m_Stack.end(), stack.begin(), stack.end());
        m_Prefix = stack_trace.m_Prefix;
    }
    return *this;
}


void CStackTrace::x_ExpandStackTrace(void) const
{
    if ( m_Impl.get() ) {
        m_Impl->Expand(m_Stack);
        m_Impl.reset();
    }
}


void CStackTrace::Write(CNcbiOstream& os) const
{
    x_ExpandStackTrace();

    if ( Empty() ) {
        os << m_Prefix << "NOT AVAILABLE" << endl;
        return;
    }

    ITERATE(TStack, it, m_Stack) {
        os << m_Prefix << it->AsString() << endl;
    }
}


// Stack trace depth limit
const unsigned int kDefaultStackTraceMaxDepth = 200;
NCBI_PARAM_DECL(int, Debug, Stack_Trace_Max_Depth);
NCBI_PARAM_DEF_EX(int, Debug, Stack_Trace_Max_Depth, kDefaultStackTraceMaxDepth,
                  eParam_NoThread, DEBUG_STACK_TRACE_MAX_DEPTH);
typedef NCBI_PARAM_TYPE(Debug, Stack_Trace_Max_Depth) TStackTraceMaxDepth;

unsigned int CStackTrace::s_GetStackTraceMaxDepth(void)
{
    static volatile bool s_InGetMaxDepth = false;
    static CAtomicCounter s_MaxDepth;

    // Check if we are already getting the max depth. If yes, something
    // probably went wrong. Just return the default value.
    unsigned int val = kDefaultStackTraceMaxDepth;
    if ( !s_InGetMaxDepth ) {
        s_InGetMaxDepth = true;
        try {
            val = (unsigned int)s_MaxDepth.Get();
            if (val > 0) {
                return val;
            }
            val = TStackTraceMaxDepth::GetDefault();
            if (val == 0) {
                val = kDefaultStackTraceMaxDepth;
            }
            s_MaxDepth.Set(val);
        }
        catch (...) {
            s_InGetMaxDepth = false;
            throw;
        }
        s_InGetMaxDepth = false;
    }
    return val;
}


END_NCBI_SCOPE
