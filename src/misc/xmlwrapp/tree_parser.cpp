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
 * This file contains the implementation of the xml::tree_parser class for
 * the libxml XML parser.
**/

// xmlwrapp includes
#include <misc/xmlwrapp/tree_parser.hpp>
#include <misc/xmlwrapp/document.hpp>
#include "utility.hpp"

// libxml includes
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/SAX.h>
#include <libxml/xmlversion.h>

// standard includes
#include <stdexcept>
#include <cstring>
#include <string>
#include <memory>

using namespace xml;
using namespace xml::impl;

//####################################################################
/*
 * This is a hack to fix a problem with a change in the libxml2 API for
 * versions starting at 2.6.0
 */
#if LIBXML_VERSION >= 20600
#   define initxmlDefaultSAXHandler xmlSAX2InitDefaultSAXHandler
#   include <libxml/SAX2.h>
#endif
//####################################################################
namespace {
    const char const_default_error[] = "unknown XML parsing error";

    extern "C" void cb_tree_error (void *v, const char *message, ...);
    extern "C" void cb_tree_warning (void *v, const char *, ...);
    extern "C" void cb_tree_ignore (void*, const xmlChar*, int);
}
//####################################################################
struct xml::impl::tree_impl {
    tree_impl (void) : last_error_(const_default_error), warnings_(false), okay_(false) {
	std::memset(&sax_, 0, sizeof(sax_));
	initxmlDefaultSAXHandler(&sax_, 0);

	sax_.warning	= cb_tree_warning;
	sax_.error	= cb_tree_error;
	sax_.fatalError	= cb_tree_error;

	if (xmlKeepBlanksDefaultValue == 0) sax_.ignorableWhitespace =  cb_tree_ignore;
    }

    document doc_;
    xmlSAXHandler sax_;
    std::string last_error_;
    bool warnings_;
    bool okay_;
};
//####################################################################
xml::tree_parser::tree_parser (const char *name, bool allow_exceptions) {
    std::auto_ptr<tree_impl> ap(pimpl_ = new tree_impl);

    pimpl_->okay_ = true;
    xmlDocPtr tmpdoc = xmlSAXParseFileWithData(&(pimpl_->sax_), name, 0, pimpl_);

    if (tmpdoc && pimpl_->okay_)
    {
        // All is fine
        pimpl_->doc_.set_doc_data(tmpdoc);
    }
    else
    {
        // A problem appeared
        if (tmpdoc) xmlFreeDoc(tmpdoc);
        if (allow_exceptions) throw std::runtime_error(pimpl_->last_error_);
    }

    ap.release();
}
//####################################################################
xml::tree_parser::tree_parser (const char *data, size_type size, bool allow_exceptions) {
    std::auto_ptr<tree_impl> ap(pimpl_ = new tree_impl);
    xmlParserCtxtPtr ctxt;

    if ( (ctxt = xmlCreateMemoryParserCtxt(data, static_cast<int>(size))) == 0) {
        throw std::bad_alloc();
    }

    if (ctxt->sax) xmlFree(ctxt->sax);
    ctxt->sax = &(pimpl_->sax_);

    ctxt->_private = pimpl_;

    pimpl_->okay_ = true;
    int ret(xmlParseDocument(ctxt));

    if (!ctxt->wellFormed || ret != 0 || !pimpl_->okay_) {
        xmlFreeDoc(ctxt->myDoc);
        ctxt->myDoc = 0;
        ctxt->sax = 0;
        xmlFreeParserCtxt(ctxt);
        pimpl_->okay_ = false;

        if (allow_exceptions) throw std::runtime_error(pimpl_->last_error_);
        ap.release(); return; // handle non-exception case
    }

    pimpl_->doc_.set_doc_data(ctxt->myDoc);
    ctxt->sax = 0;

    xmlFreeParserCtxt(ctxt);
    ap.release();
}
//####################################################################
xml::tree_parser::~tree_parser (void) {
    delete pimpl_;
}
//####################################################################
bool xml::tree_parser::operator! (void) const {
    return !pimpl_->okay_;
}
//####################################################################
const std::string& xml::tree_parser::get_error_message (void) const {
    return pimpl_->last_error_;
}
//####################################################################
bool xml::tree_parser::had_warnings (void) const {
    return pimpl_->warnings_;
}
//####################################################################
xml::document& xml::tree_parser::get_document (void) {
    return pimpl_->doc_;
}
//####################################################################
const xml::document& xml::tree_parser::get_document (void) const {
    return pimpl_->doc_;
}
//####################################################################
namespace {
    //####################################################################
    extern "C" void cb_tree_error (void *v, const char *message, ...) {
	try {

	    xmlParserCtxtPtr ctxt = static_cast<xmlParserCtxtPtr>(v);
	    tree_impl *p = static_cast<tree_impl*>(ctxt->_private);
	    if (!p) return; // handle bug in older versions of libxml

            p->okay_ = false;

	    va_list ap;
	    va_start(ap, message);
	    printf2string(p->last_error_, message, ap);
	    va_end(ap);

	    xmlStopParser(ctxt);
	} catch (...) { }
    }
    //####################################################################
    extern "C" void cb_tree_warning (void *v, const char *, ...) {
	try {

	    xmlParserCtxtPtr ctxt = static_cast<xmlParserCtxtPtr>(v);
	    tree_impl *p = static_cast<tree_impl*>(ctxt->_private);
	    if (!p) return; // handle bug in older versions of libxml

	    p->warnings_ = true;

	} catch (...) { }
    }
    //####################################################################
    extern "C" void cb_tree_ignore (void*, const xmlChar*, int) {
	return;
    }
    //####################################################################
}
//####################################################################
