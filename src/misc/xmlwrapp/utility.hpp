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

#ifndef _xmlwrapp_utility_h_
#define _xmlwrapp_utility_h_

// standard includes
#include <string>
#include <cstdarg>

// libxml2 includes
#include <libxml/tree.h>

// xmlwrapp includes
#include <misc/xmlwrapp/namespace.hpp>
#include <misc/xmlwrapp/exception.hpp>

namespace xml {

    /*
     * In some cases libxml2 accepts int values for size of objects.
     * XmlWrapp in turn accepts them as size_t.
     * The function checks if a size_t value could be converted into
     * int without loosing the value. It throws xml::exception
     * if the conversion looses the value.
     */
    int size_t_to_int_conversion (std::size_t  value,
                                  const std::string &  msg);


namespace impl {

    /*
     * exception safe wrapper around xmlChar*s that are returned from some
     * of the libxml functions that the user must free.
     */
    class xmlchar_helper {
    public:
        xmlchar_helper (xmlChar *ptr) : ptr_(ptr)
        {}

        ~xmlchar_helper (void)
        { if (ptr_) xmlFree(ptr_); }

        const char* get (void) const
        { return reinterpret_cast<const char*>(ptr_); }
    private:
        xmlChar *ptr_;
    };

    void printf2string (std::string &s, const char *message, va_list ap);

    struct ns_util {
        static bool node_ns_match (xmlNode *nd, const ns *nspace);
        static bool attr_ns_match (xmlAttr *at, const ns *nspace);
        static bool default_attr_ns_match (xmlAttribute *dat, const ns *nspace);
    };

    /*
     * Converts xmlwrapp xml save options to libxml2
     * save options. This is required because the meaning of some bits is
     * opposite in the libraries.
     */
    int convert_to_libxml2_save_options (int options);

    // Callbacks for saving a document
    int save_to_stream_cb (void *ctx, const char *buf, int len);
    int save_to_string_cb (void *ctx, const char *buf, int len);
} // end impl namespace

} // end xml namespace
#endif
