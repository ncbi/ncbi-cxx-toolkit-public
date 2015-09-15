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
 * Flags for saving XML documents and nodes.
**/

#ifndef _xmlwrapp_xml_save_hpp_
#define _xmlwrapp_xml_save_hpp_


namespace xml {

    enum save_options {
        // Compression level part (16 bit reserved)
        // Only one value should be picked
        // Default is no compression
        save_op_compress_level_1 = 1,   ///< better speed
        save_op_compress_level_2 = 2,
        save_op_compress_level_3 = 3,
        save_op_compress_level_4 = 4,
        save_op_compress_level_5 = 5,
        save_op_compress_level_6 = 6,
        save_op_compress_level_7 = 7,
        save_op_compress_level_8 = 8,
        save_op_compress_level_9 = 9,   ///< smaller size


        // Document options part - any number of the options below could be
        // picked
        save_op_no_format   = 1 << 16,  ///< Do not format save output
        save_op_no_decl     = 1 << 17,  ///< Drop the xml declaration
        save_op_no_empty    = 1 << 18,  ///< No empty tags
        save_op_no_xhtml    = 1 << 19,  ///< Disable XHTML1 specific rules
        save_op_xhtml       = 1 << 20,  ///< Force XHTML1 specific rules. Ignored if libxml2 version is < 2.7.2
        save_op_not_as_xml  = 1 << 21,  ///< Do not force XML serialization on HTML doc. Ignored if libxml2 version is < 2.7.2
        save_op_as_html     = 1 << 22,  ///< Force HTML serialization on XML doc. Ignored if libxml2 version is < 2.7.2

        /// Format with non-significant whitespace. Ignored if libxml2 version
        /// is < 2.7.8. This flag will automatically set the save_op_no_format
        /// as well due to libxml2 implementation.
        save_op_with_non_significant_ws = 1 << 23,

        save_op_default     = 0         ///< Default is:
                                        ///< - no compression
                                        ///< - to format save output and
                                        ///< - to force XML serialization on HTML doc
    };

    typedef int save_option_flags;  ///< Bitwise save options mask type and a compression level


    /// Canonicalization mode. See "Canonical XML" (http://www.w3.org/TR/xml-c14n)
    /// or "Exclusive XML Canonicalization" (http://www.w3.org/TR/xml-exc-c14n)
    enum canonicalization_option {
        c14n_1_0,               ///< Origianal C14N 1.0 spec
        c14n_exclusive_1_0,     ///< Exclusive C14N 1.0 spec
        c14n_1_1                ///< C14N 1.1 spec
    };

    enum canonicalization_comments_option {
        keep_comments,
        strip_comments
    };

    enum canonicalization_format_option {
        with_formatting,
        without_formatting
    };

    enum canonicalization_node_sort_option {
        with_node_sorting,
        without_node_sorting
    };

} // xml namespace

#endif

