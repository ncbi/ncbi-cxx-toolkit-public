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
 * This file contains the implementation of the xml::node manipulation
 * functions.
**/

// xmlwrapp includes
#include <misc/xmlwrapp/exception.hpp>
#include "node_manip.hpp"

// libxml includes
#include <libxml/tree.h>

//####################################################################
xmlNodePtr xml::impl::node_insert (xmlNodePtr parent, xmlNodePtr before, xmlNodePtr to_add) {
    xmlNodePtr new_xml_node =  xmlCopyNode(to_add, 1);
    if (!new_xml_node) throw std::bad_alloc();

    if (before == 0) { // insert at the end of the child list
        if (xmlAddChild(parent, new_xml_node) == 0) {
            xmlFreeNode(new_xml_node);
            throw xml::exception("failed to insert xml::node; xmlAddChild failed");
        }
    } else {
        if (xmlAddPrevSibling(before, new_xml_node) == 0) {
            xmlFreeNode(new_xml_node);
            throw xml::exception("failed to insert xml::node; xmlAddPrevSibling failed");
        }
    }
    if (!new_xml_node->ns) new_xml_node->ns = xmlSearchNs(NULL, parent, NULL);
    if (new_xml_node->ns) set_children_default_ns(new_xml_node, new_xml_node->ns);

    return new_xml_node;
}
//####################################################################
xmlNodePtr xml::impl::node_replace (xmlNodePtr old_node, xmlNodePtr new_node) {
    xmlNodePtr copied_node =  xmlCopyNode(new_node, 1);
    if (!copied_node) throw std::bad_alloc();

    // hack to see if xmlReplaceNode was successful
    copied_node->doc = reinterpret_cast<xmlDocPtr>(old_node);
    xmlReplaceNode(old_node, copied_node);

    if (copied_node->doc == reinterpret_cast<xmlDocPtr>(old_node)) {
        xmlFreeNode(copied_node);
        throw xml::exception("failed to replace xml::node; xmlReplaceNode() failed");
    }

    xmlFreeNode(old_node);

    if (!copied_node->ns) copied_node->ns = xmlSearchNs(NULL, copied_node->parent, NULL);
    if (copied_node->ns) set_children_default_ns(copied_node, copied_node->ns);

    return copied_node;
}
//####################################################################
xmlNodePtr xml::impl::node_erase (xmlNodePtr to_erase) {
    xmlNodePtr after = to_erase->next;

    xmlUnlinkNode(to_erase);
    xmlFreeNode(to_erase);

    return after;
}
//####################################################################
void xml::impl::set_children_default_ns (xmlNodePtr node, xmlNsPtr default_ns) {
    if (!node->ns)  node->ns = default_ns;
    node = node->children;
    while (node) {
        if (!has_default_ns_definition(node)) {
            set_children_default_ns(node, default_ns);
            if (!node->ns) node->ns = default_ns;
        }
        node = node->next;
    }
}
//####################################################################
bool xml::impl::has_default_ns_definition (xmlNodePtr node) {
    if (!node ||!node->nsDef) return false;
    xmlNs *     current(node->nsDef);
    while (current) {
        if (!current->prefix) return true;
        current = current->next;
    }
    return false;
}
//####################################################################
bool xml::impl::is_ns_used (xmlNodePtr node, xmlNsPtr ns) {
    if (!node) return false;

    // Does the node itself use namespace
    if (node->ns == ns) return true;

    // Do the node attributes use namespace
    for (xmlAttrPtr current = node->properties; current; current = current->next)
        if (current->ns == ns) return true;

    node = node->children;
    while (node) {
        if (is_ns_used(node, ns)) return true;
        node = node->next;
    }
    return false;
}
//####################################################################
void xml::impl::update_children_default_ns (xmlNodePtr node, xmlNsPtr newns) {
    if (!node) return;
    node = node->children;
    while (node) {
        if (!has_default_ns_definition(node)) {
            update_children_default_ns(node, newns);
            if (!node->ns || !node->ns->prefix)
                node->ns = newns;
        }
        node = node->next;
    }
}
//####################################################################
void xml::impl::erase_ns_definition (xmlNodePtr node, xmlNsPtr definition) {
    if (!node->nsDef) return;
    if (node->nsDef != definition) {
        xmlNs *prev(node->nsDef);
        while (prev && prev->next != definition)
            prev = prev->next;
        if (!prev) return;
        prev->next = definition->next;
    }
    else {
        node->nsDef = definition->next;
    }
    xmlFreeNs(definition);
}
//####################################################################
xmlNsPtr xml::impl::lookup_ns_definition (xmlNodePtr node, const char *prefix) {
    xmlNs *current(node->nsDef);
    while (current) {
        if (!prefix && !current->prefix) return current;
        if (prefix && current->prefix) {
            if (xmlStrEqual(reinterpret_cast<const xmlChar*>(prefix), current->prefix))
                return current;
        }
        current = current->next;
    }
    return NULL;
}
//####################################################################
xmlNsPtr xml::impl::lookup_default_ns_above (xmlNodePtr node) {
    if (!node)
        return NULL;

    xmlNs *current(node->nsDef);
    while (current) {
        if (!current->prefix)
            return current;
        current = current->next;
    }
    return lookup_default_ns_above(node->parent);
}
//####################################################################
void xml::impl::replace_ns (xmlNodePtr node, xmlNsPtr oldNs, xmlNsPtr newNs) {
    if (!node) return;

    // Does the node itself use namespace
    if (node->ns == oldNs) node->ns = newNs;

    // Do the node attributes use namespace
    for (xmlAttrPtr current = node->properties; current; current = current->next)
        if (current->ns == oldNs) {
            // Attributes may not have a default namspace
            if (newNs && newNs->prefix)
                current->ns = newNs;
            else
                current->ns = NULL;
        }

    node = node->children;
    while (node) {
        replace_ns(node, oldNs, newNs);
        node = node->next;
    }
}
//####################################################################

