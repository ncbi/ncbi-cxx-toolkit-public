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
**/
class ns {
public:

    /// enum to identify empty namespace explicitly
    enum ns_type {
    type_void       ///< a namespace with both prefix and uri empty
    };

    //####################################################################
    /**
     * Create a new xml::ns object with an empty namespace prefix
     * and with the given uri. It is helpful for a default namespace.
     *
     * @param uri The namespace uri.
     * @author Sergey Satskiy, NCBI
    **/
    //####################################################################
    explicit ns (const char *  uri);

    //####################################################################
    /**
     * Create a new xml::ns object with the given prefix and uri.
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

private:
    std::string     prefix_;    // namespace prefix
    std::string     uri_;       // namespace uri
    };

} // xml namespace

#endif

