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
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#if defined(HAVE_CXA_DEMANGLE)
#  include <cxxabi.h>
#endif
#include <stdio.h>
#include <dlfcn.h>

BEGIN_NCBI_SCOPE


// Call this function to get a backtrace.
class CStackTraceImpl
{
public:
    CStackTraceImpl(void);
    ~CStackTraceImpl(void);

    void Expand(CStackTrace::TStack& stack);

private:
    typedef list<unw_cursor_t> TFrames;

    TFrames m_Frames;
};


CStackTraceImpl::CStackTraceImpl(void)
{
    unw_cursor_t cursor;
    unw_context_t context;

    // Initialize cursor to current frame for local unwinding.
    unw_getcontext(&context);
    unw_init_local(&cursor, &context);

    // Unwind frames one by one, going up the frame stack.
    while (unw_step(&cursor) > 0) {
        m_Frames.push_back(cursor);
    }
}


CStackTraceImpl::~CStackTraceImpl(void)
{
}


void CStackTraceImpl::Expand(CStackTrace::TStack& stack)
{
    ITERATE(TFrames, it, m_Frames) {
        unw_cursor_t cursor = *it;
        CStackTrace::SStackFrameInfo info;
        unw_word_t offset;

        char sym[256];
        if (unw_get_proc_name(&cursor, sym, sizeof(sym), &offset) == 0 && sym[0]) {
            info.func = sym;
            info.offs = offset;
#if defined(HAVE_CXA_DEMANGLE)
            // use abi::__cxa_demangle
            size_t len = 0;
            char* buf = 0;
            int status = 0;
            buf = abi::__cxa_demangle(info.func.c_str(),
                                      buf, &len, &status);
            if ( !status ) {
                info.func = buf;
                free(buf);
            }
#endif
        } else {
            info.func = "???";
        }
        info.file = "???";

        unw_proc_info_t u;
        if (unw_get_proc_info(&cursor, &u) == 0) {
            info.addr = (void*)(u.start_ip+offset);
            if (info.addr) {
                Dl_info dlinfo;
                if (dladdr(info.addr, &dlinfo) != 0) {
                    info.module = dlinfo.dli_fname;
                }
            }
        }

        stack.push_back(info);
    }
}

END_NCBI_SCOPE
