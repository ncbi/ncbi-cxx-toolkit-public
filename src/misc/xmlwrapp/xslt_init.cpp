/*
 * Copyright (C) 2001-2003 Peter J Jones (pjones@pmade.org)
 * All Rights Reserved
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $Id$ 
 * NOTE: This file was modified from its original version 0.6.0
 *       to fit the NCBI C++ Toolkit build framework and
 *       API and functionality requirements. 
 */

/** @file
 * This file contains the implementation of the xslt::init class.
**/

// defintion include
#include <misc/xmlwrapp/xslt_init.hpp>

// libxslt includes
#include <libxslt/xslt.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>

// HAVE_LIBEXSLT define is here
#include <ncbiconf.h>

#ifdef HAVE_LIBEXSLT
    #include <libexslt/exslt.h>
#endif

//####################################################################
namespace {
    extern "C" void xslt_error (void *, const char*, ...);
}
//####################################################################
int xslt::init::ms_counter = 0;
//####################################################################
xslt::init::init (void) {
    if ( ms_counter++ == 0 )
        init_library();
}
//####################################################################
xslt::init::~init (void) {
    if ( --ms_counter == 0 )
        shutdown_library();
}
//####################################################################
void xslt::init::init_library() {
    xsltInit();

    // set some defautls
    process_xincludes(true);

    // keep libxslt silent
    xsltSetGenericErrorFunc(0, xslt_error);
    xsltSetGenericDebugFunc(0, xslt_error);

    #ifdef HAVE_LIBEXSLT
        // load EXSLT
        exsltRegisterAll();
    #endif
}
//####################################################################
void xslt::init::shutdown_library() {
    xsltCleanupGlobals();
}
//####################################################################
void xslt::init::process_xincludes (bool flag) {
    xsltSetXIncludeDefault(flag ? 1 : 0);
}
//####################################################################
namespace {
    extern "C" void xslt_error (void*, const char*, ...)
    { /* don't do anything */ }
}
//####################################################################
