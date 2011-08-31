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
#include <misc/xmlwrapp/xslt_init.hpp>
#include <misc/xmlwrapp/document.hpp>
#include <misc/xmlwrapp/xslt_result_type.hpp>

// standard includes
#include <map>
#include <string>

namespace xslt {

/**
 * The xslt::stylesheet class is used to hold information about an XSLT
 * stylesheet. You can use it to load in a stylesheet and then use that
 * stylesheet to transform an XML document to something else.
**/
class stylesheet {
public:
    struct pimpl;

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
     * Clean up after an xslt::stylesheet.
     *
     * @author Peter Jones
    **/
    //####################################################################
    virtual ~stylesheet (void);

    //####################################################################
    /**
     * Apply this stylesheet to the given XML document. The result document
     * is placed in the second document parameter.
     *
     * @param doc The XML document to transform.
     * @param result The result tree after applying this stylesheet.
     * @param treat How to treat XSLT result
     * @return True if the transformation was successful and the results placed in result.
     * @return False if there was an error, result is not modified.
     * @author Peter Jones
    **/
    //####################################################################
    NCBI_DEPRECATED
    bool apply (const xml::document &doc, xml::document &result,
                result_treat_type  treat=type_no_treat);

    //####################################################################
    /**
     * Apply this stylesheet to the given XML document. The result document
     * is placed in the second document parameter.

     * @param doc The XML document to transform.
     * @param result The result tree after applying this stylesheet.
     * @param with_params Override xsl:param elements using the given key/value map
     * @param treat How to treat XSLT result
     * @return True if the transformation was successful and the results placed in result.
     * @return False if there was an error, result is not modified.
     * @author Peter Jones
    **/
    //####################################################################
    NCBI_DEPRECATED
    bool apply (const xml::document &doc, xml::document &result,
                const param_type &with_params,
                result_treat_type  treat=type_no_treat);

    //####################################################################
    /**
     * Apply this stylesheet to the given XML document. The results document
     * is returned. If there is an error during transformation, this
     * function will throw an xml::exception exception.
     *
     * @param doc The XML document to transform.
     * @param treat How to treat XSLT result
     * @return The result tree.
     * @author Denis Vakatov
    **/
    //####################################################################
    xml::document_proxy apply (const xml::document &doc,
                               result_treat_type  treat=type_no_treat);

    //####################################################################
    /**
     * Apply this stylesheet to the given XML document. The results document
     * is returned. If there is an error during transformation, this
     * function will throw an xml::exception exception.
     *
     * @param doc The XML document to transform.
     * @param with_params Override xsl:param elements using the given key/value map
     * @param treat How to treat XSLT result
     * @return The result tree.
     * @author Denis Vakatov
    **/
    //####################################################################
    xml::document_proxy apply (const xml::document &doc,
                               const param_type &with_params,
                               result_treat_type  treat=type_no_treat);

    //####################################################################
    /**
     * If you used one of the xslt::stylesheet::apply member functions that
     * return a bool, you can use this function to get the text message for
     * the transformation error.
     *
     * If you are using one of the apply member functions that throws
     * exceptions, this function should not be used. The text message for
     * the transformation error will be given to the xml::exception
     * constructor.
     *
     * @return The last error message.
     * @author Peter Jones
    **/
    //####################################################################
    NCBI_DEPRECATED
    const std::string& get_error_message (void) const;

private:
    pimpl *pimpl_;

    // an xslt::stylesheet cannot yet be copied or assigned to.
    stylesheet (const stylesheet&);
    stylesheet& operator= (const stylesheet&);
}; // end xslt::stylesheet class

} // end xslt namespace
#endif

