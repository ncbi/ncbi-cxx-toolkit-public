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
 * This file defines the xml::dtd_impl class.
**/

#ifndef _xmlwrapp_dtd_impl_h_
#define _xmlwrapp_dtd_impl_h_

// standard includes
#include <string>

// libxml2 includes
#include <libxml/parser.h>
#include <libxml/valid.h>
#include <libxml/tree.h>

namespace xml {

namespace impl {

class dtd_impl {
public:
    /*
     * load the given DTD. you should check to see if error_ is empty or
     * not, which will tell you if the DTD was loaded okay.
     */
    explicit dtd_impl (const char *filename);

    /// Don't load a DTD
    dtd_impl (void);

    /// Delete stuff
    ~dtd_impl (void);

    /*
     * check the document against the loaded DTD, or in the case where no
     * DTD is loaded try to use the one inside the document.
     */
    bool validate (xmlDocPtr xmldoc);

    /// return the dtd pointer and never again free it.
    xmlDtdPtr release (void);

    /// count of DTD parsing/validating warnings
    int warnings_;

    /// last DTD parsing/validating error message
    std::string error_;

private:
    xmlValidCtxt vctxt_;
    xmlDtdPtr dtd_;

    dtd_impl (const dtd_impl&);
    dtd_impl& operator= (const dtd_impl&);
    void init_ctxt (void);
}; // end xml::impl::dtd_impl class

} // end impl namespace

} // end xml namespace
#endif
