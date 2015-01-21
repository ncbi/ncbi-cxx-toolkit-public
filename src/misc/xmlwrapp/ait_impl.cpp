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
#include "deref_impl.hpp"
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


const char*     kDerefError = "dereferencing non initialised or out of range iterator";
const char*     kRefError   = "referencing non initialised or out of range iterator";
const char*     kAdvError   = "advancing non initialised or out of range iterator";
const char*     kInvalidDefaultIterError = "invalid default attribute iterator";


/*
 * First we have the ait_impl class.
 */

//####################################################################
ait_impl::ait_impl (xmlNodePtr node, xmlAttrPtr prop, bool from_find) :
    from_find_(from_find) {
    attr_.set_data(node, prop, false);
}
//####################################################################
ait_impl::ait_impl (xmlNodePtr node, phantom_attr* prop, bool from_find) :
    from_find_(from_find) {
    attr_.set_data(node, prop, true);
}
//####################################################################
ait_impl::ait_impl (const ait_impl &other) :
    attr_(other.attr_),
    from_find_(other.from_find_)
{}
//####################################################################
ait_impl& ait_impl::operator= (const ait_impl &other) {
    ait_impl tmp(other);

    attr_.swap(tmp.attr_);
    std::swap(from_find_, tmp.from_find_);
    return *this;
}
//####################################################################
xml::attributes::attr* ait_impl::get (void) {
    return &attr_;
}
//####################################################################
ait_impl& ait_impl::operator++ (void) {
    if (from_find_)
        throw xml::exception("cannot iterate using iterators "
                             "produced by find(...) methods");

    // Here: the iterator cannot point to a default attribute because
    //       it is not produced by find(...) methods
    if (attr_.prop_)
        attr_.prop_ = static_cast<xmlAttrPtr>(attr_.prop_)->next;
    else
        throw xml::exception(kAdvError);
    return *this;
}
//####################################################################
ait_impl ait_impl::operator++ (int) {
    ait_impl tmp(*this);
    ++(*this);
    return tmp;
}
//####################################################################

/*
 * Member functions for the xml::attributes::iterator class.
 */

//####################################################################
xml::attributes::iterator::iterator (void) {
    pimpl_ = new ait_impl(0, static_cast<xmlAttrPtr>(0), false);
}
//####################################################################
xml::attributes::iterator::iterator (void *node, void *prop,
                                     bool def_prop, bool from_find) {
    if (def_prop)
        pimpl_ = new ait_impl(static_cast<xmlNodePtr>(node),
                              static_cast<phantom_attr*>(prop), from_find);
    else
        pimpl_ = new ait_impl(static_cast<xmlNodePtr>(node),
                              static_cast<xmlAttrPtr>(prop), from_find);
}
//####################################################################
xml::attributes::iterator::iterator (const iterator &other) {
    pimpl_ = new ait_impl(*other.pimpl_);
}
//####################################################################
xml::attributes::iterator&
xml::attributes::iterator::operator= (const iterator &other) {
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
xml::attributes::iterator::reference
xml::attributes::iterator::operator* (void) const {
    xml::attributes::attr*  att = pimpl_->get();
    if (att->normalize())
        return * static_cast<xml::attributes::attr*>
                            (get_ptr_to_attr_instance(att));
    throw xml::exception(kDerefError);
}
//####################################################################
xml::attributes::iterator::pointer
xml::attributes::iterator::operator-> (void) const {
    xml::attributes::attr*  att = pimpl_->get();
    if (att->normalize())
        return static_cast<xml::attributes::attr*>
                            (get_ptr_to_attr_instance(att));
    throw xml::exception(kRefError);
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
    pimpl_ = new ait_impl(0, static_cast<xmlAttrPtr>(0), false);
}
//####################################################################
xml::attributes::const_iterator::const_iterator (void *node, void *prop,
                                                 bool def_prop,
                                                 bool from_find) {
    if (def_prop)
        pimpl_ = new ait_impl(static_cast<xmlNodePtr>(node),
                              static_cast<phantom_attr*>(prop), from_find);
    else
        pimpl_ = new ait_impl(static_cast<xmlNodePtr>(node),
                              static_cast<xmlAttrPtr>(prop), from_find);
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
xml::attributes::const_iterator&
xml::attributes::const_iterator::operator= (const const_iterator &other) {
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
xml::attributes::const_iterator::reference
xml::attributes::const_iterator::operator* (void) const {
    xml::attributes::attr*  att = pimpl_->get();
    if (att->normalize())
        return * static_cast<xml::attributes::attr*>
                            (get_ptr_to_attr_instance(att));
    throw xml::exception(kDerefError);
}
//####################################################################
xml::attributes::const_iterator::pointer
xml::attributes::const_iterator::operator-> (void) const {
    xml::attributes::attr*  att = pimpl_->get();
    if (att->normalize())
        return static_cast<xml::attributes::attr*>
                            (get_ptr_to_attr_instance(att));
    throw xml::exception(kRefError);
}
//####################################################################
xml::attributes::const_iterator&
xml::attributes::const_iterator::operator++ (void) {
    ++(*pimpl_);
    return *this;
}
//####################################################################
xml::attributes::const_iterator
xml::attributes::const_iterator::operator++ (int) {
    const_iterator tmp(*this);
    ++(*this);
    return tmp;
}
//####################################################################

/*
 * Member functions for the xml::attributes::attr class.
 */

//####################################################################
xml::attributes::attr::attr (void) : xmlnode_(0), prop_(0), phantom_prop_(0)
{}
//####################################################################
xml::attributes::attr::attr (const attr &other) :
    xmlnode_(other.xmlnode_), prop_(other.prop_),
    phantom_prop_(other.phantom_prop_), value_(other.value_)
{}
//####################################################################
xml::attributes::attr& xml::attributes::attr::operator= (const attr &other) {
    attr tmp(other);
    swap(tmp);
    return *this;
}
//####################################################################
void xml::attributes::attr::swap (attr &other) {
    std::swap(xmlnode_, other.xmlnode_);
    std::swap(prop_, other.prop_);
    std::swap(phantom_prop_, other.phantom_prop_);
    value_.swap(other.value_);
}
//####################################################################
void xml::attributes::attr::set_data (void *node, void *prop, bool def_prop) {
    xmlnode_ = node;
    value_.erase();
    if (def_prop) {
        prop_ = 0;
        phantom_prop_ = prop;
    }
    else {
        prop_ = prop;
        phantom_prop_ = 0;
    }
}
//####################################################################
void * xml::attributes::attr::normalize (void) const {
    if (!xmlnode_)  return NULL;
    if (prop_)      return prop_;

    if (phantom_prop_) {
        if (static_cast<phantom_attr*>(phantom_prop_)->prop_)
            return static_cast<phantom_attr*>(phantom_prop_)->prop_;
        if (static_cast<phantom_attr*>(phantom_prop_)->def_prop_)
            return static_cast<phantom_attr*>(phantom_prop_)->def_prop_;
    }
    return NULL;
}
//####################################################################
void * xml::attributes::attr::get_node(void) const {
    return xmlnode_;
}
//####################################################################
bool xml::attributes::attr::operator==
(const xml::attributes::attr &  other) const {
    return normalize() == other.normalize();
}
//####################################################################
bool xml::attributes::attr::is_default (void) const {
    if (phantom_prop_ && (!static_cast<phantom_attr*>(phantom_prop_)->prop_))
        return true;
    return false;
}
//####################################################################
const char* xml::attributes::attr::get_name (void) const {
    if (is_default()) {
        xmlAttributePtr  dprop = static_cast<phantom_attr*>(phantom_prop_)->def_prop_;
        if (!dprop)
            throw xml::exception(kInvalidDefaultIterError);
        return reinterpret_cast<const char*>(dprop->name);
    }
    if (prop_) {
        const xmlChar *  name = static_cast<xmlAttrPtr>(prop_)->name;
        return reinterpret_cast<const char*>(name);
    }
    const xmlChar *  name = static_cast<phantom_attr*>(phantom_prop_)->prop_->name;
    return reinterpret_cast<const char*>(name);
}
//####################################################################
const char* xml::attributes::attr::get_value (void) const {
    if (is_default()) {
        xmlAttributePtr  dprop = static_cast<phantom_attr*>(phantom_prop_)->def_prop_;
        if (!dprop)
            throw xml::exception(kInvalidDefaultIterError);
        if (dprop->defaultValue)
            return reinterpret_cast<const char*>(dprop->defaultValue);
        return "";
    }

    xmlChar *tmpstr;
    if (prop_) {
        tmpstr = xmlNodeListGetString(reinterpret_cast<xmlNodePtr>(xmlnode_)->doc,
                                      reinterpret_cast<xmlAttrPtr>(prop_)->children,
                                      1);
    }
    else {
        tmpstr = xmlNodeListGetString(reinterpret_cast<xmlNodePtr>(xmlnode_)->doc,
                                      reinterpret_cast<phantom_attr*>(phantom_prop_)->prop_->children,
                                      1);
    }
    if (tmpstr == 0)
        return "";

    xmlchar_helper helper(tmpstr);
    value_.assign(reinterpret_cast<const char*>(tmpstr));
    return value_.c_str();
}
//####################################################################
xml::ns
xml::attributes::attr::get_namespace (xml::ns::ns_safety_type type) const {
    xmlNs *     definition;
    if (is_default()) {
        definition = static_cast<xmlNs*>(resolve_default_attr_ns());
    }
    else {
        xmlAttrPtr  prop = reinterpret_cast<xmlAttrPtr>(prop_);
        if (!prop_)
            prop = reinterpret_cast<phantom_attr*>(phantom_prop_)->prop_;
        definition = prop->ns;
    }

    if (type == xml::ns::type_safe_ns) {
        return definition
            ? xml::ns(reinterpret_cast<const char*>(definition->prefix),
                      reinterpret_cast<const char*>(definition->href))
            : xml::ns(xml::ns::type_void);
    }
    // unsafe namespace
    return xml::attributes::createUnsafeNamespace(definition);
}
//####################################################################
// Resolves a namespace for a default attribute.
// If it cannot resolve it then an exception is thrown.
// NULL is returned if no namespace should be used.
void *xml::attributes::attr::resolve_default_attr_ns (void) const {
    if (!is_default())
        throw xml::exception("internal library error: "
                             "resolving non-default attribute namespace");

    xmlAttributePtr  dprop = static_cast<phantom_attr*>(phantom_prop_)->def_prop_;
    if (!dprop)
        throw xml::exception(kInvalidDefaultIterError);
    xmlNs *  definition(xmlSearchNs(NULL, reinterpret_cast<xmlNode*>(xmlnode_),
                                          dprop->prefix));
    if (dprop->prefix)
        if (!definition)
            throw xml::exception("cannot resolve default attribute namespace");
    return definition;
}
//####################################################################
// This converts a default attribute to a regular one
void xml::attributes::attr::convert (void) {
    if (!is_default())
        return;

    xmlNs * definition = static_cast<xmlNs*>(resolve_default_attr_ns());
    xmlAttributePtr  dprop = static_cast<phantom_attr*>(phantom_prop_)->def_prop_;
    if (!dprop)
        throw xml::exception(kInvalidDefaultIterError);
    xmlAttrPtr new_attr = xmlSetNsProp(reinterpret_cast<xmlNode*>(xmlnode_),
                                       definition,
                                       dprop->name,
                                       dprop->defaultValue);
    if (!new_attr)
        throw xml::exception("cannot convert default attribute into a regular one");

    // It's not a default any more
    static_cast<phantom_attr*>(phantom_prop_)->def_prop_ = NULL;
    static_cast<phantom_attr*>(phantom_prop_)->prop_ = new_attr;
    return;
}
//####################################################################
void xml::attributes::attr::set_value (const char*  value) {
    convert();
    xmlAttrPtr prop = reinterpret_cast<xmlAttrPtr>(normalize());
    xmlSetNsProp(reinterpret_cast<xmlNode*>(xmlnode_),
                 prop->ns,
                 prop->name,
                 reinterpret_cast<const xmlChar*>(value));
    return;
}
//####################################################################
void xml::attributes::attr::erase_namespace (void) {
    convert();
    xmlAttrPtr prop = reinterpret_cast<xmlAttrPtr>(normalize());
    prop->ns = NULL;
}
//####################################################################
xml::ns xml::attributes::attr::set_namespace (const char *prefix) {
    if (!prefix || prefix[0] == '\0') {
        erase_namespace();
        return xml::attributes::createUnsafeNamespace(NULL);
    }

    convert();
    xmlAttrPtr prop = reinterpret_cast<xmlAttrPtr>(normalize());
    xmlNs *  definition(xmlSearchNs(NULL, reinterpret_cast<xmlNode*>(xmlnode_),
                                          reinterpret_cast<const xmlChar*>(prefix)));
    if (!definition)
        throw xml::exception("Namespace definition is not found");
    prop->ns = definition;
    return xml::attributes::createUnsafeNamespace(definition);
}
//####################################################################
xml::ns xml::attributes::attr::set_namespace (const xml::ns &name_space) {
    if (name_space.is_void()) {
        erase_namespace();
        return xml::attributes::createUnsafeNamespace(NULL);
    }

    convert();
    xmlAttrPtr prop = reinterpret_cast<xmlAttrPtr>(normalize());

    if (!name_space.is_safe()) {
        prop->ns = reinterpret_cast<xmlNs*>(getUnsafeNamespacePointer(name_space));
    }
    else {
        const char *    prefix(name_space.get_prefix());
        if (prefix[0] == '\0')
            prefix = NULL;
        xmlNs *  definition(xmlSearchNs(NULL, reinterpret_cast<xmlNode*>(xmlnode_),
                                              reinterpret_cast<const xmlChar*>(prefix)));
        if (!definition)
            throw xml::exception("Namespace definition is not found");
        if (!xmlStrEqual(definition->href, reinterpret_cast<const xmlChar*>(name_space.get_uri())))
            throw xml::exception("Namespace definition URI differs to the given");
        prop->ns = definition;
    }
    return createUnsafeNamespace(prop->ns);
}
//####################################################################

/*
 * Now some friend functions
 */

//####################################################################
namespace xml {

namespace impl {
    //####################################################################
    xmlAttrPtr find_prop (xmlNodePtr xmlnode,
                          const char *name, const ns *nspace) {

        // The similar check is in libxml2 static function
        // xmlGetPropNodeInternal(...)
        if ( (xmlnode == NULL) ||
             (xmlnode->type != XML_ELEMENT_NODE) ||
             (name == NULL) )
            return NULL;

        const ns *      ns_to_match = nspace;
        const char *    name_to_match = name;

        // Check first if the name is qualified
        const char *  column = strchr(name, ':');

        if (column) {
            if (nspace)
                return NULL;    // Both namespace and the name is qualified

            if (column == name)
                return NULL;    // The name starts with :

            if (*(column + 1) == '\0')
                return NULL;    // No attribute name is given

            std::string prefix(name, column - name);
            xmlNsPtr  resolved_ns = xmlSearchNs(xmlnode->doc,
                                                xmlnode,
                                                reinterpret_cast<const xmlChar*>(prefix.c_str()));
            if (!resolved_ns)
                return NULL;    // No such namespace found

            name_to_match = column + 1;
            ns_to_match = new ns(reinterpret_cast<const char *>(resolved_ns->prefix),
                                 reinterpret_cast<const char *>(resolved_ns->href));
        }

        xmlAttrPtr prop = xmlnode->properties;
        for (; prop != NULL; prop = prop->next) {
            if (xmlStrEqual(prop->name,
                            reinterpret_cast<const xmlChar*>(name_to_match))) {
                if (ns_util::attr_ns_match(prop, ns_to_match)) {
                    if (ns_to_match != nspace)
                        delete ns_to_match;
                    return prop;
                }
            }
        }

        if (ns_to_match != nspace)
            delete ns_to_match;
        return NULL;
    }
    //####################################################################
    phantom_attr* find_default_prop (xmlNodePtr xmlnode,
                                     const char *name, const ns *nspace) {
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

                node_private_data *  node_data = attach_node_private_data(xmlnode);


                // Found, now check the phantom attributes list attached to the
                // node
                phantom_attr *  current = node_data->phantom_attrs_;
                while (current != NULL) {
                    if (current->def_prop_ == dtd_attr)
                        return current;
                    current = current->next;
                }

                // Not found. Create a new phantom_attr structure
                phantom_attr *  new_phantom = new phantom_attr;
                memset( new_phantom, 0, sizeof( phantom_attr ) );
                new_phantom->def_prop_ = dtd_attr;

                current = node_data->phantom_attrs_;
                new_phantom->next = current;
                node_data->phantom_attrs_ = new_phantom;
                return new_phantom;
            }
        }
        return NULL;
    }
}

    //####################################################################
    bool operator== (const attributes::iterator &lhs,
                     const attributes::iterator &rhs) {
        return *(lhs.pimpl_) == *(rhs.pimpl_);
    }
    //####################################################################
    bool operator!= (const attributes::iterator &lhs,
                     const attributes::iterator &rhs) {
        return !(lhs == rhs);
    }
    //####################################################################
    bool operator== (const attributes::const_iterator &lhs,
                     const attributes::const_iterator &rhs) {
        return *(lhs.pimpl_) == *(rhs.pimpl_);
    }
    //####################################################################
    bool operator!= (const attributes::const_iterator &lhs,
                     const attributes::const_iterator &rhs) {
        return !(lhs == rhs);
    }
    //####################################################################
namespace impl {
    bool operator== (const ait_impl &lhs, const ait_impl &rhs) {
        return ((lhs.attr_.xmlnode_ == rhs.attr_.xmlnode_) &&
                (lhs.attr_.normalize() == rhs.attr_.normalize()));
    }
    //####################################################################
    bool operator!= (const ait_impl &lhs, const ait_impl &rhs) {
        return !(lhs == rhs);
    }
}
    //####################################################################
}
//####################################################################
