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
 * This file contains definitions required for iterators dereferencing
 * support.
**/


#include <misc/xmlwrapp/exception.hpp>
#include "deref_impl.hpp"

// libxml2 includes
#include <libxml/tree.h>

namespace xml
{
    namespace impl
    {
        void cleanup_node(xmlNodePtr  xmlnode)
        {
            if (xmlnode->_private == NULL)
                return;

            node_private_data *  node_data = static_cast<node_private_data *>(xmlnode->_private);

            // Clean the phantom attributes
            {{
                phantom_attr *  current = node_data->phantom_attrs_;
                phantom_attr *  next = NULL;
                while (current != NULL)
                {
                    next = current->next;
                    delete current;
                    current = next;
                }
            }}

            // Clean the attributes instances
            {{
                attr_instance *  current = node_data->attr_instances_;
                attr_instance *  next = NULL;
                while (current != NULL)
                {
                    next = current->next;
                    delete current;
                    current = next;
                }
            }}

            delete node_data;
        }

        // The argument is actually of type xmlNodePtr
        node_private_data *  attach_node_private_data(void *  xmlnode)
        {
            if (xmlnode == NULL)
                throw xml::exception("Dereferencing non-initialized iterator");

            xmlNodePtr      raw_node = static_cast<xmlNodePtr>(xmlnode);
            if (raw_node->_private == NULL) {
                node_private_data *     data = new node_private_data;

                data->phantom_attrs_ = NULL;
                data->attr_instances_ = NULL;
                data->node_instance_.set_node_data(raw_node);
                raw_node->_private = data;
            }
            return static_cast<node_private_data*>(raw_node->_private);
        }

        void *  get_ptr_to_attr_instance(void *  att)
        {
            attributes::attr *  att_ptr = static_cast<attributes::attr *>(att);
            node_private_data * node_data = attach_node_private_data(att_ptr->get_node());
            attr_instance *     current = node_data->attr_instances_;

            while (current != NULL)
            {
                if (*att_ptr == current->attr_)
                    return & current->attr_;
                current = current->next;
            }

            // Not found, so insert a new node to the head
            attr_instance *     new_attr_instance = new attr_instance(*att_ptr);

            new_attr_instance->next = node_data->attr_instances_;
            node_data->attr_instances_ = new_attr_instance;
            return & new_attr_instance->attr_;
        }

    } // namespace impl
} // namespace xml

