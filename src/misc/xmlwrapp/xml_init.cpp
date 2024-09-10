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

// definition include
#include <misc/xmlwrapp/xml_init.hpp>
#include "deref_impl.hpp"
#include "https_input_impl.hpp"

// libxml includes
#include <libxml/globals.h>
#include <libxml/xmlerror.h>
#include <libxml/parser.h>

#include <optional>

// libxml2 global variables replacement.
// The globals were deprecated (there are compilation warnings for the newer
// library version). However it is necessary to keep the backward
// compatibility. Thus the following variables are introduced together with the
// thread local counterparts

// xmlKeepBlanksDefaultValue replacement
bool                                g_remove_whitespaces = false;
thread_local std::optional<bool>    thr_remove_whitespaces;

// xmlSubstituteEntitiesDefaultValue replacement
bool                                g_substitute_entities = true;
thread_local std::optional<bool>    thr_substitute_entities;

// xmlLoadExtDtdDefaultValue replacement
bool                                g_load_external_subsets = true;
thread_local std::optional<bool>    thr_load_external_subsets;

// xmlDoValidityCheckingDefaultValue replacement
bool                                g_validate_xml = false;
thread_local std::optional<bool>    thr_validate_xml;


//####################################################################
namespace {
    extern "C" void xml_error (void *, const char*, ...);
}
//####################################################################
int xml::init::ms_counter = 0;
bool xml::init::do_cleanup_at_exit = true;
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
    global_remove_whitespace(false);
    global_substitute_entities(true);
    global_load_external_subsets(true);
    global_validate_xml(false);

    // keep libxml2 from using stderr
    xmlSetGenericErrorFunc(0, xml_error);

    // Register the nodes cleanup function
    xmlDeregisterNodeDefault(xml::impl::cleanup_node);
    xmlThrDefDeregisterNodeDefault(xml::impl::cleanup_node);

    // init the parser (keeps libxml2 thread safe)
    xmlInitParser();

    xml::impl::register_https_input();
}
//####################################################################
void xml::init::shutdown_library() {
    if ( do_cleanup_at_exit )
        xmlCleanupParser();
}
//####################################################################
void xml::init::indent_output (bool flag) {
    xmlIndentTreeOutput = flag ? 1 : 0;
}
//####################################################################
void xml::init::remove_whitespace (bool flag) {
    thr_remove_whitespaces = flag;
}
//####################################################################
bool xml::init::get_remove_whitespace (void) {
    if (thr_remove_whitespaces.has_value())
        return thr_remove_whitespaces.value();
    return g_remove_whitespaces;
}

void xml::init::global_remove_whitespace (bool flag) {
    g_remove_whitespaces = flag;
}

bool xml::init::get_global_remove_whitespace (void) {
    return g_remove_whitespaces;
}

//####################################################################
void xml::init::substitute_entities (bool flag) {
    thr_substitute_entities = flag;
}

bool xml::init::get_substitute_entities (void) {
    if (thr_substitute_entities.has_value())
        return thr_substitute_entities.value();
    return g_substitute_entities;
}

void xml::init::global_substitute_entities (bool flag) {
    g_substitute_entities = flag;
}

bool xml::init::get_global_substitute_entities (void) {
    return g_substitute_entities;
}

//####################################################################
void xml::init::load_external_subsets (bool flag) {
    thr_load_external_subsets = flag;
}

bool xml::init::get_load_external_subsets (void) {
    if (thr_load_external_subsets.has_value())
        return thr_load_external_subsets.value();
    return g_load_external_subsets;
}

void xml::init::global_load_external_subsets (bool flag) {
    g_load_external_subsets = flag;
}

bool xml::init::get_global_load_external_subsets (void) {
    return g_load_external_subsets;
}

//####################################################################
void xml::init::validate_xml (bool flag) {
    thr_validate_xml = flag;
}

bool xml::init::get_validate_xml (void) {
    if (thr_validate_xml.has_value())
        return thr_validate_xml.value();
    return g_validate_xml;
}

void xml::init::global_validate_xml (bool flag) {
    g_validate_xml = flag;
}

bool xml::init::get_global_validate_xml (void) {
    return g_validate_xml;
}

//####################################################################
void xml::init::library_cleanup_on_exit (bool flag) {
    do_cleanup_at_exit = flag;
}
//####################################################################
namespace {
    extern "C" void xml_error (void*, const char*, ...)
    { /* don't do anything */ }
}
//####################################################################
