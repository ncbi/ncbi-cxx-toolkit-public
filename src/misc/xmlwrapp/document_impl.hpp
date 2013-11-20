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


#ifndef _xmlwrapp_document_impl_h_
#define _xmlwrapp_document_impl_h_


// xmlwrapp includes
#include <misc/xmlwrapp/node.hpp>
#include <misc/xmlwrapp/dtd.hpp>
#include "node_manip.hpp"

// standard includes
#include <string>

// libxml includes
#include <libxml/tree.h>
#include <libxml/xinclude.h>
#include <libxslt/xslt.h>
#include <libxslt/xsltInternals.h>


namespace xml {
namespace impl {

struct doc_impl {
    doc_impl (void);
    doc_impl (const char *root_name);
    doc_impl (const doc_impl &other);
    void set_doc_data (xmlDocPtr newdoc, bool root_is_okay);
    void set_root_node (const node &n);
    void set_ownership (bool owe);
    bool get_ownership (void) const;
    ~doc_impl (void);

    xmlDocPtr               doc_;
    xsltStylesheetPtr       xslt_stylesheet_;

    node                    root_;
    std::string             version_;
    mutable std::string     encoding_;
    xml::dtd                internal_subset_;
    xml::dtd                external_subset_;
    bool                    owe_;
};

} // namespace impl
} // namespace xml

#endif

