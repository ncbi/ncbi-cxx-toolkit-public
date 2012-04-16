/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Sergey Satskiy, NCBI
 * Credits: Denis Vakatov, NCBI (API design)
 *
 */


/** @file
 * XPath execution result set for XmlWrapp.
**/

#ifndef _xmlwrapp_node_set_hpp_
#define _xmlwrapp_node_set_hpp_

// xmlwrapp includes
#include <misc/xmlwrapp/xpath_expression.hpp>
#include <misc/xmlwrapp/node.hpp>

// standard includes
#include <cstddef>

namespace xml {

// forward declaration
namespace impl
{
    struct nset_impl;
}


/**
 * The xml::node_set class is used to store xpath query result set.
 * The object implements reference counting so once created while an xpath
 * query was executed, the copies of this object refer to the same set of xml
 * document nodes as the original one.
 * Please note that the reference counting implementation is NOT thread safe.
**/

class node_set
{
public:

    /**
     * Create a new empty xml::node_set object.
     *
     * @author Sergey Satskiy, NCBI
    **/
    node_set ();

    /**
     * Create a new xml::node_set object using another one as a template.
     * The new object refers to the same set of nodes as the templete one.
     *
     * @param other
     *  Another xml::node_set object.
     * @author Sergey Satskiy, NCBI
    **/
    node_set (const node_set& other);

    /**
     * Destroy the object and clean up the memory.
     *
     * @author Sergey Satskiy, NCBI
    **/
    virtual ~node_set ();

    /**
     * Creates a copy of the xml::node_set object.
     * A copy refers to the same set of nodes as the given object.
     *
     * @param other
     *  Another xml::node_set object.
     * @author Sergey Satskiy, NCBI
    **/
    node_set& operator= (const node_set& other);

    // forward declaration
    class const_iterator;

    /**
     * The xml::node_set::iterator class is used to iterate over nodes in a
     * node set. The iterator is one directional.
    **/
    class iterator
    {
    public:
        typedef node                        value_type;
        typedef std::ptrdiff_t              difference_type;
        typedef value_type*                 pointer;
        typedef value_type&                 reference;
        typedef std::forward_iterator_tag   iterator_category;

        /**
         * Create a new uninitialised xml::node_set::iterator object.
         *
         * @author Sergey Satskiy, NCBI
        **/
        iterator () : parent_(NULL), current_index_(-1) {}

        /**
         * Create a new xml::node_set::iterator object using another one as a
         * template.
         *
         * @author Sergey Satskiy, NCBI
        **/
        iterator (const iterator& other);

        /**
         * Create a copy of the xml::node_set::iterator object.
         *
         * @param other
         *  Another xml::node_set::iterator object
         * @author Sergey Satskiy, NCBI
        **/
        iterator& operator= (const iterator& other);

        /**
         * Destroy the object.
         *
         * @author Sergey Satskiy, NCBI
        **/
        ~iterator () {}

        /**
         * Provide a reference to the node.
         *
         * @exception
         *  throws exception if the iterator is invalid
         * @author Sergey Satskiy, NCBI
        **/
        reference operator* () const;

        /**
         * Provide a pointer to the node.
         *
         * @exception
         *  throws exception if the iterator is invalid
         * @author Sergey Satskiy, NCBI
        **/
        pointer   operator-> () const;

        /**
         * Prefix increment.
         *
         * @exception
         *  Throws exception if the iterator is not initialised or out of range
         * @author Sergey Satskiy, NCBI
        **/
        iterator& operator++ ();

        /**
         * Postfix increment.
         *
         * @exception
         *  Throws exception if the iterator is not initialised or out of range
         * @author Sergey Satskiy, NCBI
        **/
        iterator  operator++ (int);

        /**
         * Compare two iterators.
         *
         * @param other
         *  Another iterator
         * @return
         *  True if the iterators are equal
         * @author Sergey Satskiy, NCBI
        **/
        bool operator== (const iterator& other) const
        { return ((parent_ == other.parent_) &&
                  (current_index_ == other.current_index_)); }

        /**
         * Compare two iterators.
         *
         * @param other
         *  Another iterator
         * @return
         *  True if the iterators are not equal
         * @author Sergey Satskiy, NCBI
        **/
        bool operator!= (const iterator& other) const
        { return !(*this == other); }

    private:
        // Used by node_set to create iterators
        iterator(node_set* parent, int index) :
            parent_(parent), current_index_(index) {}

        void swap (iterator& other);

        node_set *      parent_;            // Node set to iterate over
        int             current_index_;     // Index of the node in the set

        friend class node_set;
        friend class const_iterator;
    };

    /**
     * The xml::node_set::const_iterator class is used to iterate over nodes in a
     * node set. The iterator is one directional.
    **/
    class const_iterator
    {
    public:
        typedef const node                  value_type;
        typedef std::ptrdiff_t              difference_type;
        typedef value_type*                 pointer;
        typedef value_type&                 reference;
        typedef std::forward_iterator_tag   iterator_category;

        /**
         * Create a new uninitialised xml::node_set::const_iterator object.
         *
         * @author Sergey Satskiy, NCBI
        **/
        const_iterator () : parent_(NULL), current_index_(-1) {}

        /**
         * Create a new xml::node_set::const_iterator object using another
         * one as a template.
         *
         * @author Sergey Satskiy, NCBI
        **/
        const_iterator (const const_iterator& other);

        /**
         * Create a new xml::node_set::const_iterator object using another
         * one as a template.
         *
         * @author Sergey Satskiy, NCBI
        **/
        const_iterator (const iterator& other);

        /**
         * Create a copy of the xml::node_set::const_iterator object.
         *
         * @param other
         *  Another xml::node_set::const_iterator object
         * @author Sergey Satskiy, NCBI
        **/
        const_iterator& operator= (const const_iterator& other);

        /**
         * Create a copy of the xml::node_set::const_iterator object.
         *
         * @param other
         *  Another const xml::node_set::iterator object
         * @author Sergey Satskiy, NCBI
        **/
        const_iterator& operator= (const iterator& other);

        /**
         * Destroy the object.
         *
         * @author Sergey Satskiy, NCBI
        **/
        ~const_iterator () {}

        /**
         * Provide a const reference to the node.
         *
         * @exception
         *  throws exception if the iterator is invalid
         * @author Sergey Satskiy, NCBI
        **/
        reference operator* () const;

        /**
         * Provide a const pointer to the node.
         *
         * @exception
         *  throws exception if the iterator is invalid
         * @author Sergey Satskiy, NCBI
        **/
        pointer   operator-> () const;

        /**
         * Prefix increment.
         *
         * @exception
         *  Throws exception if the iterator is not initialised or out of range
         * @author Sergey Satskiy, NCBI
        **/
        const_iterator& operator++ ();

        /**
         * Postfix increment.
         *
         * @exception
         *  Throws exception if the iterator is not initialised or out of range
         * @author Sergey Satskiy, NCBI
        **/
        const_iterator  operator++ (int);

        /**
         * Compare two iterators.
         *
         * @param other
         *  Another iterator
         * @return
         *  True if the iterators are equal
         * @author Sergey Satskiy, NCBI
        **/
        bool operator== (const const_iterator& other) const
        { return ((parent_ == other.parent_) &&
                  (current_index_ == other.current_index_)); }

        /**
         * Compare two iterators.
         *
         * @param other
         *  Another iterator
         * @return
         *  True if the iterators are not equal
         * @author Sergey Satskiy, NCBI
        **/
        bool operator!= (const const_iterator& other) const
        { return !(*this == other); }

    private:
        // Used by node_set to create iterators
        const_iterator(const node_set* parent, int index) :
            parent_(parent), current_index_(index) {}

        void swap (const_iterator& other);

        const node_set *    parent_;            // Node set to iterate over
        int                 current_index_;     // Index of the node in the set

        friend class node_set;
    };

    /**
     * Get an iterator that points to the beginning of the xpath query result
     * node set.
     *
     * @return
     *  An iterator that points to the beginning of the xpath query result set
     * @author Sergey Satskiy, NCBI
    **/
    iterator begin ();

    /**
     * Get a const_iterator that points to the beginning of the xpath query
     *  result node set.
     *
     * @return
     *  A const_iterator that points to the beginning of the xpath query result set
     * @author Sergey Satskiy, NCBI
    **/
    const_iterator begin () const;

    /**
     * Get an iterator that points one past the last node in the xpath query
     * result node set.
     *
     * @return
     *  An iterator that points one past the last node in the xpath query
     *  result node set.
     * @author Sergey Satskiy, NCBI
    **/
    iterator end ();

    /**
     * Get a const_iterator that points one past the last node in the xpath
     *  query result node set.
     *
     * @return
     *  A const_iterator that points one past the last node in the xpath query
     *  result node set.
     * @author Sergey Satskiy, NCBI
    **/
    const_iterator end () const;

    /**
     * Inform if the xpath query result node set is empty.
     *
     * @return
     *  true if the xpath query result node set is empty
     * @author Sergey Satskiy, NCBI
    **/
    bool empty () const;

    /**
     * Get the number of nodes in the xpath query result node set.
     *
     * @return
     *  The number of nodes in the xpath query result node set
     * @author Sergey Satskiy, NCBI
    **/
    size_t size() const;

private:
    // relay to create a fake node basing on the libxml2 raw pointer
    node_set(void* result_set);

    impl::nset_impl*    pimpl_;     // private implementation

    friend class node;
    friend class iterator;
    friend class const_iterator;
    friend struct impl::nset_impl;
};


} // xml namespace

#endif

