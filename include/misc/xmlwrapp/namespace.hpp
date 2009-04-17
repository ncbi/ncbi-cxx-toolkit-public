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
 * The xml::ns class is used to access nodes and attributes
 * name spaces.
 * There are two kinds of the object: safe and unsafe. The safe version holds
 * copies of the namespace prefix and uri and is not linked to the libxml2 data
 * structures. The unsafe version does not holds a copy but a raw pointer to
 * the libxml2 data structures. The unsafe version is faster when it is require
 * to set nodes'/attributes' namespaces however must be used very attentively.
 * Misuse of the unsafe version will lead to an unpredictable behavior. Example
 * of improper usage unsafe namespace: get an unsafe namespace from one document
 * and use the object to set a namespace of a node from another document.
**/
class ns {
public:

    /// enum to identify empty namespace explicitly
    enum ns_type {
    type_void       ///< a namespace with both prefix and uri empty
    };

    /// enum to identify ns object safety
    enum ns_safety_type {
    type_safe_ns,   ///< an object is safe, i.e. holds prefix and uri copies
    type_unsafe_ns  ///< an unsafe is unsafe, i.e. holds raw libxml2 pointer
    };

    //####################################################################
    /**
     * Create a new xml::ns object with an empty namespace prefix
     * and with the given uri. It is helpful for a default namespace.
     * The created object is safe.
     *
     * @param uri The namespace uri.
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    explicit ns (const char *  uri);

    //####################################################################
    /**
     * Create a new xml::ns object with the given prefix and uri.
     * The created object is safe.
     *
     * @param prefix The namespace prefix.
     * @param uri The namespace uri.
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    ns (const char *  prefix, const char *  uri);

    //####################################################################
    /**
     * Create a new xml::ns object with both prefix and uri empty.
     * The created object is safe.
     *
     * @param type Dummy parameter to make a void namespace creation
     *             explicit.
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    explicit ns (enum  ns_type  type);

    //####################################################################
    /**
     * Get the namespace prefix.
     *
     * @return The namespace prefix.
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    const char *  get_prefix (void) const;

    //####################################################################
    /**
     * Get the namespace uri.
     *
     * @return The namespace uri.
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    const char *  get_uri (void) const;

    //####################################################################
    /**
     * If a node or an attribute has no namespace then a namespace
     * with both an empty prefix and uri is returned to the user. The
     * method checks if the namespace is actually set.
     *
     * @return true if the namespace is blank i.e. uri is empty
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    bool is_void (void) const;

    //####################################################################
    /**
     * Convert the namespace object to a safe one i.e. the object will
     * hold copies of prefix and uri and will not be unsafely linked to
     * internal libxml2 structures.
     * If the object is safe already the function does nothing.
     *
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    void make_safe (void);

    //####################################################################
    /**
     * Check if the object is safe i.e. holds a copy of the libxml2 prefix and
     * uri.
     *
     * @return true if the object is safe
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    bool is_safe (void) const;

    //####################################################################
    /**
     * Compare with another namespace.
     *
     * @return true if the namespace is equal to the other
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    bool operator==(const ns &  other) const;

    // The default copy constructor and operator=
    // are just fine for the xml::ns class

private:
    ns (void);

    //####################################################################
    /**
     * Create a new unsafe xml::ns object with the given pointer.
     * The unsafe objects cannot be created by the user and can be created only
     * by the libxmlwrapp.
     * It is not checked that the pointer is valid.
     *
     * @param rawLibXML2Namespace The libxml2 xmlNs structure pointer.
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    ns (void * rawLibXML2Namespace);

private:
    std::string             prefix_;        // namespace prefix
    std::string             uri_;           // namespace uri
    void *                  unsafe_ns_;     // unsafe pointer to the libxml2 structure
    enum ns_safety_type     safety_;        // object type: safe/unsafe

    friend class node;
    friend class attributes;
    };

} // xml namespace

#endif

