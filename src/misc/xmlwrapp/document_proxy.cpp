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
#include "result.hpp"



namespace xml {


/* Internal library use only */
document_proxy::document_proxy (xslt::impl::result *     result,
                                xslt::result_treat_type  treat) :
    owner_(true), xslt_result_(result), treat_(treat)
{}


document_proxy::document_proxy (const document_proxy &  other) :
    owner_(true),
    xslt_result_(xslt::impl::make_copy(other.xslt_result_)),
    treat_(other.treat_)
{}

document_proxy::~document_proxy ()
{
    if (owner_)
        delete xslt_result_;
}

/* xml::document can grab the ownership */
void document_proxy::release (void) const {
    owner_ = false;
}

} // xml namespace

