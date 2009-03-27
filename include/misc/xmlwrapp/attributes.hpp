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
 */

/** @file
 * This file contains the definition of the xml::attributes class.
**/

#ifndef _xmlwrapp_attributes_h_
#define _xmlwrapp_attributes_h_

// xmlwrapp includes
#include <misc/xmlwrapp/xml_init.hpp>
#include <misc/xmlwrapp/name_space.hpp>

// standard includes
#include <cstddef>
#include <iosfwd>
#include <string>

namespace xml {
   
// forward declarations
class node;

namespace impl {
class ait_impl;
struct node_impl;
}

/**
 * The xml::attributes class is used to access all the attributes of one
 * xml::node. You can add, find and erase attributes by name, and for some
 * member functions, use the provided iterator classes.
 *
 * The iterator classes allow you to access one XML attribute. This is done
 * using the xml::attributes::attr class interface.
**/
class attributes {
public:
    typedef std::size_t size_type; ///< size type

    //####################################################################
    /** 
     * Create a new xml::attributes object with no attributes.
     *
     * @author Peter Jones
    **/
    //####################################################################
    attributes (void);

    //####################################################################
    /** 
     * Copy construct a xml::attributes object.
     *
     * @param other The xml::attributes object to copy from.
     * @author Peter Jones
    **/
    //####################################################################
    attributes (const attributes &other);

    //####################################################################
    /** 
     * Copy the given xml::attributes object into this one.
     *
     * @param other The xml::attributes object to copy from.
     * @return *this.
     * @author Peter Jones
    **/
    //####################################################################
    attributes& operator= (const attributes &other);

    //####################################################################
    /** 
     * Swap this xml::attributes object with another one.
     *
     * @param other The other xml::attributes object to swap with.
     * @author Peter Jones
    **/
    //####################################################################
    void swap (attributes &other);

    //####################################################################
    /** 
     * Clean up after a xml::attributes object.
     *
     * @author Peter Jones
    **/
    //####################################################################
    ~attributes (void);

    // forward declarations
    class const_iterator;

    /**
     * The xml::attributes::attr class is used to hold information about one
     * attribute.
     */
    class attr {
    public:
	//####################################################################
	/** 
	 * Get the name of this attribute.
	 *
	 * @return The name for this attribute.
	 * @author Peter Jones
	**/
	//####################################################################
	const char* get_name (void) const;

	//####################################################################
	/** 
	 * Get the value of this attribute.
	 *
	 * @return The value for this attribute.
	 * @author Peter Jones
	**/
	//####################################################################
	const char* get_value (void) const;

    //####################################################################
    /**
     * Get the attribute namespace. If the attribute has no namespace
     * then an object with both empty a prefix and uri is returned (the
     * is_void() method of the xml::name_space object will return true
     * in this case)
     *
     * @return The attribute namespace. Void namespace object if no
     *         namespace is associated.
     * @author Sergey Satskiy
    **/
    //####################################################################
    xml::name_space get_namespace (void) const;

    private:
	void *node_;
	void *prop_;
	std::string name_;
	mutable std::string value_;

	attr (void);
	attr (const attr &other);
	attr& operator= (const attr &other);
	void swap (attr &other);

	void set_data (void *node, void *prop);
	void set_data (const char *name, const char *value, bool);
	friend class impl::ait_impl;
    }; // end xml::attributes::attr class

    /**
     * Iterator class for accessing attribute pairs.
     */
    class iterator {
    public:
	typedef attr value_type;
	typedef std::ptrdiff_t difference_type;
	typedef value_type* pointer;
	typedef value_type& reference;
	typedef std::forward_iterator_tag iterator_category;

	iterator  (void);
	iterator  (const iterator &other);
	iterator& operator= (const iterator& other);
	~iterator (void);

	reference operator*  (void) const;
	pointer   operator-> (void) const;

	/// prefix increment
	iterator& operator++ (void);

	/// postfix increment (avoid if possible for better performance)
	iterator  operator++ (int);

	friend bool operator== (const iterator &lhs, const iterator &rhs);
	friend bool operator!= (const iterator &lhs, const iterator &rhs);
    private:
    impl::ait_impl *pimpl_;
	iterator (void *node, void *prop);
	iterator (const char *name, const char *value, bool);
	void swap (iterator &other);
	void* get_raw_attr (void);
	friend class attributes;
	friend class const_iterator;
    }; // end xml::attributes::iterator class

    /**
     * Const Iterator class for accessing attribute pairs.
     */
    class const_iterator {
    public:
	typedef const attr value_type;
	typedef std::ptrdiff_t difference_type;
	typedef value_type* pointer;
	typedef value_type& reference;
	typedef std::forward_iterator_tag iterator_category;

	const_iterator  (void);
	const_iterator  (const const_iterator &other);
	const_iterator  (const iterator &other);
	const_iterator& operator= (const const_iterator& other);
	~const_iterator (void);

	reference operator*  (void) const;
	pointer   operator-> (void) const;

	/// prefix increment
	const_iterator& operator++ (void);

	/// postfix increment (avoid if possible better for performance)
	const_iterator  operator++ (int);

	friend bool operator== (const const_iterator &lhs, const const_iterator &rhs);
	friend bool operator!= (const const_iterator &lhs, const const_iterator &rhs);
    private:
    impl::ait_impl *pimpl_;
	const_iterator (void *node, void *prop);
	const_iterator (const char *name, const char *value, bool);
	void swap (const_iterator &other);
	void* get_raw_attr (void);
	friend class attributes;
    }; // end xml::attributes::const_iterator class

    //####################################################################
    /** 
     * Get an iterator that points to the first attribute.
     *
     * @return An iterator that points to the first attribute.
     * @return An iterator equal to end() if there are no attributes.
     * @see xml::attributes::iterator
     * @see xml::attributes::attr
     * @author Peter Jones
    **/
    //####################################################################
    iterator begin (void);

    //####################################################################
    /** 
     * Get a const_iterator that points to the first attribute.
     *
     * @return A const_iterator that points to the first attribute.
     * @return A const_iterator equal to end() if there are no attributes.
     * @see xml::attributes::const_iterator
     * @see xml::attributes::attr
     * @author Peter Jones
    **/
    //####################################################################
    const_iterator begin (void) const;

    //####################################################################
    /** 
     * Get an iterator that points one past the the last attribute.
     *
     * @return An "end" iterator.
     * @author Peter Jones
    **/
    //####################################################################
    iterator end (void);

    //####################################################################
    /** 
     * Get a const_iterator that points one past the last attribute.
     *
     * @return An "end" const_iterator.
     * @author Peter Jones
    **/
    //####################################################################
    const_iterator end (void) const;

    //####################################################################
    /** 
     * Add an attribute to the attributes list. If there is another
     * attribute with the same name, it will be replaced with this one.
     *
     * @param name The name of the attribute to add.
     * @param value The value of the attribute to add.
     * @author Peter Jones
    **/
    //####################################################################
    void insert (const char *name, const char *value);

    //####################################################################
    /** 
     * Find the attribute with the given name. If the attribute is not found
     * on the current node, the DTD will be searched for a default value.
     * This is, of course, if there was a DTD parsed with the XML document.
     *
     * @param name The name of the attribute to find.
     * @return An iterator that points to the attribute with the given name.
     * @return If the attribute was not found, find will return end().
     * @see xml::attributes::iterator
     * @see xml::attributes::attr
     * @author Peter Jones
    **/
    //####################################################################
    iterator find (const char *name);

    //####################################################################
    /** 
     * Find the attribute with the given name. If the attribute is not found
     * on the current node, the DTD will be searched for a default value.
     * This is, of course, if there was a DTD parsed with the XML document.
     *
     * @param name The name of the attribute to find.
     * @return A const_iterator that points to the attribute with the given name.
     * @return If the attribute was not found, find will return end().
     * @see xml::attributes::const_iterator
     * @see xml::attributes::attr
     * @author Peter Jones
    **/
    //####################################################################
    const_iterator find (const char *name) const;

    //####################################################################
    /** 
     * Erase the attribute that is pointed to by the given iterator. This
     * will invalidate any iterators for this attribute, as well as any
     * pointers or references to it.
     *
     * @param to_erase An iterator that points to the attribute to erased.
     * @return An iterator that points to the attribute after the one to be erased.
     * @see xml::attributes::iterator
     * @see xml::attributes::attr
     * @author Peter Jones
    **/
    //####################################################################
    iterator erase (iterator to_erase);

    //####################################################################
    /** 
     * Erase the attribute with the given name. This will invalidate any
     * iterators that are pointing to that attribute, as well as any
     * pointers or references to that attribute.
     *
     * @param name The name of the attribute to erase.
     * @author Peter Jones
    **/
    //####################################################################
    void erase (const char *name);

    //####################################################################
    /** 
     * Find out if there are any attributes in this xml::attributes object.
     *
     * @return True if there are no attributes.
     * @return False if there is at least one attribute.
     * @author Peter Jones
    **/
    //####################################################################
    bool empty (void) const;

    //####################################################################
    /** 
     * Find out how many attributes there are in this xml::attributes
     * object.
     *
     * @return The number of attributes in this xml::attributes object.
     * @author Peter Jones
    **/
    //####################################################################
    size_type size (void) const;

private:
    struct pimpl; pimpl *pimpl_;

    // private ctor to create uninitialized instance
    explicit attributes (int);

    void set_data (void *node);
    void* get_data (void);
    friend struct impl::node_impl;
    friend class node;
}; // end xml::attributes class
    
} // end xml namespace
#endif
