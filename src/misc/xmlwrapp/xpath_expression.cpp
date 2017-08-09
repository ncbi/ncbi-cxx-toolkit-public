/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $Id$
 */

/** @file
 * This file contains the implementation of the xml::xpath_expression cass.
**/

// xmlwrapp includes
#include <misc/xmlwrapp/xpath_expression.hpp>
#include <misc/xmlwrapp/exception.hpp>

// standard includes
#include <string.h>
#include <stdexcept>

// libxml includes
#include <libxml/xpath.h>

namespace xml
{

const char*  kEmptyExpression = "xpath expression can't be empty";
const char*  kDefaultNamespace = "default namespace cannot be registered";



xpath_expression::xpath_expression (const char* xpath,
                                    compile_type do_compile) :
    compile_(do_compile),
    expression_(xpath ? xpath : ""),
    compiled_expression_(NULL)
{
    if (expression_.empty())
        throw exception(kEmptyExpression);
    compile_expression();
}


xpath_expression::xpath_expression (const char* xpath,
                                    const ns& nspace,
                                    compile_type do_compile) :
    compile_(do_compile),
    expression_(xpath ? xpath : ""),
    compiled_expression_(NULL)
{
    if (expression_.empty())
        throw xml::exception(kEmptyExpression);

    if (strlen(nspace.get_prefix()) == 0)
        throw xml::exception(kDefaultNamespace);

    nspace_list_.push_back(nspace);

    compile_expression();
}


xpath_expression::xpath_expression (const char* xpath,
                                    const ns_list_type& nspace_list,
                                    compile_type do_compile) :
    compile_(do_compile),
    expression_(xpath ? xpath : ""),
    compiled_expression_(NULL)
{
    if (expression_.empty())
        throw xml::exception(kEmptyExpression);

    for (ns_list_type::const_iterator  k(nspace_list.begin());
         k != nspace_list.end(); ++k)
        if (strlen(k->get_prefix()) == 0)
            throw xml::exception(kDefaultNamespace);

    nspace_list_ = nspace_list;
    compile_expression();
}


xpath_expression::xpath_expression (const xpath_expression&  other) :
    compile_(other.compile_), expression_(other.expression_),
    nspace_list_(other.nspace_list_), compiled_expression_(NULL)
{
    compile_expression();
}


xpath_expression& xpath_expression::operator= (const xpath_expression& other)
{
    if (this != &other) {
        compile_ = other.compile_;
        expression_ = other.expression_;
        nspace_list_ = other.nspace_list_;

        if (compiled_expression_) {
            xmlXPathFreeCompExpr(
                reinterpret_cast<xmlXPathCompExprPtr>(compiled_expression_));
            compiled_expression_ = NULL;
        }
        compile_expression();
    }
    return *this;
}


xpath_expression::~xpath_expression ()
{
    if (compiled_expression_)
        xmlXPathFreeCompExpr(
            reinterpret_cast<xmlXPathCompExprPtr>(compiled_expression_));
    compiled_expression_ = NULL;
}


xpath_expression::xpath_expression(xpath_expression &&other) :
    compile_(other.compile_),
    expression_(std::move(other.expression_)),
    nspace_list_(std::move(other.nspace_list_)),
    compiled_expression_(other.compiled_expression_)
{
    other.compiled_expression_ = NULL;
}


xpath_expression &  xpath_expression::operator=(xpath_expression &&other)
{
    if (this != &other) {
        if (compiled_expression_)
            xmlXPathFreeCompExpr(
                reinterpret_cast<xmlXPathCompExprPtr>(compiled_expression_));
        compile_ = other.compile_;
        expression_ = std::move(other.expression_);
        nspace_list_ = std::move(other.nspace_list_);
        compiled_expression_ = other.compiled_expression_;

        other.compiled_expression_ = NULL;
    }
    return *this;
}


void xpath_expression::compile ()
{
    if (compile_ != type_compile) {
        compile_ = type_compile;
        compile_expression();
    }
}


const char* xpath_expression::get_xpath() const
{
    return expression_.c_str();
}


const ns_list_type& xpath_expression::get_namespaces() const
{
    return nspace_list_;
}


xpath_expression::compile_type xpath_expression::get_compile_type() const
{
    return compile_;
}


void* xpath_expression::get_compiled_expression () const
{
    return compiled_expression_;
}


void xpath_expression::compile_expression ()
{
    if (compile_ == type_compile)
    {
        compiled_expression_ = xmlXPathCompile(
            reinterpret_cast<const xmlChar*>(expression_.c_str()));
        if (!compiled_expression_) {
            xmlErrorPtr     last_error(xmlGetLastError());
            std::string     message("xpath expression compilation error");

            if (last_error && last_error->message)
                message += " : " + std::string(last_error->message);
            throw xml::exception(message);
        }
    }
}

} // namespace xml
