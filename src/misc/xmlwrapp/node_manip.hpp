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
 * This file contains the definition of the xml::node manipulation
 * functions.
**/

#ifndef _xmlwrapp_node_manip_h_
#define _xmlwrapp_node_manip_h_

// libxml includes
#include <libxml/tree.h>

namespace xml {

namespace impl {

    //####################################################################
    /** 
     * Insert a node somewhere in the child list of a parent node.
     *
     * @param parent The parent who's child list will be inserted into.
     * @param before Insert to_add before this node, or, if this node is 0 (null), insert at the end of the child list.
     * @param to_add The node to be copied and then inserted into the child list.
     * @return The new node that was inserted into the child list.
     * @author Peter Jones
    **/
    //####################################################################
    xmlNodePtr node_insert (xmlNodePtr parent, xmlNodePtr before, xmlNodePtr to_add);

    //####################################################################
    /** 
     * Replace a node with another one. The node being replaced will be
     * freed from memory.
     *
     * @param old_node The old node to remove and free.
     * @param new_node The new node to copy and insert where old node was.
     * @return The new node that was crated from copying new_node and inserted into the child list where old_node was.
     * @author Peter Jones
    **/
    //####################################################################
    xmlNodePtr node_replace (xmlNodePtr old_node, xmlNodePtr new_node);

    //####################################################################
    /** 
     * Erase a node from the child list, and then free it from memory.
     *
     * @param to_erase The node to remove and free.
     * @return The node that was after to_erase (may be 0 (null) if to_erase was the last node in the list)
     * @author Peter Jones
    **/
    //####################################################################
    xmlNodePtr node_erase (xmlNodePtr to_erase);

    //####################################################################
    /** 
     * Set the node and its children default namespace to the given. It is
     * necessary to do when a node is inserted into a document which has
     * a default namespace declared.
     *
     * @param node The node to start from.
     * @param default_ns The pointer to the document default namespace
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    void set_children_default_ns (xmlNodePtr node, xmlNsPtr default_ns);

    //####################################################################
    /** 
     * Check if the node holds default namespace definition
     *
     * @param node The node to be checked.
     * @return true if the node holds default namespace definition.
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    bool has_default_ns_definition (xmlNodePtr node);

    //####################################################################
    /** 
     * Check if the node, attributes and children use the namespace
     *
     * @param node The node to be checked.
     * @param ns The namespace to match (pointer comparison is used)
     * @return true if the namespace is used
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    bool is_ns_used (xmlNodePtr node, xmlNsPtr ns);

    //####################################################################
    /**
     * Replaces the node and its children default namespace with the given.
     * It is required when a default namespace definition is added to the node.
     *
     * @param node The node to start from.
     * @param newns The namespace to be set
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    void update_children_default_ns (xmlNodePtr node, xmlNsPtr newns);

    //####################################################################
    /**
     * Erases namespace definition in the node. libxml2 namespace is freed.
     * The function does not check whether the namespace is used.
     *
     * @param node The node to delete from.
     * @param definition The namespace to be erased
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    void erase_ns_definition (xmlNodePtr node, xmlNsPtr definition);

    //####################################################################
    /**
     * Searches for a namspace definition in the given node
     *
     * @param node The node to search in.
     * @param prefix The namespace definition prefix
     * @return pointer to the namespace definition or NULL if not found
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    xmlNsPtr lookup_ns_definition (xmlNodePtr node, const char *prefix);

    //####################################################################
    /**
     * Searches for a default namspace definition in the given node and above
     *
     * @param node The node to start the search from.
     * @return pointer to the namespace definition or NULL if not found
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    xmlNsPtr lookup_default_ns_above (xmlNodePtr node);

    //####################################################################
    /**
     * Replaces old namspace with a new one in nodes and attributes all
     * the way down in the hierarchy
     *
     * @param node The node to start with.
     * @param oldNs The old namespace
     * @param newNs The new namespace
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    void replace_ns (xmlNodePtr node, xmlNsPtr oldNs, xmlNsPtr newNs);
}

}
#endif
