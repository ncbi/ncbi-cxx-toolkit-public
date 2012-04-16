/*
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
 */

/** @file
 * This file contains declarations required for iterators dereferencing
 * support.
**/



#ifndef _xmlwrapp_deref_impl_h_
#define _xmlwrapp_deref_impl_h_

// xmlwrapp includes
#include <misc/xmlwrapp/attributes.hpp>
#include <misc/xmlwrapp/namespace.hpp>
#include <misc/xmlwrapp/node.hpp>

#include "pimpl_base.hpp"

// libxml2 includes
#include <libxml/tree.h>

namespace xml
{
    namespace impl
    {

        /*
         * The struct is used to keep a list of the default attributes proxies
         * which help to track attributes conversions from default to non default ones
         */
        struct phantom_attr
        {
            xmlAttributePtr         def_prop_;  // Must always be set correspondingly
            xmlAttrPtr              prop_;      // Set if only this default attribute has been converted
            struct phantom_attr *   next;       // The next phantom attribute
        };

        /*
         * The struct is used to keep a list of attributes to which a pointer
         * or a reference was provided when an iterator's operator*() or
         * operator->() was used.
         */
        struct attr_instance
        {
            xml::attributes::attr   attr_;
            struct attr_instance *  next;

            attr_instance(const xml::attributes::attr &  att) :
                attr_(att), next(NULL)
            {}
        };


        struct node_private_data
        {
            struct phantom_attr *   phantom_attrs_;
            struct attr_instance *  attr_instances_;
            xml::node               node_instance_;
        };


        /* libxml2 will call it each time a node is destroyed.
         * The function cleans up the linked list of the
         * phantom_attrs attached to the node, the linked list of dereferenced
         * attributes and the dereferenced node
         */
        void cleanup_node (xmlNodePtr xmlnode);

        /* Attaches a new private data structure to the node if it has not been
         * attached yet.
         * The argument is actually of type xmlNodePtr
         */
        node_private_data *  attach_node_private_data(void *  xmlnode);

        /* Searches for a dereferenced attribute and provides a reference to
         * it. If not found then a new node is inserted.
         */
        void *  get_ptr_to_attr_instance(void *  att);

    } // end impl namespace

} // end xml namespace

#endif

