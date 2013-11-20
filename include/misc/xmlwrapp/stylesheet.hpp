/*
 * Copyright (C) 2001-2003 Peter J Jones (pjones@pmade.org)
 * All Rights Reserved
 *
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
 * NOTE: This file was modified from its original version 0.6.0
 *       to fit the NCBI C++ Toolkit build framework and
 *       API and functionality requirements.
 */

/** @file
 * This file contains the definition of the xslt::stylesheet class.
**/

#ifndef _xsltwrapp_stylesheet_h_
#define _xsltwrapp_stylesheet_h_

// xmlwrapp includes
#include <misc/xmlwrapp/ownership.hpp>
#include <misc/xmlwrapp/xslt_init.hpp>
#include <misc/xmlwrapp/document.hpp>
#include <misc/xmlwrapp/extension_function.hpp>
#include <misc/xmlwrapp/extension_element.hpp>

// standard includes
#include <map>
#include <string>


namespace xslt {

    namespace impl {
        // Forward declaration
        struct stylesheet_impl;
    }


/**
 * The xslt::stylesheet class is used to hold information about an XSLT
 * stylesheet. You can use it to load in a stylesheet and then use that
 * stylesheet to transform an XML document to something else.
**/
class stylesheet {
public:
    /// Type for passing parameters to the stylesheet
    typedef std::map<std::string, std::string> param_type;

    //####################################################################
    /**
     * Create a new xslt::stylesheet object and load and parse the
     * stylesheet in the given filename.
     *
     * @param filename The name of the file that contains the stylesheet.
     * @author Peter Jones
    **/
    //####################################################################
    explicit stylesheet (const char *filename);

    //####################################################################
    /**
     * Create a new xslt::stylesheet object from an xml::document object
     * that contains the parsed stylesheet. The given xml::document is
     * copied. This is needed because the stylesheet will own the
     * document and free it.
     *
     * @param doc The parsed stylesheet.
     * @author Peter Jones
    **/
    //####################################################################
    explicit stylesheet (const xml::document &  doc);

    //####################################################################
    /**
     * Create a new xslt::stylesheet object by parsing the given XML from a
     * memory buffer.
     *
     * @param data The XML memory buffer.
     * @param size Size of the memory buffer.
     * @author Denis Vakatov
    **/
    stylesheet (const char* data, size_t size);

    //####################################################################
    /**
     * Create a new xslt::stylesheet object by parsing the given XML from a
     * stream.
     *
     * @param stream Stream to read from
     * @author Denis Vakatov
    **/
    stylesheet (std::istream & stream);

    //####################################################################
    /**
     *
     * Register an XSLT extension function.
     *
     * @param ef The extension function pointer.
     * @param name Extension function name. It cannot be NULL.
     * @param uri Extension function URI. It cannot be NULL.
     * @param ownership If owned then xslt::stylesheet is responsible for
     *                  deleting the extension function. The responsibility
     *                  starts from the moment this member is called, i.e. even
     *                  if the registration failed the extension function will
     *                  be deleted.
     * @exception Throw xslt::exception in case of problems
     * @author Denis Vakatov, NCBI
    **/
    void register_extension_function (extension_function *  ef,
                                      const char *          name,
                                      const char *          uri,
                                      xml::ownership_type   ownership = xml::type_not_own);

    //####################################################################
    /**
     *
     * Register an XSLT extension element.
     *
     * @param ef The extension element pointer.
     * @param name Extension element name. It cannot be NULL.
     * @param uri Extension element URI. It cannot be NULL.
     * @param ownership If owned then xslt::stylesheet is responsible for
     *                  deleting the extension element. The responsibility
     *                  starts from the moment this member is called, i.e. even
     *                  if the registration failed the extension element will
     *                  be deleted.
     * @exception Throw xslt::exception in case of problems
     * @author Denis Vakatov, NCBI
    **/
    void register_extension_element (extension_element *  ee,
                                     const char *         name,
                                     const char *         uri,
                                     xml::ownership_type  ownership = xml::type_not_own);

    //####################################################################
    /**
     * Clean up after an xslt::stylesheet.
     *
     * @author Peter Jones
    **/
    //####################################################################
    virtual ~stylesheet (void);

    //####################################################################
    /**
     * Apply this stylesheet to the given XML document. The results document
     * is returned. If there is an error during transformation, this
     * function will throw an xml::exception exception.
     *
     * @attention The method is not thread safe
     * @note The xslt output method is taken into account when the result
     * document is saved later into a string or a stream as in (e.g. to
     * suppress XML declaration):
     *     mydoc.save_to_string(mystring, xml::save_op_no_decl);
     * The formatting flags are taken into consideration only if the xslt
     * output method is xml. (The xslt default output method is xml).
     *
     * @param doc The XML document to transform.
     * @return The result tree.
    **/
    //####################################################################
    xml::document_proxy apply (const xml::document &doc);

    //####################################################################
    /**
     * Apply this stylesheet to the given XML document. The results document
     * is returned. If there is an error during transformation, this
     * function will throw an xml::exception exception.
     *
     * @attention The method is not thread safe
     * @note The xslt output method is taken into account when the result
     * document is saved later into a string or a stream as in (e.g. to
     * suppress XML declaration):
     *     mydoc.save_to_string(mystring, xml::save_op_no_decl);
     * The formatting flags are taken into consideration only if the xslt
     * output method is xml. (The xslt default output method is xml).
     *
     * @param doc The XML document to transform.
     * @param with_params Override xsl:param elements using the given key/value map
     * @note each (simple) param name must be enclosed in quotes as per libxslt specs
     * @note string parameter value has to be enclosed in quotes. e.g.,
     * @note params["ParamString"] = "'Text'";
     * @note each xpath parameter value (including simple numerics) should be given as is. e.g.
     * @note params["ParamXpath"] = "//NodeName";
     * @note params["ParamNumber"] = "123565";
     * @return The result tree.
     **/
    //####################################################################
    xml::document_proxy apply (const xml::document &doc,
                               const param_type &with_params);

private:
    impl::stylesheet_impl *pimpl_;

    void attach_refcount (void);

    // an xslt::stylesheet cannot yet be copied or assigned to.
    stylesheet (const stylesheet&);
    stylesheet& operator= (const stylesheet&);

}; // end xslt::stylesheet class

} // end xslt namespace
#endif

