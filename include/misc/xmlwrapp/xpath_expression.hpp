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
 * XPath expression storage for XmlWrapp.
**/

#ifndef _xmlwrapp_xpath_expression_hpp_
#define _xmlwrapp_xpath_expression_hpp_

// standard includes
#include <string>

// xmlwrapp includes
#include <misc/xmlwrapp/namespace.hpp>

namespace xml {

// Forward declaration
class node;


/**
 * The xml::xpath_expression class is used to store xpath query string
 * and optional XML namespaces.
 *
**/

class xpath_expression
{
public:
    /// xpath expression compilation flag
    enum compile_type {
        type_no_compile, //< Do not compile the given expression
        type_compile     //< Compile the given expression
    };

    /**
     * Create a new xml::xpath_expression object with the given xpath
     * expression.
     *
     * @param xpath
     *  The xpath expression.
     * @param do_compile
     *  Whether to compile or not the xpath expression. Default: do not.
     * @exception
     *  Throws exceptions in case of problems
     * @author Sergey Satskiy, NCBI
    **/
    xpath_expression (const char* xpath,
                      compile_type do_compile = type_no_compile);


    /**
     * Create a new xml::xpath_expression object with the given xpath
     * expression and a registered namespace. The namespace cannot be a default
     * one.
     *
     * @param xpath
     *  The xpath expression.
     * @param nspace
     *  Registered namespace.
     * @param do_compile
     *  Whether to compile or not the xpath expression. Default: do not.
     * @exception
     *  Throws exceptions in case of problems
     * @author Sergey Satskiy, NCBI
    **/
    xpath_expression (const char* xpath, const ns& nspace,
                      compile_type do_compile = type_no_compile);


    /**
     * Create a new xml::xpath_expression object with the given xpath
     * expression and a list of registered namespaces. The list of namespaces
     * cannot have a default namespace.
     *
     * @param xpath
     *  The xpath expression.
     * @param nspace_list
     *  Registered namespaces.
     * @param do_compile
     *  Whether to compile or not the xpath expression. Default: do not.
     * @exception
     *  Throws exceptions in case of problems
     * @author Sergey Satskiy, NCBI
    **/
    xpath_expression (const char* xpath, const ns_list_type& nspace_list,
                      compile_type do_compile = type_no_compile);

    /**
     * Create a new xml::xpath_expression object using another one as a
     * template.
     *
     * @param other
     *  Another xml::xpath_expression object.
     * @exception
     *  Throws exceptions in case of problems
     * @author Sergey Satskiy, NCBI
    **/
    xpath_expression (const xpath_expression&  other);

    /**
     * Create a copy of the xml::xpath_expression object.
     *
     * @param other
     *  Another xml::xpath_expression object.
     * @exception
     *  Throws exceptions in case of problems
     * @author Sergey Satskiy, NCBI
    **/
    xpath_expression& operator= (const xpath_expression& other);

    /**
     * Destroy the object and clean up the memory.
     *
     * @author Sergey Satskiy, NCBI
    **/
    virtual ~xpath_expression ();

    /**
     * Precompile the expression.
     *
     * @author Sergey Satskiy, NCBI
    **/
    void compile ();
    /**
     * Provide the xpath expression as a string.
     *
     * @return
     *  The stored xpath expression
     * @author Sergey Satskiy, NCBI
    **/
    const char* get_xpath() const;

    /**
     * Provide the list of the registered XML namespaces.
     *
     * @return
     *  The stored list of the registered XML namespaces
     * @author Sergey Satskiy, NCBI
    **/
    const ns_list_type& get_namespaces() const;

    /**
     * Provide the expression pre-compilation flag.
     *
     * @return
     *  The expression pre-compilation flag
     * @author Sergey Satskiy, NCBI
    **/
    compile_type get_compile_type () const;

    /**
     * Moving constructor.
     * @param other The other xpath expression.
    **/
    xpath_expression (xpath_expression &&other);

    /**
     * Moving assignment.
     * @param other The other xpath expression.
    **/
    xpath_expression &  operator=(xpath_expression &&other);

private:
    compile_type    compile_;               // compile flag
    std::string     expression_;            // xpath expression
    ns_list_type    nspace_list_;           // registered namespaces
    void*           compiled_expression_;   // xmlXPathCompExprPtr really

    void compile_expression ();

    void* get_compiled_expression () const;
    friend class node;
};


} // xml namespace

#endif

