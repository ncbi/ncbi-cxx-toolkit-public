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
 * This file contains the implementation of the xml::name_space class.
**/

// xmlwrapp includes
#include <misc/xmlwrapp/name_space.hpp>

// standard includes
#include <stdexcept>

using namespace xml;

//####################################################################
xml::name_space::name_space (enum name_space::name_space_type ) : prefix_(), uri_() {
}
//####################################################################
xml::name_space::name_space (const char *  uri) : prefix_(), uri_(uri ? uri : "") {
    if (uri_.empty()) throw std::runtime_error("xml::name_space::xml_namespace can't have empty uri");
}
//####################################################################
xml::name_space::name_space (const char *  prefix, const char *  uri) : prefix_(prefix ? prefix : ""), uri_(uri ? uri : "") {
    if (uri_.empty()) throw std::runtime_error("xml::name_space::xml_namespace can't have empty uri");
}
//####################################################################
const char *  xml::name_space::get_prefix (void) const {
    return prefix_.c_str();
}
//####################################################################
const char *  xml::name_space::get_uri (void) const {
    return uri_.c_str();
}
//####################################################################
bool xml::name_space::is_void (void) const {
    return uri_.empty();
}
//####################################################################
bool xml::name_space::operator==(const name_space &  other) const {
    return (prefix_==other.prefix_) && (uri_==other.uri_);
}
//####################################################################
