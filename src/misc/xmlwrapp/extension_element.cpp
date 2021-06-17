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
 * Author:  Sergey Satskiy, NCBI
 * Credits: Denis Vakatov, NCBI (API design)
 *
 */


/** @file
 * This file contains the implementation of the xslt::extension_element class.
**/


#include <string>

#include <libxslt/xsltutils.h>

// xmlwrapp includes
#include <misc/xmlwrapp/extension_element.hpp>
#include <misc/xmlwrapp/xslt_exception.hpp>

#include "extension_element_impl.hpp"


static xmlXPathObjectPtr
evaluate_xpath_expression (xsltTransformContextPtr  ctxt,
                           const char *             xpath_expression,
                           xmlNodePtr               node)
{
    int         old_context_size = ctxt->xpathCtxt->contextSize;
    int         old_proximity_position = ctxt->xpathCtxt->proximityPosition;
    int         old_ns_nr = ctxt->xpathCtxt->nsNr;
    xmlNsPtr *  old_namespaces = ctxt->xpathCtxt->namespaces;
    xmlNodePtr  old_node = ctxt->xpathCtxt->node;

    ctxt->xpathCtxt->node = node == NULL ? ctxt->node : node;
    if (node != NULL)
        ctxt->xpathCtxt->contextSize = ctxt->xpathCtxt->proximityPosition;

    xmlXPathObjectPtr result_obj = xmlXPathEvalExpression(
                            reinterpret_cast<const xmlChar*>(xpath_expression),
                            ctxt->xpathCtxt);

    ctxt->xpathCtxt->node = old_node;
    ctxt->xpathCtxt->contextSize = old_context_size;
    ctxt->xpathCtxt->proximityPosition = old_proximity_position;
    ctxt->xpathCtxt->nsNr = old_ns_nr;
    ctxt->xpathCtxt->namespaces = old_namespaces;

    if (result_obj == NULL)
        throw xslt::exception("XPath expression evaluation failed. "
                              "Expression: " +
                              std::string(xpath_expression));

    return result_obj;
}


namespace xslt {

    namespace impl {
        extension_element_impl::extension_element_impl() :
            xslt_ctxt(NULL), instruction_node(NULL)
        {}
    }


    //
    // Extension element implementation
    //

    extension_element::extension_element () :
        pimpl_(new impl::extension_element_impl)
    {}

    extension_element::~extension_element ()
    {
        if (pimpl_ != NULL)
            delete pimpl_;
    }

    extension_element::extension_element (const extension_element &  other) :
        pimpl_(new impl::extension_element_impl)
    {
        pimpl_->xslt_ctxt = other.pimpl_->xslt_ctxt;
        return;
    }

    extension_element &
    extension_element::operator= (const extension_element &  other)
    {
        pimpl_->xslt_ctxt = other.pimpl_->xslt_ctxt;
        return *this;
    }

    extension_element::extension_element (extension_element &&  other) :
        pimpl_(other.pimpl_)
    {
        other.pimpl_ = NULL;
    }

    extension_element &
    extension_element::operator= (extension_element &&  other)
    {
        if (this != &other) {
            if (pimpl_ != NULL)
                delete pimpl_;
            pimpl_ = other.pimpl_;
            other.pimpl_ = NULL;
        }
        return *this;
    }

    void extension_element::report_error (const char *  error)
    {
        if (pimpl_->xslt_ctxt == NULL)
            throw xslt::exception("Reporting XSLT extension element error "
                                  "out of XSLT context.");
        if (pimpl_->instruction_node == NULL)
            throw xslt::exception("Reporting XSLT extension element error "
                                  "when there is no XSLT instruction node.");

        xsltTransformError(pimpl_->xslt_ctxt, pimpl_->xslt_ctxt->style,
                           pimpl_->instruction_node, "%s", error);
    }

    xpath_object extension_element::evaluate (const char *  xpath_expression,
                                              const xml::node &  node)
    {
        if (pimpl_->xslt_ctxt == NULL)
            throw xslt::exception("Evaluating XPath expression "
                                  "out of XSLT context.");
        return xpath_object(evaluate_xpath_expression(
                             pimpl_->xslt_ctxt,
                             xpath_expression,
                             static_cast<xmlNodePtr>(node.get_node_data())));
    }

    xpath_object extension_element::evaluate (const char *  xpath_expression)
    {
        if (pimpl_->xslt_ctxt == NULL)
            throw xslt::exception("Evaluating XPath expression "
                                  "out of XSLT context.");
        return xpath_object(evaluate_xpath_expression(pimpl_->xslt_ctxt,
                                                      xpath_expression, NULL));
    }

} // xslt namespace

