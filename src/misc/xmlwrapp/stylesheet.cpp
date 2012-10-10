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

#include "result.hpp"
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

// temporary
#include <misc/xmlwrapp/impl/_raw_xslt.hpp>

#include "extension_function_impl.hpp"


// Typedefs to support extension functions
// Key: func name + URI
typedef std::pair<std::string,
                  std::string>                  ext_func_key;
// Properties: pointer to interface + ownership
typedef std::pair<xslt::extension_function *,
                  xml::ownership_type>          ext_func_props;
// Container to store extension functions
typedef std::map<ext_func_key,
                 ext_func_props>                ext_funcs_map_type;



struct xslt::stylesheet::pimpl
{
    pimpl (void) : ss_(0), errors_occured_(false) { }

    xsltStylesheetPtr       ss_;
    xml::document           doc_;
    std::string             error_;
    bool                    errors_occured_;

    // Extension functions support
    ext_funcs_map_type      ext_functions_;
};



extern "C" {

    // XSLT extension function callback
    void xslt_ext_func_cb(void *c, int arg_num)
    {
        xmlXPathParserContext *     ctxt =
                                reinterpret_cast<xmlXPathParserContext *>(c);
        xsltTransformContextPtr     xslt_ctxt =
                                xsltXPathGetTransformContext(ctxt);
        xslt::stylesheet::pimpl *   impl =
                                reinterpret_cast<xslt::stylesheet::pimpl *>(
                                                        xslt_ctxt->_private);
        xmlNodePtr                  current_node = ctxt->context->node;
        xmlDocPtr                   current_doc = ctxt->context->doc;

        // Search for a registered extension function
        ext_func_key                key;
        key.first = reinterpret_cast<const char *>(ctxt->context->function);
        if (ctxt->context->functionURI != NULL)
            key.second = reinterpret_cast<const char *>(
                                        ctxt->context->functionURI);

        ext_funcs_map_type::iterator  found = impl->ext_functions_.find(key);
        if (found == impl->ext_functions_.end())
            return; // No extension function were found

        // The corresponding extension function has been found.
        // Prepare parameters and call it
        std::vector<xslt::xpath_object>     args;
        xml::node                           node;
        xml::document                       doc;

        // Prepare arguments for the extension function call. Arguments are
        // coming in the reverse order.
        args.reserve( arg_num );
        for (int  k = 0; k < arg_num; ++k) {
            xmlXPathObjectPtr   current_arg = valuePop(ctxt);

            args.insert(
                    args.begin(),
                    xslt::xpath_object(reinterpret_cast<void*>(current_arg)));
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
        } catch (xslt::xpath_error  error) {
            found->second.first->report_error(error);
        } catch (const std::exception &  ex) {
            found->second.first->report_error(xslt::xptr_resource_error);
        } catch (...) {
            found->second.first->report_error(xslt::xptr_resource_error);
        }

        // Clear the context
        found->second.first->pimpl_->xpath_parser_ctxt = NULL;

        return;
    }

} // extern "C"


namespace
{

// implementation of xslt::result using xslt::stylesheet: we pass this object
// to xml::document for the documents obtained via XSLT so that some operations
// (currently only saving) could be done differently for them
class result_impl : public xslt::impl::result
{
public:
    // We don't own the pointers given to us, their lifetime must be greater
    // than the lifetime of this object.
    result_impl(xmlDocPtr doc, xsltStylesheetPtr ss) :
        doc_(doc), ss_(ss)
    {}

    virtual void save_to_string(std::string &s) const
    {
        xmlChar *xml_string;
        int xml_string_length;

        if (xsltSaveResultToString(&xml_string,
                                   &xml_string_length, doc_, ss_) >= 0)
        {
            xml::impl::xmlchar_helper helper(xml_string);
            if (xml_string_length) s.assign(helper.get(), xml_string_length);
        }
    }

    virtual bool
    save_to_file(const char *filename, int /* compression_level */) const
    {
        return xsltSaveResultToFilename(filename, doc_, ss_, 0) >= 0;
    }

private:
    virtual xmlDocPtr get_raw_doc (void)
    {
        return doc_;
    }

    friend class xml::document;
    xmlDocPtr           doc_;
    xsltStylesheetPtr   ss_;
};


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
    xslt::stylesheet::pimpl *impl = static_cast<xslt::stylesheet::pimpl*>(
                                                                ctxt->_private);

    impl->errors_occured_ = true;

    // tell the processor to stop when it gets a chance:
    if ( ctxt->state == XSLT_STATE_OK )
        ctxt->state = XSLT_STATE_STOPPED;

    // concatenate all error messages:
    if ( impl->errors_occured_ )
        impl->error_.append("\n");

    std::string formatted;

    va_list ap;
    va_start(ap, message);
    xml::impl::printf2string(formatted, message, ap);
    va_end(ap);

    impl->error_.append(formatted);
}

} // extern "C"



xmlDocPtr apply_stylesheet(xslt::stylesheet::pimpl *impl,
                           xmlDocPtr doc,
                           const xslt::stylesheet::param_type *p = NULL)
{
    xsltStylesheetPtr style = impl->ss_;

    std::vector<const char*> v;
    if (p)
        make_vector_param(v, *p);

    xsltTransformContextPtr ctxt = xsltNewTransformContext(style, doc);
    ctxt->_private = impl;
    xsltSetTransformErrorFunc(ctxt, ctxt, error_cb);

    // Register extension functions
    for (ext_funcs_map_type::iterator k = impl->ext_functions_.begin();
         k != impl->ext_functions_.end(); ++k) {
        if ( xsltRegisterExtFunction(
                ctxt,
                reinterpret_cast<const xmlChar*>(k->first.first.c_str()),
                reinterpret_cast<const xmlChar*>(k->first.second.c_str()),
                reinterpret_cast<xmlXPathFunction>(xslt_ext_func_cb)) != 0) {
            xsltFreeTransformContext(ctxt);
            throw xslt::exception("Error registering extension function " +
                                  k->first.first);
        }
    }

    // clear the error flag before applying the stylesheet
    impl->errors_occured_ = false;

    xmlDocPtr result =
        xsltApplyStylesheetUser(style, doc, p ? &v[0] : 0, NULL, NULL, ctxt);

    xsltFreeTransformContext(ctxt);

    // it's possible there was an error that didn't prevent creation of some
    // (incorrect) document
    if ( result && impl->errors_occured_ )
    {
        xmlFreeDoc(result);
        return NULL;
    }

    if ( !result )
    {
        // set generic error message if nothing more specific is known
        if ( impl->error_.empty() )
            impl->error_ = "unknown XSLT transformation error";
        return NULL;
    }

    return result;
}

} // end of anonymous namespace


xslt::impl::result *  xslt::impl::make_copy (xslt::impl::result *  pattern)
{
    result_impl *   other_instance = dynamic_cast<result_impl *>(pattern);

    if (other_instance == NULL)
        throw xslt::exception("Design error: unexpected xslt result type");

    return new result_impl(*other_instance);
}


xslt::stylesheet::stylesheet(const char *filename)
{
    std::auto_ptr<pimpl>    ap(pimpl_ = new pimpl);
    xml::error_messages     msgs;
    xml::document           doc(filename, &msgs, xml::type_warnings_not_errors);
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
    std::auto_ptr<pimpl>    ap(pimpl_ = new pimpl);

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

    // if we got this far, the xmldoc we gave to xsltParseStylesheetDoc is
    // now owned by the stylesheet and will be cleaned up in our destructor.
    doc_copy.release_doc_data();
    ap.release();
}


xslt::stylesheet::stylesheet (const char* data, size_t size)
{
    std::auto_ptr<pimpl>    ap(pimpl_ = new pimpl);
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

    // if we got this far, the xmldoc we gave to xsltParseStylesheetDoc is
    // now owned by the stylesheet and will be cleaned up in our destructor.
    doc.release_doc_data();
    ap.release();
}


xslt::stylesheet::stylesheet (std::istream & stream)
{
    std::auto_ptr<pimpl>    ap(pimpl_ = new pimpl);
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

    pimpl_->ext_functions_[ std::pair<std::string, std::string>(name, uri) ] =
                            std::pair<extension_function *,
                                      xml::ownership_type>(ef, ownership);
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

    if (pimpl_->ss_)
        xsltFreeStylesheet(pimpl_->ss_);
    delete pimpl_;
}


bool xslt::stylesheet::apply(const xml::document &doc, xml::document &result,
                             result_treat_type  treat)
{
    xmlDocPtr input = static_cast<xmlDocPtr>(doc.get_doc_data_read_only());
    xmlDocPtr xmldoc = apply_stylesheet(pimpl_, input);

    if (xmldoc)
    {
        result.set_doc_data_from_xslt(xmldoc,
                                      new result_impl(xmldoc, pimpl_->ss_),
                                      treat);
        return true;
    }

    return false;
}


bool xslt::stylesheet::apply(const xml::document &doc, xml::document &result,
                             const param_type &with_params,
                             result_treat_type  treat)
{
    xmlDocPtr input = static_cast<xmlDocPtr>(doc.get_doc_data_read_only());
    xmlDocPtr xmldoc = apply_stylesheet(pimpl_, input, &with_params);

    if (xmldoc)
    {
        result.set_doc_data_from_xslt(xmldoc,
                                      new result_impl(xmldoc, pimpl_->ss_),
                                      treat);
        return true;
    }

    return false;
}


xml::document_proxy  xslt::stylesheet::apply(const xml::document &doc,
                                             result_treat_type  treat)
{
    xmlDocPtr input = static_cast<xmlDocPtr>(doc.get_doc_data_read_only());
    xmlDocPtr xmldoc = apply_stylesheet(pimpl_, input);

    if ( !xmldoc )
        throw xslt::exception(pimpl_->error_);

    xml::document_proxy     proxy(new result_impl(xmldoc, pimpl_->ss_),
                                  treat);
    return proxy;
}


xml::document_proxy  xslt::stylesheet::apply(const xml::document &doc,
                                             const param_type &with_params,
                                             result_treat_type  treat)
{
    xmlDocPtr input = static_cast<xmlDocPtr>(doc.get_doc_data_read_only());
    xmlDocPtr xmldoc = apply_stylesheet(pimpl_, input, &with_params);

    if ( !xmldoc )
        throw xslt::exception(pimpl_->error_);

    xml::document_proxy     proxy(new result_impl(xmldoc, pimpl_->ss_),
                                  treat);
    return proxy;
}


const std::string& xslt::stylesheet::get_error_message() const
{
    return pimpl_->error_;
}


void *
xslt::impl::temporary_existing_get_raw_xslt_stylesheet(xslt::stylesheet &  s)
{
    return s.pimpl_->ss_;
}

