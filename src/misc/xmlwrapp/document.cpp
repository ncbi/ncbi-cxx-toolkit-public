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
#include "allow_auto_ptr.hpp"
#include <misc/xmlwrapp/document.hpp>
#include <misc/xmlwrapp/node.hpp>
#include <misc/xmlwrapp/dtd.hpp>
#include <misc/xmlwrapp/schema.hpp>
#include <misc/xmlwrapp/exception.hpp>
#include "stylesheet_impl.hpp"
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
#include <libxml/parser.h>
#include <libxml/parserInternals.h>
#include <libxml/SAX.h>
#include <libxml/xmlversion.h>
#include <libxml/tree.h>
#include <libxml/xinclude.h>
#include <libxml/xmlsave.h>
#include <libxml/c14n.h>


// Workshop compiler define
#include <ncbiconf.h>


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

static int  c14n_op_to_libxml2_op(xml::canonicalization_option  op)
{
    switch (op) {
        case xml::sort_attr_and_ns:
            return -1;
        case xml::sort_attr_and_ns_no_decl:
            return -1;
        case xml::c14n_1_0:
            return XML_C14N_1_0;
        case xml::c14n_exclusive_1_0:
            return XML_C14N_EXCLUSIVE_1_0;
        case xml::c14n_1_1:
            return XML_C14N_1_1;
    }

    throw xml::exception("Unknown canonicalization option");
}


struct cmp
{
    bool operator() (const xml::node &lhs, const xml::node &rhs)
    {
        if ( lhs.get_type() != xml::node::type_element ) return true;
        if ( rhs.get_type() != xml::node::type_element ) return false;
        return strcmp( lhs.get_name(), rhs.get_name() ) < 0;
    }
};
static void sort_node_recursively(xml::node &  n)
{
    n.sort(cmp());
    for (xml::node::iterator  k = n.begin(); k != n.end(); ++k)
        if (k->get_type() == xml::node::type_element)
            sort_node_recursively(*k);
}

static void remove_comments_recursively(xml::node &  n)
{
    xml::node::iterator  k = n.begin();
    while (k != n.end()) {
        if (k->get_type() == xml::node::type_comment)
            k = n.erase(k);
        else {
            remove_comments_recursively(*k);
            ++k;
        }
    }
}

static void sort_attributes_recursively(xml::node &  n)
{
    if (n.get_type() == xml::node::type_element)
        n.get_attributes().sort();
    for (xml::node::iterator  k = n.begin(); k != n.end(); ++k)
        sort_attributes_recursively(*k);
}

static void sort_namespace_definitions_recursively(xml::node &  n)
{
    n.sort_namespace_definitions();
    for (xml::node::iterator  k = n.begin(); k != n.end(); ++k)
        sort_namespace_definitions_recursively(*k);
}


static void set_parser_context_options(xmlParserCtxtPtr  ctxt)
{
    if (ctxt == NULL)
        return;

    // Parser context options
    // XML_PARSE_HUGE - overrides the default libxml2 parser limits. If it is
    //                  not here then certain large documents will not be parsed
    // XML_PARSE_NODICT - when xmlCtxtUseOptions() is called, it sets the
    //                    context field 'dictNames' to 1. We would prefer not
    //                    to change the initialized context so this flag is
    //                    supplied.
    int     options = XML_PARSE_HUGE | XML_PARSE_NODICT;

    // When the xmlCtxtUseOptions() is called it updates the context
    // 'loadsubset' field. To avoid updating it the XML_PARSE_DTDLOAD flag
    // may need to be supplied if the user requested the loading external DTD
    // (default). This behavior is controlled by the user via the
    // xml::init::load_external_subsets() call.
    if (xmlLoadExtDtdDefaultValue != 0)
        options |= XML_PARSE_DTDLOAD;

    // When the xmlCtxtUseOptions() is called it updates the context
    // 'keepBlanks' field. To avoid updating it the XML_PARSE_NOBLANKS flag
    // may need to be supplied.
    // The user controls this behavior via the xml::init::remove_whitespace()
    // call.
    if (xmlKeepBlanksDefaultValue == 0)
        options |= XML_PARSE_NOBLANKS;

    // When the xmlCtxtUseOptions() is called it updates the context
    // 'validate' field. To avoid updating it the XML_PARSE_DTDVALID flag may
    // need to be supplied.
    // The user controls this behavior via the xml::init::validate_xml() call.
    if (xmlDoValidityCheckingDefaultValue != 0)
        options |= XML_PARSE_DTDVALID;

    // When the xmlCtxtUseOptions() is called it updates the context
    // 'replaceEntities' field. To avoid updating it the XML_PARSE_NOENT flag
    // may need to be supplied.
    // The user controls this behavior via the xml::init::substitute_entities()
    // call.
    if (xmlSubstituteEntitiesDefaultValue != 0)
        options |= XML_PARSE_NOENT;

    // Set the context options
    xmlCtxtUseOptions(ctxt, options);
}


//####################################################################
namespace {
    const char const_default_encoding[] = "ISO-8859-1";

    // Callbacks for validating internal DTD
    extern "C" void cb_dtd_valid_error (void *v, const char *message, ...);
    extern "C" void cb_dtd_valid_warning (void *v, const char *message, ...);

    // Callbacks for the tree parser
    extern "C" void cb_tree_parser_fatal_error (void *v,
                                                const char *message, ...);
    extern "C" void cb_tree_parser_error (void *v, const char *message, ...);
    extern "C" void cb_tree_parser_warning (void *v, const char *message, ...);
    extern "C" void cb_tree_parser_ignore (void*, const xmlChar*, int);
}
//####################################################################
xml::document::document (void) :
    pimpl_(new doc_impl)
{}
//####################################################################


// This custom xmlSAXParseFileWithData_Custom() function is basically a copy of
// the standard libxml2 xmlSAXParseFileWithData() function with a small change.
// It allows to set the appropriate context options, notably the ability to
// parse huge XML documents.
// The standard libxml2 function hides the parser context and therefore the
// options are not available.
static xmlDocPtr xmlSAXParseFileWithData_Custom(xmlSAXHandlerPtr sax,
                                                const char *filename,
                                                void *data)
{
    xmlDocPtr ret;
    xmlParserCtxtPtr ctxt;

    ctxt = xmlCreateFileParserCtxt(filename);
    if (ctxt == NULL)
        return NULL;

    // This is the major difference to the standard libxml2 version: setting
    // the context options.
    set_parser_context_options(ctxt);

    if (sax != NULL) {
        if (ctxt->sax != NULL)
            xmlFree(ctxt->sax);
        ctxt->sax = sax;
    }

    if (data!=NULL)
        ctxt->_private = data;

    if (ctxt->directory == NULL)
        ctxt->directory = xmlParserGetDirectory(filename);

    xmlParseDocument(ctxt);
    if (ctxt->wellFormed) {
        ret = ctxt->myDoc;
        if (ret != NULL) {
            if (ctxt->input->buf->compressed > 0)
                ret->compression = 9;
            else
                ret->compression = ctxt->input->buf->compressed;
        }
    }
    else {
        ret = NULL;
        xmlFreeDoc(ctxt->myDoc);
        ctxt->myDoc = NULL;
    }
    if (sax != NULL)
        ctxt->sax = NULL;
    xmlFreeParserCtxt(ctxt);
    return ret;
}


xml::document::document (const char *               filename,
                         error_messages *           messages,
                         warnings_as_errors_type    how) :
    pimpl_(NULL)
{
    if (!filename)
        throw xml::exception("invalid file name");

    xmlSAXHandler   sax;
    memset(&sax, 0, sizeof(sax));
    initxmlDefaultSAXHandler(&sax, 0);

    sax.warning    = cb_tree_parser_warning;
    sax.error      = cb_tree_parser_error;
    sax.fatalError = cb_tree_parser_fatal_error;

    if (xmlKeepBlanksDefaultValue == 0)
        sax.ignorableWhitespace =  cb_tree_parser_ignore;

    error_messages *                temp(messages);
    std::auto_ptr<error_messages>   msgs;
    if (!messages)
        msgs.reset(temp = new error_messages);
    else
        messages->get_messages().clear();

    xmlDocPtr tmpdoc = xmlSAXParseFileWithData_Custom(&sax, filename, temp);
    if (!tmpdoc) {
        // It is a common case that the file does not exist or cannot be
        // opened. libxml2 does not recognise it so make a test here to
        // have a better error message.
        FILE *test(fopen(filename, "r"));
        if (test == NULL) {
            error_message  msg("Cannot open file",
                               error_message::type_fatal_error);
            temp->get_messages().push_back(msg);
            throw parser_exception(*temp);
        }
        fclose(test);
    }

    if (is_failure(temp, how) || !tmpdoc) {
        if (tmpdoc) xmlFreeDoc(tmpdoc);
        throw parser_exception(*temp);
    }

    pimpl_ = new doc_impl;
    set_doc_data(tmpdoc);
}
//####################################################################
xml::document::document (const char *               data,
                         size_type                  size,
                         error_messages *           messages,
                         warnings_as_errors_type    how) :
    pimpl_(NULL)
{
    if (!data)
        throw xml::exception("invalid data pointer");

    xmlParserCtxtPtr    ctxt = xmlCreateMemoryParserCtxt(data,
                                    size_t_to_int_conversion(size,
                                        "memory buffer is too large"));
    if (ctxt == 0)
        throw std::bad_alloc();

    set_parser_context_options(ctxt);

    xmlSAXHandler   sax;
    memset(&sax, 0, sizeof(sax));
    initxmlDefaultSAXHandler(&sax, 0);

    sax.warning    = cb_tree_parser_warning;
    sax.error      = cb_tree_parser_error;
    sax.fatalError = cb_tree_parser_fatal_error;

    if (xmlKeepBlanksDefaultValue == 0)
        sax.ignorableWhitespace =  cb_tree_parser_ignore;

    if (ctxt->sax)
        xmlFree(ctxt->sax);
    ctxt->sax = &sax;

    error_messages *                temp(messages);
    std::auto_ptr<error_messages>   msgs;
    if (!messages)
        msgs.reset(temp = new error_messages);
    else
        messages->get_messages().clear();
    ctxt->_private = temp;

    int ret(xmlParseDocument(ctxt));

    if (!ctxt->wellFormed || ret != 0 || is_failure(temp, how)) {
        if (ctxt->myDoc)
            xmlFreeDoc(ctxt->myDoc);
        ctxt->sax = 0;
        xmlFreeParserCtxt(ctxt);

        throw parser_exception(*temp);
    }

    pimpl_ = new doc_impl;
    set_doc_data(ctxt->myDoc);
    ctxt->sax = 0;

    xmlFreeParserCtxt(ctxt);
}
//####################################################################
xml::document::document (const char *root_name) :
    pimpl_(new doc_impl(root_name))
{}
//####################################################################
xml::document::document (const node &n) {
    std::auto_ptr<doc_impl> ap(pimpl_ = new doc_impl);
    pimpl_->set_root_node(n);
    ap.release();
}
//####################################################################
xml::document::document (const document_proxy &  doc_proxy) :
    pimpl_(new doc_impl)
{
    set_doc_data_from_xslt(doc_proxy.result_, doc_proxy.style_sheet_);
    // Now the result doc is owned
    doc_proxy.release();
}
//####################################################################
static const std::size_t   const_buffer_size = 4096;

xml::document::document (std::istream &           stream,
                         error_messages *         messages,
                         warnings_as_errors_type  how) :
    pimpl_(NULL)
{
    /* Deal with the SAX handler first */
    xmlSAXHandler       sax;

    memset(&sax, 0, sizeof(sax));
    initxmlDefaultSAXHandler(&sax, 0);

    sax.warning     = cb_tree_parser_warning;
    sax.error       = cb_tree_parser_error;
    sax.fatalError  = cb_tree_parser_fatal_error;


    if (xmlKeepBlanksDefaultValue == 0)
        sax.ignorableWhitespace =  cb_tree_parser_ignore;

    /* Make sure we have where to collect messages */
    error_messages *                temp(messages);
    std::auto_ptr<error_messages>   msgs;
    if (!messages)
        msgs.reset(temp = new error_messages);
    else
        messages->get_messages().clear();

    /* Make sure that the stream is not empty */
    if (stream && (stream.eof() || stream.peek() == std::istream::traits_type::eof()))
    {
        temp->get_messages().push_back(error_message("empty xml document",
                                                     error_message::type_error));
        throw parser_exception(*temp);
    }

    /* Create the context to parse the stream */
    xmlParserCtxtPtr    ctxt = xmlCreatePushParserCtxt(&sax, 0, 0, 0, 0);
    if (ctxt == 0)
        throw std::bad_alloc();
    ctxt->_private = temp;

    set_parser_context_options(ctxt);

    /* Parse the document chunk by chunk */
    char                buffer[const_buffer_size];

    while (stream.read(buffer, const_buffer_size) || stream.gcount())
    {
        if (xmlParseChunk(ctxt, buffer, static_cast<int>(stream.gcount()), 0) != 0)
            break;
    }
    xmlParseChunk(ctxt, 0, 0, 1);


    /* The parsing has been finished, check the results */
    if (!ctxt->wellFormed || ctxt->myDoc == NULL || is_failure(temp, how))
    {
        if (ctxt->myDoc)
            xmlFreeDoc(ctxt->myDoc);
        xmlFreeParserCtxt(ctxt);

        throw parser_exception(*temp);
    }

    /* Fine, the doc is OK */

    pimpl_ = new doc_impl;
    set_doc_data(ctxt->myDoc);
    xmlFreeParserCtxt(ctxt);
}
//####################################################################
xml::document::document (const document &other) :
    pimpl_(new doc_impl(*(other.pimpl_)))
{} /* NCBI_FAKE_WARNING */
//####################################################################
xml::document& xml::document::operator= (const document &other) {
    document tmp(other);        /* NCBI_FAKE_WARNING */
    swap(tmp);
    return *this;
}
//####################################################################
xml::document& xml::document::assign (const document &other) {
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
    pimpl_->root_.set_node_data(xmlDocGetRootElement(pimpl_->doc_));
    return pimpl_->root_;
}
//####################################################################
xml::node& xml::document::get_root_node (void) {
    pimpl_->root_.set_node_data(xmlDocGetRootElement(pimpl_->doc_));
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
    const xmlChar *     old_version = pimpl_->doc_->version;

    pimpl_->doc_->version = xmlStrdup(
                                reinterpret_cast<const xmlChar*>(version));

    if ( pimpl_->doc_->version == 0)
        throw std::bad_alloc();

    pimpl_->version_ = version;
    if (old_version)
        xmlFree(const_cast<char*>(reinterpret_cast<const char*>(old_version)));
}
//####################################################################
const std::string& xml::document::get_encoding (void) const {
    if (pimpl_->encoding_.empty())
        pimpl_->encoding_ = const_default_encoding;
    return pimpl_->encoding_;
}
//####################################################################
void xml::document::set_encoding (const char *encoding) {
    pimpl_->encoding_ = encoding;

    if (pimpl_->doc_->encoding)
        xmlFree(const_cast<xmlChar*>(pimpl_->doc_->encoding));

    pimpl_->doc_->encoding = xmlStrdup(
                                reinterpret_cast<const xmlChar*>(encoding));

    if (!pimpl_->doc_->encoding)
        throw std::bad_alloc();
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
        throw xml::exception("xml::document::set_external_subset "
                             "dtd is not loaded");

    xmlDtdPtr   copy = xmlCopyDtd(
                            static_cast<xmlDtdPtr>(dtd_.get_raw_pointer()));
    if (copy == NULL)
        throw xml::exception("Error copying DTD");

    if (pimpl_->doc_->extSubset != 0)
        xmlFreeDtd(pimpl_->doc_->extSubset);
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
    if (child.get_type() == node::type_element)
        throw xml::exception("xml::document::push_back "
                             "can't take element type nodes");
    xml::impl::node_insert(reinterpret_cast<xmlNodePtr>(pimpl_->doc_), 0,
                           static_cast<xmlNodePtr>(
                               const_cast<node&>(child).get_node_data()));
}
//####################################################################
xml::node::iterator xml::document::insert (const node &n) {
    if (n.get_type() == node::type_element)
        throw xml::exception("xml::document::insert "
                             "can't take element type nodes");
    return node::iterator(
            xml::impl::node_insert(reinterpret_cast<xmlNodePtr>(pimpl_->doc_),
                                   0,
                                   static_cast<xmlNodePtr>(
                                       const_cast<node&>(n).get_node_data())));
}
//####################################################################
xml::node::iterator xml::document::insert (node::iterator position,
                                           const node &n) {
    if (n.get_type() == node::type_element)
        throw xml::exception("xml::document::insert "
                             "can't take element type nodes");
    return node::iterator(
            xml::impl::node_insert(reinterpret_cast<xmlNodePtr>(pimpl_->doc_),
                                   static_cast<xmlNodePtr>(
                                                    position.get_raw_node()),
                                   static_cast<xmlNodePtr>(
                                       const_cast<node&>(n).get_node_data())));
}
//####################################################################
xml::node::iterator xml::document::replace (node::iterator old_node,
                                            const node &new_node) {
    if (old_node->get_type() == node::type_element ||
        new_node.get_type() == node::type_element) {
        throw xml::exception("xml::document::replace "
                             "can't replace element type nodes");
    }

    return node::iterator(
            xml::impl::node_replace(
                static_cast<xmlNodePtr>(old_node.get_raw_node()),
                static_cast<xmlNodePtr>(
                    const_cast<node&>(new_node).get_node_data())));
}
//####################################################################
xml::node::iterator xml::document::erase (node::iterator to_erase) {
    if (to_erase->get_type() == node::type_element)
        throw xml::exception("xml::document::erase "
                             "can't erase element type nodes");
    return node::iterator(
            xml::impl::node_erase(
                static_cast<xmlNodePtr>(to_erase.get_raw_node())));
}
//####################################################################
xml::node::iterator xml::document::erase (node::iterator first,
                                          node::iterator last) {
    while (first != last)
        first = erase(first);
    return first;
}
//####################################################################
void xml::document::save_to_string (std::string &s,
                                    save_option_flags flags) const {
    int compression_level = flags & 0xFFFF;

    s.clear();

    // Compression level is currently not analyzed by libxml2
    // So this might work in the future implementations but is ignored now.

    if (pimpl_->xslt_stylesheet_ != 0) {
        if (!xslt::impl::is_xml_output_method(pimpl_->xslt_stylesheet_)) {
            std::swap(pimpl_->doc_->compression, compression_level);
            xslt::impl::save_to_string(pimpl_->doc_,
                                       pimpl_->xslt_stylesheet_, s);
            std::swap(pimpl_->doc_->compression, compression_level);
            return;
        }
    }

    int libxml2_options = convert_to_libxml2_save_options(flags);

    // In theory it was possible to use the pimpl_->doc_->encoding
    // value however a cached value is used instead.
    const char *enc = pimpl_->encoding_.empty() ? 0 : pimpl_->encoding_.c_str();
    xmlSaveCtxtPtr  ctxt = xmlSaveToIO(save_to_string_cb, NULL, &s,
                                       enc, libxml2_options);

    if (ctxt) {
        std::swap(pimpl_->doc_->compression, compression_level);
        xmlSaveDoc(ctxt, pimpl_->doc_);
        std::swap(pimpl_->doc_->compression, compression_level);
        xmlSaveClose(ctxt);     // xmlSaveFlush() is called in xmlSaveClose()
    }
}
//####################################################################
void xml::document::save_to_string_canonical (
                    std::string &                      str,
                    canonicalization_option            c14n_option,
                    canonicalization_comments_option   comments_option,
                    canonicalization_format_option     format_option,
                    canonicalization_node_sort_option  node_sort_option) const
{
    if (pimpl_->xslt_stylesheet_ != 0)
        if (!xslt::impl::is_xml_output_method(pimpl_->xslt_stylesheet_))
            throw xml::exception("cannot canonicalize the document: "
                                 "it holds xslt transformation results "
                                 "which are not an xml");

    int     libxml2_mode = -1;  // libxml2 options start from 0 and go up
    if (c14n_option != xml::sort_attr_and_ns &&
        c14n_option != xml::sort_attr_and_ns_no_decl)
        libxml2_mode = c14n_op_to_libxml2_op(c14n_option);
    int     libxml2_with_comments = 0;
    if (comments_option == keep_comments)
        libxml2_with_comments = 1;

    // Here: the document format could be a mess. A part of it could come from
    //       a nicely formatted file and then some nodes could be added
    //       programmatically without formatting.
    //       The libxml2 library allows to skip the formatting text nodes only
    //       at the time of initial parsing. So if the formatting is required
    //       the document we have at hand has to be saved and loaded again with
    //       stripping formatting nodes.

    // Serialize and deserialize: this covers the XSLT results processing
    // and checks if the document is well formed
    int             old_remove_whitespaces = xmlKeepBlanksDefaultValue;
    std::string     serialized_doc;

    save_to_string(serialized_doc, save_op_no_format);
    if (format_option == with_formatting)
        xmlKeepBlanksDefaultValue = 0;
    else
        xmlKeepBlanksDefaultValue = 1;

    document    tmp_doc;
    try {
        tmp_doc = document(serialized_doc.c_str(), serialized_doc.size(), NULL,
                           type_warnings_not_errors);
    } catch (...) {
        xmlKeepBlanksDefaultValue = old_remove_whitespaces;
        throw;
    }

    // Now, recursively sort the nodes in the document
    // The sorting must be done on a copy of the document to avoid any changes
    // in the original document
    if (node_sort_option == with_node_sorting)
        sort_node_recursively(tmp_doc.get_root_node());

    if (format_option == with_formatting) {
        // It was loaded with stripped formatting whitespaces.
        // Now we save and load keeping whitespaces
        xmlKeepBlanksDefaultValue = 1;
        // save_op_default makes libxml2 to insert the formatting
        tmp_doc.save_to_string(serialized_doc, save_op_default);
        // Load the document with saved formatting nodes
        tmp_doc = document(serialized_doc.c_str(), serialized_doc.size(), NULL,
                           type_warnings_not_errors);
    }

    // Restore the previous value of the xmlKeepBlanksDefaultValue
    xmlKeepBlanksDefaultValue = old_remove_whitespaces;

    if (libxml2_mode != -1) {
        // Finally, canonicalize the temporary document
        xmlChar *   result = NULL;
        int         size = xmlC14NDocDumpMemory(tmp_doc.pimpl_->doc_,
                                                NULL, libxml2_mode,
                                                NULL, libxml2_with_comments,
                                                &result);
        if (size < 0)
            throw xml::exception("xml::document::save_to_string_canonical "
                                 "failed to canonicalize");

        str.assign(reinterpret_cast<const char *>(result), size);
        if (result != NULL)
            xmlFree(result);
    } else {
        node &      root_node = tmp_doc.get_root_node();

        // No real canonicalization required. The following will be done:
        // - remove comments if required
        // - sort attributes
        // - sort namespace definitions
        if (comments_option == strip_comments) {
            if (root_node.get_type() == xml::node::type_comment) {
                str.assign("");
                return;
            }
            remove_comments_recursively(root_node);
        }

        sort_attributes_recursively(root_node);
        sort_namespace_definitions_recursively(root_node);


        save_option_flags   flags = 0;
        if (c14n_option == xml::sort_attr_and_ns_no_decl)
            flags |= save_op_no_decl;
        if (format_option == without_formatting)
            flags |= save_op_no_format;

        tmp_doc.save_to_string(str, flags);
    }
}
//####################################################################
void xml::document::save_to_stream (std::ostream &stream,
                                    save_option_flags flags) const {
    int compression_level = flags & 0xFFFF;

    // Compression level is currently not analyzed by libxml2
    // So this might work in the future implementations but is ignored now.

    if (pimpl_->xslt_stylesheet_ != 0) {
        if (!xslt::impl::is_xml_output_method(pimpl_->xslt_stylesheet_)) {
            std::swap(pimpl_->doc_->compression, compression_level);
            std::string s;
            xslt::impl::save_to_string(pimpl_->doc_,
                                       pimpl_->xslt_stylesheet_, s);
            stream << s;
            std::swap(pimpl_->doc_->compression, compression_level);
            return;
        }
    }

    int libxml2_options = convert_to_libxml2_save_options(flags);

    // In theory it was possible to use the pimpl_->doc_->encoding
    // value however a cached value is used instead.
    const char *enc = pimpl_->encoding_.empty() ? 0 : pimpl_->encoding_.c_str();
    xmlSaveCtxtPtr  ctxt = xmlSaveToIO(save_to_stream_cb, NULL, &stream,
                                       enc, libxml2_options);

    if (ctxt) {
        std::swap(pimpl_->doc_->compression, compression_level);
        xmlSaveDoc(ctxt, pimpl_->doc_);
        std::swap(pimpl_->doc_->compression, compression_level);
        xmlSaveClose(ctxt);     // xmlSaveFlush() is called in xmlSaveClose()
    }
    std::swap(pimpl_->doc_->compression, compression_level);
    return;
}
//####################################################################
bool xml::document::save_to_file (const char *filename,
                                  save_option_flags flags) const {
    int compression_level = flags & 0xFFFF;

    // Compression level is currently not analyzed by libxml2
    // So this might work in the future implementations but is ignored now.

    if (pimpl_->xslt_stylesheet_ != 0) {
        if (!xslt::impl::is_xml_output_method(pimpl_->xslt_stylesheet_)) {
            std::swap(pimpl_->doc_->compression, compression_level);
            bool rc = xslt::impl::save_to_file(pimpl_->doc_,
                                               pimpl_->xslt_stylesheet_,
                                               filename, compression_level);
            std::swap(pimpl_->doc_->compression, compression_level);
            return rc;
        }
    }

    int libxml2_options = convert_to_libxml2_save_options(flags);

    // In theory it was possible to use the pimpl_->doc_->encoding
    // value however a cached value is used instead.
    const char *enc = pimpl_->encoding_.empty() ? 0 : pimpl_->encoding_.c_str();
    xmlSaveCtxtPtr  ctxt = xmlSaveToFilename(filename,
                                             enc, libxml2_options);

    if (!ctxt)
        return false;

    long rc = 0;

    std::swap(pimpl_->doc_->compression, compression_level);
    rc = xmlSaveDoc(ctxt, pimpl_->doc_);
    std::swap(pimpl_->doc_->compression, compression_level);
    xmlSaveClose(ctxt);     // xmlSaveFlush() is called in xmlSaveClose()

    return (rc != -1);
}
//####################################################################
void xml::document::set_doc_data (void *data) {
    // we own the doc now, don't free it!
    pimpl_->set_doc_data(static_cast<xmlDocPtr>(data), false);

    if (pimpl_->xslt_stylesheet_)
        if (pimpl_->xslt_stylesheet_->_private)
            static_cast<xslt::impl::stylesheet_refcount*>
                (pimpl_->xslt_stylesheet_->_private)->inc_ref();

    pimpl_->xslt_stylesheet_ = 0;
}
//####################################################################
void xml::document::set_doc_data_from_xslt (void *data, void *ssheet) {
    // this document came from a XSLT transformation
    xsltStylesheetPtr   ss = static_cast<xsltStylesheetPtr>(ssheet);
    pimpl_->set_doc_data(static_cast<xmlDocPtr>(data), false);
    pimpl_->xslt_stylesheet_ = ss;
    if (ss->_private)
        static_cast<xslt::impl::stylesheet_refcount*>
            (ss->_private)->inc_ref();
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
    doc.save_to_stream(stream, save_op_default);
    return stream;
}
//####################################################################
bool xml::document::is_failure (error_messages* messages,
                                warnings_as_errors_type how) const {
    // if there are fatal errors or errors it is a failure
    if (messages->has_errors() ||
        messages->has_fatal_errors())
        return true;
    // if there are warnings and they are treated as errors it is a failure
    if ((how == type_warnings_are_errors) &&
         messages->has_warnings())
        return true;
    return false;
}
//####################################################################

namespace {

    // DTD related callbacks
    static void register_dtd_error_helper (error_message::message_type mt,
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

        register_dtd_error_helper(error_message::type_error, v, temporary);
    }

    extern "C" void cb_dtd_valid_warning (void *v, const char *message, ...) {
        std::string temporary;

        va_list ap;
        va_start(ap, message);
        printf2string(temporary, message, ap);
        va_end(ap);

        register_dtd_error_helper(error_message::type_warning, v, temporary);
    }

    // Tree parser related callbacks
    static void register_tree_error_helper (error_message::message_type mt,
                                            void *v,
                                            const std::string &message) {
        try {
            xmlParserCtxtPtr ctxt = static_cast<xmlParserCtxtPtr>(v);
            error_messages *p = static_cast<error_messages*>(ctxt->_private);

            if (p) // handle bug in older versions of libxml
                p->get_messages().push_back(error_message(message, mt));
        } catch (...) {}
    }

    extern "C" void cb_tree_parser_fatal_error (void *v,
                                                const char *message, ...) {
        std::string temporary;

        va_list ap;
        va_start(ap, message);
        printf2string(temporary, message, ap);
        va_end(ap);

        register_tree_error_helper(error_message::type_fatal_error,
                                   v, temporary);
    }

    extern "C" void cb_tree_parser_error (void *v, const char *message, ...) {
        std::string temporary;

        va_list ap;
        va_start(ap, message);
        printf2string(temporary, message, ap);
        va_end(ap);

        register_tree_error_helper(error_message::type_error, v, temporary);
    }

    extern "C" void cb_tree_parser_warning (void *v, const char *message, ...) {
        std::string temporary;

        va_list ap;
        va_start(ap, message);
        printf2string(temporary, message, ap);
        va_end(ap);

        register_tree_error_helper(error_message::type_warning, v, temporary);
    }

    extern "C" void cb_tree_parser_ignore (void*, const xmlChar*, int) {
        return;
    }
}

