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

#ifndef _xmlwrapp_libxml2_xmlwrapp_hpp_
#define _xmlwrapp_libxml2_xmlwrapp_hpp_


#include <misc/xmlwrapp/document.hpp>

// Forward declaration
#include <libxml/tree.h>


namespace xml {

/// Used to specify if xmlwrapp should grab the ownership of a libxml2 object
enum ownership_type {
    type_own,      ///< Grab the ownership, i.e. the object will be freed by xmlwrapp
    type_not_own   ///< Do not grab the ownership, i.e. the object will not be freed by xmlwrapp
};



/// Extension to the xml::document class which allows working with raw libxml2
/// documents.
class libxml2_document : public document
{
    public:
        /**
         * Create a new document using the given raw libxml2 object.
         * Use it at your own risk.
         *
         * @param raw_doc
         *  Raw libxml2 document
         * @param ownership
         *  Whether to grab the libxml2 document ownership or not. If the
         *  ownership is grabbed then the object will be freed in the
         *  destructor.
         * @author Denis Vakatov, NSBI
        **/
        libxml2_document(xmlDoc *  raw_doc, ownership_type  ownership);

        /**
         * Provides a raw libxml2 document.
         *
         * @return
         *  Raw libxml2 document
         * @author Denis Vakatov, NCBI
        **/
        xmlDoc *  get_raw_doc();

        /// Whether to call XmlDocFree(...) in the destructor
        /**
         * Set the document ownership.
         *
         * @param ownership
         *  The new ownership. If it is 'type_own' then the libxml2 document
         *  will be freed in the destructor.
        **/
        void set_ownership(ownership_type ownership);

    private:
        libxml2_document(const libxml2_document &);
        libxml2_document &  operator=(const libxml2_document &);
};

}

#endif

