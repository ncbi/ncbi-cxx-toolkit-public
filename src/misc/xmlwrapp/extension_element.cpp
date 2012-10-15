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



#include <libxslt/xsltutils.h>

// xmlwrapp includes
#include <misc/xmlwrapp/extension_element.hpp>
#include <misc/xmlwrapp/xslt_exception.hpp>

#include "extension_element_impl.hpp"


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

    void extension_element::report_error (const char *  error)
    {
        if (pimpl_->xslt_ctxt == NULL)
            throw xslt::exception("Reporting XSLT extension element error "
                                  "out of XSLT context.");
        if (pimpl_->instruction_node == NULL)
            throw xslt::exception("Reporting XSLT extension element error "
                                  "when there is no XSLT instruction node.");

        xsltTransformError(pimpl_->xslt_ctxt, pimpl_->xslt_ctxt->style,
                           pimpl_->instruction_node, error);
    }

} // xslt namespace

