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
 * xpath error codes
**/

#ifndef _xmlwrapp_xpath_errors_hpp_
#define _xmlwrapp_xpath_errors_hpp_


namespace xslt {

    /// XPath error which could be reported by a user XSLT extension function.
    enum xpath_error {
        xpath_expression_ok,
        xpath_number_error,
        xpath_unfinished_literal_error,
        xpath_start_literal_error,
        xpath_variable_ref_error,
        xpath_undef_variable_error,
        xpath_invalid_predicate_error,
        xpath_expr_error,
        xpath_unclosed_error,
        xpath_unknown_func_error,
        xpath_invalid_operand,
        xpath_invalid_type,
        xpath_invalid_arity,
        xpath_invalid_ctxt_size,
        xpath_invalid_ctxt_position,
        xpath_memory_error,
        xptr_syntax_error,
        xptr_resource_error,
        xptr_sub_resource_error,
        xpath_undef_prefix_error,
        xpath_encoding_error,
        xpath_invalid_char_error,
        xpath_invalid_ctxt
    };

} // xslt namespace

#endif

