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
 * XPath object. Used in XSLT extension functions to store function arguments
 * and return values.
**/

#ifndef _xmlwrapp_xpath_object_hpp_
#define _xmlwrapp_xpath_object_hpp_

// standard includes
#include <string>

// xmlwrapp includes
#include <misc/xmlwrapp/xpath_object_type.hpp>
#include <misc/xmlwrapp/node_set.hpp>

// Forward declaration for a friend below
extern "C" { void xslt_ext_func_cb(void *, int); }

namespace xml {
    // Forward declaration
    class node;
}


namespace xslt {

namespace impl
{
    // Forward declaration
    struct xpath_obj_impl;
}

class extension_function;
class extension_element;


/**
 * The xslt::xpath_object class is used to store extension function
 * arguments and return values.
 *
**/
class xpath_object
{
public:
    /// Source of nodes to construct xpath_object of the nodeset type
    typedef std::vector<xml::node>  type_node_source;

public:

    /**
     * Create a new xslt::xpath_object object.
    **/
    xpath_object ();

    /**
     * Destroy the object and clean up the memory.
    **/
    ~xpath_object ();

    /**
     * Create a new xslt::xpath_object object using another as a template.
     * The new object refers to the same xpath_object as the template one.
     *
     * @param other
     *  Another xslt::xpath_object object.
    **/
    xpath_object (const xpath_object &  other);

    /**
     * Create a copy of the xslt::xpath_object object.
     * The copy refers to the same xpath_object as the given object.
     *
     * @param other
     *  Another xslt::xpath_object object.
    **/
    xpath_object & operator= (const xpath_object &  other);

    /**
     * Moving constructor.
     * @param other The other xpath object.
    **/
    xpath_object (xpath_object &&  other);

    /**
     * Moving assignment.
     * @param other The other xpath object.
    **/
    xpath_object & operator= (xpath_object &&  other);

    /**
     * Create a new xslt::xpath_object object of the string type.
     *
     * @param value
     *  String to be used as the xpath_object value.
    **/
    xpath_object (const char *  value);

    /**
     * Create a new xslt::xpath_object object of the bool type.
     *
     * @param value
     *  Boolean to be used as the xpath_object value.
    **/
    xpath_object (bool  value);

    /**
     * Create a new xslt::xpath_object object of the integer type.
     *
     * @param value
     *  Integer to be used as the xpath_object value.
    **/
    xpath_object (int  value);

    /**
     * Create a new xslt::xpath_object object of the float type.
     *
     * @param value
     *  Double to be used as the xpath_object value.
    **/
    xpath_object (double  value);

    /**
     * Create a new xslt::xpath_object object of the node type.
     *
     * @param value
     *  xml::node to be used as the xpath_object value.
    **/
    xpath_object (const xml::node &  value);

    /**
     * Create a new xslt::xpath_object of the nodeset type.
     *
     * @param value
     *  xml::node_set to be used as a source of nodes.
    **/
    xpath_object (const xml::node_set &  value);

    /**
     * Create a new xslt::xpath_object of the nodeset type.
     *
     * @param value
     *  container of nodes as a source of nodes.
    **/
    xpath_object (const type_node_source &  value);

    /**
     * Provide the object value as a string.
     * It tries to make a conversion if needed.
     *
     * @return The stored or converted value.
     * @exception Throws xslt::exception in case of problems.
    **/
    std::string get_as_string (void) const;

    /**
     * Provide the object value as a boolean.
     * It tries to make a conversion if needed.
     *
     * @return The stored or converted value.
     * @exception Throws xslt::exception in case of problems.
    **/
    bool get_as_bool (void) const;

    /**
     * Provide the object value as an integer.
     * It tries to make a conversion if needed.
     *
     * @return The stored or converted value.
     * @exception Throws xslt::exception in case of problems.
    **/
    int get_as_int (void) const;

    /**
     * Provide the object value as a float.
     * It tries to make a conversion if needed.
     *
     * @return The stored or converted value.
     * @exception Throws xslt::exception in case of problems.
    **/
    double get_as_float (void) const;

    /**
     * Provide the object value as a node.
     *
     * @return A reference to a node. If there were many nodes in the result
     *         set then a reference to the first one is provided.
     * @exception Throws xslt::exception in case of problems.
    **/
    xml::node &  get_as_node (void) const;

    /**
     * Provide the object value as a node set.
     *
     * @return A set of nodes.
     * @exception Throws xslt::exception in case of problems.
    **/
    xml::node_set  get_as_node_set (void) const;

    /**
     * Provides the object type.
     *
     * @return The type of the stored value.
     * @exception Throws xslt::exception in case of problems.
    **/
    xpath_object_type get_type (void) const;

private:
    friend class extension_function;
    friend class extension_element;
    void *  get_object (void) const;
    void revoke_ownership (void) const;
    void set_from_xslt(void) const;
    bool get_from_xslt(void) const;
    void test_int_convertability(double val) const;
    xpath_object (void *);
    friend void ::xslt_ext_func_cb(void *, int);

private:
    impl::xpath_obj_impl *      pimpl_;     // private implementation
};


} // xslt namespace

#endif

