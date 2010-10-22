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
 * Most importantly, it adds support for XML namespaces (see "namespace.hpp").
 */

/** @file
 * This file implements the ait_impl, xml::attributes::iterator,
 * xml::attributes::const_iterator and xml::attributes::attr classes.
**/

// xmlwrapp includes
#include "ait_impl.hpp"
#include "utility.hpp"
#include <misc/xmlwrapp/attributes.hpp>
#include <misc/xmlwrapp/exception.hpp>

// standard includes
#include <algorithm>
#include <stdexcept>
#include <string.h>

// libxml2 includes
#include <libxml/tree.h>

using namespace xml;
using namespace xml::impl;

/*
 * First we have the ait_impl class.
 */

//####################################################################
ait_impl::ait_impl (xmlNodePtr node, xmlAttrPtr prop) : xmlnode_(node), xmlattr_(prop), fake_(false) {
    attr_.set_data(xmlnode_, xmlattr_);
}
//####################################################################
ait_impl::ait_impl (const char *name, const char *value, bool) : xmlnode_(0), xmlattr_(0), fake_(true) {
    /*
     * in this constructor and in the functions to follow, the last
     * parameter, the bool, is only used to create a unique signature
     */
    attr_.set_data(name, value, true);
}
//####################################################################
ait_impl::ait_impl (const ait_impl &other) : xmlnode_(other.xmlnode_), xmlattr_(other.xmlattr_), fake_(other.fake_)  {
    if (fake_) attr_.set_data(other.attr_.get_name(), other.attr_.get_value(), true);
    else attr_.set_data(xmlnode_, xmlattr_);
}
//####################################################################
ait_impl& ait_impl::operator= (const ait_impl &other) {
    ait_impl tmp(other);

    std::swap(xmlnode_, tmp.xmlnode_);
    std::swap(xmlattr_, tmp.xmlattr_);
    std::swap(fake_, tmp.fake_);
    attr_.swap(tmp.attr_);

    return *this;
}
//####################################################################
xml::attributes::attr* ait_impl::get (void) {
    return &attr_;
}
//####################################################################
xmlAttrPtr ait_impl::get_raw_attr (void) {
    return xmlattr_;
}
//####################################################################
ait_impl& ait_impl::operator++ (void) {
    if (xmlattr_) xmlattr_ = xmlattr_->next;
    else fake_ = false;

    attr_.set_data(xmlnode_, xmlattr_);
    return *this;
}
//####################################################################
ait_impl ait_impl::operator++ (int) {
    ait_impl tmp(xmlnode_, xmlattr_);
    ++(*this);
    return tmp;
}
//####################################################################

/*
 * Member functions for the xml::attributes::iterator class.
 */

//####################################################################
xml::attributes::iterator::iterator (void) {
    pimpl_ = new ait_impl(0, 0);
}
//####################################################################
xml::attributes::iterator::iterator (void *node, void *prop) {
    pimpl_ = new ait_impl(static_cast<xmlNodePtr>(node), static_cast<xmlAttrPtr>(prop));
}
//####################################################################
xml::attributes::iterator::iterator (const char *name, const char *value, bool) {
    pimpl_ = new ait_impl(name, value, true);
}
//####################################################################
xml::attributes::iterator::iterator (const iterator &other) {
    pimpl_ = new ait_impl(*other.pimpl_);
}
//####################################################################
xml::attributes::iterator& xml::attributes::iterator::operator= (const iterator &other) {
    iterator tmp(other);
    swap(tmp);
    return *this;
}
//####################################################################
void xml::attributes::iterator::swap (iterator &other) {
    std::swap(pimpl_, other.pimpl_);
}
//####################################################################
xml::attributes::iterator::~iterator (void) {
    delete pimpl_;
}
//####################################################################
void* xml::attributes::iterator::get_raw_attr (void) {
    return pimpl_->get_raw_attr();
}
//####################################################################
xml::attributes::iterator::reference xml::attributes::iterator::operator* (void) const {
    return *(pimpl_->get());
}
//####################################################################
xml::attributes::iterator::pointer xml::attributes::iterator::operator-> (void) const {
    return pimpl_->get();
}
//####################################################################
xml::attributes::iterator& xml::attributes::iterator::operator++ (void) {
    ++(*pimpl_);
    return *this;
}
//####################################################################
xml::attributes::iterator xml::attributes::iterator::operator++ (int) {
    iterator tmp(*this);
    ++(*this);
    return tmp;
}
//####################################################################

/*
 * Member functions for the xml::attributes::const_iterator class.
 */

//####################################################################
xml::attributes::const_iterator::const_iterator (void) {
    pimpl_ = new ait_impl(0, 0);
}
//####################################################################
xml::attributes::const_iterator::const_iterator (void *node, void *prop) {
    pimpl_ = new ait_impl(static_cast<xmlNodePtr>(node), static_cast<xmlAttrPtr>(prop));
}
//####################################################################
xml::attributes::const_iterator::const_iterator (const char *name, const char *value, bool) {
    pimpl_ = new ait_impl(name, value, true);
}
//####################################################################
xml::attributes::const_iterator::const_iterator (const const_iterator &other) {
    pimpl_ = new ait_impl(*other.pimpl_);
}
//####################################################################
xml::attributes::const_iterator::const_iterator (const iterator &other) {
    pimpl_ = new ait_impl(*other.pimpl_);
}
//####################################################################
xml::attributes::const_iterator& xml::attributes::const_iterator::operator= (const const_iterator &other) {
    const_iterator tmp(other);
    swap(tmp);
    return *this;
}
//####################################################################
void xml::attributes::const_iterator::swap (const_iterator &other) {
    std::swap(pimpl_, other.pimpl_);
}
//####################################################################
xml::attributes::const_iterator::~const_iterator (void) {
    delete pimpl_;
}
//####################################################################
void* xml::attributes::const_iterator::get_raw_attr (void) {
    return pimpl_->get_raw_attr();
}
//####################################################################
xml::attributes::const_iterator::reference xml::attributes::const_iterator::operator* (void) const {
    return *(pimpl_->get());
}
//####################################################################
xml::attributes::const_iterator::pointer xml::attributes::const_iterator::operator-> (void) const {
    return pimpl_->get();
}
//####################################################################
xml::attributes::const_iterator& xml::attributes::const_iterator::operator++ (void) {
    ++(*pimpl_);
    return *this;
}
//####################################################################
xml::attributes::const_iterator xml::attributes::const_iterator::operator++ (int) {
    const_iterator tmp(*this);
    ++(*this);
    return tmp;
}
//####################################################################

/*
 * Member functions for the xml::attributes::attr class.
 */

//####################################################################
xml::attributes::attr::attr (void) : node_(0), prop_(0)
{ }
//####################################################################
xml::attributes::attr::attr (const attr &other) : node_(other.node_), prop_(other.prop_), name_(other.name_), value_(other.value_)
{ }
//####################################################################
xml::attributes::attr& xml::attributes::attr::operator= (const attr &other) {
    attr tmp(other);
    swap(tmp);
    return *this;
}
//####################################################################
void xml::attributes::attr::swap (attr &other) {
    std::swap(node_, other.node_);
    std::swap(prop_, other.prop_);
    name_.swap(other.name_);
    value_.swap(other.value_);
}
//####################################################################
void xml::attributes::attr::set_data (void *node, void *prop) {
    node_ = node;
    prop_ = prop;
    name_.erase();
    value_.erase();
}
//####################################################################
void xml::attributes::attr::set_data (const char *name, const char *value, bool) {
    node_ = 0;
    prop_ = 0;
    name_ = name;
    value_ = value;
}
//####################################################################
const char* xml::attributes::attr::get_name (void) const {
    if (!name_.empty()) return name_.c_str(); // we were given a name not a node
    if (!node_ || !prop_) throw xml::exception("access to invalid xml::attributes::attr object!");
    return reinterpret_cast<const char*>(static_cast<xmlAttrPtr>(prop_)->name);
}
//####################################################################
const char* xml::attributes::attr::get_value (void) const {
    if (!value_.empty()) return value_.c_str(); // we were given a value, not a node
    if (!node_ || !prop_) throw xml::exception("access to invalid xml::attributes::attr object!");

    xmlChar *tmpstr = xmlNodeListGetString(reinterpret_cast<xmlNodePtr>(node_)->doc, reinterpret_cast<xmlAttrPtr>(prop_)->children, 1);
    if (tmpstr == 0) return "";

    xmlchar_helper helper(tmpstr);
    value_.assign(helper.get());
    return value_.c_str();
}
//####################################################################
xml::ns xml::attributes::attr::get_namespace (xml::ns::ns_safety_type type) const {
    if (!node_ || !prop_) throw xml::exception("access to invalid xml::attributes::attr object!");
    xmlNs *     nspace(reinterpret_cast<xmlAttrPtr>(prop_)->ns);
    if (type == xml::ns::type_safe_ns) {
        return nspace
            ? xml::ns(reinterpret_cast<const char*>(nspace->prefix), reinterpret_cast<const char*>(nspace->href))
            : xml::ns(xml::ns::type_void);
    }
    // unsafe namespace
    return xml::attributes::createUnsafeNamespace(nspace);
}
//####################################################################
void xml::attributes::attr::erase_namespace (void) {
    if (!node_ || !prop_) throw xml::exception("access to invalid xml::attributes::attr object!");
    reinterpret_cast<xmlAttrPtr>(prop_)->ns = NULL;
}
//####################################################################
xml::ns xml::attributes::attr::set_namespace (const char *prefix) {
    if (!node_ || !prop_) throw xml::exception("access to invalid xml::attributes::attr object!");
    if (!prefix || prefix[0] == '\0') {
        erase_namespace();
        return xml::attributes::createUnsafeNamespace(NULL);
    }
    xmlNs *  definition(xmlSearchNs(NULL, reinterpret_cast<xmlNode*>(node_),
                                          reinterpret_cast<const xmlChar*>(prefix)));
    if (!definition) throw xml::exception("Namespace definition is not found");
    reinterpret_cast<xmlAttrPtr>(prop_)->ns = definition;
    return xml::attributes::createUnsafeNamespace(definition);
}
//####################################################################
xml::ns xml::attributes::attr::set_namespace (const xml::ns &name_space) {
    if (!node_ || !prop_) throw xml::exception("access to invalid xml::attributes::attr object!");
    if (name_space.is_void()) {
        erase_namespace();
        return xml::attributes::createUnsafeNamespace(NULL);
    }
    if (!name_space.is_safe()) {
        reinterpret_cast<xmlAttrPtr>(prop_)->ns = reinterpret_cast<xmlNs*>(xml::attributes::getUnsafeNamespacePointer(name_space));
    }
    else {
        const char *    prefix(name_space.get_prefix());
        if (prefix[0] == '\0') {
            erase_namespace();
            return xml::attributes::createUnsafeNamespace(NULL);
        }
        xmlNs *  definition(xmlSearchNs(NULL, reinterpret_cast<xmlNode*>(node_),
                                              reinterpret_cast<const xmlChar*>(prefix)));
        if (!definition) throw xml::exception("Namespace definition is not found");
        if (!xmlStrEqual(definition->href, reinterpret_cast<const xmlChar*>(name_space.get_uri()))) throw xml::exception("Namespace definition URI differs to the given");
        reinterpret_cast<xmlAttrPtr>(prop_)->ns = definition;
    }
    return xml::attributes::createUnsafeNamespace(reinterpret_cast<xmlAttrPtr>(prop_)->ns);
}
//####################################################################

/*
 * Now some friend functions
 */

//####################################################################
namespace xml {

namespace impl {
    //####################################################################
    xmlAttrPtr find_prop (xmlNodePtr xmlnode, const char *name, const ns *nspace) {
        xmlAttrPtr prop = xmlnode->properties;

        for (; prop!=0; prop = prop->next) {
            if (xmlStrEqual(prop->name, reinterpret_cast<const xmlChar*>(name))) {
                if (ns_util::attr_ns_match(prop, nspace))
                    return prop;
            }
        }

        return 0;
    }
    //####################################################################
    xmlAttributePtr find_default_prop (xmlNodePtr xmlnode, const char *name, const ns *nspace) {
        if (xmlnode->doc != 0) {
            xmlAttributePtr dtd_attr = 0;
            const xmlChar* prefix = 0;

            if (nspace && strlen(nspace->get_prefix()) > 0)
                prefix = reinterpret_cast<const xmlChar*>(nspace->get_prefix());

            if (xmlnode->doc->intSubset != 0) {
                if (nspace)
                    dtd_attr = xmlGetDtdQAttrDesc(xmlnode->doc->intSubset,
                                                  xmlnode->name,
                                                  reinterpret_cast<const xmlChar*>(name),
                                                  prefix);
                else
                    dtd_attr = xmlGetDtdAttrDesc(xmlnode->doc->intSubset,
                                                 xmlnode->name,
                                                 reinterpret_cast<const xmlChar*>(name));
            }

            if (dtd_attr == 0 && xmlnode->doc->extSubset != 0) {
                if (nspace)
                    dtd_attr = xmlGetDtdQAttrDesc(xmlnode->doc->extSubset,
                                                  xmlnode->name,
                                                  reinterpret_cast<const xmlChar*>(name),
                                                  prefix);
                else
                    dtd_attr = xmlGetDtdAttrDesc(xmlnode->doc->extSubset,
                                                 xmlnode->name,
                                                 reinterpret_cast<const xmlChar*>(name));
            }

            if (dtd_attr != 0 && dtd_attr->defaultValue != 0) {
                return dtd_attr;
            }
        }

        return 0;
    }
}

    //####################################################################
    bool operator== (const attributes::iterator &lhs, const attributes::iterator &rhs) {
	return *(lhs.pimpl_) == *(rhs.pimpl_);
    }
    //####################################################################
    bool operator!= (const attributes::iterator &lhs, const attributes::iterator &rhs) {
	return !(lhs == rhs);
    }
    //####################################################################
    bool operator== (const attributes::const_iterator &lhs, const attributes::const_iterator &rhs) {
	return *(lhs.pimpl_) == *(rhs.pimpl_);
    }
    //####################################################################
    bool operator!= (const attributes::const_iterator &lhs, const attributes::const_iterator &rhs) {
	return !(lhs == rhs);
    }
    //####################################################################
namespace impl {
    bool operator== (const ait_impl &lhs, const ait_impl &rhs) {
	if (lhs.fake_ || rhs.fake_) return false;
	return lhs.xmlattr_ == rhs.xmlattr_;
    }
    //####################################################################
    bool operator!= (const ait_impl &lhs, const ait_impl &rhs) {
	return !(lhs == rhs);
    }
}
    //####################################################################
}
//####################################################################
