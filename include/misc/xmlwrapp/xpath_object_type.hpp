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
 * Type for the xpath_object
**/

#ifndef _xmlwrapp_xpath_object_type_hpp_
#define _xmlwrapp_xpath_object_type_hpp_


namespace xslt {

    /// Identifies what is stored in an xpath_object instance.
    /// The only limited support is provided for the libxml2 types.
    /// In particular the following libxml2 types are not supported:
    /// - point
    /// - range
    /// - locationset
    /// - users
    /// - xslt tree
    enum xpath_object_type {
        type_undefined,
        type_nodeset,
        type_boolean,
        type_number,
        type_string,

        type_not_implemented

        /* The types below exist in libxml2 however they are
           not implemented in XmlWrapp.
        type_point,
        type_range,
        type_locationset,
        type_users,
        type_xslt_tree
        */
    };

    /*
     * Note: xslt_tree type is very similar to node_set however it was
     * non-trivial to support it. The problem comes from the fact that libxml2
     * internally uses the node _private field in some cases. XmlWrapp uses it
     * too when it needs to dereference a node_set iterator and this leads to a
     * conflict with memory corruption. So, at least for now, the xslt_tree
     * support is not provided.
     */

} // xslt namespace

#endif

