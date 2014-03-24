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
 * Most importantly, it adds support for XML namespaces (see "namespace.hpp").
 */

/** @file
 * This file contains the implementation of the xml::node class.
**/

// xmlwrapp includes
#include <misc/xmlwrapp/node.hpp>
#include <misc/xmlwrapp/attributes.hpp>
#include <misc/xmlwrapp/node_set.hpp>
#include <misc/xmlwrapp/exception.hpp>
#include "utility.hpp"
#include "ait_impl.hpp"
#include "node_manip.hpp"
#include "pimpl_base.hpp"
#include "node_iterator.hpp"

// standard includes
#include <cstring>
#include <new>
#include <memory>
#include <string>
#include <vector>
#include <iostream>
#include <iterator>
#include <algorithm>
#include <stdexcept>
#include <functional>

// libxml includes
#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/xmlsave.h>

// Workshop compiler define
#include <ncbiconf.h>

using namespace xml;
using namespace xml::impl;

//####################################################################
struct xml::impl::node_impl : public pimpl_base<xml::impl::node_impl> {
    //####################################################################
    node_impl (void) : xmlnode_(0), owner_(true), attrs_(0)
    { }
    //####################################################################
    ~node_impl (void)
    { release(); }
    //####################################################################
    void release (void) {
        if (xmlnode_ && owner_) {
            xmlFreeNode(xmlnode_);
        }
    }
    //####################################################################

    xmlNodePtr xmlnode_;
    bool owner_;
    attributes attrs_;
    std::string tmp_string;
};
//####################################################################
struct xml::impl::node_cmp : public std::binary_function<xmlNodePtr, xmlNodePtr, bool> {
    //####################################################################
    node_cmp (cbfo_node_compare &cb) : cb_(cb) { }
    //####################################################################
    bool operator() (xmlNodePtr lhs, xmlNodePtr rhs) {
        xml::node l_node, r_node;
        l_node.set_node_data(lhs);
        r_node.set_node_data(rhs);

        return cb_(l_node, r_node);
    }
    //####################################################################

    cbfo_node_compare &cb_;
};
//####################################################################
namespace {
    // help turn a node into a document
    class node2doc {
    public:
        node2doc (xmlNodePtr xmlnode) : xmlnode_(xmlnode), prev_(0), next_(0) {
            xmldoc_ = xmlNewDoc(0);
            if (!xmldoc_) throw std::bad_alloc();

            xmldoc_->children   = xmlnode_;
            xmldoc_->last       = xmlnode_;

            std::swap(prev_, xmlnode_->prev);
            std::swap(next_, xmlnode_->next);
        }

        ~node2doc (void) {
            xmldoc_->children   = 0;
            xmldoc_->last       = 0;

            xmlFreeDoc(xmldoc_);

            std::swap(prev_, xmlnode_->prev);
            std::swap(next_, xmlnode_->next);
        }

        xmlDocPtr get_doc (void)
        { return xmldoc_; }
    private:
        xmlDocPtr xmldoc_;
        xmlNodePtr xmlnode_;
        xmlNodePtr prev_;
        xmlNodePtr next_;
    };

    // sort compare function to sort based on attribute
    struct compare_attr : public std::binary_function<xmlNodePtr, xmlNodePtr, bool> {
        compare_attr (const char *attr_name) : name_(attr_name) { }

        bool operator() (xmlNodePtr lhs, xmlNodePtr rhs) {
            xmlAttrPtr attr_l, attr_r;
            phantom_attr * dtd_l(0);
            phantom_attr * dtd_r(0);

            attr_l = find_prop(lhs, name_, NULL);
            if (attr_l == 0 && (dtd_l = find_default_prop(lhs, name_, NULL)) == 0) return true;

            attr_r = find_prop(rhs, name_, NULL);
            if (attr_r == 0 && (dtd_r = find_default_prop(rhs, name_, NULL)) == 0) return false;

            xmlChar *value_l, *value_r;

            if (dtd_l) value_l = const_cast<xmlChar*>(dtd_l->def_prop_->defaultValue);
            else value_l = xmlNodeListGetString(lhs->doc, attr_l->children, 1);

            if (dtd_r) value_r = const_cast<xmlChar*>(dtd_r->def_prop_->defaultValue);
            else value_r = xmlNodeListGetString(rhs->doc, attr_r->children, 1);

            int rc = xmlStrcmp(value_l, value_r);

            if (!dtd_l) xmlFree(value_l);
            if (!dtd_r) xmlFree(value_r);

            return rc < 0;
        }

        const char *name_;
    };


    // add a node as a child
    struct insert_node : public std::unary_function<xmlNodePtr, void> {
        insert_node (xmlNodePtr parent) : parent_(parent) { }
        void operator() (xmlNodePtr child) { xmlAddChild(parent_, child); }

        xmlNodePtr parent_;
    };

    // an element node finder
    xmlNodePtr find_element(const char *name, xmlNodePtr first, const ns *nspace) {
        while (first != 0) {
            if (first->type == XML_ELEMENT_NODE && xmlStrcmp(first->name, reinterpret_cast<const xmlChar*>(name)) == 0) {
                if (ns_util::node_ns_match(first, nspace))
                    return first;
            }
            first = first->next;
        }

        return 0;
    }

    xmlNodePtr find_element(xmlNodePtr first) {
        while (first != 0) {
            if (first->type == XML_ELEMENT_NODE) return first;
            first = first->next;
        }

        return 0;
    }
}
//####################################################################
xml::node::node (int) {
    pimpl_ = new node_impl;
}
//####################################################################
xml::node::node (void) {
    std::auto_ptr<node_impl> ap(pimpl_ = new node_impl);

    pimpl_->xmlnode_ = xmlNewNode(0, reinterpret_cast<const xmlChar*>("blank"));
    if (!pimpl_->xmlnode_) throw std::bad_alloc();

    ap.release();
}
//####################################################################
xml::node::node (const char *name) {
    std::auto_ptr<node_impl> ap(pimpl_ = new node_impl);

    pimpl_->xmlnode_ = xmlNewNode(0, reinterpret_cast<const xmlChar*>(name));
    if (!pimpl_->xmlnode_) throw std::bad_alloc();

    ap.release();
}
//####################################################################
xml::node::node (const char *name, const char *content) {
    std::auto_ptr<node_impl> ap(pimpl_ = new node_impl);

    pimpl_->xmlnode_ = xmlNewNode(0, reinterpret_cast<const xmlChar*>(name));
    if (!pimpl_->xmlnode_) throw std::bad_alloc();

    xmlNodePtr content_node = xmlNewText(reinterpret_cast<const xmlChar*>(content));
    if (!content_node) throw std::bad_alloc();

    if (!xmlAddChild(pimpl_->xmlnode_, content_node)) {
        xmlFreeNode(content_node);
        throw std::bad_alloc();
    }

    ap.release();
}
//####################################################################
xml::node::node (cdata cdata_info) {
    std::auto_ptr<node_impl> ap(pimpl_ = new node_impl);

    if ( (pimpl_->xmlnode_ = xmlNewCDataBlock(0, reinterpret_cast<const xmlChar*>(cdata_info.t),
                                              static_cast<int>(std::strlen(cdata_info.t)))) == 0) {
        throw std::bad_alloc();
    }

    ap.release();
}
//####################################################################
xml::node::node (comment comment_info) {
    std::auto_ptr<node_impl> ap(pimpl_ = new node_impl);

    if ( (pimpl_->xmlnode_ =  xmlNewComment(reinterpret_cast<const xmlChar*>(comment_info.t))) == 0) {
        throw std::bad_alloc();
    }

    ap.release();
}
//####################################################################
xml::node::node (pi pi_info) {
    std::auto_ptr<node_impl> ap(pimpl_ = new node_impl);

    if ( (pimpl_->xmlnode_ = xmlNewPI(reinterpret_cast<const xmlChar*>(pi_info.n), reinterpret_cast<const xmlChar*>(pi_info.c))) == 0) {
        throw std::bad_alloc();
    }

    ap.release();
}
//####################################################################
xml::node::node (text text_info) {
    std::auto_ptr<node_impl> ap(pimpl_ = new node_impl);

    if ( (pimpl_->xmlnode_ =  xmlNewText(reinterpret_cast<const xmlChar*>(text_info.t))) == 0) {
        throw std::bad_alloc();
    }

    ap.release();
}
//####################################################################
xml::node::node (const node &other) {
    std::auto_ptr<node_impl> ap(pimpl_ = new node_impl);

    pimpl_->xmlnode_ = xmlCopyNode(other.pimpl_->xmlnode_, 1);
    if (!pimpl_->xmlnode_) throw std::bad_alloc();

    ap.release();
} /* NCBI_FAKE_WARNING */
//####################################################################
xml::node& xml::node::operator= (const node &other) {
    node tmp_node(other);                   /* NCBI_FAKE_WARNING */
    swap(tmp_node);
    return *this;
}
//####################################################################
xml::node* xml::node::detached_copy (void) const {
    try {
        node* new_node = new node(*this);   /* NCBI_FAKE_WARNING */
        return new_node;
    }
    catch (std::exception & ex) {
        throw xml::exception(ex.what());
    }
}
//####################################################################
void xml::node::swap (node &other) {
    std::swap(pimpl_, other.pimpl_);
}
//####################################################################
xml::node::~node (void) {
    delete pimpl_;
}
//####################################################################
void xml::node::set_node_data (void *data) {
    pimpl_->release();
    pimpl_->xmlnode_ = static_cast<xmlNodePtr>(data);
    pimpl_->owner_ = false;
}
//####################################################################
void* xml::node::get_node_data (void) const {
    return pimpl_->xmlnode_;
}
//####################################################################
void* xml::node::release_node_data (void) {
    pimpl_->owner_ = false;
    return pimpl_->xmlnode_;
}
//####################################################################
void xml::node::set_name (const char *name) {
    xmlNodeSetName(pimpl_->xmlnode_, reinterpret_cast<const xmlChar*>(name));
}
//####################################################################
const char* xml::node::get_name (void) const {
    return reinterpret_cast<const char*>(pimpl_->xmlnode_->name);
}
//####################################################################
void xml::node::set_content (const char *content) {
    if (pimpl_->xmlnode_->type == XML_ELEMENT_NODE && content != NULL) {
        xmlChar *  encoded_content =
                    xmlEncodeSpecialChars(
                            pimpl_->xmlnode_->doc,
                            reinterpret_cast<const xmlChar*>(content));
        if (encoded_content == NULL)
            throw std::bad_alloc();
        xmlNodeSetContent(pimpl_->xmlnode_, encoded_content);
        xmlFree(encoded_content);
        return;
    }
    xmlNodeSetContent(pimpl_->xmlnode_,
                      reinterpret_cast<const xmlChar*>(content));
}
//####################################################################
void xml::node::set_raw_content (const char *raw_content) {
    xmlNodeSetContent(pimpl_->xmlnode_,
                      reinterpret_cast<const xmlChar*>(raw_content));
}
//####################################################################
const char* xml::node::get_content (void) const {
    xmlchar_helper content(xmlNodeGetContent(pimpl_->xmlnode_));
    if (!content.get()) return 0;

    pimpl_->tmp_string = content.get();
    return pimpl_->tmp_string.c_str();
}
//####################################################################
xml::node::node_type xml::node::get_type (void) const {
    switch (pimpl_->xmlnode_->type) {
        case XML_ELEMENT_NODE:              return type_element;
        case XML_TEXT_NODE:                 return type_text;
        case XML_CDATA_SECTION_NODE:        return type_cdata;
        case XML_ENTITY_REF_NODE:           return type_entity_ref;
        case XML_ENTITY_NODE:               return type_entity;
        case XML_PI_NODE:                   return type_pi;
        case XML_COMMENT_NODE:              return type_comment;
        case XML_DOCUMENT_NODE:             return type_document;
        case XML_DOCUMENT_TYPE_NODE:        return type_document_type;
        case XML_DOCUMENT_FRAG_NODE:        return type_document_frag;
        case XML_NOTATION_NODE:             return type_notation;
        case XML_DTD_NODE:                  return type_dtd;
        case XML_ELEMENT_DECL:              return type_dtd_element;
        case XML_ATTRIBUTE_DECL:            return type_dtd_attribute;
        case XML_ENTITY_DECL:               return type_dtd_entity;
        case XML_NAMESPACE_DECL:            return type_dtd_namespace;
        case XML_XINCLUDE_START:            return type_xinclude;
        case XML_XINCLUDE_END:              return type_xinclude;
        default:                            return type_element;
    }
}
//####################################################################
xml::attributes& xml::node::get_attributes (void) {
    if (pimpl_->xmlnode_->type != XML_ELEMENT_NODE) {
        throw xml::exception("get_attributes called on non-element node");
    }

    pimpl_->attrs_.set_data(pimpl_->xmlnode_);
    return pimpl_->attrs_;
}
//####################################################################
const xml::attributes& xml::node::get_attributes (void) const {
    if (pimpl_->xmlnode_->type != XML_ELEMENT_NODE) {
        throw xml::exception("get_attributes called on non-element node");
    }

    pimpl_->attrs_.set_data(pimpl_->xmlnode_);
    return pimpl_->attrs_;
}
//####################################################################
xml::attributes::iterator xml::node::find_attribute (const char* name,
                                                     const ns* nspace) {
    return get_attributes().find(name, nspace);
}
//####################################################################
xml::attributes::const_iterator xml::node::find_attribute (const char* name,
                                                           const ns* nspace) const {
    return get_attributes().find(name, nspace);
}
//####################################################################
xml::ns xml::node::get_namespace (xml::ns::ns_safety_type type) const {
    if (type == xml::ns::type_safe_ns) {
        return pimpl_->xmlnode_->ns
            ? xml::ns(reinterpret_cast<const char*>(pimpl_->xmlnode_->ns->prefix),
                      reinterpret_cast<const char*>(pimpl_->xmlnode_->ns->href))
            : xml::ns(xml::ns::type_void);
    }
    // unsafe namespace
    return xml::ns(pimpl_->xmlnode_->ns);
}
//####################################################################
xml::ns_list_type xml::node::get_namespace_definitions (xml::ns::ns_safety_type type) const {
    return get_namespace_definitions(pimpl_->xmlnode_, type);
}
//####################################################################
xml::ns_list_type xml::node::get_namespace_definitions (void* nd, xml::ns::ns_safety_type type) const {
    xml::ns_list_type      namespace_definitions;
    if (!reinterpret_cast<xmlNodePtr>(nd)->nsDef) {
        return namespace_definitions;
    }
    for (xmlNs *  ns(reinterpret_cast<xmlNodePtr>(nd)->nsDef); ns; ns = ns->next) {
        if (type == xml::ns::type_safe_ns) {
            namespace_definitions.push_back(xml::ns(reinterpret_cast<const char*>(ns->prefix),
                                                    reinterpret_cast<const char*>(ns->href)));
        }
        else {
            namespace_definitions.push_back(xml::ns(ns));
        }
    }
    return namespace_definitions;
}
//####################################################################
xml::ns xml::node::set_namespace (const char *prefix) {
    if (prefix && prefix[0] == '\0') prefix = NULL;
    xmlNs *  definition(xmlSearchNs(NULL, pimpl_->xmlnode_, reinterpret_cast<const xmlChar*>(prefix)));
    if (!definition) throw xml::exception("Namespace definition is not found");
    pimpl_->xmlnode_->ns = definition;
    return xml::ns(definition);
}
//####################################################################
xml::ns xml::node::set_namespace (const xml::ns &name_space) {
    if (name_space.is_void()) {
        erase_namespace();
        // The erase_namespace updates the node->ns pointer approprietly
        return xml::ns(pimpl_->xmlnode_->ns);
    }
    if (!name_space.is_safe()) {
        pimpl_->xmlnode_->ns = reinterpret_cast<xmlNs*>(name_space.unsafe_ns_);
    }
    else {
        const char *    prefix(name_space.get_prefix());
        if (prefix[0] == '\0') prefix = NULL;

        xmlNs *  definition(xmlSearchNs(NULL, pimpl_->xmlnode_, reinterpret_cast<const xmlChar*>(prefix)));
        if (!definition)
            throw xml::exception("Namespace definition is not found");
        if (!xmlStrEqual(definition->href, reinterpret_cast<const xmlChar*>(name_space.get_uri())))
            throw xml::exception("Namespace definition URI differs to the given");
        pimpl_->xmlnode_->ns = definition;
    }
    return xml::ns(pimpl_->xmlnode_->ns);
}
//####################################################################
xml::ns xml::node::add_namespace_definition (const xml::ns &name_space, ns_definition_adding_type type) {
    if (name_space.is_void()) throw xml::exception("void namespace cannot be added to namespace definitions");
    if (!pimpl_->xmlnode_->nsDef)   return add_namespace_def(name_space.get_uri(), name_space.get_prefix());

    // Search in the list of existed
    const char *    patternPrefix(name_space.get_prefix());
    if (patternPrefix[0] == '\0') patternPrefix = NULL;

    xmlNs *         current(pimpl_->xmlnode_->nsDef);
    while (current) {
        if (current->prefix == NULL) {
            // Check if the default namespace matched
            if (patternPrefix == NULL) return add_matched_namespace_def(current, name_space.get_uri(), type);
        }
        else {
            // Check if a non default namespace matched
            if (xmlStrEqual(reinterpret_cast<const xmlChar*>(patternPrefix), current->prefix))
                return add_matched_namespace_def(current, name_space.get_uri(), type);
        }
        current = current->next;
    }

    // Not found in the existed, so simply add the namespace
    return add_namespace_def(name_space.get_uri(), name_space.get_prefix());
}
//####################################################################
void xml::node::add_namespace_definitions (const xml::ns_list_type &name_spaces,
                                           ns_definition_adding_type type) {
    xml::ns_list_type::const_iterator  first(name_spaces.begin()), last(name_spaces.end());
    for (; first != last; ++first) { add_namespace_definition(*first, type); }
}
//####################################################################
xml::ns xml::node::add_namespace_def (const char *uri, const char *prefix) {
    if (prefix && prefix[0] == '\0') prefix = NULL;
    if (uri && uri[0] == '\0')       uri = NULL;
    xmlNs *     newNs(xmlNewNs(pimpl_->xmlnode_, reinterpret_cast<const xmlChar*>(uri),
                      reinterpret_cast<const xmlChar*>(prefix)));
    if (!newNs) throw std::bad_alloc();

    if (!prefix) {
        // The added namespace is the default. Set the default namespace to all
        // the nested nodes until another default namespace is not defined and
        // the nodes used the old default namespace
        if (!pimpl_->xmlnode_->ns || !pimpl_->xmlnode_->ns->prefix)
            pimpl_->xmlnode_->ns = newNs;
        update_children_default_ns(pimpl_->xmlnode_, newNs);
    }
    return xml::ns(newNs);
}
//####################################################################
xml::ns xml::node::add_matched_namespace_def (void *libxml2RawNamespace, const char *uri, ns_definition_adding_type type) {
    if (type == type_throw_if_exists) throw xml::exception("namespace is already defined");
    if (reinterpret_cast<xmlNs*>(libxml2RawNamespace)->href != NULL )
        xmlFree((char*)(reinterpret_cast<xmlNs*>(libxml2RawNamespace)->href));
    reinterpret_cast<xmlNs*>(libxml2RawNamespace)->href = xmlStrdup(reinterpret_cast<const xmlChar*>(uri));
    return xml::ns(libxml2RawNamespace);
}
//####################################################################
xml::ns xml::node::lookup_namespace (const char *prefix, xml::ns::ns_safety_type type) const {
    if (prefix && prefix[0] == '\0') prefix = NULL;
    xmlNs *     found(xmlSearchNs(NULL, pimpl_->xmlnode_, reinterpret_cast<const xmlChar*>(prefix)));

    if (type == xml::ns::type_safe_ns) {
        if (found) return xml::ns(reinterpret_cast<const char*>(found->prefix),
                                  reinterpret_cast<const char*>(found->href));
        return xml::ns(xml::ns::type_void);
    }
    // Unsafe namespace requested
    return xml::ns(found);
}
//####################################################################
void xml::node::erase_namespace_definition (const char *prefix,
                                            ns_definition_erase_type how) {
    if (prefix && prefix[0] == '\0')
        prefix = NULL;

    // Search for the ns definition
    xmlNs *definition(lookup_ns_definition(pimpl_->xmlnode_, prefix));
    if (!definition)
        return;    // Not found

    if (how == type_ns_def_erase_if_not_used) {
        // Here: remove if not in use
        if (is_ns_used(pimpl_->xmlnode_, definition))
            throw xml::exception( "Namespace is in use" );

        // Update the linked list pointers and free the namespace
        erase_ns_definition(pimpl_->xmlnode_, definition);
        return;
    }

    // Here: remove namespace even if in use

    // It is safe to remove the namespace definition because the places where
    // it is used are adjusted below anyway
    erase_ns_definition(pimpl_->xmlnode_, definition);

    // find a default namespace above. Could be null if no definition above
    // is found.
    xmlNs *default_namespace(lookup_default_ns_above(pimpl_->xmlnode_));

    // Replace the old ns with a new one recursively
    replace_ns(pimpl_->xmlnode_, definition, default_namespace);
    return;
}
//####################################################################
void xml::node::erase_namespace (void) {
    if (!pimpl_->xmlnode_->ns) return;
    if (!pimpl_->xmlnode_->ns->prefix) return;
    pimpl_->xmlnode_->ns = xmlSearchNs(NULL, pimpl_->xmlnode_, NULL);
}
//####################################################################
void xml::node::erase_duplicate_ns_defs (void) {
    std::deque<xml::ns_list_type> definitions_stack;
    definitions_stack.push_front(get_namespace_definitions(xml::ns::type_unsafe_ns));
    return erase_duplicate_ns_defs(pimpl_->xmlnode_, definitions_stack);
}
//####################################################################
void xml::node::erase_duplicate_ns_defs (void* nd, std::deque<ns_list_type>& defs) {
    xmlNodePtr current = reinterpret_cast<xmlNodePtr>(nd)->children;
    while (current) {
        erase_duplicate_ns_defs_single_node(current, defs);
        defs.push_front(get_namespace_definitions(current, xml::ns::type_unsafe_ns));
        erase_duplicate_ns_defs(current, defs);
        defs.pop_front();
        current = current->next;
    }
}
//####################################################################
void xml::node::erase_duplicate_ns_defs_single_node (void* nd, std::deque<ns_list_type>& defs) {
    xmlNsPtr  ns(reinterpret_cast<xmlNodePtr>(nd)->nsDef);
    while (ns) {
        xmlNsPtr  replacement(reinterpret_cast<xmlNsPtr>(find_replacement_ns_def(defs, ns)));
        if (replacement) {
            replace_ns(reinterpret_cast<xmlNodePtr>(nd), ns, replacement);
            xmlNsPtr next(ns->next);
            erase_ns_definition(reinterpret_cast<xmlNodePtr>(nd), ns);
            ns = next;
        }
        else {
            ns = ns->next;
        }
    }
}
//####################################################################
void* xml::node::find_replacement_ns_def (std::deque<ns_list_type>& defs, void* ns) {
    xmlNsPtr nspace(reinterpret_cast<xmlNsPtr>(ns));
    for (std::deque<ns_list_type>::const_iterator k(defs.begin()); k != defs.end(); ++k) {
        for (ns_list_type::const_iterator j(k->begin()); j != k->end(); ++j) {
            if (xmlStrcmp(nspace->prefix, reinterpret_cast<xmlNsPtr>(j->unsafe_ns_)->prefix) == 0) {
                if (xmlStrcmp(nspace->href, reinterpret_cast<xmlNsPtr>(j->unsafe_ns_)->href) == 0) {
                    return j->unsafe_ns_;
                }
                return NULL;
            }
        }
    }
    return NULL;
}
//####################################################################
void xml::node::erase_unused_ns_defs (void) {
    return erase_unused_ns_defs(pimpl_->xmlnode_);
}
//####################################################################
void xml::node::erase_unused_ns_defs (void* nd) {
    // Node itsef
    xmlNsPtr  ns(reinterpret_cast<xmlNodePtr>(nd)->nsDef);
    while (ns) {
        if (!is_ns_used(reinterpret_cast<xmlNodePtr>(nd), ns)) {
            xmlNsPtr next(ns->next);
            erase_ns_definition(reinterpret_cast<xmlNodePtr>(nd), ns);
            ns = next;
        }
        else {
            ns = ns->next;
        }
    }
    // Children
    xmlNodePtr current = reinterpret_cast<xmlNodePtr>(nd)->children;
    while (current) {
        erase_unused_ns_defs(current);
        current = current->next;
    }
}
//####################################################################
std::string xml::node::get_path (void) const {
    xmlChar* path(xmlGetNodePath(pimpl_->xmlnode_));
    if (path) {
        std::string node_path(reinterpret_cast<const char*>(path));
        xmlFree(path);
        return node_path;
    }
    throw xml::exception("Cannot get node path");
}
//####################################################################
bool xml::node::is_text (void) const {
    return xmlNodeIsText(pimpl_->xmlnode_) != 0;
}
//####################################################################
void xml::node::push_back (const node &child) {
    xml::impl::node_insert(pimpl_->xmlnode_, 0, child.pimpl_->xmlnode_);
}
//####################################################################
xml::node::size_type xml::node::size (void) const {
    #ifdef NCBI_COMPILER_WORKSHOP
        xml::node::size_type       dist(0);
        xml::node::const_iterator  first(begin()), last(end());
        for (; first != last; ++first) { ++dist; }
        return dist;
    #else
        return static_cast<xml::node::size_type>(std::distance(begin(), end()));
    #endif
}
//####################################################################
bool xml::node::empty (void) const {
    return pimpl_->xmlnode_->children == 0;
}
//####################################################################
xml::node::iterator xml::node::begin (void) {
    return iterator(pimpl_->xmlnode_->children);
}
//####################################################################
xml::node::const_iterator xml::node::begin (void) const {
    return const_iterator(pimpl_->xmlnode_->children);
}
//####################################################################
xml::node::iterator xml::node::self (void) {
    return iterator(pimpl_->xmlnode_);
}
//####################################################################
xml::node::const_iterator xml::node::self (void) const {
    return const_iterator(pimpl_->xmlnode_);
}
//####################################################################
bool xml::node::is_root (void) const {
    if (pimpl_->xmlnode_->parent == NULL)
        return true;
    return pimpl_->xmlnode_->parent->type == XML_DOCUMENT_NODE;
}
//####################################################################
xml::node::iterator xml::node::parent (void) {
    if (is_root())
        iterator();
    return iterator(pimpl_->xmlnode_->parent);
}
//####################################################################
xml::node::const_iterator xml::node::parent (void) const {
    if (is_root())
        const_iterator();
    return const_iterator(pimpl_->xmlnode_->parent);
}
//####################################################################
xml::node::iterator xml::node::find (const char *name,
                                     const ns *nspace) {
    xmlNodePtr found = find_element(name, pimpl_->xmlnode_->children, nspace);
    if (found) return iterator(found);
    return end();
}
//####################################################################
xml::node::const_iterator xml::node::find (const char *name,
                                           const ns *nspace) const {
    xmlNodePtr found = find_element(name, pimpl_->xmlnode_->children, nspace);
    if (found) return const_iterator(found);
    return end();
}
//####################################################################
xml::node::iterator xml::node::find (const char *name,
                                     const iterator& start,
                                     const ns *nspace) {
    xmlNodePtr n = static_cast<xmlNodePtr>(start.get_raw_node());
    if ( (n = find_element(name, n, nspace))) return iterator(n);
    return end();
}
//####################################################################
xml::node::const_iterator xml::node::find (const char *name,
                                           const const_iterator& start,
                                           const ns *nspace) const {
    xmlNodePtr n = static_cast<xmlNodePtr>(start.get_raw_node());
    if ( (n = find_element(name, n, nspace))) return const_iterator(n);
    return end();
}
//####################################################################
xml::node_set xml::node::run_xpath_query (const xml::xpath_expression& expr) {
    xmlXPathContextPtr      xpath_context(reinterpret_cast<xmlXPathContextPtr>(create_xpath_context(expr)));
    xmlXPathObjectPtr       object(reinterpret_cast<xmlXPathObjectPtr>(evaluate_xpath_expression(expr, xpath_context)));
    xmlXPathFreeContext(xpath_context);

    switch (object->type) {
        case XPATH_NODESET:
            return node_set(object);
        case XPATH_BOOLEAN:     /* These three cases are the same */
        case XPATH_NUMBER:
        case XPATH_STRING:
            return convert_to_nset(object);
        default: ;
    }
    throw xml::exception("Unsupported xpath run result type");
}
//####################################################################
const xml::node_set xml::node::run_xpath_query (const xml::xpath_expression& expr) const {
    // Create a context
    xmlXPathContextPtr      xpath_context(reinterpret_cast<xmlXPathContextPtr>(create_xpath_context(expr)));
    xmlXPathObjectPtr       object(reinterpret_cast<xmlXPathObjectPtr>(evaluate_xpath_expression(expr, xpath_context)));
    xmlXPathFreeContext(xpath_context);

    switch (object->type) {
        case XPATH_NODESET:     /* These two cases are the same */
        case XPATH_XSLT_TREE:
            return node_set(object);
        case XPATH_BOOLEAN:     /* These three cases are the same */
        case XPATH_NUMBER:
        case XPATH_STRING:
            return convert_to_nset(object);
        default: ;
    }
    throw xml::exception("Unsupported xpath run result type");
}
//####################################################################
xml::node_set xml::node::run_xpath_query (const char *  expr) {
    return run_xpath_query(xpath_expression(expr, get_effective_namespaces(true)));
}
//####################################################################
const xml::node_set xml::node::run_xpath_query (const char *  expr) const {
    return run_xpath_query(xpath_expression(expr, get_effective_namespaces(true)));
}
//####################################################################
ns_list_type xml::node::get_effective_namespaces (bool excludeDefault) const {
    if (!pimpl_->xmlnode_)
        throw xml::exception("invalid node to get effective namespaces");

    ns_list_type    nspaces;
    xmlNsPtr *      nsList = xmlGetNsList(pimpl_->xmlnode_->doc,
                                          pimpl_->xmlnode_);
    xmlNsPtr *      current = nsList;

    if (!nsList)
        return nspaces;

    while (*current != NULL) {
        if (excludeDefault) {
            if ((*current)->prefix != NULL)
                nspaces.push_back(ns(*current));
        }
        else
            nspaces.push_back(ns(*current));
        ++current;
    }
    if (nsList)
        xmlFree(nsList);
    return nspaces;
}
//####################################################################
void* xml::node::create_xpath_context (const xml::xpath_expression& expr) const {
    if (!pimpl_->xmlnode_ || !pimpl_->xmlnode_->doc) {
        throw xml::exception("cannot create xpath context (reference to document is not set)");
    }
    xmlXPathContextPtr  xpath_context(xmlXPathNewContext(pimpl_->xmlnode_->doc));
    if (!xpath_context) {
        xmlErrorPtr     last_error(xmlGetLastError());
        std::string     message("cannot create xpath context");

        if (last_error && last_error->message)
            message += " : " + std::string(last_error->message);

        throw xml::exception(message);
    }
    const ns_list_type& nspaces(expr.get_namespaces());
    for (ns_list_type::const_iterator k(nspaces.begin()); k!=nspaces.end(); ++k) {
        const char* prefix(k->get_prefix());
        if (strlen(prefix) == 0) {
            prefix = NULL;
        }
        if (xmlXPathRegisterNs(xpath_context, reinterpret_cast<const xmlChar*>(prefix),
                                              reinterpret_cast<const xmlChar*>(k->get_uri())) != 0) {
            xmlErrorPtr     last_error(xmlGetLastError());
            std::string     message("cannot create xpath context (namespace registering error)");

            if (last_error && last_error->message)
                message += " : " + std::string(last_error->message);

            xmlXPathFreeContext(xpath_context);
            throw xml::exception(message);
        }
    }
    xpath_context->node = pimpl_->xmlnode_;
    return xpath_context;
}
//####################################################################
void* xml::node::evaluate_xpath_expression (const xml::xpath_expression& expr, void* context) const {
    xmlXPathObjectPtr object(NULL);

    if (expr.get_compile_type() == xpath_expression::type_compile) {
        object = xmlXPathCompiledEval(reinterpret_cast<xmlXPathCompExprPtr>(expr.get_compiled_expression()),
                                      reinterpret_cast<xmlXPathContextPtr>(context));
    }
    else {
        object = xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>(expr.get_xpath()),
                                        reinterpret_cast<xmlXPathContextPtr>(context));
    }
    if (!object) {
        xmlErrorPtr     last_error(xmlGetLastError());
        std::string     message("error evaluating xpath expression");

        if (last_error && last_error->message)
            message += " : " + std::string(last_error->message);

        xmlXPathFreeContext(reinterpret_cast<xmlXPathContextPtr>(context));
        throw xml::exception(message);
    }
    return object;
}
//####################################################################
xml::node::iterator xml::node::insert (const node &n) {
    return iterator(xml::impl::node_insert(pimpl_->xmlnode_, 0, n.pimpl_->xmlnode_));
}
//####################################################################
xml::node::iterator xml::node::insert (const iterator& position, const node &n) {
    return iterator(xml::impl::node_insert(pimpl_->xmlnode_, static_cast<xmlNodePtr>(position.get_raw_node()), n.pimpl_->xmlnode_));
}
//####################################################################
xml::node::iterator xml::node::replace (const iterator& old_node, const node &new_node) {
    return iterator(xml::impl::node_replace(static_cast<xmlNodePtr>(old_node.get_raw_node()), new_node.pimpl_->xmlnode_));
}
//####################################################################
xml::node::iterator xml::node::erase (const iterator& to_erase) {
    return iterator(xml::impl::node_erase(static_cast<xmlNodePtr>(to_erase.get_raw_node())));
}
//####################################################################
xml::node::iterator xml::node::erase (iterator first, const iterator& last) {
    while (first != last) first = erase(first);
    return first;
}
//####################################################################
xml::node::size_type xml::node::erase (const char *name) {
    size_type removed_count(0);
    iterator to_remove(begin()), the_end(end());

    while ( (to_remove = find(name, to_remove)) != the_end) {
        ++removed_count;
        to_remove = erase(to_remove);
    }

    return removed_count;
}
//####################################################################
void xml::node::clear (void) {
    if (!pimpl_->xmlnode_->children)
        return;
    xmlFreeNodeList(pimpl_->xmlnode_->children);
    pimpl_->xmlnode_->children = NULL;
    pimpl_->xmlnode_->last = NULL;
}
//####################################################################
void xml::node::sort (const char *node_name, const char *attr_name) {
    xmlNodePtr i(pimpl_->xmlnode_->children), next(0);
    std::vector<xmlNodePtr> node_list;

    while (i!=0) {
        next = i->next;

        if (i->type == XML_ELEMENT_NODE && xmlStrcmp(i->name, reinterpret_cast<const xmlChar*>(node_name)) == 0) {
            xmlUnlinkNode(i);
            node_list.push_back(i);
        }

        i = next;
    }

    if (node_list.empty()) return;

    std::sort(node_list.begin(), node_list.end(), compare_attr(attr_name));
    std::for_each(node_list.begin(), node_list.end(), insert_node(pimpl_->xmlnode_));
}
//####################################################################
void xml::node::sort_fo (cbfo_node_compare &cb) {
    xmlNodePtr i(pimpl_->xmlnode_->children), next(0);
    std::vector<xmlNodePtr> node_list;

    while (i!=0) {
        next = i->next;

        if (i->type == XML_ELEMENT_NODE) {
            xmlUnlinkNode(i);
            node_list.push_back(i);
        }

        i = next;
    }

    if (node_list.empty()) return;

    std::sort(node_list.begin(), node_list.end(), node_cmp(cb));
    std::for_each(node_list.begin(), node_list.end(), insert_node(pimpl_->xmlnode_));
}
//####################################################################
void xml::node::node_to_string (std::string &xml,
                                save_option_flags flags) const {
    int compression_level = flags & 0xFFFF;
    node2doc n2d(pimpl_->xmlnode_);
    xmlDocPtr doc = n2d.get_doc();

    // Compression level is currently not analyzed by libxml2
    // So this might work in the future implementations but is ignored now.
    doc->compression = compression_level;

    int libxml2_options = convert_to_libxml2_save_options(flags);

    const char *  enc = NULL;
    if (pimpl_->xmlnode_->doc)
        enc = (const char *)(pimpl_->xmlnode_->doc->encoding);

    xmlSaveCtxtPtr  ctxt = xmlSaveToIO(save_to_string_cb, NULL, &xml,
                                       enc, libxml2_options);

    if (ctxt) {
        xmlSaveDoc(ctxt, doc);
        xmlSaveClose(ctxt);     // xmlSaveFlush() is called in xmlSaveClose()
    }
    return;
}
//####################################################################
node_set xml::node::convert_to_nset(void *  object_as_void) const {
    // It converts a scalar xpath object into a nodeset with 1 node in it
    xmlXPathObjectPtr   object = reinterpret_cast<xmlXPathObjectPtr>(object_as_void);
    std::string         type_name;
    std::string         content;

    switch (object->type) {
        case XPATH_BOOLEAN:
            type_name = "boolean";
            if (object->boolval == 0)   content = "false";
            else                        content = "true";
            break;
        case XPATH_NUMBER:
            type_name = "number";
            char    buffer[64];
            sprintf(buffer, "%g", object->floatval);
            content = std::string(buffer);
            break;
        case XPATH_STRING:
            type_name = "string";
            content = std::string(reinterpret_cast<const char*>(object->stringval));
            break;
        default:
            throw xml::exception("Internal logic error: unexpected xpath "
                                 "object type to be converted "
                                 "into a node set");
    }

    // Create a node
    xml::node   new_node("xpath_scalar_result", content.c_str());
    new_node.get_attributes().insert("type", type_name.c_str());

    // Add node to a node set
    xmlNodeSetPtr   new_node_set = xmlXPathNodeSetCreate(NULL);
    if (new_node_set == NULL)
        throw xml::exception("Cannot create node set while "
                             "converting xpath result");
    xmlXPathNodeSetAdd(new_node_set,
                       reinterpret_cast<xmlNodePtr>(new_node.get_node_data()));
    new_node.release_node_data();

    // Update the type
    object->type = XPATH_NODESET;
    object->nodesetval = new_node_set;
    object->boolval = 1;    // Hack: to have the contained nodes deleted as well

    return node_set(object);
}





namespace xml {
    std::ostream& operator<< (std::ostream &stream, const xml::node &n) {
        node2doc n2d(n.pimpl_->xmlnode_);
        xmlDocPtr doc = n2d.get_doc();

        int libxml2_options = convert_to_libxml2_save_options(save_op_default);

        const char *  enc = NULL;
        if (n.pimpl_->xmlnode_->doc)
            enc = (const char *)(n.pimpl_->xmlnode_->doc->encoding);

        xmlSaveCtxtPtr  ctxt = xmlSaveToIO(save_to_stream_cb, NULL, &stream,
                                           enc, libxml2_options);

        if (ctxt) {
            xmlSaveDoc(ctxt, doc);
            xmlSaveClose(ctxt);     // xmlSaveFlush() is called in xmlSaveClose()
        }
        return stream;
    }
}
//####################################################################
