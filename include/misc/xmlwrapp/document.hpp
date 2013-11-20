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
 * This file contains the definition of the xml::document class.
**/

#ifndef _xmlwrapp_document_h_
#define _xmlwrapp_document_h_

// for NCBI_DEPRECATED
#include <ncbiconf.h>

// xmlwrapp includes
#include <misc/xmlwrapp/xml_init.hpp>
#include <misc/xmlwrapp/node.hpp>
#include <misc/xmlwrapp/errors.hpp>
#include <misc/xmlwrapp/xml_save.hpp>
#include <misc/xmlwrapp/document_proxy.hpp>

// standard includes
#include <iosfwd>
#include <string>
#include <cstddef>

// Forward declaration for a friend below
extern "C" { void xslt_ext_func_cb(void *, int); }
extern "C" { void xslt_ext_element_cb(void*, void*, void*, void*); }

namespace xml {

// forward declarations
class dtd;
class schema;
class tree_parser;

namespace impl {
struct doc_impl;
}

/**
 * The xml::document class is used to hold the XML tree and various bits of
 * information about it.
**/
class document {
public:
    /// size type
    typedef std::size_t size_type;

    //####################################################################
    /**
     * Create a new XML document with the default settings. The new document
     * will contain a root node with a name of "blank".
     *
     * @author Peter Jones
    **/
    //####################################################################
    document (void);

    //####################################################################
    /**
     * Create a new XML document object by parsing the given XML file.
     *
     * @param filename The XML file name.
     * @param messages A pointer to the object where all the warnings are
     *                 collected. If NULL then no messages will be collected.
     * @param how How to treat warnings (default: warnings are not treated as
     *            errors). If warnings are treated as errors then an exception
     *            is thrown in case of both errors and/or warnings. If warnings
     *            are not treated as errors then an exception will be thrown
     *            only when there are errors.
     * @exception Throws xml::parser_exception in case of parsing errors
     *            and std::exception in case of other problems.
     * @author Sergey Satskiy, NCBI
    **/
    document (const char* filename, error_messages* messages,
              warnings_as_errors_type how = type_warnings_not_errors);

    //####################################################################
    /**
     * Create a new XML documant object by parsing the given XML from a
     * memory buffer.
     *
     * @param data The XML memory buffer.
     * @param size Size of the memory buffer.
     * @param messages A pointer to the object where all the warnings are
     *                 collected. If NULL then no messages will be collected.
     * @param how How to treat warnings (default: warnings are not treated as
     *            errors). If warnings are treated as errors then an exception
     *            is thrown in case of both errors and/or warnings. If warnings
     *            are not treated as errors then an exception will be thrown
     *            only when there are errors.
     * @exception Throws xml::parser_exception in case of parsing errors
     *            and std::exception in case of other problems.
     * @author Sergey Satskiy, NCBI
    **/
    document (const char* data, size_type size,
              error_messages* messages,
              warnings_as_errors_type how = type_warnings_not_errors);

    //####################################################################
    /**
     * Create a new XML document and set the name of the root element to the
     * given text.
     *
     * @param root_name What to set the name of the root element to.
     * @author Peter Jones
    **/
    //####################################################################
    explicit document (const char *root_name);

    //####################################################################
    /**
     * Create a new XML document and set the root node.
     *
     * @param n The node to use as the root node. n will be copied.
     * @author Peter Jones
    **/
    //####################################################################
    explicit document (const node &n);

    //####################################################################
    /**
     * Creates a new XML document using the document_proxy, i.e. essentially
     * xslt results. (see CXX-2458)
     *
     * @param doc_proxy XSLT results
     * @author Denis Vakatov
    **/
    //####################################################################
    document (const document_proxy &  doc_proxy);

    //####################################################################
    /**
     * Creates a new XML document by parsing the given XML from a stream.
     *
     * @param stream The stream to read XML document from.
     * @param messages A pointer to the object where all the warnings are
     *                 collected. If NULL then no messages will be collected.
     * @param how How to treat warnings (default: warnings are not treated as
     *            errors). If warnings are treated as errors then an exception
     *            is thrown in case of both errors and/or warnings. If warnings
     *            are not treated as errors then an exception will be thrown
     *            only when there are errors.
     * @exception Throws xml::parser_exception in case of parsing errors
     *            and std::exception in case of other problems.
     * @author Denis Vakatov
    **/
    //####################################################################
    document (std::istream &  stream,
              error_messages *  messages,
              warnings_as_errors_type  how = type_warnings_not_errors);

    //####################################################################
    /**
     * Swap one xml::document object for another.
     *
     * @param other The other document to swap
     * @author Peter Jones
    **/
    //####################################################################
    void swap (document &other);

    //####################################################################
    /**
     * Clean up after an XML document object.
     *
     * @author Peter Jones
    **/
    //####################################################################
    virtual ~document (void);

    //####################################################################
    /**
     * Get a reference to the root node of this document. If no root node
     * has been set, the returned node will be a blank node. You should take
     * caution to use a reference so that you don't copy the whole node
     * tree!
     *
     * @return A const reference to the root node.
     * @author Peter Jones
    **/
    //####################################################################
    const node& get_root_node (void) const;

    //####################################################################
    /**
     * Get a reference to the root node of this document. If no root node
     * has been set, the returned node will be a blank node. You should take
     * caution to use a reference so that you don't copy the whole node
     * tree!
     *
     * @return A reference to the root node.
     * @author Peter Jones
    **/
    //####################################################################
    node& get_root_node (void);

    //####################################################################
    /**
     * Set the root node to the given node. A full copy is made and stored
     * in the document object.
     *
     * @param n The new root node to use.
     * @author Peter Jones
    **/
    //####################################################################
    void set_root_node (const node &n);

    //####################################################################
    /**
     * Get the XML version for this document. For generated documents, the
     * version will be the default. For parsed documents, this will be the
     * version from the XML processing instruction.
     *
     * @return The XML version string for this document.
     * @author Peter Jones
    **/
    //####################################################################
    const std::string& get_version (void) const;

    //####################################################################
    /**
     * Set the XML version number for this document. This version string
     * will be used when generating the XML output.
     *
     * @param version The version string to use, like "1.0".
     * @author Peter Jones
    **/
    //####################################################################
    void set_version (const char *version);

    //####################################################################
    /**
     * Get the XML encoding for this document. The default encoding is
     * ISO-8859-1.
     *
     * @return The encoding string.
     * @author Peter Jones
    **/
    //####################################################################
    const std::string& get_encoding (void) const;

    //####################################################################
    /**
     * Set the XML encoding string. If you don't set this, it will default
     * to ISO-8859-1.
     *
     * @param encoding The XML encoding to use.
     * @author Peter Jones
     * @author Dmitriy Nikitinskiy
    **/
    //####################################################################
    void set_encoding (const char *encoding);

    //####################################################################
    /**
     * Find out if the current document is a standalone document. For
     * generated documents, this will be the default. For parsed documents
     * this will be set based on the XML processing instruction.
     *
     * @return True if this document is standalone.
     * @return False if this document is not standalone.
     * @author Peter Jones
    **/
    //####################################################################
    bool get_is_standalone (void) const;

    //####################################################################
    /**
     * Set the standalone flag. This will show up in the XML output in the
     * correct processing instruction.
     *
     * @param sa What to set the standalone flag to.
     * @author Peter Jones
    **/
    //####################################################################
    void set_is_standalone (bool sa);

    //####################################################################
    /**
     * Walk through the document and expand <xi:include> elements. For more
     * information, please see the w3c recomendation for XInclude.
     * http://www.w3.org/2001/XInclude.
     *
     * The return value of this function may change to int after a bug has
     * been fixed in libxml2 (xmlXIncludeDoProcess).
     *
     * @return False if there was an error with substitutions.
     * @return True if there were no errors (with or without substitutions).
     * @author Peter Jones
     * @author Daniel Evison
    **/
    //####################################################################
    bool process_xinclude (void);

    //####################################################################
    /**
     * Test to see if this document has an internal subset. That is, DTD
     * data that is declared within the XML document itself.
     *
     * @return True if this document has an internal subset.
     * @return False otherwise.
     * @author Peter Jones
    **/
    //####################################################################
    bool has_internal_subset (void) const;

    //####################################################################
    /**
     * Provides the DTD data that is declared within the XML document itself.
     *
     * @return The internal document DTD
     * @exception Throws exception if the document does not have the internal
     *            DTD.
     * @author: Sergey Satskiy, NCBI
    **/
    const dtd& get_internal_subset (void) const;

    //####################################################################
    /**
     * Test to see if this document has an external subset. That is, it
     * references a DTD from an external source, such as a file or URL.
     *
     * @return True if this document has an external subset.
     * @return False otherwise.
     * @author Peter Jones
    **/
    //####################################################################
    bool has_external_subset (void) const;

    //####################################################################
    /**
     * Provides the DTD data that is referenced from an external source, such
     * as a file or URL.
     *
     * @return The external document DTD
     * @exception Throws exception if the document does not have the external
     *            DTD.
     * @author: Sergey Satskiy, NCBI
    **/
    const dtd& get_external_subset (void) const;

    //####################################################################
    /**
     * Sets the document external subset.
     *
     * @param dtd_ use this dtd to set as an external subset
     * @exception Throws exception in case of problems.
     * @author Sergey Satskiy, NCBI
    **/
    void set_external_subset (const dtd& dtd_);

    //####################################################################
    /**
     * Validate this document against the DTD that has been attached to it.
     * This would happen at parse time if there was a !DOCTYPE definition.
     * If the DTD is valid, and the document is valid, this member function
     * will return true.
     *
     * The warnings and error messages are collected in the given
     * xml::error_messages object.
     *
     * @param messages_ A pointer to the object where the warnings and error
     *                  messages are collected. If NULL is passed then no
     *                  messages will be collected.
     * @param how How to treat warnings (default: warnings are treated as
     *            errors). If warnings are treated as errors false is
     *            returned in case of both errors and/or warnings. If warnings
     *            are not treated as errors then false is returned
     *            only when there are errors.
     * @return True if the document is valid.
     * @return False if there was a problem with the XML doc.
     * @author Sergey Satskiy, NCBI
    **/
    bool validate (error_messages *  messages_ = NULL,
                   warnings_as_errors_type how = type_warnings_are_errors) const;

    //####################################################################
    /**
     * Validate this document against the given DTD. If the document is
     * valid, this member function will return true.
     *
     * The warnings and error messages are collected in the given
     * xml::error_messages object.
     *
     * @param dtd_ A DTD constructed from a file or URL or empty DTD. The empty
     *             DTD is for validating the document against the internal DTD.
     * @param messages_ A pointer to the object where the warnings and error
     *                  messages are collected. If NULL is passed then no
     *                  messages will be collected.
     * @param how How to treat warnings (default: warnings are treated as
     *            errors). If warnings are treated as errors false is
     *            returned in case of both errors and/or warnings. If warnings
     *            are not treated as errors then false is returned
     *            only when there are errors.
     * @return True if the document is valid.
     * @return False if there was a problem with the DTD or XML doc.
     * @author Sergey Satskiy, NCBI
    **/
    bool validate (const dtd& dtd_, error_messages* messages_,
                   warnings_as_errors_type how = type_warnings_are_errors) const;

    //####################################################################
    /**
     * Validate this document against the given XSD schema. If the document is
     * valid, this member function will return true.
     *
     * The warnings and error messages are collected in the given
     * xml::error_messages object.
     *
     * @param xsd_schema A constructed XSD schema.
     * @param messages_ A pointer to the object where the warnings and error
     *                  messages are collected. If NULL is passed then no
     *                  messages will be collected.
     * @param how How to treat warnings (default: warnings are treated as
     *            errors). If warnings are treated as errors false is
     *            returned in case of both errors and/or warnings. If warnings
     *            are not treated as errors then false is returned
     *            only when there are errors.
     * @return True if the document is valid.
     * @return False if there was a problem with the XML doc.
     * @author Sergey Satskiy, NCBI
    **/
    bool validate (const schema& schema_, error_messages* messages_,
                   warnings_as_errors_type how = type_warnings_are_errors) const;

    //####################################################################
    /**
     * Returns the number of child nodes of this document. This will always
     * be at least one, since all xmlwrapp documents must have a root node.
     * This member function is useful to find out how many document children
     * there are, including processing instructions, comments, etc.
     *
     * @return The number of children nodes that this document has.
     * @author Peter Jones
    **/
    //####################################################################
    size_type size (void) const;

    //####################################################################
    /**
     * Get an iterator to the first child node of this document. If what you
     * really wanted was the root node (the first element) you should use
     * the get_root_node() member function instead.
     *
     * @return A xml::node::iterator that points to the first child node.
     * @return An end iterator if there are no children in this document
     * @author Peter Jones
    **/
    //####################################################################
    node::iterator begin (void);

    //####################################################################
    /**
     * Get a const_iterator to the first child node of this document. If
     * what you really wanted was the root node (the first element) you
     * should use the get_root_node() member function instead.
     *
     * @return A xml::node::const_iterator that points to the first child node.
     * @return An end const_iterator if there are no children in this document.
     * @author Peter Jones
    **/
    //####################################################################
    node::const_iterator begin (void) const;

    //####################################################################
    /**
     * Get an iterator that points one past the last child node for this
     * document.
     *
     * @return An end xml::node::iterator.
     * @author Peter Jones
    **/
    //####################################################################
    node::iterator end (void);

    //####################################################################
    /**
     * Get a const_iterator that points one past the last child node for
     * this document.
     *
     * @return An end xml::node::const_iterator.
     * @author Peter Jones
    **/
    //####################################################################
    node::const_iterator end (void) const;

    //####################################################################
    /**
     * Add a child xml::node to this document. You should not add a element
     * type node, since there can only be one root node. This member
     * function is only useful for adding processing instructions, comments,
     * etc.. If you do try to add a node of type element, an exception will
     * be thrown.
     *
     * @param child The child xml::node to add.
     * @author Peter Jones
    **/
    //####################################################################
    void push_back (const node &child);

    //####################################################################
    /**
     * Insert a new child node. The new node will be inserted at the end of
     * the child list. This is similar to the xml::node::push_back member
     * function except that an iterator to the inserted node is returned.
     *
     * The rules from the push_back member function apply here. Don't add a
     * node of type element.
     *
     * @param n The node to insert as a child of this document.
     * @return An iterator that points to the newly inserted node.
     * @see xml::document::push_back
     * @author Peter Jones
    **/
    //####################################################################
    node::iterator insert (const node &n);

    //####################################################################
    /**
     * Insert a new child node. The new node will be inserted before the
     * node pointed to by the given iterator.
     *
     * The rules from the push_back member function apply here. Don't add a
     * node of type element.
     *
     * @param position An iterator that points to the location where the new
     *                 node should be inserted (before it).
     * @param n The node to insert as a child of this document.
     * @return An iterator that points to the newly inserted node.
     * @see xml::document::push_back
     * @author Peter Jones
    **/
    //####################################################################
    node::iterator insert (node::iterator position, const node &n);

    //####################################################################
    /**
     * Replace the node pointed to by the given iterator with another node.
     * The old node will be removed, including all its children, and
     * replaced with the new node. This will invalidate any iterators that
     * point to the node to be replaced, or any pointers or references to
     * that node.
     *
     * Do not replace this root node with this member function. The same
     * rules that apply to push_back apply here. If you try to replace a
     * node of type element, an exception will be thrown.
     *
     * @param old_node An iterator that points to the node that should be removed.
     * @param new_node The node to put in old_node's place.
     * @return An iterator that points to the new node.
     * @see xml::document::push_back
     * @author Peter Jones
    **/
    //####################################################################
    node::iterator replace (node::iterator old_node, const node &new_node);

    //####################################################################
    /**
     * Erase the node that is pointed to by the given iterator. The node
     * and all its children will be removed from this node. This will
     * invalidate any iterators that point to the node to be erased, or any
     * pointers or references to that node.
     *
     * Do not remove the root node using this member function. The same
     * rules that apply to push_back apply here. If you try to erase the
     * root node, an exception will be thrown.
     *
     * @param to_erase An iterator that points to the node to be erased.
     * @return An iterator that points to the node after the one being erased.
     * @see xml::document::push_back
     * @author Peter Jones
    **/
    //####################################################################
    node::iterator erase (node::iterator to_erase);

    //####################################################################
    /**
     * Erase all nodes in the given range, from frist to last. This will
     * invalidate any iterators that point to the nodes to be erased, or any
     * pointers or references to those nodes.
     *
     * Do not remove the root node using this member function. The same
     * rules that apply to push_back apply here. If you try to erase the
     * root node, an exception will be thrown.
     *
     * @param first The first node in the range to be removed.
     * @param last An iterator that points one past the last node to erase. Think xml::node::end().
     * @return An iterator that points to the node after the last one being erased.
     * @see xml::document::push_back
     * @author Peter Jones
    **/
    //####################################################################
    node::iterator erase (node::iterator first, node::iterator last);

    //####################################################################
    /**
     * Convert the XML document tree into XML text data and place it into
     * the given string.
     *
     * @param s The string to place the XML text data.
     * @param flags
     *        Bitwise mask of the save options. Does not affect XSLT result.
     *        documents.
     * @see xml::save_option
     * @note compression part of the options is currently ignored.
     * @author Peter Jones and Sergey Satskiy, NCBI
    **/
    //####################################################################
    void save_to_string (std::string &s,
                         save_option_flags flags=save_op_default) const;

    //####################################################################
    /**
     * Convert the XML document tree into XML text data and place it into
     * the given filename.
     *
     * @param filename The name of the file to place the XML text data into.
     * @param flags
     *        Bitwise mask of the save options. Does not affect XSLT result
     *        documents.
     * @see xml::save_option
     * @note compression part of the options is currently ignored.
     * @return True if the data was saved successfully.
     * @return False otherwise.
     * @author Peter Jones and Sergey Satskiy, NCBI
    **/
    //####################################################################
    bool save_to_file (const char *filename,
                       save_option_flags flags=save_op_default) const;

    //####################################################################
    /**
     * Convert the XML document tree into XML text data and then insert it
     * into the given stream.
     *
     * @param stream The stream to insert the XML into.
     * @param flags
     *        Bitwise mask of the save options. Does not affect XSLT result
     *        documents.
     * @note compression part of the options is currently ignored.
     * @see xml::save_option
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    void save_to_stream (std::ostream &stream,
                         save_option_flags flags=save_op_default) const;

    //####################################################################
    /**
     * Convert the XML document tree into XML text data and then insert it
     * into the given stream.
     *
     * @param stream The stream to insert the XML into.
     * @param doc The document to insert.
     * @return The stream from the first parameter.
     * @author Peter Jones
    **/
    //####################################################################
    friend std::ostream& operator<< (std::ostream &stream, const document &doc);

    //####################################################################
    /**
     * Copy construct a new XML document. The new document will be an exact
     * copy of the original.
     *
     * @param other The other document object to copy from.
     * @deprecated
     * @author Peter Jones
    **/
    //####################################################################
    NCBI_DEPRECATED
    document (const document &other);

    //####################################################################
    /**
     * Copy another document object into this one using the assignment
     * operator. This document object will be an exact copy of the other
     * document after the assignement.
     *
     * @param other The document to copy from.
     * @return *this.
     * @deprecated
     * @author Peter Jones
    **/
    //####################################################################
    NCBI_DEPRECATED
    document& operator= (const document &other);

private:
    impl::doc_impl *pimpl_;
    void set_doc_data (void *data);
    void set_doc_data_from_xslt (void *data, void *ssheet);
    void* get_doc_data (void);
    void* get_doc_data_read_only (void) const;
    void* release_doc_data (void);

    bool is_failure (error_messages* messages,
                     warnings_as_errors_type how) const;

    friend class tree_parser;
    friend class xslt::stylesheet;
    friend class schema;
    friend class dtd;
    friend class libxml2_document;
    friend void ::xslt_ext_func_cb(void *, int);
    friend void ::xslt_ext_element_cb(void*, void*, void*, void*);
}; // end xml::document class

// This makes newest Intel compilers happy
std::ostream& operator<< (std::ostream &stream, const document &doc);

} // end xml namespace
#endif
