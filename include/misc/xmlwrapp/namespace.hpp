/*
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
 */

/** @file
 * This file contains the definition of the xml::ns class.
**/

#ifndef _xmlwrapp_ns_hpp_
#define _xmlwrapp_ns_hpp_

// standard includes
#include <string>


namespace xml {


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

public:

    /**
     * Create a new xml::ns object with the given prefix and URI.
     * The created object is safe.
     *
     * @param prefix
     *  The namespace prefix. Use NULL or empty string for default namespace.
     * @param uri
     *  The namespace URI.
     * @author Sergey Satskiy, NCBI
    **/
    ns (const char *  prefix, const char *  uri);


    /**
     * Get the namespace prefix.
     *
     * @return The namespace prefix.
     * @note
     *  The lifetime of the returned string is either the lifetime of the
     *  namespace object itself (if it's a "safe" object) or the lifetime
     *  of the underlying libxml2 document (if it's an "unsafe" object). 
     * @author Sergey Satskiy, NCBI
    **/
    const char *  get_prefix (void) const;


    /**
     * Get the namespace URI.
     *
     * @return The namespace URI.
     * @note
     *  The lifetime of the returned string is either the lifetime of the
     *  namespace object itself (if it's a "safe" object) or the lifetime
     *  of the underlying libxml2 document (if it's an "unsafe" object). 
     * @author Sergey Satskiy, NCBI
    **/
    const char *  get_uri (void) const;


    /**
     * If a node or an attribute has no namespace, then a namespace
     * with empty prefix and empty URI is returned to the user. The
     * method checks if the namespace is actually set.
     *
     * @return TRUE if the namespace is void
     * @author Sergey Satskiy, NCBI
    **/
    bool is_void (void) const;


    /**
     * Convert the namespace object to a safe one (i.e. the object will
     * hold copies of prefix and URI and will not be unsafely linked to
     * internal libxml2 structures).
     * If the object is safe already the function does nothing.
     *
     * @author Sergey Satskiy, NCBI
    **/
    void make_safe (void);


    /**
     * Check if the object is safe i.e. holds its own copies of prefix and URI.
     *
     * @return TRUE if the object is safe
     * @author Sergey Satskiy, NCBI
    **/
    bool is_safe (void) const;


    /**
     * Compare with another namespace.
     *
     * @return TRUE if the namespaces have the equal prefixes and URIs.
     * @author Sergey Satskiy, NCBI
    **/
    bool operator==(const ns &  other) const;


    // Create a "void" xml::ns object -- with both prefix and URI empty.
    // The created object is safe. This is a rather unusual operation which
    // is used by the XmlWrapp internal code.
    enum ns_type {
        type_void
    };
    explicit ns (enum  ns_type  type);


    // The default copy constructor and operator=
    // are just fine for the xml::ns class


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
     * @author Sergey Satskiy, NCBI
    **/
    ns (void * rawLibXML2Namespace);

private:
    std::string             prefix_;     // namespace prefix
    std::string             uri_;        // namespace URI
    void *                  unsafe_ns_;  // pointer to the libxml2 structure
    enum ns_safety_type     safety_;     // object type: safe/unsafe

    friend class node;
    friend class attributes;
};


} // xml namespace

#endif

