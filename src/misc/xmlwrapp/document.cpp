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
 * This file contains the implementation of the xml::document class.
**/

// xmlwrapp includes
#include <misc/xmlwrapp/document.hpp>
#include <misc/xmlwrapp/node.hpp>
#include <misc/xmlwrapp/dtd.hpp>
#include <misc/xmlwrapp/schema.hpp>
#include <misc/xmlwrapp/exception.hpp>
#include "utility.hpp"
#include "document_impl.hpp"
#include "node_manip.hpp"

// standard includes
#include <new>
#include <memory>
#include <iterator>
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <string.h>

// libxml includes
#include <libxml/tree.h>
#include <libxml/xinclude.h>

// bring in private libxslt stuff (see bug #1927398)
#include "result.hpp"

// Workshop compiler define
#include <ncbiconf.h>


using namespace xml;
using namespace xml::impl;


//####################################################################
namespace {
    const char const_default_encoding[] = "ISO-8859-1";

    // Callbacks for validating internal DTD
    extern "C" void cb_dtd_valid_error (void *v, const char *message, ...);
    extern "C" void cb_dtd_valid_warning (void *v, const char *message, ...);
}
//####################################################################
xml::document::document (void) {
    pimpl_ = new doc_impl;
}
//####################################################################
xml::document::document (const char *root_name) {
    pimpl_ = new doc_impl(root_name);
}
//####################################################################
xml::document::document (const node &n) {
    std::auto_ptr<doc_impl> ap(pimpl_ = new doc_impl);
    pimpl_->set_root_node(n);
    ap.release();
}
//####################################################################
xml::document::document (const document &other) {
    pimpl_ = new doc_impl(*(other.pimpl_));
}
//####################################################################
xml::document& xml::document::operator= (const document &other) {
    document tmp(other);
    swap(tmp);
    return *this;
}
//####################################################################
void xml::document::swap (document &other) {
    std::swap(pimpl_, other.pimpl_);
}
//####################################################################
xml::document::~document (void) {
    delete pimpl_;
}
//####################################################################
const xml::node& xml::document::get_root_node (void) const {
    return pimpl_->root_;
}
//####################################################################
xml::node& xml::document::get_root_node (void) {
    return pimpl_->root_;
}
//####################################################################
void xml::document::set_root_node (const node &n) {
    pimpl_->set_root_node(n);
}
//####################################################################
const std::string& xml::document::get_version (void) const {
    return pimpl_->version_;
}
//####################################################################
void xml::document::set_version (const char *version) {
    const xmlChar *old_version = pimpl_->doc_->version;
    if ( (pimpl_->doc_->version = xmlStrdup(reinterpret_cast<const xmlChar*>(version))) == 0) throw std::bad_alloc();
    pimpl_->version_ = version;
    if (old_version) xmlFree(const_cast<char*>(reinterpret_cast<const char*>(old_version)));
}
//####################################################################
const std::string& xml::document::get_encoding (void) const {
    if (pimpl_->encoding_.empty()) pimpl_->encoding_ = const_default_encoding;
    return pimpl_->encoding_;
}
//####################################################################
void xml::document::set_encoding (const char *encoding) {
    pimpl_->encoding_ = encoding;

    if (pimpl_->doc_->encoding) xmlFree(const_cast<xmlChar*>(pimpl_->doc_->encoding));
    pimpl_->doc_->encoding = xmlStrdup(reinterpret_cast<const xmlChar*>(encoding));

    if (!pimpl_->doc_->encoding) throw std::bad_alloc();
}
//####################################################################
bool xml::document::get_is_standalone (void) const {
    return pimpl_->doc_->standalone == 1;
}
//####################################################################
void xml::document::set_is_standalone (bool sa) {
    pimpl_->doc_->standalone = sa ? 1 : 0;
}
//####################################################################
bool xml::document::process_xinclude (void) {
    // xmlXIncludeProcess does not return what is says it does
    return xmlXIncludeProcess(pimpl_->doc_) >= 0;
}
//####################################################################
bool xml::document::has_internal_subset (void) const {
    return pimpl_->doc_->intSubset != 0;
}
//####################################################################
const xml::dtd& xml::document::get_internal_subset (void) const {
    if (!has_internal_subset())
        throw xml::exception("The document does not have internal subset.");
    pimpl_->internal_subset_.set_dtd_data(pimpl_->doc_->intSubset);
    return pimpl_->internal_subset_;
}
//####################################################################
bool xml::document::has_external_subset (void) const {
    return pimpl_->doc_->extSubset != 0;
}
//####################################################################
const xml::dtd& xml::document::get_external_subset (void) const {
    if (!has_external_subset())
        throw xml::exception("The document does not have external subset.");
    pimpl_->external_subset_.set_dtd_data(pimpl_->doc_->extSubset);
    return pimpl_->external_subset_;
}
//####################################################################
bool xml::document::validate (void) {
    dtd     empty_dtd;
    return empty_dtd.validate(*this, type_warnings_not_errors);
}
//####################################################################
bool xml::document::validate (const char *dtdname) {
    dtd     file_dtd(dtdname, type_warnings_not_errors);
    return file_dtd.validate(*this, type_warnings_not_errors);
}
//####################################################################
bool xml::document::validate (dtd &dtd_,
                              warnings_as_errors_type how) {
    return dtd_.validate(*this, how);
}
//####################################################################
bool xml::document::validate (schema &xsd_schema,
                              warnings_as_errors_type how) const {
    return xsd_schema.validate(*this, how);
}
//####################################################################
bool document::validate (error_messages *  messages_,
                         warnings_as_errors_type how) const {
    error_messages *                temp(messages_);
    std::auto_ptr<error_messages>   msgs;
    if (!messages_)
        msgs.reset(temp = new error_messages);

    xmlValidCtxt        vctxt;

    memset(&vctxt, 0, sizeof(vctxt));
    vctxt.error    = cb_dtd_valid_error;
    vctxt.warning  = cb_dtd_valid_warning;
    vctxt.userData = temp;

    temp->get_messages().clear();

    int retCode = xmlValidateDocument(&vctxt, pimpl_->doc_);
    if (retCode == 0)
        return false;
    if (static_cast<error_messages*>(vctxt.userData)->has_errors())
        return false;
    if (static_cast<error_messages*>(vctxt.userData)->has_warnings()) {
        if (how == type_warnings_are_errors)
            return false;
    }
    return true;
}
//####################################################################
bool document::validate (const dtd& dtd_, error_messages* messages_,
                         warnings_as_errors_type how) const {
    return dtd_.validate(*this, messages_, how);
}
//####################################################################
bool document::validate (const schema& schema_, error_messages* messages_,
                         warnings_as_errors_type how) const {
    return schema_.validate(*this, messages_, how);
}
//####################################################################
void document::set_external_subset (const dtd& dtd_) {
    if (!dtd_.get_raw_pointer())
        throw xml::exception("xml::document::set_external_subset dtd is not loaded");

    xmlDtdPtr   copy = xmlCopyDtd(static_cast<xmlDtdPtr>(dtd_.get_raw_pointer()));
    if (copy == NULL)
        throw xml::exception("Error copying DTD");

    if (pimpl_->doc_->extSubset != 0) xmlFreeDtd(pimpl_->doc_->extSubset);
    pimpl_->doc_->extSubset = copy;
}

//####################################################################
xml::document::size_type xml::document::size (void) const {
    #ifdef NCBI_COMPILER_WORKSHOP
        xml::document::size_type   dist(0);
        xml::node::const_iterator  first(begin()), last(end());
        for (; first != last; ++first) { ++dist; }
        return dist;
    #else
        return static_cast<xml::document::size_type>(std::distance(begin(),
                                                                   end()));
    #endif
}
//####################################################################
xml::node::iterator xml::document::begin (void) {
    return node::iterator(pimpl_->doc_->children);
}
//####################################################################
xml::node::const_iterator xml::document::begin (void) const {
    return node::const_iterator(pimpl_->doc_->children);
}
//####################################################################
xml::node::iterator xml::document::end (void) {
    return node::iterator(0);
}
//####################################################################
xml::node::const_iterator xml::document::end (void) const {
    return node::const_iterator(0);
}
//####################################################################
void xml::document::push_back (const node &child) {
    if (child.get_type() == node::type_element) throw xml::exception("xml::document::push_back can't take element type nodes");
    xml::impl::node_insert(reinterpret_cast<xmlNodePtr>(pimpl_->doc_), 0, static_cast<xmlNodePtr>(const_cast<node&>(child).get_node_data()));
}
//####################################################################
xml::node::iterator xml::document::insert (const node &n) {
    if (n.get_type() == node::type_element) throw xml::exception("xml::document::insert can't take element type nodes");
    return node::iterator(xml::impl::node_insert(reinterpret_cast<xmlNodePtr>(pimpl_->doc_), 0, static_cast<xmlNodePtr>(const_cast<node&>(n).get_node_data())));
}
//####################################################################
xml::node::iterator xml::document::insert (node::iterator position, const node &n) {
    if (n.get_type() == node::type_element) throw xml::exception("xml::document::insert can't take element type nodes");
    return node::iterator(xml::impl::node_insert(reinterpret_cast<xmlNodePtr>(pimpl_->doc_), static_cast<xmlNodePtr>(position.get_raw_node()), static_cast<xmlNodePtr>(const_cast<node&>(n).get_node_data())));
}
//####################################################################
xml::node::iterator xml::document::replace (node::iterator old_node, const node &new_node) {
    if (old_node->get_type() == node::type_element || new_node.get_type() == node::type_element) {
        throw xml::exception("xml::document::replace can't replace element type nodes");
    }

    return node::iterator(xml::impl::node_replace(static_cast<xmlNodePtr>(old_node.get_raw_node()), static_cast<xmlNodePtr>(const_cast<node&>(new_node).get_node_data())));
}
//####################################################################
xml::node::iterator xml::document::erase (node::iterator to_erase) {
    if (to_erase->get_type() == node::type_element) throw xml::exception("xml::document::erase can't erase element type nodes");
    return node::iterator(xml::impl::node_erase(static_cast<xmlNodePtr>(to_erase.get_raw_node())));
}
//####################################################################
xml::node::iterator xml::document::erase (node::iterator first, node::iterator last) {
    while (first != last) first = erase(first);
    return first;
}
//####################################################################
void xml::document::save_to_string (std::string &s) const {
    xmlChar *xml_string;
    int xml_string_length;

    if (pimpl_->xslt_result_ != 0) {
        pimpl_->xslt_result_->save_to_string(s);

        return;
    }

    const char *enc = pimpl_->encoding_.empty() ? 0 : pimpl_->encoding_.c_str();
    xmlDocDumpFormatMemoryEnc(pimpl_->doc_, &xml_string, &xml_string_length, enc, 1);

    xmlchar_helper helper(xml_string);
    if (xml_string_length) s.assign(helper.get(), xml_string_length);
}
//####################################################################
bool xml::document::save_to_file (const char *filename, int compression_level) const {
    std::swap(pimpl_->doc_->compression, compression_level);

    if (pimpl_->xslt_result_ != 0) {
        bool rc = pimpl_->xslt_result_->save_to_file(filename, compression_level);
        std::swap(pimpl_->doc_->compression, compression_level);

        return rc;
    }

    const char *enc = pimpl_->encoding_.empty() ? 0 : pimpl_->encoding_.c_str();
    bool rc = xmlSaveFormatFileEnc(filename, pimpl_->doc_, enc, 1) > 0;
    std::swap(pimpl_->doc_->compression, compression_level);

    return rc;
}
//####################################################################
void xml::document::set_doc_data (void *data) {
    // we own the doc now, don't free it!
    pimpl_->set_doc_data(static_cast<xmlDocPtr>(data), false);
    pimpl_->xslt_result_ = 0;
}
//####################################################################
void xml::document::set_doc_data_from_xslt (void *data, xslt::impl::result *xr) {
    // this document came from a XSLT transformation
    pimpl_->set_doc_data(static_cast<xmlDocPtr>(data), false);
    pimpl_->xslt_result_ = xr;
}
//####################################################################
void* xml::document::get_doc_data (void) {
    return pimpl_->doc_;
}
//####################################################################
void* xml::document::get_doc_data_read_only (void) const {
    return pimpl_->doc_;
}
//####################################################################
void* xml::document::release_doc_data (void) {
    xmlDocPtr xmldoc = pimpl_->doc_;
    pimpl_->doc_ = 0;

    return xmldoc;
}
//####################################################################
std::ostream& xml::operator<< (std::ostream &stream, const xml::document &doc) {
    std::string xmldata;
    doc.save_to_string(xmldata);
    stream << xmldata;
    return stream;
}
//####################################################################

namespace {
    void register_error_helper (error_message::message_type mt,
                                void *v,
                                const std::string &message) {
        try {
            error_messages *p = static_cast<error_messages*>(v);
            if (p)
                p->get_messages().push_back(error_message(message, mt));
        } catch (...) {}
    }

    extern "C" void cb_dtd_valid_error (void *v, const char *message, ...) {
        std::string temporary;

        va_list ap;
        va_start(ap, message);
        printf2string(temporary, message, ap);
        va_end(ap);

        register_error_helper(error_message::type_error, v, temporary);
    }

    extern "C" void cb_dtd_valid_warning (void *v, const char *message, ...) {
        std::string temporary;

        va_list ap;
        va_start(ap, message);
        printf2string(temporary, message, ap);
        va_end(ap);

        register_error_helper(error_message::type_warning, v, temporary);
    }
}

