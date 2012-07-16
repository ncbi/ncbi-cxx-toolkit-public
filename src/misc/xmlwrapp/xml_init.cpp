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
 * This file contains the implementation of the xml::init class for libxml.
**/

// defintion include
#include <misc/xmlwrapp/xml_init.hpp>
#include "deref_impl.hpp"

// libxml includes
#include <libxml/globals.h>
#include <libxml/xmlerror.h>
#include <libxml/parser.h>

//####################################################################
namespace {
    extern "C" void xml_error (void *, const char*, ...);
}
//####################################################################
int xml::init::ms_counter = 0;
//####################################################################
xml::init::init (void) {
    if ( ms_counter++ == 0 )
        init_library();
}
//####################################################################
xml::init::~init (void) {
    if ( --ms_counter == 0 )
        shutdown_library();
}
//####################################################################
void xml::init::init_library() {
    // set some libxml global variables
    indent_output(true);
    remove_whitespace(false);
    substitute_entities(true);
    load_external_subsets(true);
    validate_xml(false);

    // keep libxml2 from using stderr
    xmlSetGenericErrorFunc(0, xml_error);

    // Register the nodes cleanup function
    xmlDeregisterNodeDefault(xml::impl::cleanup_node);
    xmlThrDefDeregisterNodeDefault(xml::impl::cleanup_node);

    // init the parser (keeps libxml2 thread safe)
    xmlInitParser();
}
//####################################################################
void xml::init::shutdown_library() {
    xmlCleanupParser();
}
//####################################################################
void xml::init::indent_output (bool flag) {
    xmlIndentTreeOutput = flag ? 1 : 0;
}
//####################################################################
void xml::init::remove_whitespace (bool flag) {
    xmlKeepBlanksDefaultValue = flag ? 0 : 1;
}
//####################################################################
void xml::init::substitute_entities (bool flag) {
    xmlSubstituteEntitiesDefaultValue = flag ? 1 : 0;
}
//####################################################################
void xml::init::load_external_subsets (bool flag) {
    xmlLoadExtDtdDefaultValue = flag ? 1 : 0;
}
//####################################################################
void xml::init::validate_xml (bool flag) {
    xmlDoValidityCheckingDefaultValue = flag ? 1 : 0;
}
//####################################################################
namespace {
    extern "C" void xml_error (void*, const char*, ...)
    { /* don't do anything */ }
}
//####################################################################
