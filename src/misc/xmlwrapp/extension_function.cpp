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
 * This file contains the implementation of the xslt::extension_function class.
**/


// xmlwrapp includes
#include <misc/xmlwrapp/xpath_object.hpp>
#include <misc/xmlwrapp/xpath_errors.hpp>
#include <misc/xmlwrapp/extension_function.hpp>
#include <misc/xmlwrapp/xslt_exception.hpp>
#include <misc/xmlwrapp/xslt_init.hpp>

// libxml2
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxslt/extensions.h>
#include <libxslt/xsltutils.h>

#include "extension_function_impl.hpp"
#include "stylesheet_impl.hpp"

namespace xslt {

    namespace impl {
        extension_function_impl::extension_function_impl() :
            xpath_parser_ctxt(NULL)
        {}
    }


    // Converts xmlwrapp error code to libxml2 one
    static std::string  convert_error_code (xpath_error  error)
    {
        switch (error) {
            case xpath_expression_ok:
                return "XPATH_EXPRESSION_OK";
            case xpath_number_error:
                return "XPATH_NUMBER_ERROR";
            case xpath_unfinished_literal_error:
                return "XPATH_UNFINISHED_LITERAL_ERROR";
            case xpath_start_literal_error:
                return "XPATH_START_LITERAL_ERROR";
            case xpath_variable_ref_error:
                return "XPATH_VARIABLE_REF_ERROR";
            case xpath_undef_variable_error:
                return "XPATH_UNDEF_VARIABLE_ERROR";
            case xpath_invalid_predicate_error:
                return "XPATH_INVALID_PREDICATE_ERROR";
            case xpath_expr_error:
                return "XPATH_EXPR_ERROR";
            case xpath_unclosed_error:
                return "XPATH_UNCLOSED_ERROR";
            case xpath_unknown_func_error:
                return "XPATH_UNKNOWN_FUNC_ERROR";
            case xpath_invalid_operand:
                return "XPATH_INVALID_OPERAND";
            case xpath_invalid_type:
                return "XPATH_INVALID_TYPE";
            case xpath_invalid_arity:
                return "XPATH_INVALID_ARITY";
            case xpath_invalid_ctxt_size:
                return "XPATH_INVALID_CTXT_SIZE";
            case xpath_invalid_ctxt_position:
                return "XPATH_INVALID_CTXT_POSITION";
            case xpath_memory_error:
                return "XPATH_MEMORY_ERROR";
            case xptr_syntax_error:
                return "XPTR_SYNTAX_ERROR";
            case xptr_resource_error:
                return "XPTR_RESOURCE_ERROR";
            case xptr_sub_resource_error:
                return "XPTR_SUB_RESOURCE_ERROR";
            case xpath_undef_prefix_error:
                return "XPATH_UNDEF_PREFIX_ERROR";
            case xpath_encoding_error:
                return "XPATH_ENCODING_ERROR";
            case xpath_invalid_char_error:
                return "XPATH_INVALID_CHAR_ERROR";
            case xpath_invalid_ctxt:
                return "XPATH_INVALID_CTXT";
            default:
                ;
        }
        throw xslt::exception("Unknown XPath error code.");
    }


    //
    // etension_function implementation
    //

    extension_function::extension_function ():
        pimpl_(new impl::extension_function_impl)
    {}

    extension_function::~extension_function ()
    {
        if (pimpl_ != NULL)
            delete pimpl_;
    }

    extension_function::extension_function (const extension_function &  other) :
        pimpl_(new impl::extension_function_impl)
    {
        pimpl_->xpath_parser_ctxt = other.pimpl_->xpath_parser_ctxt;
        return;
    }

    extension_function &
    extension_function::operator= (const extension_function &  other)
    {
        pimpl_->xpath_parser_ctxt = other.pimpl_->xpath_parser_ctxt;
        return *this;
    }

    extension_function::extension_function (extension_function &&  other) :
        pimpl_(other.pimpl_)
    {
        other.pimpl_ = NULL;
    }

    extension_function &
    extension_function::operator= (extension_function &&  other)
    {
        if (this != &other) {
            if (pimpl_ != NULL)
                delete pimpl_;
            pimpl_ = other.pimpl_;
            other.pimpl_ = NULL;
        }
        return *this;
    }

    void extension_function::report_error (xpath_error  error)
    {
        if (pimpl_->xpath_parser_ctxt == NULL)
            throw xslt::exception("Reporting XSLT extension function error "
                                  "out of XSLT context.");

        // The initial idea of reporting about errors was to use the
        // xmlXPathErr(...) function which accepts an integer error code.
        // Unfortunately this did not work as expected - the XSLT processor did
        // not notice the error report and just skipped it. Another approach
        // though - to use the xsltTransformError(...) function - worked quite
        // well. So this member was re-implemented to user a string report
        // version.
        std::string     message("Extension function error: " +
                                convert_error_code(error));

        report_error(message.c_str());
        return;
    }

    void extension_function::report_error (const char *  error)
    {
        if (pimpl_->xpath_parser_ctxt == NULL)
            throw xslt::exception("Reporting XSLT extension function error "
                                  "out of XSLT context.");

        xsltTransformContextPtr     xslt_ctxt =
                        xsltXPathGetTransformContext(pimpl_->xpath_parser_ctxt);
        xsltTransformError(xslt_ctxt, xslt_ctxt->style, NULL, "%s", error);
        return;
    }

    void extension_function::set_return_value (const xpath_object &  ret_val)
    {
        if (pimpl_->xpath_parser_ctxt == NULL)
            throw xslt::exception("Setting XSLT extension function return value "
                                  "out of XSLT context.");

        xmlXPathObjectPtr           object =
                    reinterpret_cast<xmlXPathObjectPtr>(ret_val.get_object());
        if (object == NULL)
            throw xslt::exception("Uninitialised xpath_object");

        // The xslt context holds a list of nodes to be freed later
        xsltTransformContextPtr     xslt_ctxt =
                    xsltXPathGetTransformContext(pimpl_->xpath_parser_ctxt);

        // This allows to avoid copying the real xpath object
        ret_val.revoke_ownership();


        // This is a part of a hack of leak-less handling nodeset return values
        // from XSLT extension functions. XSLT frees nodes in a nodeset too
        // early so the boolval must not be 1 (auto node deletion).
        // So the boolval here is set to 0 and to avoid leaks the nodes are
        // memorized in a transformation context to be freed later.
        // See stylesheet.cpp and xpath_object.cpp as well.
        if (init::get_allow_extension_functions_leak() == false) {
            if (object->type == XPATH_NODESET ||
                object->type == XPATH_XSLT_TREE) {
                if (ret_val.get_from_xslt() == false) {
                    object->boolval = 0;
                    for (int k = 0; k < object->nodesetval->nodeNr; ++k)
                        reinterpret_cast<xslt::impl::stylesheet_impl*>
                            (xslt_ctxt->_private)->nodes_to_free_.push_back(
                                                object->nodesetval->nodeTab[k]);
                }
            }
        }

        valuePush(pimpl_->xpath_parser_ctxt, object);
    }

} // xslt namespace


