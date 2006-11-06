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
#include <ucontext.h>
//#include <demangle.h>
#include <dlfcn.h>
#include <corelib/ncbistr.hpp>

BEGIN_NCBI_SCOPE


struct SFrame
{
    struct SFrame* next;
    void* ret_addr;
};


extern "C"
static int s_StackWalker(uintptr_t int_ptr, int, void* data)
{
    CStackTrace::TStack* stack_trace = (CStackTrace::TStack*)data;
    CStackTrace::SStackFrameInfo sf_info;
    Dl_info info;
    if (dladdr((void*)int_ptr, &info)) {
        sf_info.func = info.dli_sname;
        /*
        char buf[256];
        if ( !cplus_demangle(info.dli_sname, buf, 256) ) {
            sf_info.func = buf;
        }
        */
        sf_info.offs = (unsigned)info.dli_saddr - (unsigned)info.dli_fbase;
        sf_info.module = info.dli_fname;
    } else {
        sf_info.func = NStr::IntToString(int_ptr);
    }
    stack_trace->push_back(sf_info);
    return 0;
}


void CStackTrace::GetStackTrace(TStack& stack_trace)
{
    ucontext_t ctx;
    getcontext(&ctx);
    walkcontext(&ctx, s_StackWalker, &stack_trace);
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2006/11/06 17:37:39  grichenk
 * Initial revision
 *
 * ===========================================================================
 */
