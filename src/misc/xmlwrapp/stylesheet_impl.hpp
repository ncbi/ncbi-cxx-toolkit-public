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
 * This file contains definition of the
 * xslt::impl::stylesheet_impl class.
**/


#ifndef _xmlwrapp_stylesheet_impl_hpp_
#define _xmlwrapp_stylesheet_impl_hpp_

#include <string>
#include <vector>
#include <map>

#include <misc/xmlwrapp/ownership.hpp>
#include <misc/xmlwrapp/document.hpp>

#include "extension_function_impl.hpp"
#include "extension_element_impl.hpp"

namespace xslt {
    class extension_function;
    class extension_element;
}


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

// Typedefs to support extension elements
// Key: element name + URI
typedef std::pair<std::string,
                  std::string>                  ext_elem_key;
// Properties: pointer to interface + ownership
typedef std::pair<xslt::extension_element *,
                  xml::ownership_type>          ext_elem_props;
// Container to store extension elements
typedef std::map<ext_elem_key,
                 ext_elem_props>                ext_elems_map_type;



namespace xslt {
    namespace impl {
        struct stylesheet_refcount
        {
            stylesheet_refcount (void) : count_(0) {}

            void inc_ref (void) { ++count_; }
            size_t dec_ref (void) { return --count_; }

            size_t  count_;
        };


        struct stylesheet_impl
        {
            stylesheet_impl (void) : ss_(0), errors_occured_(false) { }

            xsltStylesheetPtr       ss_;
            xml::document           doc_;
            std::string             error_;
            bool                    errors_occured_;

            // Extension functions support
            ext_funcs_map_type      ext_functions_;
            ext_elems_map_type      ext_elements_;

            // Workaround for xslt xpath_objects nodesets return values
            std::vector<xmlNodePtr> nodes_to_free_;

            void clear_nodes (void);
        };

        // Decrements the stylesheet reference counter and
        // destroys it if the counter reached zero
        void destroy_stylesheet (xsltStylesheetPtr ss);

        // Helper function to detect the XSLT output method
        bool is_xml_output_method (xsltStylesheetPtr ss);

        // Helper serialization functions
        void save_to_string(xmlDocPtr           doc,
                            xsltStylesheetPtr   ss,
                            std::string &       s);
        bool save_to_file(xmlDocPtr          doc,
                          xsltStylesheetPtr  ss,
                          const char *       filename,
                          int                compression_level);
    }
}

#endif

