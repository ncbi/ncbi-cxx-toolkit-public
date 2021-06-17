/*
 * Copyright (C) 2001-2003 Peter J Jones (pjones@pmade.org)
 *               2009      Vaclav Slavik <vslavik@fastmail.fm>
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
 * This file implements the xml::impl::node_iterator class for libxml2.
**/


// definition include
#include "node_iterator.hpp"
#include "pimpl_base.hpp"
#include "deref_impl.hpp"

// xmlwrapp includes
#include <misc/xmlwrapp/node.hpp>

// standard includes
#include <algorithm>
#include <cassert>

// libxml includes
#include <libxml/tree.h>

using namespace xml;
using namespace xml::impl;

// xml::node::iterator pimpl
struct xml::impl::nipimpl : public pimpl_base<xml::impl::nipimpl> {
    node_iterator it;

    nipimpl (void) {};
    nipimpl (xmlNodePtr ptr) : it(ptr) {}
    nipimpl (const nipimpl &other) : it(other.it) {}
};

/*
 * xml::impl::node_iterator Real Iterator class
 */

//####################################################################
xml::node* xml::impl::node_iterator::get (void) const {
    node_private_data *     node_data = attach_node_private_data(node_);
    return &(node_data->node_instance_);
}
//####################################################################

/*
 * xml::node::iterator wrapper iterator class.
 */

//####################################################################
xml::node::iterator::iterator (void *data) {
    pimpl_ = new nipimpl(static_cast<xmlNodePtr>(data));
}
//####################################################################
xml::node::iterator::iterator (const iterator &other) {
    pimpl_ = other.pimpl_ ? new nipimpl(*(other.pimpl_)) : 0;
}
//####################################################################
xml::node::iterator& xml::node::iterator::operator= (const iterator &other) {
    iterator tmp(other);
    swap(tmp);
    return *this;
}
//####################################################################
void xml::node::iterator::swap (iterator &other) {
    std::swap(pimpl_, other.pimpl_);
}
//####################################################################
xml::node::iterator::~iterator (void) {
    delete pimpl_;
}
//####################################################################
xml::node::iterator::reference xml::node::iterator::operator* (void) const {
    return *(pimpl_->it.get());
}
//####################################################################
xml::node::iterator::pointer xml::node::iterator::operator-> (void) const {
    return pimpl_->it.get();
}
//####################################################################
xml::node::iterator& xml::node::iterator::operator++ (void) {
    pimpl_->it.advance();
    return *this;
}
//####################################################################
xml::node::iterator xml::node::iterator::operator++ (int) {
    iterator tmp(*this);
    ++(*this);
    return tmp;
}
//####################################################################
void* xml::node::iterator::get_raw_node (void) const {
    return pimpl_ ? pimpl_->it.get_raw_node() : 0;
}
//####################################################################

/*
 * xml::node::const_iterator wrapper iterator class.
 */

//####################################################################
xml::node::const_iterator::const_iterator (void *data) {
    pimpl_ = new nipimpl(static_cast<xmlNodePtr>(data));
}
//####################################################################
xml::node::const_iterator::const_iterator (const const_iterator &other) {
    pimpl_ = other.pimpl_ ? new nipimpl(*(other.pimpl_)) : 0;
}
//####################################################################
xml::node::const_iterator::const_iterator (const iterator &other) {
    pimpl_ = other.pimpl_ ? new nipimpl(*(other.pimpl_)) : 0;
}
//####################################################################
xml::node::const_iterator& xml::node::const_iterator::operator= (const const_iterator &other) {
    const_iterator tmp(other);
    swap(tmp);
    return *this;
}
//####################################################################
void xml::node::const_iterator::swap (const_iterator &other) {
    std::swap(pimpl_, other.pimpl_);
}
//####################################################################
xml::node::const_iterator::~const_iterator (void) {
    delete pimpl_;
}
//####################################################################
xml::node::const_iterator::reference xml::node::const_iterator::operator* (void) const {
    return *(pimpl_->it.get());
}
//####################################################################
xml::node::const_iterator::pointer xml::node::const_iterator::operator-> (void) const {
    return pimpl_->it.get();
}
//####################################################################
xml::node::const_iterator& xml::node::const_iterator::operator++ (void) {
    pimpl_->it.advance();
    return *this;
}
//####################################################################
xml::node::const_iterator xml::node::const_iterator::operator++ (int) {
    const_iterator tmp(*this);
    ++(*this);
    return tmp;
}
//####################################################################
void* xml::node::const_iterator::get_raw_node (void) const {
    return pimpl_ ? pimpl_->it.get_raw_node() : 0;
}

