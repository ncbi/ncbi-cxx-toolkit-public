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

#include "utility.hpp"
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>

// libxml2 includes
#include <libxml/xmlversion.h>
#include <libxml/xmlsave.h>

// xmlwrapp includes
#include <misc/xmlwrapp/xml_save.hpp>


// hack to pull in vsnprintf for MSVC
#if defined(_MSC_VER) || (defined(__COMO__) && defined(__WIN32__))
#  undef vsnprintf
#  define vsnprintf _vsnprintf
#endif

namespace xml {

int size_t_to_int_conversion (std::size_t  value,
                              const std::string &  msg)
{
    int     as_int = static_cast<int>(value);

    if (as_int < 0)
        throw xml::exception(msg);
    if (value != static_cast<std::size_t>(as_int))
        throw xml::exception(msg);
    return as_int;
}


namespace impl {

    //####################################################################
    void printf2string (std::string &s, const char *message, va_list ap) {
	char buffer[512];

	std::memset(buffer, 0, sizeof(buffer));

	// XXX vsnprintf is non-standard
	if (vsnprintf(buffer, sizeof(buffer), message, ap) > 0) {
	    std::string::size_type size = std::strlen(buffer);

	    if (buffer[size-1] == '\n') --size;
	    s.assign(buffer, size);
	}
    }
    //####################################################################
    bool ns_util::node_ns_match (xmlNode *nd, const ns *nspace) {
        if (nd == NULL)
            throw exception( "Internal logic error. Node must be supplied to check matching a namespace." );

        // NULL namespace matches everything
        if (nspace == NULL) return true;

        if (nd->ns == NULL)
            return nspace->is_void();

        return ns(nd->ns) == (*nspace);
    }
    //####################################################################
    bool ns_util::attr_ns_match (xmlAttr *at, const ns *nspace) {
        if (at == NULL)
            throw exception( "Internal logic error. Attribute must be supplied to check matching a namespace." );

        // NULL namespace matches everything
        if (nspace == NULL) return true;

        if (at->ns == NULL)
            return nspace->is_void();

        return ns(at->ns) == (*nspace);
    }
    //####################################################################
    bool ns_util::default_attr_ns_match (xmlAttribute *dat, const ns *nspace) {
        if (dat == NULL)
            throw exception( "Internal logic error. Default attribute must be supplied to check matching a namespace." );

        // NULL namespace matches everything
        if (nspace == NULL) return true;

        // Default attributes do not have prefix and uri. They have only
        // prefix.
        if (dat->prefix == NULL)
            return nspace->is_void();

        return strcmp(nspace->get_prefix(), reinterpret_cast<const char*>(dat->prefix)) == 0;
    }
    //####################################################################
    int convert_to_libxml2_save_options (int options) {
        int libxml2_options = 0;

        // The if statements below are to avoid problems if the libxml2
        // enumeration values are extended or change their positions
        if ((options & save_op_no_format) == 0)   libxml2_options |= XML_SAVE_FORMAT;
        if (options & save_op_no_decl)            libxml2_options |= XML_SAVE_NO_DECL;
        if (options & save_op_no_empty)           libxml2_options |= XML_SAVE_NO_EMPTY;
        if (options & save_op_no_xhtml)           libxml2_options |= XML_SAVE_NO_XHTML;

        #if LIBXML_VERSION >= 20702
        if (options & save_op_xhtml)              libxml2_options |= XML_SAVE_XHTML;
        if ((options & save_op_not_as_xml) == 0)  libxml2_options |= XML_SAVE_AS_XML;
        if (options & save_op_as_html)            libxml2_options |= XML_SAVE_AS_HTML;
        #endif

        return libxml2_options;
    }
    //####################################################################
    int save_to_stream_cb (void *ctx, const char *buf, int len) {
        std::ostream *dst = reinterpret_cast<std::ostream*>(ctx);
        dst->write(buf, len);
        return len;
    }
    //####################################################################
    int save_to_string_cb (void *ctx, const char *buf, int len) {
        std::string *dst = reinterpret_cast<std::string*>(ctx);
        dst->append(buf, len);
        return len;
    }
    //####################################################################

} // end impl namespace

} // end xml namespace
