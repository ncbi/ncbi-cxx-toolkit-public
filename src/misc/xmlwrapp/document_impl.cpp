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
 * This file contains the implementation of the xml::impl::doc_impl struct.
**/

// xmlwrapp includes
#include "document_impl.hpp"
#include "stylesheet_impl.hpp"


using namespace xml;
using namespace xml::impl;


doc_impl::doc_impl (void) : doc_(0), xslt_stylesheet_(0), owe_(true) { /* NCBI_FAKE_WARNING */
    xmlDocPtr       tmpdoc;
    if ( (tmpdoc = xmlNewDoc(0)) == 0)
        throw std::bad_alloc();
    set_doc_data(tmpdoc, true);
}

doc_impl::doc_impl (const char *root_name) : doc_(0), xslt_stylesheet_(0), root_(root_name), owe_(true) { /* NCBI_FAKE_WARNING */
    xmlDocPtr       tmpdoc;
    if ( (tmpdoc = xmlNewDoc(0)) == 0)
        throw std::bad_alloc();
    set_doc_data(tmpdoc, true);
}

doc_impl::doc_impl (const doc_impl &other) : doc_(0), xslt_stylesheet_(other.xslt_stylesheet_), owe_(true) { /* NCBI_FAKE_WARNING */
    xmlDocPtr       tmpdoc;
    if ( (tmpdoc = xmlCopyDoc(other.doc_, 1)) == 0)
        throw std::bad_alloc();
    set_doc_data(tmpdoc, false);

    if (xslt_stylesheet_)
        if (xslt_stylesheet_->_private)
            static_cast<xslt::impl::stylesheet_refcount*>
                (xslt_stylesheet_->_private)->inc_ref();
}

void doc_impl::set_doc_data (xmlDocPtr newdoc, bool root_is_okay) {
    if (doc_ && owe_)
        xmlFreeDoc(doc_);
    doc_ = newdoc;

    if (doc_->version)
        version_  = reinterpret_cast<const char*>(doc_->version);
    if (doc_->encoding)
        encoding_ = reinterpret_cast<const char*>(doc_->encoding);

    if (root_is_okay) {
        xmlDocSetRootElement(doc_, static_cast<xmlNodePtr>(root_.release_node_data()));
    } else {
        xmlNodePtr libxml_root_node = xmlDocGetRootElement(doc_);

        if (libxml_root_node)  {
            root_.set_node_data(libxml_root_node);
        } else {
            node tmpnode;
            root_.swap(tmpnode);

            xmlDocSetRootElement(doc_, static_cast<xmlNodePtr>(root_.release_node_data()));
        }
    }
}

void doc_impl::set_root_node (const node &n) {
    node &      non_const_node = const_cast<node&>(n);
    xmlNodePtr  new_root_node = xmlCopyNode(static_cast<xmlNodePtr>(non_const_node.get_node_data()), 1);
    if (!new_root_node)
        throw std::bad_alloc();

    xmlNodePtr old_root_node = xmlDocSetRootElement(doc_, new_root_node);
    root_.set_node_data(new_root_node);
    if (old_root_node)
        xmlFreeNode(old_root_node);
}

void doc_impl::set_ownership (bool owe) {
    owe_ = owe;
}

bool doc_impl::get_ownership (void) const {
    return owe_;
}

doc_impl::~doc_impl (void) {
    if (owe_) {
        if (doc_)
            xmlFreeDoc(doc_);
    }

    if (xslt_stylesheet_)
        if (xslt_stylesheet_->_private)
            xslt::impl::destroy_stylesheet(xslt_stylesheet_);
}

