/*
 * Copyright (C) 2008 Vadim Zeitlin (vadim@zeitlins.org)
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
 * This file contains the declaration of the xslt::result class.
**/

#ifndef _xsltwrapp_result_h_
#define _xsltwrapp_result_h_

// standard includes
#include <string>

// forward declarations
typedef struct _xmlDoc *xmlDocPtr;

namespace xml {
    class document;
}

namespace xslt {

namespace impl {

/**
 * The xslt::result class is used as a callback by xml::document to allow
 * special treatment of XML documents which were created by XSLT.
 *
 * This class is only meant to be used internally by the library and is
 * necessary to avoid the dependency of xml::document, which is part of
 * libxmlwrapp, on libxslt which should be only a dependency of libxsltwrapp
 * as this precludes calling the XSLT functions which must be used with such
 * "result" documents directly from xml::document code.
 *
 * @author Vadim Zeitlin
 * @internal
**/
class result {
public:
    //####################################################################
    /**
     * Save the contents of the given XML document in the provided string.
     *
     * @param s The string to place the XML text data.
    **/
    //####################################################################
    virtual void save_to_string(std::string &s) const = 0;

    //####################################################################
    /** 
     * Save the contents of the given XML document in the provided filename.
     *
     * @param filename The name of the file to place the XML text data into.
     * @param compression_level 0 is no compression, 1-9 allowed, where 1 is for better speed, and 9 is for smaller size
     * @return True if the data was saved successfully.
     * @return False otherwise.
    **/
    //####################################################################
    virtual bool save_to_file (const char *filename,
                               int compression_level) const = 0;

    //####################################################################
    /**
     * Trivial but virtual base class destructor.
    **/
    //####################################################################
    virtual ~result (void) { }

private:
    virtual xmlDocPtr get_raw_doc (void) = 0;

    friend class xml::document;
};


// The auxiliary function which needs for the document_proxy copy constructor
// only. The function essentially takes a pointer to a class which derives from
// result and makes a copy of the derived class.
// It throws an exception if copying error occured.
// the function should be deleted when document_proxy is deleted.
result *  make_copy (result *  pattern);

} // end impl namespace

} // end xslt namespace

#endif
