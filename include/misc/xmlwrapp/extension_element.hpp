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
 * XSLT extension element object
**/

#ifndef _xmlwrapp_extension_element_hpp_
#define _xmlwrapp_extension_element_hpp_

// standard includes
#include <vector>

// xmlwrapp includes
#include <misc/xmlwrapp/xpath_object.hpp>
#include <misc/xmlwrapp/xpath_errors.hpp>

// Forward declaration for a friend below
extern "C" { void xslt_ext_element_cb(void *, void *, void *, void *); }


namespace xml {
    class node;
    class document;
}


namespace xslt {

    namespace impl {
        struct extension_element_impl;
    }

/**
 * The XSLT extension element object is used to be a base class for the user
 * provided XSLT extension elements.
 *
**/
class extension_element
{
public:
    /**
     * Create a new extension element.
    **/
    extension_element ();

    /**
     * Destroy extension element object and clean the memory up.
    **/
    virtual ~extension_element ();

    /**
     * Create a new extension element using another one as a template.
     *
     * @param other
     *  Another xslt::extension_element object.
    **/
    extension_element (const extension_element &  other);

    /**
     * Create a copy of the extension element object.
     *
     * @param other
     *  Another xslt::extension_element object.
    **/
    extension_element &  operator= (const extension_element &  other);

    /**
     * Moving constructor.
     * @param other The other extension element.
    **/
    extension_element (extension_element &&  other);

    /**
     * Moving assignment.
     * @param other The other extension element.
    **/
    extension_element &  operator= (extension_element &&  other);

protected:
    /**
     * This member is called by the XSLT processor when it sees the
     * corresponding extension element.
     *
     * @param current_node
     *  Extension element current node
     * @param instruction_node
     *  The stylesheet instruction node
     * @param insert_point
     *  The insertion point
     * @param doc
     *  The current document
    **/
    virtual void process (xml::node &               input_node,
                          const xml::node &         instruction_node,
                          xml::node &               insert_point,
                          const xml::document &     doc) = 0;

    /**
     * Report an error to the XSLT processor.
     *
     * @note It can be called only within execute(...) member.
     *
     * @param error
     *  Error message to be reported
    **/
    void report_error (const char *  error);

    /**
     * Evaluate XPath expression.
     *
     * @note It can be called only within execute(...) member.
     *
     * @param xpath_expression
     *  XPath expression to evaluate
     * @param node
     *  Node for which the expression is evaluated
    **/
    xpath_object evaluate (const char *       xpath_expression,
                           const xml::node &  node);

    /**
     * Evaluate XPath expression for the current node.
     *
     * @note It can be called only within execute(...) member.
     * @param xpath_expression
     *  XPath expression to evaluate
    **/
    xpath_object evaluate (const char *  xpath_expression);

private:
    impl::extension_element_impl *      pimpl_;
    friend void ::xslt_ext_element_cb(void *, void *, void *, void *);
};


} // xslt namespace

#endif

