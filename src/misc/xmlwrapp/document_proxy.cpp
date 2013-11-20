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
 * Author:  Denis Vakatov, NCBI
 *
 */


/** @file
 * XML document proxy for XSLT apply(...) methods
**/



// xmlwrapp includes
#include <misc/xmlwrapp/document_proxy.hpp>
#include "stylesheet_impl.hpp"



namespace xml {


/* Internal library use only */
document_proxy::document_proxy (void *  result, void *  ssheet) :
    owner_(true), result_(result), style_sheet_(ssheet)
{
    xsltStylesheetPtr   ss = static_cast<xsltStylesheetPtr>(style_sheet_);
    if (ss->_private)
        static_cast<xslt::impl::stylesheet_refcount*>
            (ss->_private)->inc_ref();
}


/* Used by compilers which are unable to avoid it (older ones) */
document_proxy::document_proxy (const document_proxy &  other) :
    owner_(true), result_(NULL), style_sheet_(other.style_sheet_)
{
    xmlDocPtr   tmpdoc;
    if ( (tmpdoc = xmlCopyDoc(static_cast<xmlDocPtr>(other.result_), 1)) == 0)
        throw std::bad_alloc();

    result_ = tmpdoc;

    xsltStylesheetPtr   ss = static_cast<xsltStylesheetPtr>(style_sheet_);
    if (ss->_private)
        static_cast<xslt::impl::stylesheet_refcount*>
            (ss->_private)->inc_ref();
}

document_proxy::~document_proxy ()
{
    if (owner_)
        xmlFreeDoc(static_cast<xmlDocPtr>(result_));

    xsltStylesheetPtr   ss = static_cast<xsltStylesheetPtr>(style_sheet_);
    if (ss)
        if (ss->_private)
            xslt::impl::destroy_stylesheet(ss);
}

/* xml::document can grab the ownership */
void document_proxy::release (void) const {
    owner_ = false;
}

} // xml namespace

