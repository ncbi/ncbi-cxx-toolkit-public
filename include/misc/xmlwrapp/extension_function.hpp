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
 * XSLT extension function object
**/

#ifndef _xmlwrapp_extension_function_hpp_
#define _xmlwrapp_extension_function_hpp_

// standard includes
#include <vector>

// xmlwrapp includes
#include <misc/xmlwrapp/xpath_object.hpp>
#include <misc/xmlwrapp/xpath_errors.hpp>

// Forward declaration for a friend below
extern "C" { void xslt_ext_func_cb(void *, int); }


namespace xml {
    class node;
    class document;
}


namespace xslt {

    namespace impl {
        struct extension_function_impl;
    }

/**
 * The XSLT extension function object is used to be a base class for the user
 * provided XSLT functions.
 *
**/
class extension_function
{
public:
    /**
     * Create a new extension function.
    **/
    extension_function ();

    /**
     * Destroy extension function object and clean the memory up.
    **/
    virtual ~extension_function ();

    /**
     * Create a new extension function using another one as a template.
     *
     * @param other
     *  Another xslt::extension_function object.
    **/
    extension_function (const extension_function &  other);

    /**
     * Create a copy of the extension function object.
     *
     * @param other
     *  Another xslt::extension_function object.
    **/
    extension_function &  operator= (const extension_function &  other);

    /**
     * Moving constructor.
     * @param other The other extension function.
    **/
    extension_function (extension_function &&  other);

    /**
     * Moving assignment.
     * @param other The other extension function.
    **/
    extension_function & operator= (extension_function &&  other);

protected:
    /**
     * This member is called by the XSLT processor when it sees the
     * corresponding extension function call.
     *
     * @param args
     *  Extension function arguments
     * @param node
     *  The current document node
     * @param doc
     *  The current document
    **/
    virtual void execute (const std::vector<xpath_object> &  args,
                          const xml::node &                  node,
                          const xml::document &              doc) = 0;

    /**
     * Report an error to the XSLT processor. This is the recommended way to
     * report errors.
     *
     * @note It can be called only within execute(...) member.
     *
     * @param error
     *  Error message to be reported.
    **/
    void report_error (const char *  error);

    /**
     * Report an error to the XSLT processor.
     *
     * @note It can be called only within execute(...) member.
     *
     * @param error
     *  Error code to be reported
    **/
    void report_error (xpath_error  error);

    /**
     * Set the extension function return value.
     *
     * @note It can be called only within execute(...) member.
     *
     * @param ret_val
     *  The extension function return value
    **/
    void set_return_value (const xpath_object &  ret_val);

private:
    impl::extension_function_impl *     pimpl_;
    friend void ::xslt_ext_func_cb(void *, int);
};


} // xslt namespace

#endif

