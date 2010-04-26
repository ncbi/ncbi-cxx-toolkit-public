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
 * This file contains the implementation of the xml::ns class.
**/

// xmlwrapp includes
#include <misc/xmlwrapp/namespace.hpp>
#include <misc/xmlwrapp/exception.hpp>

// standard includes
#include <stdexcept>
#include <string.h>

// libxml includes
#include <libxml/tree.h>

using namespace xml;


xml::ns::ns (enum ns::ns_type ) : prefix_(), uri_(),
                                  unsafe_ns_(NULL),
                                  safety_(ns::type_safe_ns)
{
}


xml::ns::ns (const char *  prefix, const char *  uri) : prefix_(prefix ? prefix : ""),
                                                        uri_(uri ? uri : ""),
                                                        unsafe_ns_(NULL),
                                                        safety_(ns::type_safe_ns)
{
    if (uri_.empty())
        throw xml::exception("xml::ns can't have empty uri");
}


xml::ns::ns (void * rawLibXML2Namespace) : prefix_(), uri_(),
                                           unsafe_ns_(rawLibXML2Namespace),
                                           safety_(ns::type_unsafe_ns)
{
}


const char *  xml::ns::get_prefix (void) const
{
    if (safety_ == ns::type_safe_ns)
        return prefix_.c_str();

    if (!unsafe_ns_)
        return prefix_.c_str();

    return reinterpret_cast<xmlNs*>(unsafe_ns_)->prefix
           ? reinterpret_cast<const char*>(reinterpret_cast<xmlNs*>(unsafe_ns_)->prefix)
           : prefix_.c_str();
}


const char *  xml::ns::get_uri (void) const
{
    if (safety_ == ns::type_safe_ns)
        return uri_.c_str();

    if (!unsafe_ns_)
        return uri_.c_str();

    return reinterpret_cast<xmlNs*>(unsafe_ns_)->href
           ? reinterpret_cast<const char*>(reinterpret_cast<xmlNs*>(unsafe_ns_)->href)
           : uri_.c_str();
}


bool xml::ns::is_void (void) const
{
    if (safety_ == ns::type_safe_ns)
        return uri_.empty();

    if (!unsafe_ns_)
        return true;

    return reinterpret_cast<xmlNs*>(unsafe_ns_)->href == NULL;
}


void xml::ns::make_safe (void)
{
    if (safety_ == ns::type_safe_ns)
        return;

    if (unsafe_ns_)
    {
        uri_ = reinterpret_cast<xmlNs*>(unsafe_ns_)->href
               ? std::string(reinterpret_cast<const char*>(reinterpret_cast<xmlNs*>(unsafe_ns_)->href))
               : std::string();
        prefix_ = reinterpret_cast<xmlNs*>(unsafe_ns_)->prefix
                  ? std::string(reinterpret_cast<const char*>(reinterpret_cast<xmlNs*>(unsafe_ns_)->prefix))
                  : std::string();
    }
    unsafe_ns_ = NULL;
    safety_ = ns::type_safe_ns;
}


bool xml::ns::is_safe (void) const
{
    return safety_ == ns::type_safe_ns;
}


bool xml::ns::operator==(const ns &  other) const
{
    return (strcmp(this->get_prefix(), other.get_prefix()) == 0) &&
           (strcmp(this->get_uri(), other.get_uri()) == 0);
}

