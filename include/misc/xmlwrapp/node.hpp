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
 * This file contains the definition of the xml::node class.
**/

#ifndef _xmlwrapp_node_h_
#define _xmlwrapp_node_h_

// for NCBI_DEPRECATED
#include <ncbiconf.h>

// xmlwrapp includes
#include <misc/xmlwrapp/xml_init.hpp>
#include <misc/xmlwrapp/namespace.hpp>
#include <misc/xmlwrapp/attributes.hpp>
#include <misc/xmlwrapp/xml_save.hpp>

// hidden stuff
#include <misc/xmlwrapp/impl/_cbfo.hpp>


// standard includes
#include <cstddef>
#include <iosfwd>
#include <string>
#include <deque>

// Forward declaration for a friend below
extern "C" { void xslt_ext_func_cb(void *, int); }
extern "C" { void xslt_ext_element_cb(void*, void*, void*, void*); }

namespace xslt {
class xpath_object;
class extension_element;
}

namespace xml {

// forward declarations
class document;
class xpath_expression;
class node_set;

namespace impl {
class node_iterator;
class iter_advance_functor;
struct node_impl;
struct doc_impl;
struct nipimpl;
struct node_cmp;
struct node_private_data;
node_private_data*  attach_node_private_data(void *);
}

/**
 * The xml::node class is used to hold information about one XML node. This
 * includes the name of the node, the namespace of the node and attributes
 * for the node. It also has an iterator whereby you can get to the children
 * nodes.
 *
 * It should be noted that any member function that returns a const char*
 * returns a temporary value. The pointer that is returned will change with
 * ANY operation to the xml::node. If you need the data to stick around a
 * little longer you should put it inside a std::string.
**/
class node {
public:
    /// size type
    typedef std::size_t size_type;

    /// enum for the different types of XML nodes
    enum node_type {
        type_element,           ///< XML element such as "<chapter/>"
        type_text,              ///< Text node
        type_cdata,             ///< <![CDATA[text]]>
        type_pi,                ///< Processing Instruction
        type_comment,           ///< XML comment
        type_entity,            ///< Entity as in &amp;amp;
        type_entity_ref,        ///< Entity ref
        type_xinclude,          ///< <xi:include/> node
        type_document,          ///< Document node
        type_document_type,     ///< DOCTYPE node
        type_document_frag,     ///< Document Fragment
        type_notation,          ///< Notation
        type_dtd,               ///< DTD node
        type_dtd_element,       ///< DTD <!ELEMENT> node
        type_dtd_attribute,     ///< DTD <!ATTRLIST> node
        type_dtd_entity,        ///< DTD <!ENTITY>
        type_dtd_namespace      ///< ?
    };

    /// enum for policies of adding namespace definitions
    enum ns_definition_adding_type {
        type_replace_if_exists, ///< replace URI if ns with the same prefix exists
        type_throw_if_exists    ///< throw exception if ns with the same prefix exists
    };

    /// emun to specify how to remove namespace definitions
    enum ns_definition_erase_type {
        type_ns_def_erase_if_not_used,  ///< Remove the definition only if it
                                        ///< is not in use.
                                        ///< If the definition is in use then
                                        ///< throw an exception.
        type_ns_def_erase_enforce       ///< Remove the definition regardless
                                        ///< if it is used or not. If any
                                        ///< attribute or node uses the
                                        ///< definition then its namespace will
                                        ///< be adjusted to a default one (if
                                        ///< defined above) or will be set to
                                        ///< no namespace (otherwise).
    };


    /**
     * Helper struct for creating a xml::node of type_cdata.
     *
     * @code
     * xml::node mynode(xml::node::cdata("This is a CDATA section"));
     * @endcode
     */
    struct cdata {
        explicit cdata (const char *text) : t(text) { }
        const char *t;
    };

    /**
     * Helper struct for creating a xml::node of type_comment.
     *
     * @code
     * xml::node mynode(xml::node::comment("This is an XML comment"));
     * @endcode
     */
    struct comment {
        explicit comment (const char *text) : t(text) { }
        const char *t;
    };

    /**
     * Helper struct for creating a xml::node of type_pi.
     *
     * @code
     * xml::node mynode(xml::node::pi("xslt", "stylesheet=\"test.xsl\""));
     * @endcode
     */
    struct pi {
        explicit pi (const char *name, const char *content=0) : n(name), c(content) { }
        const char *n, *c;
    };

    /**
     * Helper struct for creating a xml::node of type_text.
     *
     * @code
     * xml::node mynode(xml::node::text("This is an XML text fragment"));
     * @endcode
     */
    struct text {
        explicit text (const char *text) : t(text) { }
        const char *t;
    };

    //####################################################################
    /**
     * Construct a new blank xml::node.
     *
     * @author Peter Jones
    **/
    //####################################################################
    node (void);

    //####################################################################
    /**
     * Construct a new xml::node and set the name of the node.
     *
     * @param name The name of the new node.
     * @author Peter Jones
    **/
    //####################################################################
    explicit node (const char *name);

    //####################################################################
    /**
     * Construct a new xml::node given a name and content. The content will
     * be used to create a new child text node.
     * All the special symbols ('<', '>', '&', '"', '\r') in the given
     * content are encoded before assigning the new content.
     * If entities are needed in the content please use set_raw_content(...).
     *
     * @param name The name of the new element.
     * @param content The text that will be used to create a child node.
     * @author Peter Jones
    **/
    //####################################################################
    node (const char *name, const char *content);

    //####################################################################
    /**
     * Construct a new xml::node that is of type_cdata. The cdata_info
     * parameter should contain the contents of the CDATA section.
     *
     * @note Sample Use Example:
     * @code
     * xml::node mynode(xml::node::cdata("This is a CDATA section"));
     * @endcode
     *
     * @param cdata_info A cdata struct that tells xml::node what the content will be.
     * @author Peter Jones
    **/
    //####################################################################
    explicit node (cdata cdata_info);

    //####################################################################
    /**
     * Construct a new xml::node that is of type_comment. The comment_info
     * parameter should contain the contents of the XML comment.
     *
     * @note Sample Use Example:
     * @code
     * xml::node mynode(xml::node::comment("This is an XML comment"));
     * @endcode
     *
     * @param comment_info A comment struct that tells xml::node what the comment will be.
     * @author Peter Jones
    **/
    //####################################################################
    explicit node (comment comment_info);

    //####################################################################
    /**
     * Construct a new xml::node that is of type_pi. The pi_info parameter
     * should contain the name of the XML processing instruction (PI), and
     * optionally, the contents of the XML PI.
     *
     * @note Sample Use Example:
     * @code
     * xml::node mynode(xml::node::pi("xslt", "stylesheet=\"test.xsl\""));
     * @endcode
     *
     * @param pi_info A pi struct that tells xml::node what the name and contents of the XML PI are.
     * @author Peter Jones
    **/
    //####################################################################
    explicit node (pi pi_info);

    //####################################################################
    /**
     * Construct a new xml::node that is of type_text. The text_info
     * parameter should contain the text.
     *
     * @note Sample Use Example:
     * @code
     * xml::node mynode(xml::node::text("This is XML text"));
     * @endcode
     *
     * @param text_info A text struct that tells xml::node what the text will be.
     * @author Vaclav Slavik
    **/
    //####################################################################
    explicit node (text text_info);

    //####################################################################
    /**
     * Create a copy of the node which is detached from the document.
     * The nested nodes as well as namespace definitions are copied too.
     *
     * @return A pointer to the copied node. The user is responsible to delete
     *         it.
     * @exception Throws xml::exception if the copying failed.
    **/
    //####################################################################
    node* detached_copy (void) const;

    //####################################################################
    /**
     * Class destructor
     *
     * @author Peter Jones
    **/
    //####################################################################
    virtual ~node (void);

    //####################################################################
    /**
     * Set the name of this xml::node.
     *
     * @param name The new name for this xml::node.
     * @author Peter Jones
    **/
    //####################################################################
    void set_name (const char *name);

    //####################################################################
    /**
     * Get the name of this xml::node.
     *
     * This function may change in the future to return std::string.
     * Feedback is welcome.
     *
     * @return The name of this node.
     * @author Peter Jones
    **/
    //####################################################################
    const char* get_name (void) const;

    //####################################################################
    /**
     * Set the content of a node. If this node is an element node, this
     * function will remove all of its children nodes and replace them
     * with one text node set to the new content.
     * All the special symbols ('<', '>', '&', '"', '\r') in the given
     * content are encoded before assigning the new content.
     * If entities are needed in the content please use
     * set_raw_content(...).
     *
     * @param content The content of the text node.
     * @author Peter Jones, Sergey Satskiy
    **/
    //####################################################################
    void set_content (const char *content);

    //####################################################################
    /**
     * Set the raw content of a node. If this node is an element node,
     * this function will remove all of its children nodes and replace
     * them with one text node set to the new content.
     * The given content is checked for '<' and '>' characters. If found
     * they will be replaced with '&lt;' and '&gt;' respectively and this
     * is the only potential conversion done for the given raw content.
     * This member is likely used if entities are needed in the node content.
     * In any case it is the user responsibility to provide valid
     * content for this member.
     *
     * @param raw_content The raw content of the text node.
     * @author Sergey Satskiy
    **/
    //####################################################################
    void set_raw_content (const char *raw_content);

    //####################################################################
    /**
     * Get the content for this text node. If this node is not a text node
     * but it has children nodes that are text nodes, the contents of those
     * child nodes will be returned. If there is no content or these
     * conditions do not apply, zero will be returned.
     *
     * This function may change in the future to return std::string.
     * Feedback is welcome.
     *
     * @return The content or 0.
     * @author Peter Jones
    **/
    //####################################################################
    const char* get_content (void) const;

    //####################################################################
    /**
     * Get this node's "type". You can use that information to know what you
     * can and cannot do with it.
     *
     * @return The node's type.
     * @author Peter Jones
    **/
    //####################################################################
    node_type get_type (void) const;

    //####################################################################
    /**
     * Get the list of attributes. You can use the returned object to get
     * and set the attributes for this node. Make sure you use a reference
     * to this returned object, to prevent a copy.
     *
     * @return The xml::attributes object for this node.
     * @author Peter Jones
    **/
    //####################################################################
    xml::attributes& get_attributes (void);

    //####################################################################
    /**
     * Get the list of attributes. You can use the returned object to get
     * the attributes for this node. Make sure you use a reference to this
     * returned object, to prevent a copy.
     *
     * @return The xml::attributes object for this node.
     * @author Peter Jones
    **/
    //####################################################################
    const xml::attributes& get_attributes (void) const;

    //####################################################################
    /**
     * Search for a node attribute.
     *
     * @param name
     *   The name of the attribute to find. The name could be given as a
     *   qualified name, e.g. 'prefix:attr_name'. If the name is qualified then
     *   the nspace argument must be NULL (otherwise an exception is
     *   generated) and the attribute search is namespace aware with an
     *   effective namespace identified by the given prefix.
     * @param nspace
     *   The namespace of the atrribute to find:
     *   - NULL matches any namespace
     *   - Void namespace matches attributes without a namespace set
     *   - Unsafe namespace is used as it is
     *   - A safe namespace is resolved basing on the uri only
     * @return iterator to the found attribute. If there is no such an
     *         attribute then the provided iterator equals to
     *         attributes::end().
     * @author Sergey Satskiy, NCBI
    **/
    attributes::iterator find_attribute (const char* name,
                                         const ns* nspace = NULL);

    //####################################################################
    /**
     * Search for a node attribute.
     *
     * @param name
     *   The name of the attribute to find. The name could be given as a
     *   qualified name, e.g. 'prefix:attr_name'. If the name is qualified then
     *   the nspace argument must be NULL (otherwise an exception is
     *   generated) and the attribute search is namespace aware with an
     *   effective namespace identified by the given prefix.
     * @param nspace
     *   The namespace of the atrribute to find:
     *   - NULL matches any namespace
     *   - Void namespace matches attributes without a namespace set
     *   - Unsafe namespace is used as it is
     *   - A safe namespace is resolved basing on the uri only
     * @return const iterator to the found attribute. If there is no such an
     *         attribute then the provided iterator equals to
     *         attributes::end().
     * @author Sergey Satskiy, NCBI
    **/
    attributes::const_iterator find_attribute (const char* name,
                                               const ns* nspace = NULL) const;

    //####################################################################
    /**
     * Get the namespace of this xml::node.
     *
     * @param type
     *  The required type of namespace object (safe/unsafe).
     * @return
     *  The namespace of this node. If the node has no namespace
     *  then return a "void" namespace object with empty prefix and URI
     *  (for which xml::ns::is_void() returns TRUE).
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    ns get_namespace (ns::ns_safety_type type = ns::type_safe_ns) const;

    //####################################################################
    /**
      * Get the namespaces defined at this xml::node.
      *
      * @param type The required type of namespace objects (safe/unsafe).
      * @return The namespaces defined at this node.
      *         If no namespaces are defined then return an empty container.
      * @author Sergey Satskiy, NCBI
     **/
    //####################################################################
    ns_list_type get_namespace_definitions (ns::ns_safety_type type = ns::type_safe_ns) const;

    //####################################################################
    /**
      * Set the node namespace.
      *
      * The namespace definition is searched up in the hierarchy of nodes.
      * If a namespace with the given prefix and URI is not found
      * then throw an exception.
      *
      * @param name_space
      *  Namespace to set to the node.
      *  "Void" namespace is treated as a namespace removal request --
      *  exactly the same as erase_namespace() call.
      * @note There are no checks at all if an unsafe ns object is provided.
      * @return  Unsafe namespace
      * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    ns set_namespace (const ns& name_space);

    //####################################################################
    /**
      * Set the node namespace.
      *
      * The namespace definition is searched up in the hierarchy of nodes. If
      * a namespace with the given prefix is not found then throw an exception.
      *
      * @param prefix
      *  Namespace prefix. For the default namespace use NULL or empty string.
      * @return  Unsafe namespace
      * @author Sergey Satskiy, NCBI
     **/
    //####################################################################
    ns set_namespace (const char* prefix);

    //####################################################################
    /**
      * Add namespace definition to the node.
      *
      * If the node already has a namespace definition with the same
      * prefix then its URI will be replaced with the new one, and that's it.
      * Otherwise, the hierarchy of nodes (including their attributes) is
      * walked down, updating all namespaces (with the same prefix) which do
      * not use namespace definitions (with the same prefix) which are
      * redefined below this node.
      *
      * @param name_space
      *  The namespace definition to add to the node.
      * @param type
      *  What to do (replace or throw exception) when encountering a
      *  namespace definition with the same prefix.
      * @return  Unsafe namespace
      * @author Sergey Satskiy, NCBI
     **/
    //####################################################################
    ns add_namespace_definition (const ns&                 name_space,
                                 ns_definition_adding_type type);

    //####################################################################
    /**
      * Add namespace definitions to the node.
      *
      * @sa add_namespace_definition
      *
      * @param name_spaces
      *  List of namespace definitions to add to the node.
      * @param type
      *  What to do (replace or throw exception) when encountering a
      *  namespace definition with the same prefix.
      * @author Sergey Satskiy, NCBI
     **/
    //####################################################################
    void add_namespace_definitions (const ns_list_type&       name_spaces,
                                    ns_definition_adding_type type);

    //####################################################################
    /**
      * Remove the node namespace definition.
      *
      * @param prefix
      *  The prefix of the namespace to be removed from the node namespace
      *  definitions.
      *  For the default namespace use NULL or empty string.
      *  If there is no such namespace definition, then do nothing.
      * @param how
      *  Specifies what to do if the given namespace is in use.
      * @author Sergey Satskiy, NCBI
     **/
    //####################################################################
    void erase_namespace_definition (const char* prefix,
                                     ns_definition_erase_type how =
                                         type_ns_def_erase_if_not_used);

    //####################################################################
    /**
      * Remove the node namespace.
      *
      * The hierarchy of nodes is searched up and if a default namespace is
      * found then it is used as a new node namespace.
      *
      * @author Sergey Satskiy, NCBI
     **/
    //####################################################################
    void erase_namespace (void);

    //####################################################################
    /**
      * Look up a namespace with the given prefix.
      *
      * Walk the nodes hierarchy up and check the namespace definition
      * prefixes. If the prefix matches, then return the
      * corresponding safe/unsafe namespace object.
      *
      * @param prefix
      *  Namespace prefix to look for.
      *  For the default namespace use NULL or empty string.
      * @param type
      *  Type of namespace object (safe/unsafe) to return.
      * @return
      *  Namespace object ("void" namespace if none found).
      * @author Sergey Satskiy, NCBI
     **/
    //####################################################################
    ns lookup_namespace (const char*        prefix,
                         ns::ns_safety_type type = ns::type_safe_ns) const;

    //####################################################################
    /**
      * Erase duplicate namespace definitions.
      *
      * Walks the nodes hierarchy down and erases dulicate namespace
      * definitions.
      *
      * @author Sergey Satskiy, NCBI
     **/
    //####################################################################
    void erase_duplicate_ns_defs (void);

    //####################################################################
    /**
      * Erase unused namespace definitions.
      *
      * Walks the nodes hierarchy down and erases unused namespace
      * definitions.
      *
      * @author Sergey Satskiy, NCBI
     **/
    //####################################################################
    void erase_unused_ns_defs (void);

    //####################################################################
    /**
      * Get the node path.
      *
      * @return node path
      * @exception throw an exception in case of errors
      * @author Sergey Satskiy, NCBI
     **/
    //####################################################################
    std::string get_path (void) const;

    //####################################################################
    /**
     * Find out if this node is a text node or sometiming like a text node,
     * CDATA for example.
     *
     * @return True if this node is a text node; false otherwise.
     * @author Peter Jones
    **/
    //####################################################################
    bool is_text (void) const;

    //####################################################################
    /**
     * Add a child xml::node to this node.
     *
     * @param child The child xml::node to add.
     * @author Peter Jones
    **/
    //####################################################################
    void push_back (const node &child);

    //####################################################################
    /**
     * Swap this node with another one.
     *
     * @param other The other node to swap with.
     * @author Peter Jones
    **/
    //####################################################################
    void swap (node &other);

    class const_iterator; // forward declaration

    /**
     * The xml::node::iterator provides a way to access children nodes
     * similar to a standard C++ container. The nodes that are pointed to by
     * the iterator can be changed.
     */
    class iterator {
    public:
        typedef node value_type;
        typedef std::ptrdiff_t difference_type;
        typedef value_type* pointer;
        typedef value_type& reference;
        typedef std::forward_iterator_tag iterator_category;

        iterator  (void) : pimpl_(0) {}
        iterator  (const iterator &other);
        iterator& operator= (const iterator& other);
        ~iterator (void);

        reference operator*  (void) const;
        pointer   operator-> (void) const;

        /// prefix increment
        iterator& operator++ (void);

        /// postfix increment (avoid if possible for better performance)
        iterator  operator++ (int);

        bool operator==(const iterator& other) const
        { return get_raw_node() == other.get_raw_node(); }
        bool operator!=(const iterator& other) const
        { return !(*this == other); }

    private:
        impl::nipimpl *pimpl_;
        explicit iterator (void *data);
        void* get_raw_node (void) const;
        void swap (iterator &other);
        friend class node;
        friend class document;
        friend class const_iterator;
    };

    /**
     * The xml::node::const_iterator provides a way to access children nodes
     * similar to a standard C++ container. The nodes that are pointed to by
     * the const_iterator cannot be changed.
     */
    class const_iterator {
    public:
        typedef const node value_type;
        typedef std::ptrdiff_t difference_type;
        typedef value_type* pointer;
        typedef value_type& reference;
        typedef std::forward_iterator_tag iterator_category;

        const_iterator  (void) : pimpl_(0) {}
        const_iterator  (const const_iterator &other);
        const_iterator  (const iterator &other);
        const_iterator& operator= (const const_iterator& other);
        ~const_iterator (void);

        reference operator*  (void) const;
        pointer   operator-> (void) const;

        /// prefix increment
        const_iterator& operator++ (void);

        /// postfix increment (avoid if possible for better performance)
        const_iterator  operator++ (int);

        bool operator==(const const_iterator& other) const
        { return get_raw_node() == other.get_raw_node(); }
        bool operator!=(const const_iterator& other) const
        { return !(*this == other); }
    private:
        impl::nipimpl *pimpl_;
        explicit const_iterator (void *data);
        void* get_raw_node (void) const;
        void swap (const_iterator &other);
        friend class document;
        friend class node;
    };

    //####################################################################
    /**
     * Returns the number of childer this nodes has. If you just want to
     * know how if this node has children or not, you should use
     * xml::node::empty() instead.
     *
     * @return The number of children this node has.
     * @author Peter Jones
    **/
    //####################################################################
    size_type size (void) const;

    //####################################################################
    /**
     * Find out if this node has any children. This is the same as
     * xml::node::size() == 0 except it is much faster.
     *
     * @return True if this node DOES NOT have any children.
     * @return False if this node does have children.
     * @author Peter Jones
    **/
    //####################################################################
    bool empty (void) const;

    //####################################################################
    /**
     * Get an iterator that points to the beginning of this node's children.
     *
     * @return An iterator that points to the beginning of the children.
     * @author Peter Jones
    **/
    //####################################################################
    iterator begin (void);

    //####################################################################
    /**
     * Get a const_iterator that points to the beginning of this node's
     * children.
     *
     * @return A const_iterator that points to the beginning of the children.
     * @author Peter Jones
    **/
    //####################################################################
    const_iterator begin (void) const;

    //####################################################################
    /**
     * Get an iterator that points one past the last child for this node.
     *
     * @return A "one past the end" iterator.
     * @author Peter Jones
    **/
    //####################################################################
    iterator end (void) { return iterator(); }

    //####################################################################
    /**
     * Get a const_iterator that points one past the last child for this
     * node.
     *
     * @return A "one past the end" const_iterator
     * @author Peter Jones
    **/
    //####################################################################
    const_iterator end (void) const { return const_iterator(); }

    //####################################################################
    /**
     * Get an iterator that points back at this node.
     *
     * @return An iterator that points at this node.
     * @author Peter Jones
    **/
    //####################################################################
    iterator self (void);

    //####################################################################
    /**
     * Get a const_iterator that points back at this node.
     *
     * @return A const_iterator that points at this node.
     * @author Peter Jones
    **/
    //####################################################################
    const_iterator self (void) const;

    //####################################################################
    /**
     * Find out if this node is a root one, i.e. has no parent.
     *
     * @return true if the node is root.
     * @author Sergey Satskiy
    **/
    //####################################################################
    bool is_root (void) const;

    //####################################################################
    /**
     * Get an iterator that points at the parent of this node. If this node
     * does not have a parent, this member function will return an "end"
     * iterator.
     *
     * @note
     *  It is recommended to call is_root() function before calling parent().
     *  If is_root() returns true then the parent() provided iterator cannot
     *  be dereferenced.
     *
     * @return An iterator that points to this nodes parent.
     * @return If no parent, returns the same iterator that xml::node::end() returns.
     * @author Peter Jones
    **/
    //####################################################################
    iterator parent (void);

    //####################################################################
    /**
     * Get a const_iterator that points at the parent of this node. If this
     * node does not have a parent, this member function will return an
     * "end" const_iterator.
     *
     * @note
     *  It is recommended to call is_root() function before calling parent().
     *  If is_root() returns true then the parent() provided iterator cannot
     *  be dereferenced.
     *
     * @return A const_iterator that points to this nodes parent.
     * @return If no parent, returns the same const_iterator that xml::node::end() returns.
     * @author Peter Jones
    **/
    //####################################################################
    const_iterator parent (void) const;

    //####################################################################
    /**
     * Find the first child node that has the given name and namespace.
     * If no such node can be found, this function will return the same
     * iterator that end() would return.
     *
     * This function is not recursive. That is, it will not search down the
     * tree for the requested node. Instead, it will only search one level
     * deep, only checking the children of this node.
     *
     * @param name The name of the node you want to find.
     * @param nspace The namespace of the node to find. NULL matches
     *               any namespace. Void namespace matches node without
     *               namespace set.
     * @return An iterator that points to the node if found.
     * @return An end() iterator if the node was not found.
     * @author Peter Jones; Sergey Satskiy, NCBI
     *
     * @see elements(const char*), find(const char*, iterator)
    **/
    //####################################################################
    iterator find (const char *name, const ns *nspace=NULL);

    //####################################################################
    /**
     * Find the first child node that has the given name and namespace.
     * If no such node can be found, this function will return the same
     * const_iterator that end() would return.
     *
     * This function is not recursive. That is, it will not search down the
     * tree for the requested node. Instead, it will only search one level
     * deep, only checking the children of this node.
     *
     * @param name The name of the node you want to find.
     * @param nspace The namespace of the node to find. NULL matches
     *               any namespace. Void namespace matches node without
     *               namespace set.
     * @return A const_iterator that points to the node if found.
     * @return An end() const_iterator if the node was not found.
     * @author Peter Jones; Sergey Satskiy, NCBI
     *
     * @see elements(const char*) const, find(const char*, const_iterator) const
    **/
    //####################################################################
    const_iterator find (const char *name, const ns *nspace=NULL) const;

    //####################################################################
    /**
     * Find the first child node, starting with the given iterator, that has
     * the given name and namespace. If no such node can be found, this
     * function will return the same iterator that end() would return.
     *
     * This function should be given an iterator to one of this node's
     * children. The search will begin with that node and continue with all
     * its sibliings. This function will not recurse down the tree, it only
     * searches in one level.
     *
     * @param name The name of the node you want to find.
     * @param start Where to begin the search.
     * @param nspace The namespace of the node to find. NULL matches
     *               any namespace. Void namespace matches node without
     *               namespace set.
     * @return An iterator that points to the node if found.
     * @return An end() iterator if the node was not found.
     * @author Peter Jones; Sergey Satskiy, NCBI
     *
     * @see elements(const char*)
    **/
    //####################################################################
    iterator find (const char *name, const iterator& start, const ns *nspace=NULL);

    //####################################################################
    /**
     * Find the first child node, starting with the given const_iterator,
     * that has the given name and namespace. If no such node can be found,
     * this function will return the same const_iterator that end() would
     * return.
     *
     * This function should be given a const_iterator to one of this node's
     * children. The search will begin with that node and continue with all
     * its siblings. This function will not recurse down the tree, it only
     * searches in one level.
     *
     * @param name The name of the node you want to find.
     * @param start Where to begin the search.
     * @param nspace The namespace of the node to find. NULL matches
     *               any namespace. Void namespace matches node without
     *               namespace set.
     * @return A const_iterator that points to the node if found.
     * @return An end() const_iterator if the node was not found.
     * @author Peter Jones; Sergey Satskiy, NCBI
     *
     * @see elements(const char*) const
    **/
    //####################################################################
    const_iterator find (const char *name, const const_iterator& start,
                         const ns *nspace=NULL) const;

    /**
     * Run the given XPath query.
     *
     * @param expr
     *  XPath expression to run
     * @return
     *  XPath query result nodes set
     * @attention
     *  Expressions like "root/node" will result in 0 matches even if the
     *  document has <root><node/></root>, due to a bug in libxml2 (at least
     *  till version 2.9.1). The workaround is to use "/root/node" or
     *  "//root/node" depending on circumstances.
     * @note
     *  If the query result is a scalar value (e.g. count() function) then
     *  the result set will have a single node of the following format:
     *  <xpath_scalar_result type="TYPE">VALUE</xpath_scalar_result>
     *  where TYPE is one of the following: boolean, number, or
     *  string depending on the result type. The VALUE is the actual result
     *  scalar value.
     * @author Sergey Satskiy, NCBI
    **/
    node_set run_xpath_query (const xpath_expression& expr);

    /**
     * Run the given XPath query.
     *
     * @param expr
     *  XPath expression to run
     * @return
     *  XPath query const result nodes set
     * @attention
     *  Expressions like "root/node" will result in 0 matches even if the
     *  document has <root><node/></root>, due to a bug in libxml2 (at least
     *  till version 2.9.1). The workaround is to use "/root/node" or
     *  "//root/node" depending on circumstances.
     * @note
     *  If the query result is a scalar value (e.g. count() function) then
     *  the result set will have a single node of the following format:
     *  <xpath_scalar_result type="TYPE">VALUE</xpath_scalar_result>
     *  where TYPE is one of the following: boolean, number, or
     *  string depending on the result type. The VALUE is the actual result
     *  scalar value.
     * @author Sergey Satskiy, NCBI
    **/
    const node_set run_xpath_query (const xpath_expression& expr) const;

    /**
     * Run the given XPath query.
     * The method collects all the effective namespace definitions for the node
     * and register them automatically before running the query.
     *
     * @param expr
     *  XPath expression to run, must not be NULL
     * @return
     *  XPath query result nodes set
     * @attention
     *  Expressions like "root/node" will result in 0 matches even if the
     *  document has <root><node/></root>, due to a bug in libxml2 (at least
     *  till version 2.9.1). The workaround is to use "/root/node" or
     *  "//root/node" depending on circumstances.
     * @exception
     *  Throws exceptions in case of problems
     * @note
     *  Default namespace, if so, will not be registered
     * @note
     *  If the query result is a scalar value (e.g. count() function) then
     *  the result set will have a single node of the following format:
     *  <xpath_scalar_result type="TYPE">VALUE</xpath_scalar_result>
     *  where TYPE is one of the following: boolean, number, or
     *  string depending on the result type. The VALUE is the actual result
     *  scalar value.
     * @author Sergey Satskiy, NCBI
    **/
    node_set run_xpath_query (const char *  expr);

    /**
     * Run the given XPath query.
     * The method collects all the effective namespace definitions for the node
     * and register them automatically before running the query.
     *
     * @param expr
     *  XPath expression to run, must not be NULL
     * @return
     *  XPath query const result nodes set
     * @attention
     *  Expressions like "root/node" will result in 0 matches even if the
     *  document has <root><node/></root>, due to a bug in libxml2 (at least
     *  till version 2.9.1). The workaround is to use "/root/node" or
     *  "//root/node" depending on circumstances.
     * @exception
     *  Throws exceptions in case of problems
     * @note
     *  Default namespace, if so, will not be registered
     * @note
     *  If the query result is a scalar value (e.g. count() function) then
     *  the result set will have a single node of the following format:
     *  <xpath_scalar_result type="TYPE">VALUE</xpath_scalar_result>
     *  where TYPE is one of the following: boolean, number, or
     *  string depending on the result type. The VALUE is the actual result
     *  scalar value.
     * @author Sergey Satskiy, NCBI
    **/
    const node_set run_xpath_query (const char *  expr) const;

    //####################################################################
    /**
     * Insert a new child node. The new node will be inserted at the end of
     * the child list. This is similar to the xml::node::push_back member
     * function except that an iterator to the inserted node is returned.
     *
     * @param n The node to insert as a child of this node.
     * @return An iterator that points to the newly inserted node.
     * @author Peter Jones
    **/
    //####################################################################
    iterator insert (const node &n);

    //####################################################################
    /**
     * Insert a new child node. The new node will be inserted before the
     * node pointed to by the given iterator.
     *
     * @param position An iterator that points to the location where the new
     *                 node should be inserted (before it).
     * @param n The node to insert as a child of this node.
     * @return An iterator that points to the newly inserted node.
     * @author Peter Jones
    **/
    //####################################################################
    iterator insert (const iterator& position, const node &n);

    //####################################################################
    /**
     * Replace the node pointed to by the given iterator with another node.
     * The old node will be removed, including all its children, and
     * replaced with the new node. This will invalidate any iterators that
     * point to the node to be replaced, or any pointers or references to
     * that node.
     *
     * @param old_node An iterator that points to the node that should be removed.
     * @param new_node The node to put in old_node's place.
     * @return An iterator that points to the new node.
     * @author Peter Jones
    **/
    //####################################################################
    iterator replace (const iterator& old_node, const node &new_node);

    //####################################################################
    /**
     * Erase the node that is pointed to by the given iterator. The node
     * and all its children will be removed from this node. This will
     * invalidate any iterators that point to the node to be erased, or any
     * pointers or references to that node.
     *
     * @param to_erase An iterator that points to the node to be erased.
     * @return An iterator that points to the node after the one being erased.
     * @author Peter Jones
     * @author Gary A. Passero
    **/
    //####################################################################
    iterator erase (const iterator& to_erase);

    //####################################################################
    /**
     * Erase all nodes in the given range, from frist to last. This will
     * invalidate any iterators that point to the nodes to be erased, or any
     * pointers or references to those nodes.
     *
     * @param first The first node in the range to be removed.
     * @param last An iterator that points one past the last node to erase. Think xml::node::end().
     * @return An iterator that points to the node after the last one being erased.
     * @author Peter Jones
    **/
    //####################################################################
    iterator erase (iterator first, const iterator& last);

    //####################################################################
    /**
     * Erase all children nodes with the given name. This will find all
     * nodes that have the given node name and remove them from this node.
     * This will invalidate any iterators that point to the nodes to be
     * erased, or any pointers or references to those nodes.
     *
     * @param name The name of nodes to remove.
     * @return The number of nodes removed.
     * @author Peter Jones
    **/
    //####################################################################
    size_type erase (const char *name);

    //####################################################################
    /**
     * Erase all children nodes.
     *
     * @author tbrowder2
    */
    //####################################################################
    void clear (void);

    //####################################################################
    /**
     * Sort all the children nodes of this node using one of thier
     * attributes. Only nodes that are of xml::node::type_element will be
     * sorted, and they must have the given node_name.
     *
     * The sorting is done by calling std::strcmp on the value of the given
     * attribute.
     *
     * @param node_name The name of the nodes to sort.
     * @param attr_name The attribute to sort on.
     * @author Peter Jones
    **/
    //####################################################################
    void sort (const char *node_name, const char *attr_name);

    //####################################################################
    /**
     * Sort all the children nodes of this node using the given comparison
     * function object. All element type nodes will be considered for
     * sorting.
     *
     * @param compare The binary function object to call in order to sort all child nodes.
     * @author Peter Jones
    **/
    //####################################################################
    template <typename T> void sort (T compare)
    { impl::sort_callback<T> cb(compare); sort_fo(cb); }

    //####################################################################
    /**
     * Convert the node and all its children into XML text and set the given
     * string to that text.
     *
     * @param xml The string to set the node's XML data to.
     * @param flags
     *        Bitwise mask of the save options. Does not affect XSLT result.
     *        documents.
     * @see xml::save_option
     * @note compression part of the options is currently ignored.
     * @author Peter Jones and Sergey Satskiy, NCBI
    **/
    //####################################################################
    void node_to_string (std::string &xml,
                         save_option_flags flags=save_op_default) const;

    //####################################################################
    /**
     * Write a node and all of its children to the given stream.
     *
     * @param stream The stream to write the node as XML.
     * @param n The node to write to the stream.
     * @return The stream.
     * @author Peter Jones
    **/
    //####################################################################
    friend std::ostream& operator<< (std::ostream &stream, const node &n);

    //####################################################################
    /**
     * Construct a new xml::node by copying another xml::node.
     *
     * @param other The other node to copy.
     * @author Peter Jones
     * @deprecated
    **/
    //####################################################################
    NCBI_DEPRECATED
    node (const node &other);

    //####################################################################
    /**
     * Make this node equal to some other node via assignment.
     *
     * @param other The other node to copy.
     * @return A reference to this node.
     * @author Peter Jones
     * @deprecated
    **/
    //####################################################################
    NCBI_DEPRECATED
    node& operator= (const node &other);

private:
    impl::node_impl *pimpl_;

    // private ctor to create uninitialized instance
    explicit node (int);

    void set_node_data (void *data);
    void* get_node_data (void) const;
    void* release_node_data (void);
    node_set convert_to_nset(void *) const;
    friend class tree_parser;
    friend class impl::node_iterator;
    friend class document;
    friend class xslt::xpath_object;
    friend struct impl::doc_impl;
    friend struct impl::node_cmp;
    friend class xslt::extension_element;

    friend struct impl::node_private_data *  impl::attach_node_private_data(void *);

    void sort_fo (impl::cbfo_node_compare &fo);

    // XML namespaces support
    ns add_namespace_def (const char* uri, const char* prefix);
    ns add_matched_namespace_def (void* libxml2RawNamespace, const char* uri,
                                  ns_definition_adding_type type);
    void erase_duplicate_ns_defs (void* nd, std::deque<ns_list_type>& defs);
    void erase_duplicate_ns_defs_single_node (void* nd, std::deque<ns_list_type>& defs);
    void erase_unused_ns_defs (void* nd);
    ns_list_type get_namespace_definitions (void* nd, ns::ns_safety_type type) const;
    void* find_replacement_ns_def (std::deque<ns_list_type>& defs, void* ns);

    // XML XPath support
    void* create_xpath_context (const xml::xpath_expression& expr) const;
    void* evaluate_xpath_expression (const xml::xpath_expression& expr, void* context) const;
    ns_list_type get_effective_namespaces (bool excludeDefault = false) const;

    // XSLT extensions support
    friend void ::xslt_ext_func_cb(void *, int);
    friend void ::xslt_ext_element_cb(void*, void*, void*, void*);

}; // end xml::node class

} // end xml namespace
#endif
