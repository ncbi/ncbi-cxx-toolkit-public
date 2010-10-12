/*
 * Copyright (C) 2009 Vaclav Slavik <vslavik@fastmail.fm>
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
 * This file contains the definition of the xml::nodes_view and
 * xml::const_nodes_view classes.
**/

#ifndef _xmlwrapp_nodes_view_h_
#define _xmlwrapp_nodes_view_h_

// Have this functionality only if it is explicitly requested
#ifdef XMLWRAPP_USE_NODE_VIEW

// xmlwrapp includes
#include <misc/xmlwrapp/xml_init.hpp>

// standard includes
#include <cstddef>
#include <iterator>

namespace xml
{

class node;
class const_nodes_view;

namespace impl
{
struct nipimpl;
class iter_advance_functor;
}

/**
 * This class implements a view of XML nodes. A @em view is a container-like
 * class that only allows access to a subset of xml::node's child nodes. The
 * exact content depends on how the view was obtained; typical uses are
 * e.g. a view of all element children or all elements with a given name.
 *
 * The nodes_view class implements the same container interface that xml::node
 * does: it has begin() and end() methods.
 *
 * @author Vaclav Slavik
 * @since  0.6.0
 *
 * @see xml::node::elements(), xml::node::elements(const char*)
**/
class nodes_view
{
public:
    nodes_view() : data_begin_(0), advance_func_(0) {}
    nodes_view(const nodes_view& other);
    virtual ~nodes_view();

    nodes_view& operator=(const nodes_view& other);

    class const_iterator;

    /**
     * The iterator provides a way to access nodes in the view
     * similar to a standard C++ container.
     *
     * @see xml::node::iterator
    **/
    class iterator
    {
    public:
        typedef node value_type;
        typedef std::ptrdiff_t difference_type;
        typedef value_type* pointer;
        typedef value_type& reference;
        typedef std::forward_iterator_tag iterator_category;

        iterator() : pimpl_(0), advance_func_(0) {}
        iterator(const iterator& other);
        iterator& operator=(const iterator& other);
        ~iterator();

        reference operator*() const;
        pointer   operator->() const;

        iterator& operator++();
        iterator  operator++(int);

        bool operator==(const iterator& other) const
            { return get_raw_node() == other.get_raw_node(); }
        bool operator!=(const iterator& other) const
            { return !(*this == other); }

    private:
        explicit iterator(void *data, impl::iter_advance_functor *advance_func);
        void* get_raw_node() const;
        void swap(iterator& other);

        impl::nipimpl *pimpl_;
        // function for advancing the iterator (note that it is "owned" by the
        // parent view object, so we don't have to care about its reference
        // count here)
        impl::iter_advance_functor *advance_func_;

        friend class nodes_view;
        friend class const_iterator;
    };

    /**
     * The const_iterator provides a way to access nodes in the view
     * similar to a standard C++ container. The nodes that are pointed to by
     * the iterator cannot be changed.
     *
     * @see xml::node::const_iterator
    **/
    class const_iterator
    {
    public:
        typedef const node value_type;
        typedef std::ptrdiff_t difference_type;
        typedef value_type* pointer;
        typedef value_type& reference;
        typedef std::forward_iterator_tag iterator_category;

        const_iterator() : pimpl_(0), advance_func_(0) {}
        const_iterator(const const_iterator& other);
        const_iterator(const iterator& other);
        const_iterator& operator=(const const_iterator& other);
        const_iterator& operator=(const iterator& other);
        ~const_iterator();

        reference operator*() const;
        pointer   operator->() const;

        const_iterator& operator++();
        const_iterator  operator++(int);

        bool operator==(const const_iterator& other) const
            { return get_raw_node() == other.get_raw_node(); }
        bool operator!=(const const_iterator& other) const
            { return !(*this == other); }

    private:
        explicit const_iterator(void *data, impl::iter_advance_functor *advance_func);
        void* get_raw_node() const;
        void swap(const_iterator& other);

        impl::nipimpl *pimpl_;
        // function for advancing the iterator (note that it is "owned" by the
        // parent view object, so we don't have to care about its reference
        // count here)
        impl::iter_advance_functor *advance_func_;

        friend class const_nodes_view;
        friend class nodes_view;
    };

    /**
     * Get an iterator that points to the beginning of this node's
     * children.
     *
     * @return An iterator that points to the beginning of the children.
    **/
    iterator begin() { return iterator(data_begin_, advance_func_); }

    /**
     * Get an iterator that points to the beginning of this node's
     * children.
     *
     * @return An iterator that points to the beginning of the children.
    **/
    const_iterator begin() const { return const_iterator(data_begin_, advance_func_); }

    /**
     * Get an iterator that points one past the last child for this node.
     *
     * @return A "one past the end" iterator.
    **/
    iterator end() { return iterator(); }

    /**
     * Get an iterator that points one past the last child for this node.
     *
     * @return A "one past the end" iterator.
    **/
    const_iterator end() const { return const_iterator(); }

    /// Is the view empty?
    bool empty() const { return !data_begin_; }

private:
    explicit nodes_view(void *data_begin, impl::iter_advance_functor *advance_func)
        : data_begin_(data_begin), advance_func_(advance_func) {}

    // begin iterator
    void *data_begin_;
    // function for advancing the iterator (owned by the view object)
    impl::iter_advance_functor *advance_func_;

    friend class node;
    friend class const_nodes_view;
};


/**
 * This class implements a @em read-only view of XML nodes. The only difference
 * from xml::nodes_view is that it doesn't allow modifications of the nodes,
 * it is otherwise identical.
 *
 * @see nodes_view
 *
 * @author Vaclav Slavik
 * @since  0.6.0
**/
class const_nodes_view
{
public:
    const_nodes_view() : data_begin_(0), advance_func_(0) {}
    const_nodes_view(const const_nodes_view& other);
    const_nodes_view(const nodes_view& other);
    virtual ~const_nodes_view();

    const_nodes_view& operator=(const const_nodes_view& other);
    const_nodes_view& operator=(const nodes_view& other);

    typedef nodes_view::const_iterator iterator;
    typedef nodes_view::const_iterator const_iterator;

    /**
     * Get an iterator that points to the beginning of this node's
     * children.
     *
     * @return An iterator that points to the beginning of the children.
    **/
    const_iterator begin() const
        { return const_iterator(data_begin_, advance_func_); }

    /**
     * Get an iterator that points one past the last child for this node.
     *
     * @return A "one past the end" iterator.
    **/
    const_iterator end() const { return const_iterator(); }

    /// Is the view empty?
    bool empty() const { return !data_begin_; }

private:
    explicit const_nodes_view(void *data_begin, impl::iter_advance_functor *advance_func)
        : data_begin_(data_begin), advance_func_(advance_func) {}

    // begin iterator
    void *data_begin_;
    // function for advancing the iterator (owned by the view object)
    impl::iter_advance_functor *advance_func_;

    friend class node;
};

} // end xml namespace

#endif // XMLWRAPP_USE_NODE_VIEW

#endif // _xmlwrapp_nodes_view_h_
