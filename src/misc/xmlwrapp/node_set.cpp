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
 * This file contains the implementation of the xml::node_set cass.
**/

// xmlwrapp includes
#include <misc/xmlwrapp/node_set.hpp>
#include <misc/xmlwrapp/exception.hpp>

#include "deref_impl.hpp"

// standard includes
#include <stdexcept>

// libxml2
#include <libxml/xpath.h>

namespace xml
{
    namespace impl
    {
        // Private implementation of the node_set class.
        // It holds the libxml2 pointer to the XPath query results and provides
        // the reference counting
        struct nset_impl
        {
            // XPath query results
            xmlXPathObjectPtr   results_;

            nset_impl(void* results) :
                results_(reinterpret_cast<xmlXPathObjectPtr>(results)),
                refcnt_(1)
            {}
            void inc_ref() { ++refcnt_; }
            void dec_ref() {
                if (--refcnt_ == 0) {
                    if (results_) xmlXPathFreeObject(results_);
                    delete this;
                }
            }

            node &  get_reference( int  index ) {
                /* The index range is checked by the caller */

                node_private_data *  node_data = attach_node_private_data(results_->nodesetval->nodeTab[index]);
                return node_data->node_instance_;
            }

        protected:
            ~nset_impl() {}

        private:
            size_t  refcnt_;    // reference counter
        };
    }

    //
    // node_set
    //

    const char*     kDerefError = "dereferencing non initialised or out of range iterator";
    const char*     kRefError   = "referencing non initialised or out of range iterator";
    const char*     kAdvError   = "advancing non initialised or out of range iterator";

    node_set::node_set() :
        pimpl_(NULL)
    {
        /* Avoid compiler warnings */
        pimpl_ = new impl::nset_impl(0);
    }

    node_set::node_set(void* result_set) :
        pimpl_(NULL)
    {
        /* Avoid compiler warnings */
        pimpl_ = new impl::nset_impl(result_set);
    }

    node_set::node_set(const node_set& other) :
        pimpl_(other.pimpl_)
    {
        pimpl_->inc_ref();
    }

    node_set::~node_set()
    {
        pimpl_->dec_ref();
    }

    node_set& node_set::operator=(const node_set& other)
    {
        pimpl_->dec_ref();
        pimpl_ = other.pimpl_;
        pimpl_->inc_ref();
        return *this;
    }

    bool node_set::empty() const
    {
        return !pimpl_->results_ ||
               !pimpl_->results_->nodesetval ||
               pimpl_->results_->nodesetval->nodeNr == 0;
    }

    size_t node_set::size() const
    {
        if (empty()) return 0;
        return pimpl_->results_->nodesetval->nodeNr;
    }

    node_set::iterator node_set::begin()
    {
        if (empty()) return iterator(this, -1);
        return iterator(this, 0);
    }

    node_set::const_iterator node_set::begin() const
    {
        if (empty()) return const_iterator(this, -1);
        return const_iterator(this, 0);
    }

    node_set::iterator node_set::end()
    {
        return iterator(this, -1);
    }

    node_set::const_iterator node_set::end() const
    {
        return const_iterator(this, -1);
    }

    //
    // node_set::iterator
    //

    node_set::iterator::iterator(const iterator& other)
    {
        parent_ = other.parent_;
        current_index_ = other.current_index_;
    }

    node_set::iterator& node_set::iterator::operator=(const iterator& other)
    {
        iterator tmp(other);
        swap(tmp);
        return *this;
    }

    node_set::iterator::reference node_set::iterator::operator*() const
    {
        if (!parent_ || current_index_ == -1)
            throw xml::exception(kDerefError);
        return parent_->pimpl_->get_reference(current_index_);
    }

    node_set::iterator::pointer node_set::iterator::operator->() const
    {
        if (!parent_ || current_index_ == -1)
            throw xml::exception(kRefError);
        return &parent_->pimpl_->get_reference(current_index_);
    }

    node_set::iterator& node_set::iterator::operator++()
    {
        if (!parent_ || current_index_ == -1)
            throw xml::exception(kAdvError);
        if (static_cast<size_t>(++current_index_) >= parent_->size())
            current_index_ = -1;
        return *this;
    }

    node_set::iterator node_set::iterator::operator++(int)
    {
        iterator tmp(*this);
        ++(*this);
        return tmp;
    }

    void node_set::iterator::swap(iterator& other)
    {
        std::swap(parent_, other.parent_);
        std::swap(current_index_, other.current_index_);
    }

    //
    // node_set::const_iterator
    //

    node_set::const_iterator::const_iterator (const const_iterator& other)
    {
        parent_ = other.parent_;
        current_index_ = other.current_index_;
    }

    node_set::const_iterator::const_iterator (const iterator& other)
    {
        parent_ = other.parent_;
        current_index_ = other.current_index_;
    }

    node_set::const_iterator& node_set::const_iterator::operator= (const const_iterator& other)
    {
        const_iterator tmp(other);
        swap(tmp);
        return *this;
    }

    node_set::const_iterator& node_set::const_iterator::operator= (const iterator& other)
    {
        const_iterator tmp(other);
        swap(tmp);
        return *this;
    }

    node_set::const_iterator::reference node_set::const_iterator::operator* () const
    {
        if (!parent_ || current_index_ == -1)
            throw xml::exception(kDerefError);
        return parent_->pimpl_->get_reference(current_index_);
    }

    node_set::const_iterator::pointer node_set::const_iterator::operator-> () const
    {
        if (!parent_ || current_index_ == -1)
            throw xml::exception(kRefError);
        return &parent_->pimpl_->get_reference(current_index_);
    }

    node_set::const_iterator& node_set::const_iterator::operator++ ()
    {
        if (!parent_ || current_index_ == -1)
            throw xml::exception(kAdvError);
        if (static_cast<size_t>(++current_index_) >= parent_->size())
            current_index_ = -1;
        return *this;
    }

    node_set::const_iterator node_set::const_iterator::operator++ (int)
    {
        const_iterator tmp(*this);
        ++(*this);
        return tmp;
    }

    void node_set::const_iterator::swap (const_iterator& other)
    {
        std::swap(parent_, other.parent_);
        std::swap(current_index_, other.current_index_);
    }

} // namespace xml

