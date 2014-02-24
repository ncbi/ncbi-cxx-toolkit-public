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
 * This file contains the implementation of the xslt::stylesheet class.
**/

// xmlwrapp includes
#include <misc/xmlwrapp/stylesheet.hpp>
#include <misc/xmlwrapp/document.hpp>
#include <misc/xmlwrapp/tree_parser.hpp>
#include <misc/xmlwrapp/exception.hpp>
#include <misc/xmlwrapp/xslt_exception.hpp>

#include "utility.hpp"
#include "document_impl.hpp"

// libxslt includes
#include <libxslt/xslt.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>
#include <libxslt/extensions.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

// standard includes
#include <stdexcept>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <string.h>

#include "extension_function_impl.hpp"
#include "extension_element_impl.hpp"

#include "stylesheet_impl.hpp"


namespace xslt {
    namespace impl {
        bool is_xml_output_method (xsltStylesheetPtr ss)
        {
            if (ss->method == NULL)
                return true;
            return strcmp(reinterpret_cast<const char *>(ss->method),
                          "xml") == 0;
        }

        void destroy_stylesheet (xsltStylesheetPtr ss)
        {
            if (ss->_private == NULL)
                xsltFreeStylesheet(ss);
            else {
                stylesheet_refcount * ss_rc =
                        static_cast<stylesheet_refcount *>(ss->_private);
                if (ss_rc->dec_ref() == 0) {
                    delete ss_rc;
                    xsltFreeStylesheet(ss);
                }
            }
        }

        void save_to_string(xmlDocPtr           doc,
                            xsltStylesheetPtr   ss,
                            std::string &       s)
        {
            xmlChar *   xml_string;
            int         xml_string_length;

            if (xsltSaveResultToString(&xml_string,
                                       &xml_string_length, doc, ss) >= 0)
            {
                xml::impl::xmlchar_helper   helper(xml_string);
                if (xml_string_length)
                    s.assign(helper.get(), xml_string_length);
            }
        }

        bool
        save_to_file(xmlDocPtr          doc,
                     xsltStylesheetPtr  ss,
                     const char *       filename,
                     int                /* compression_level */)
        {
            return xsltSaveResultToFilename(filename, doc, ss, 0) >= 0;
        }
    } // End of impl namespace
} // End of xslt namespace


void xslt::impl::stylesheet_impl::clear_nodes (void)
{
    for (std::vector<xmlNodePtr>::const_iterator k = nodes_to_free_.begin();
         k != nodes_to_free_.end(); ++k)
        xmlFreeNode(*k);
    nodes_to_free_.clear();
}


extern "C" {

    // XSLT extension function callback
    void xslt_ext_func_cb(void *c, int arg_num)
    {
        xmlXPathParserContext *     ctxt =
                    reinterpret_cast<xmlXPathParserContext *>(c);
        xsltTransformContextPtr     xslt_ctxt =
                    xsltXPathGetTransformContext(ctxt);
        xslt::impl::stylesheet_impl *   s_impl =
                    reinterpret_cast<xslt::impl::stylesheet_impl *>(
                                                        xslt_ctxt->_private);
        xmlNodePtr                  current_node = ctxt->context->node;
        xmlDocPtr                   current_doc = ctxt->context->doc;

        // Search for a registered extension function
        ext_func_key                key;
        key.first = reinterpret_cast<const char *>(ctxt->context->function);
        if (ctxt->context->functionURI != NULL)
            key.second = reinterpret_cast<const char *>(
                                        ctxt->context->functionURI);

        ext_funcs_map_type::iterator  found = s_impl->ext_functions_.find(key);
        if (found == s_impl->ext_functions_.end())
            return; // No extension function were found

        // The corresponding extension function has been found.
        // Prepare parameters and call it
        std::vector<xslt::xpath_object>     args;
        xml::node                           node;
        xml::document                       doc;

        // Prepare arguments for the extension function call. Arguments are
        // coming in the reverse order.
        args.reserve(arg_num);
        for (int  k = 0; k < arg_num; ++k) {
            xmlXPathObjectPtr   current_arg = valuePop(ctxt);

            args.insert(args.begin(),
                        xslt::xpath_object(reinterpret_cast<void*>(current_arg)));
            args[0].set_from_xslt();
        }


        // Wrap libxml2 data with xmlwrapp and make sure that xmlwrapp does NOT
        // have ownership on the node and the document.
        node.set_node_data(current_node);
        doc.set_doc_data(current_doc);
        doc.pimpl_->set_ownership(false);

        // Set the context to make error reporting and setting retval
        // working properly
        found->second.first->pimpl_->xpath_parser_ctxt = ctxt;

        // Make a call
        try {
            found->second.first->execute(args, node, doc);
        } catch (const std::exception &  ex) {
            std::string     error("Exception in the user extension function '" +
                                  key.first + "': " + std::string(ex.what()));
            found->second.first->report_error(error.c_str());
        } catch (...) {
            std::string     error("Unknown exception in the user "
                                  "extension function '" + key.first + "'");
            found->second.first->report_error(error.c_str());
        }

        // Clear the context
        found->second.first->pimpl_->xpath_parser_ctxt = NULL;

        return;
    }

    // XSLT extension element callback
    void xslt_ext_element_cb(void *c, void *input_node_,
                             void *instruction_node_,
                             void *compiled_stylesheet_info)
    {
        xsltTransformContextPtr     xslt_ctxt =
                    reinterpret_cast<xsltTransformContextPtr>(c);
        xmlNodePtr                  instruction_node =
                    reinterpret_cast<xmlNodePtr>(instruction_node_);
        xslt::impl::stylesheet_impl *   s_impl =
                    reinterpret_cast<xslt::impl::stylesheet_impl *>(
                                                        xslt_ctxt->_private);

        ext_elem_key                key;
        key.first = reinterpret_cast<const char *>(instruction_node->name);
        if (instruction_node->ns != NULL && instruction_node->ns->href != NULL)
            key.second = reinterpret_cast<const char *>(instruction_node->ns->href);

        ext_elems_map_type::iterator    found = s_impl->ext_elements_.find(key);
        if (found == s_impl->ext_elements_.end())
            return; // No extension element were found

        // The corresponding extension element has been found.
        // Prepare the parameters.
        xmlNodePtr                  input_node =
                                reinterpret_cast<xmlNodePtr>(input_node_);

        xml::node                   inp_node;
        xml::node                   instr_node;
        xml::node                   insert_node;
        xml::document               doc;

        // Wrap libxml2 data with xmlwrapp and make sure that xmlwrapp does NOT
        // have ownership on the nodes and the document.
        inp_node.set_node_data(input_node);
        instr_node.set_node_data(instruction_node);
        insert_node.set_node_data(xslt_ctxt->insert);
        doc.set_doc_data(xslt_ctxt->xpathCtxt->doc);
        doc.pimpl_->set_ownership(false);

        // Set the context to make error reporting working properly
        found->second.first->pimpl_->xslt_ctxt = xslt_ctxt;
        found->second.first->pimpl_->instruction_node = instruction_node;

        // Make a call
        try {
            found->second.first->process(inp_node, instr_node,
                                         insert_node, doc);
        } catch (const std::exception &  ex) {
            std::string     error("Exception in the user extension element '" +
                                  key.first + "': " +
                                  std::string(ex.what()));
            found->second.first->report_error(error.c_str());
        } catch (...) {
            std::string     error("Unknown error in the user "
                                  "extension element '" +
                                  key.first + "'");
            found->second.first->report_error(error.c_str());
        }

        // Clear the context
        found->second.first->pimpl_->xslt_ctxt = NULL;
        found->second.first->pimpl_->instruction_node = NULL;

        return;
    }

} // extern "C"


namespace
{

void make_vector_param(std::vector<const char*> &v,
                       const xslt::stylesheet::param_type &p)
{
    v.reserve(p.size());

    xslt::stylesheet::param_type::const_iterator i = p.begin(), end = p.end();
    for (; i != end; ++i)
    {
        v.push_back(i->first.c_str());
        v.push_back(i->second.c_str());
    }

    v.push_back(static_cast<const char*>(0));
}


extern "C"
{

static void error_cb(void *c, const char *message, ...)
{
    xsltTransformContextPtr ctxt = static_cast<xsltTransformContextPtr>(c);
    xslt::impl::stylesheet_impl *s_impl = static_cast<xslt::impl::stylesheet_impl*>(
                                                                ctxt->_private);

    s_impl->errors_occured_ = true;

    // tell the processor to stop when it gets a chance:
    if ( ctxt->state == XSLT_STATE_OK )
        ctxt->state = XSLT_STATE_STOPPED;

    // concatenate all error messages:
    if ( s_impl->errors_occured_ )
        s_impl->error_.append("\n");

    std::string formatted;

    va_list ap;
    va_start(ap, message);
    xml::impl::printf2string(formatted, message, ap);
    va_end(ap);

    s_impl->error_.append(formatted);
}

} // extern "C"



xmlDocPtr apply_stylesheet(xslt::impl::stylesheet_impl *s_impl,
                           xmlDocPtr doc,
                           const xslt::stylesheet::param_type *p = NULL)
{
    xsltStylesheetPtr style = s_impl->ss_;

    std::vector<const char*> v;
    if (p)
        make_vector_param(v, *p);

    xsltTransformContextPtr ctxt = xsltNewTransformContext(style, doc);
    ctxt->_private = s_impl;
    xsltSetTransformErrorFunc(ctxt, ctxt, error_cb);

    // Register extension functions
    for (ext_funcs_map_type::iterator k = s_impl->ext_functions_.begin();
         k != s_impl->ext_functions_.end(); ++k) {
        if (xsltRegisterExtFunction(
                ctxt,
                reinterpret_cast<const xmlChar*>(k->first.first.c_str()),
                reinterpret_cast<const xmlChar*>(k->first.second.c_str()),
                reinterpret_cast<xmlXPathFunction>(xslt_ext_func_cb)) != 0) {
            xsltFreeTransformContext(ctxt);
            throw xslt::exception("Error registering extension function " +
                                  k->first.first);
        }
    }

    // Register extension elements
    for (ext_elems_map_type::iterator k = s_impl->ext_elements_.begin();
         k != s_impl->ext_elements_.end(); ++k) {
        if (xsltRegisterExtElement(
                ctxt,
                reinterpret_cast<const xmlChar*>(k->first.first.c_str()),
                reinterpret_cast<const xmlChar*>(k->first.second.c_str()),
                reinterpret_cast<xsltTransformFunction>(xslt_ext_element_cb)) != 0) {
            xsltFreeTransformContext(ctxt);
            throw xslt::exception("Error registering extension element " +
                                  k->first.first);
        }
    }

    // clear the error flag before applying the stylesheet
    s_impl->errors_occured_ = false;

    xmlDocPtr result =
        xsltApplyStylesheetUser(style, doc, p ? &v[0] : 0, NULL, NULL, ctxt);

    // This is a part of a hack of leak-less handling nodeset return values
    // from XSLT extension functions. XSLT frees nodes in a nodeset too early
    // so the boolval is not set to 1 to free them automatically. Instead the
    // nodes are memorized in a vector. These nodes are freed here, i.e. when
    // the transformation is completed.
    // See extension_function.cpp and xpath_object.cpp as well.
    s_impl->clear_nodes();

    xsltFreeTransformContext(ctxt);

    // it's possible there was an error that didn't prevent creation of some
    // (incorrect) document
    if ( result && s_impl->errors_occured_ )
    {
        xmlFreeDoc(result);
        return NULL;
    }

    if ( !result )
    {
        // set generic error message if nothing more specific is known
        if ( s_impl->error_.empty() )
            s_impl->error_ = "unknown XSLT transformation error";
        return NULL;
    }

    return result;
}

} // end of anonymous namespace


void xslt::stylesheet::attach_refcount(void)
{
    impl::stylesheet_refcount *  refcount;
    try {
        refcount = new impl::stylesheet_refcount;
    } catch (...) {
        xsltFreeStylesheet(pimpl_->ss_);
        throw;
    }
    refcount->inc_ref();
    pimpl_->ss_->_private = refcount;
}


xslt::stylesheet::stylesheet(const char *filename)
{
    std::auto_ptr<impl::stylesheet_impl>
                    ap(pimpl_ = new impl::stylesheet_impl);
    xml::error_messages msgs;
    xml::document       doc(filename, &msgs, xml::type_warnings_not_errors);
    xmlDocPtr           xmldoc = static_cast<xmlDocPtr>(doc.get_doc_data());

    if ( (pimpl_->ss_ = xsltParseStylesheetDoc(xmldoc)) == 0)
    {
        // TODO error_ can't get set yet. Need changes from libxslt first
        if (pimpl_->error_.empty())
            pimpl_->error_ = "unknown XSLT parser error";

        msgs.get_messages().push_back(xml::error_message(
                                            pimpl_->error_,
                                            xml::error_message::type_error));
        throw xml::parser_exception(msgs);
    }

    attach_refcount();

    // if we got this far, the xmldoc we gave to xsltParseStylesheetDoc is
    // now owned by the stylesheet and will be cleaned up in our destructor.
    doc.release_doc_data();
    ap.release();
}


xslt::stylesheet::stylesheet(const xml::document &  doc)
{
    xml::document           doc_copy(doc);  /* NCBI_FAKE_WARNING */
    xmlDocPtr               xmldoc = static_cast<xmlDocPtr>(
                                                doc_copy.get_doc_data());
    std::auto_ptr<impl::stylesheet_impl>
                            ap(pimpl_ = new impl::stylesheet_impl);

    if ( (pimpl_->ss_ = xsltParseStylesheetDoc(xmldoc)) == 0)
    {
        // TODO error_ can't get set yet. Need changes from libxslt first
        if (pimpl_->error_.empty())
            pimpl_->error_ = "unknown XSLT parser error";

        xml::error_messages     messages;
        messages.get_messages().push_back(xml::error_message(
                                            pimpl_->error_,
                                            xml::error_message::type_error));
        throw xml::parser_exception(messages);
    }

    attach_refcount();

    // if we got this far, the xmldoc we gave to xsltParseStylesheetDoc is
    // now owned by the stylesheet and will be cleaned up in our destructor.
    doc_copy.release_doc_data();
    ap.release();
}


xslt::stylesheet::stylesheet (const char* data, size_t size)
{
    std::auto_ptr<impl::stylesheet_impl>
                            ap(pimpl_ = new impl::stylesheet_impl);
    xml::error_messages     msgs;
    xml::document           doc(data, size, &msgs,
                                xml::type_warnings_not_errors);
    xmlDocPtr               xmldoc = static_cast<xmlDocPtr>(doc.get_doc_data());

    if ( (pimpl_->ss_ = xsltParseStylesheetDoc(xmldoc)) == 0)
    {
        // TODO error_ can't get set yet. Need changes from libxslt first
        if (pimpl_->error_.empty())
            pimpl_->error_ = "unknown XSLT parser error";

        msgs.get_messages().push_back(xml::error_message(
                                            pimpl_->error_,
                                            xml::error_message::type_error));
        throw xml::parser_exception(msgs);
    }

    attach_refcount();

    // if we got this far, the xmldoc we gave to xsltParseStylesheetDoc is
    // now owned by the stylesheet and will be cleaned up in our destructor.
    doc.release_doc_data();
    ap.release();
}


xslt::stylesheet::stylesheet (std::istream & stream)
{
    std::auto_ptr<impl::stylesheet_impl>
                            ap(pimpl_ = new impl::stylesheet_impl);
    xml::error_messages     msgs;
    xml::document           doc(stream, &msgs, xml::type_warnings_not_errors);
    xmlDocPtr               xmldoc = static_cast<xmlDocPtr>(doc.get_doc_data());

    if ( (pimpl_->ss_ = xsltParseStylesheetDoc(xmldoc)) == 0)
    {
        // TODO error_ can't get set yet. Need changes from libxslt first
        if (pimpl_->error_.empty())
            pimpl_->error_ = "unknown XSLT parser error";

        msgs.get_messages().push_back(xml::error_message(
                                            pimpl_->error_,
                                            xml::error_message::type_error));
        throw xml::parser_exception(msgs);
    }

    attach_refcount();

    // if we got this far, the xmldoc we gave to xsltParseStylesheetDoc is
    // now owned by the stylesheet and will be cleaned up in our destructor.
    doc.release_doc_data();
    ap.release();
}


void
xslt::stylesheet::register_extension_function (extension_function *  ef,
                                               const char *          name,
                                               const char *          uri,
                                               xml::ownership_type   ownership)
{
    if (name == NULL) {
        if (ownership == xml::type_own)
            delete ef;
        throw xslt::exception("Extension function name is uninitialised");
    }

    if (uri == NULL) {
        if (ownership == xml::type_own)
            delete ef;
        throw xslt::exception("Extension function URI is uninitialised");
    }

    ext_func_key                    key = std::pair<std::string,
                                                    std::string>(name, uri);
    ext_funcs_map_type::iterator    found(pimpl_->ext_functions_.find(key));

    if (found != pimpl_->ext_functions_.end()) {
        if (found->second.second == xml::type_own)
            delete found->second.first;
    }

    pimpl_->ext_functions_[ key ] = std::pair<extension_function *,
                                          xml::ownership_type>(ef, ownership);
    return;
}


void
xslt::stylesheet::register_extension_element (extension_element *  ee,
                                              const char *          name,
                                              const char *          uri,
                                              xml::ownership_type   ownership)
{
    if (name == NULL) {
        if (ownership == xml::type_own)
            delete ee;
        throw xslt::exception("Extension element name is uninitialised");
    }

    if (uri == NULL) {
        if (ownership == xml::type_own)
            delete ee;
        throw xslt::exception("Extension element URI is uninitialised");
    }

    ext_elem_key                    key = std::pair<std::string,
                                                    std::string>(name, uri);
    ext_elems_map_type::iterator    found(pimpl_->ext_elements_.find(key));

    if (found != pimpl_->ext_elements_.end()) {
        if (found->second.second == xml::type_own)
            delete found->second.first;
    }

    pimpl_->ext_elements_[ key ] = std::pair<extension_element *,
                                        xml::ownership_type>(ee, ownership);
    return;
}


xslt::stylesheet::~stylesheet()
{
    // Delete extension functions we owe
    for (ext_funcs_map_type::iterator k = pimpl_->ext_functions_.begin();
         k != pimpl_->ext_functions_.end(); ++k) {
        if (k->second.second == xml::type_own)
            delete k->second.first;
    }

    // Delete extension elements we owe
    for (ext_elems_map_type::iterator k = pimpl_->ext_elements_.begin();
         k != pimpl_->ext_elements_.end(); ++k) {
        if (k->second.second == xml::type_own)
            delete k->second.first;
    }

    if (pimpl_->ss_)
        xslt::impl::destroy_stylesheet(pimpl_->ss_);
    delete pimpl_;
}


xml::document_proxy xslt::stylesheet::apply (const xml::document &doc)
{
    xmlDocPtr input = static_cast<xmlDocPtr>(doc.get_doc_data_read_only());
    xmlDocPtr xmldoc = apply_stylesheet(pimpl_, input);

    if ( !xmldoc )
        throw xslt::exception(pimpl_->error_);

    return xml::document_proxy(xmldoc, pimpl_->ss_);
}


xml::document_proxy xslt::stylesheet::apply (const xml::document &doc,
                                             const param_type &with_params)
{
    xmlDocPtr input = static_cast<xmlDocPtr>(doc.get_doc_data_read_only());
    xmlDocPtr xmldoc = apply_stylesheet(pimpl_, input, &with_params);

    if ( !xmldoc )
        throw xslt::exception(pimpl_->error_);

    return xml::document_proxy(xmldoc, pimpl_->ss_);
}

