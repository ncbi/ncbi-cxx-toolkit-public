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
 * Extension to work with raw libxml2 objects
**/

#include <misc/xmlwrapp/libxml2_xmlwrapp.hpp>
#include "document_impl.hpp"


// libxml2 includes
#include <libxml/parser.h>


namespace xml {


libxml2_document::libxml2_document(xmlDoc *        raw_doc,
                                   ownership_type  ownership)
{
    set_doc_data(raw_doc);
    set_ownership(ownership);
}


xmlDoc *  libxml2_document::get_raw_doc()
{
    return static_cast<xmlDoc *>(get_doc_data());
}


void libxml2_document::set_ownership(ownership_type ownership)
{
    pimpl_->set_ownership(ownership == type_own);
}

} // namespace xml

