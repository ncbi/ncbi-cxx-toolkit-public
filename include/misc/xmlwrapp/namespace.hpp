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
 * Credits: Denis Vakatov, NCBI (help with the original design)
 *
 */


/** @file
 * XML namespace API for XmlWrapp.
**/

#ifndef _xmlwrapp_ns_hpp_
#define _xmlwrapp_ns_hpp_

// standard includes
#include <string>
#include <vector>

namespace xml {

namespace impl {
    struct ns_util;
}


/**
 * The xml::ns class is used to access and handle namespaces of nodes and
 * attributes.
 *
 * There are two kinds of the object: safe and unsafe. The safe version holds
 * copies of the namespace prefix and URI, and it is not linked to the libxml2
 * data structures. The unsafe version holds a raw pointer to the libxml2 data
 * structures. The unsafe version is faster but it must be used very carefully.
 * Misuse of the unsafe version will lead to an unpredictable behavior. Example
 * of improper usage of unsafe namespace: get an unsafe namespace from one
 * document and use the object to set a namespace of a node in another
 * document (the unsafe namespaces "belong" to a particular document).
**/

class ns
{
public:
    /// Namespace object "safety"
    enum ns_safety_type {
        type_safe_ns,   //< "safe" object - holds prefix and URI copies
        type_unsafe_ns  //< "unsafe" object - holds raw libxml2 pointer
    };

    /**
     * Create a new xml::ns object with the given prefix and URI.
     * The created object is safe.
     *
     * @param prefix
     *  The namespace prefix. Use NULL or empty string for default namespace.
     * @param uri
     *  The namespace URI.
    **/
    ns (const char* prefix, const char* uri);


    /**
     * Moving constructor.
     * @param other The other namespace.
    **/
    ns (ns && other);


    /**
     * Moving assignment.
     * @param other The other namespace.
    **/
    ns &  operator= (ns && other);

    /**
     * Get the namespace prefix.
     *
     * @return The namespace prefix.
     * @note
     *  The lifetime of the returned string is either the lifetime of the
     *  namespace object itself (if it's a "safe" object) or the lifetime
     *  of the underlying libxml2 document (if it's an "unsafe" object).
    **/
    const char* get_prefix (void) const;


    /**
     * Get the namespace URI.
     *
     * @return The namespace URI.
     * @note
     *  The lifetime of the returned string is either the lifetime of the
     *  namespace object itself (if it's a "safe" object) or the lifetime
     *  of the underlying libxml2 document (if it's an "unsafe" object).
    **/
    const char* get_uri (void) const;


    /**
     * If a node or an attribute has no namespace, then a namespace
     * with empty prefix and empty URI is returned to the user. The
     * method checks if the namespace is actually set.
     *
     * @return TRUE if the namespace is void
    **/
    bool is_void (void) const;


    /**
     * Convert the namespace object to a safe one (i.e. the object will
     * hold copies of prefix and URI and will not be unsafely linked to
     * internal libxml2 structures).
     * If the object is safe already the function does nothing.
    **/
    void make_safe (void);


    /**
     * Check if the object is safe i.e. holds its own copies of prefix and URI.
     *
     * @return TRUE if the object is safe
    **/
    bool is_safe (void) const;


    /**
     * Compare with another namespace.
     *
     * @return TRUE if the namespaces have the equal URIs.
     * @note libxml2 compares namespaces basing on URIs so xmlwrapp does.
    **/
    bool operator==(const ns&  other) const;


    /**
     * Compare with another namespace.
     *
     * @return TRUE if the namespaces URIs differ.
     * @note libxml2 compares namespaces basing on URIs so xmlwrapp does.
    **/
    bool operator!=(const ns&  other) const;


    // Create a "void" xml::ns object -- with both prefix and URI empty.
    // The created object is safe. This is a rather unusual operation which
    // is used by the XmlWrapp internal code.
    enum ns_type {
        type_void
    };
    explicit ns (enum  ns_type  type);

    ns (const ns&  other) = default;
    ns &  operator= (const ns&  other) = default;

private:
    /**
     * Use more explicit "ns(enum ns_type)" ctor to create "void" namespace
    **/
    ns (void);

    /**
     * Create a new unsafe xml::ns object with the given pointer.
     * The unsafe objects cannot be created by the user and can be created only
     * by the libxmlwrapp.
     * It is not checked that the pointer is valid.
     *
     * @param rawLibXML2Namespace The libxml2 xmlNs structure pointer.
    **/
    ns (void* rawLibXML2Namespace);

private:
    std::string             prefix_;     // namespace prefix
    std::string             uri_;        // namespace URI
    void *                  unsafe_ns_;  // pointer to the libxml2 structure
    enum ns_safety_type     safety_;     // object type: safe/unsafe

    friend class node;
    friend class attributes;
    friend struct impl::ns_util;
};


/**
 * type for holding XML namespaces
**/
typedef std::vector<xml::ns>  ns_list_type;


} // xml namespace

#endif

