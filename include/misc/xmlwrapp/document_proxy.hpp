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

#ifndef _xmlwrapp_document_proxy_hpp_
#define _xmlwrapp_document_proxy_hpp_

// for compiler versions defines
#include <ncbiconf.h>

// xmlwrapp includes
#include <misc/xmlwrapp/errors.hpp>


namespace xslt {
    class stylesheet;
}

namespace xml {


/**
 * The xml::document_proxy class to be a relay between XSLT results and the
 * document. See CXX-2458
**/
class document_proxy
{
public:
    ~document_proxy ();

// Some compilers (GCC 4.4.2 and up) are clever enough to avoid using the
// copy constructor, so for those compilers it is private.
// MSVC behaves different depending on a case. E.g. in case of the '?'
// operator MSVC needs a copy constructor.
#if defined(NCBI_COMPILER_GCC)  &&  NCBI_COMPILER_VERSION >= 442
private:
#else
public:
#endif
    document_proxy (const document_proxy &  other);

private:
    /* Internal library use only */
    document_proxy (void *  result, void *  ssheet);

private:
    /* xml::document can grab the ownership */
    void release (void) const;

    mutable bool    owner_;
    void *          result_;
    void *          style_sheet_;

    friend class document;
    friend class xslt::stylesheet;

    document_proxy &  operator= (const document_proxy &  other);
};

} // xml namespace

#endif

