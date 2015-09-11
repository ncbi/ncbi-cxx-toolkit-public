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
 * This file contains the implementation of the xml::attributes class.
**/

// xmlwrapp includes
#include <misc/xmlwrapp/attributes.hpp>
#include <misc/xmlwrapp/exception.hpp>
#include "ait_impl.hpp"
#include "pimpl_base.hpp"

// standard includes
#include <string.h>
#include <new>
#include <algorithm>
#include <string.h>

// libxml2 includes
#include <libxml/tree.h>

using namespace xml;
using namespace xml::impl;

const char *    kInsertError = "inserting attribute error";


//####################################################################
struct xml::attributes::pimpl : public pimpl_base<xml::attributes::pimpl> {
    //####################################################################
    pimpl (void) : owner_(true) {
        xmlnode_ = xmlNewNode(0, reinterpret_cast<const xmlChar*>("blank"));
        if (!xmlnode_) throw std::bad_alloc();
    }
    //####################################################################
    pimpl (xmlNodePtr node) : xmlnode_(node), owner_(false)
    {}
    //####################################################################
    pimpl (const pimpl &other) : owner_(true) {
        xmlnode_ = xmlCopyNode(other.xmlnode_, 2);
        if (!xmlnode_) throw std::bad_alloc();
    }
    //####################################################################
    ~pimpl (void)
    { release(); }
    //####################################################################
    void release (void)
    { if (owner_ && xmlnode_) xmlFreeNode(xmlnode_); }
    //####################################################################

    xmlNodePtr xmlnode_;
    bool owner_;

    private:
        pimpl &  operator=(const pimpl &);
};
//####################################################################
xml::attributes::attributes (void) {
    pimpl_ = new pimpl;
}
//####################################################################
xml::attributes::attributes (int) {
    pimpl_ = new pimpl(0);
}
//####################################################################
xml::attributes::attributes (const attributes &other) {
    pimpl_ = new pimpl(*other.pimpl_);
}
//####################################################################
xml::attributes& xml::attributes::operator= (const attributes &other) {
    attributes tmp(other);
    swap(tmp);
    return *this;
}
//####################################################################
void xml::attributes::swap (attributes &other) {
    std::swap(pimpl_, other.pimpl_);
}
//####################################################################
xml::attributes::~attributes (void) {
    delete pimpl_;
}
//####################################################################
void* xml::attributes::get_data (void) {
    return pimpl_->xmlnode_;
}
//####################################################################
xml::ns xml::attributes::createUnsafeNamespace (void *  libxml2RawNamespace) {
    return xml::ns(libxml2RawNamespace);
}
//####################################################################
void * xml::attributes::getUnsafeNamespacePointer (const xml::ns &name_space) {
    return name_space.unsafe_ns_;
}
//####################################################################
void xml::attributes::set_data (void *node) {
    xmlNodePtr x = static_cast<xmlNodePtr>(node);

    pimpl_->release();
    pimpl_->owner_ = false;
    pimpl_->xmlnode_ = x;
}
//####################################################################
xml::attributes::iterator xml::attributes::begin (void) {
    return iterator(pimpl_->xmlnode_,
                    pimpl_->xmlnode_->properties,
                    false,  // not default
                    false); // not from find
}
//####################################################################
xml::attributes::const_iterator xml::attributes::begin (void) const {
    return const_iterator(pimpl_->xmlnode_,
                          pimpl_->xmlnode_->properties,
                          false,    // not default
                          false);   // not from find
}
//####################################################################
xml::attributes::iterator xml::attributes::end (void) {
    return iterator(pimpl_->xmlnode_,
                    NULL,
                    false,
                    false);
}
//####################################################################
xml::attributes::const_iterator xml::attributes::end (void) const {
    return const_iterator(pimpl_->xmlnode_,
                          NULL,
                          false,
                          false);
}
//####################################################################
void xml::attributes::insert (const char *name, const char *value,
                              const ns *nspace ) {
    if ( (!name) || (!value))
        throw xml::exception("name and value of an attribute to "
                             "insert must not be NULL");
    if (name[0] == '\0')
        throw xml::exception("name cannot be empty");

    bool            only_whitespaces = true;
    const char *    current = name;
    while (*current != '\0') {
        if (memchr(" \t\n\r", *current, 4) == NULL) {
            only_whitespaces = false;
            break;
        }
        ++current;
    }
    if (only_whitespaces)
        throw xml::exception("name may not consist "
                             "of only whitespace characters");


    const char *  column = strchr(name, ':');

    if (!nspace) {
        // It means ithat the user does not care about namespaces.
        // So we should bother about them.
        if (column) {
            // The given name is qualified. Search for the namespace basing on
            // the prefix
            if (*(column + 1) == '\0')
                throw xml::exception("invalid attribute name");
            if (column == name)
                throw xml::exception("an attribute may not have a default namespace");

            std::string prefix(name, column - name);
            xmlNsPtr  resolved_ns = xmlSearchNs(pimpl_->xmlnode_->doc,
                                                pimpl_->xmlnode_,
                                                reinterpret_cast<const xmlChar*>(prefix.c_str()));
            if (!resolved_ns)
                throw xml::exception("cannot resolve namespace");

            if (xmlSetNsProp(pimpl_->xmlnode_,
                             resolved_ns,
                             reinterpret_cast<const xmlChar*>(column + 1),
                             reinterpret_cast<const xmlChar*>(value)) == NULL)
                throw xml::exception(kInsertError);
            return;
        }

        // The given name is not qualified.
        if (xmlSetProp(pimpl_->xmlnode_,
                       reinterpret_cast<const xmlChar*>(name),
                       reinterpret_cast<const xmlChar*>(value)) == NULL)
            throw xml::exception(kInsertError);
        return;
    }

    // Some namespace is provided. Check that the name is not qualified.
    if (column)
        throw xml::exception("cannot specify both a qualified name and a namespace");

    if (nspace->is_void()) {
        // The user wanted to insert an attribute without a namespace at all.
        if (xmlSetProp(pimpl_->xmlnode_,
                         reinterpret_cast<const xmlChar*>(name),
                         reinterpret_cast<const xmlChar*>(value)) == NULL)
            throw xml::exception(kInsertError);
        return;
    }

    if (nspace->get_prefix() == std::string(""))
        throw xml::exception("an attribute may not have a default namespace");

    if (!nspace->is_safe()) {
        if (xmlSetNsProp(pimpl_->xmlnode_,
                         reinterpret_cast<xmlNsPtr>(nspace->unsafe_ns_),
                         reinterpret_cast<const xmlChar*>(name),
                         reinterpret_cast<const xmlChar*>(value)) == NULL)
            throw xml::exception(kInsertError);
        return;
    }


    // Resolve namespace basing on uri
    xmlNsPtr  resolved_ns = xmlSearchNsByHref(pimpl_->xmlnode_->doc,
                                              pimpl_->xmlnode_,
                                              reinterpret_cast<const xmlChar*>(nspace->get_uri()));
    if (!resolved_ns)
        throw xml::exception("inserting attribute error: "
                             "cannot resolve namespace");

    if (xmlSetNsProp(pimpl_->xmlnode_,
                     resolved_ns,
                     reinterpret_cast<const xmlChar*>(name),
                     reinterpret_cast<const xmlChar*>(value)) == NULL)
        throw xml::exception(kInsertError);
    return;
}
//####################################################################
xml::attributes::iterator xml::attributes::find(const char *name,
                                                const ns *nspace) {
    xmlAttrPtr prop = find_prop(pimpl_->xmlnode_, name, nspace);
    if (prop != 0)
        return iterator(pimpl_->xmlnode_, prop, false, true);

    phantom_attr* dtd_prop = find_default_prop(pimpl_->xmlnode_, name, nspace);
    if (dtd_prop != 0)
        return iterator(pimpl_->xmlnode_, dtd_prop, true, true);

    return iterator(pimpl_->xmlnode_, NULL, false, true);
}
//####################################################################
xml::attributes::const_iterator xml::attributes::find (const char *name,
                                                       const ns *nspace) const {
    xmlAttrPtr prop = find_prop(pimpl_->xmlnode_, name, nspace);
    if (prop != 0)
        return const_iterator(pimpl_->xmlnode_, prop, false, true);

    phantom_attr * dtd_prop = find_default_prop(pimpl_->xmlnode_, name, nspace);
    if (dtd_prop != 0)
        return const_iterator(pimpl_->xmlnode_, dtd_prop, true, true);

    return const_iterator(pimpl_->xmlnode_, NULL, false, true);
}
//####################################################################
xml::attributes::iterator xml::attributes::erase (iterator to_erase) {
    // Check that the iterator is initialized and
    // that it is of the same node
    if (to_erase == iterator() ||
        to_erase.pimpl_->get()->xmlnode_ != pimpl_->xmlnode_)
        throw xml::exception("cannot erase attribute, the iterator is "
                             "not initialized or belongs to another node attributes");

    // There is nothing to erase for these two cases
    if (to_erase == end())  // It must be first because otherwise end() is referenced
        return end();
    if (to_erase->is_default())
        return end();

    // It's not default so we should delete it
    xmlAttrPtr prop_to_erase = static_cast<xmlAttrPtr>(to_erase.pimpl_->get()->prop_);
    to_erase.pimpl_->get()->prop_ = static_cast<xmlAttrPtr>(to_erase.pimpl_->get()->prop_)->next;

    xmlUnsetNsProp(pimpl_->xmlnode_, prop_to_erase->ns, prop_to_erase->name);
    return to_erase;
}
//####################################################################
xml::attributes::size_type xml::attributes::erase (const char *name, const ns *nspace) {

    if (!name)
        return 0;

    const char *  column = strchr(name, ':');
    if (!nspace) {
        if (column) {
            // name is qualified
            if (column == name)
                return 0;   // Default NS is provided
            if (*(column + 1) == '\0')
                return 0;   // No name is provided

            std::string prefix(name, column - name);
            xmlNsPtr  resolved_ns = xmlSearchNs(pimpl_->xmlnode_->doc,
                                                pimpl_->xmlnode_,
                                                reinterpret_cast<const xmlChar*>(prefix.c_str()));
            if (!resolved_ns)
                return 0;

            if (xmlUnsetNsProp(pimpl_->xmlnode_,
                               resolved_ns,
                               reinterpret_cast<const xmlChar*>(column + 1)) == 0)
                return 1;
            return 0;
        }

        // It is a non-qualified name. Search basing on names only.
        size_type  count = 0;
        xmlAttrPtr  att = find_prop(pimpl_->xmlnode_, name, NULL);
        while (att != NULL) {
            ++count;
            xmlUnlinkNode(reinterpret_cast<xmlNodePtr>(att));
            xmlFreeProp(att);
            att = find_prop(pimpl_->xmlnode_, name, NULL);
        }
        return count;
    }

    if (column)
        return 0;   // Both a namespace and a prefix in the name are provided

    if (nspace->is_void()) {
        // The attribute must not have a namespace at all
        if (xmlUnsetProp(pimpl_->xmlnode_,
                         reinterpret_cast<const xmlChar*>(name)) == 0)
            return 1;
        return 0;
    }

    if (!nspace->is_safe()) {
        if (xmlUnsetNsProp(pimpl_->xmlnode_,
                           reinterpret_cast<xmlNsPtr>(nspace->unsafe_ns_),
                           reinterpret_cast<const xmlChar*>(name)) == 0)
            return 1;
        return 0;
    }

    // Safe namespace. Resolve it basing on the uri.
    xmlNsPtr  resolved_ns = xmlSearchNsByHref(pimpl_->xmlnode_->doc,
                                              pimpl_->xmlnode_,
                                              reinterpret_cast<const xmlChar*>(nspace->get_uri()));

    if (!resolved_ns)
        return 0;   // Namespace is not resolved. Nothing will be deleted anyway.

    // There could be many attributes with different prefixes but with the same
    // URIs and names
    size_type  count = 0;
    while (xmlUnsetNsProp(pimpl_->xmlnode_,
                          resolved_ns,
                          reinterpret_cast<const xmlChar*>(name)) == 0)
        ++count;
    return count;
}
//####################################################################
bool xml::attributes::empty (void) const {
    return pimpl_->xmlnode_->properties == 0;
}
//####################################################################
xml::attributes::size_type xml::attributes::size (void) const {
    size_type count = 0;

    xmlAttrPtr prop = pimpl_->xmlnode_->properties;
    while (prop != 0) { ++count; prop = prop->next; }

    return count;
}
//####################################################################
