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
#include <execinfo.h>
#if NCBI_COMPILER_VERSION >= 310
#  include <cxxabi.h>
#endif
#include <vector>
#include <corelib/ncbistr.hpp>

BEGIN_NCBI_SCOPE


void CStackTrace::GetStackTrace(TStack& stack_trace)
{
    vector<void*> trace(100);
    size_t items = backtrace(&trace[0], 100);
    trace.resize(items);

    char** syms = backtrace_symbols(&trace[0], trace.size());
    for (size_t i = 0;  i < items;  ++i) {
        string sym = syms[i];

        SStackFrameInfo info;
        info.func = sym;

        string::size_type pos = sym.find_first_of("(");
        if (pos != string::npos) {
            info.module = sym.substr(0, pos);
            sym.erase(0, pos + 1);
        }

        pos = sym.find_first_of(")");
        if (pos != string::npos) {
            sym.erase(pos);
            pos = sym.find_last_of("+");
            if (pos != string::npos) {
                string sub = sym.substr(pos + 1, sym.length() - pos);
                info.func = sym.substr(0, pos);
                info.offs = NStr::StringToInt(sub, 0, 16);
            }
        }

        stack_trace.push_back(info);
    }

    free(syms);

    //
    // name demangling
    //
    NON_CONST_ITERATE (list<SStackFrameInfo>, iter, stack_trace) {
        SStackFrameInfo& info = *iter;

        if ( !info.func.empty()  &&  info.func[0] == '_') {
#if NCBI_COMPILER_VERSION >= 310
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
        }
    }

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
