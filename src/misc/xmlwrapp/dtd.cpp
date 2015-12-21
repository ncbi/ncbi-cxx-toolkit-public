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
 * This file contains the implementation of the xml::dtd class.
**/

// xmlwrapp includes
#include "allow_auto_ptr.hpp"
#include <misc/xmlwrapp/dtd.hpp>
#include <misc/xmlwrapp/document.hpp>
#include <misc/xmlwrapp/exception.hpp>
#include "document_impl.hpp"
#include "utility.hpp"

// standard includes
#include <stdexcept>
#include <cstdio>
#include <string.h>
#include <memory>

// libxml includes
#include <libxml/valid.h>


using namespace xml;
using namespace xml::impl;


namespace {
    extern "C" void cb_dtd_error (void *v, const char *message, ...);
    extern "C" void cb_dtd_warning (void *v, const char *message, ...);
}

struct xml::impl::dtd_impl {
    dtd_impl () : dtd_(NULL), owned_(true) {}

    xmlDtdPtr       dtd_;
    bool            owned_;
};


static std::string get_dtd_parsing_error_message(const char*  filename)
{
    FILE *test(fopen(filename, "r"));
    std::string message;
    if (test != NULL) {
        fclose(test);
        return std::string("unable to parse DTD ") + filename;
    }
    return std::string("cannot open DTD ") + filename;
}


// There is a hope that libxml2 will be extended eventually and
// will provide the detailed errors and warnings for the parsed
// dtd file.
dtd::dtd (const char* filename, error_messages* messages,
          warnings_as_errors_type how) : pimpl_(NULL)
{
    if (!filename)
        throw xml::exception("invalid file name");
    std::auto_ptr<dtd_impl> ap(pimpl_ = new dtd_impl);

    if (messages)
        messages->get_messages().clear();

    pimpl_->dtd_ = xmlParseDTD(0, reinterpret_cast<const xmlChar*>(filename));
    if (pimpl_->dtd_ == NULL) {
        // It is a common case that the file does not exist or cannot be
        // opened. libxml2 does not recognise it so make a test here to
        // have a better error message.
        error_messages dtd_parser_messages_;
        error_message msg(get_dtd_parsing_error_message(filename),
                          error_message::type_error,
                          0, filename);
        if (messages)
            messages->get_messages().push_back(msg);
        dtd_parser_messages_.get_messages().push_back(msg);
        throw parser_exception(dtd_parser_messages_);
    }
    ap.release();
}

dtd::dtd () : pimpl_(new dtd_impl)
{}

bool dtd::validate (const document& doc, error_messages* messages,
                    warnings_as_errors_type how) const
{
    if (!pimpl_->dtd_)
        throw xml::exception("dtd has not been loaded");

    error_messages *    temp(messages);
    std::auto_ptr<error_messages>   msgs;
    if (!messages)
        msgs.reset(temp = new error_messages);

    xmlValidCtxt        vctxt;

    memset(&vctxt, 0, sizeof(vctxt));
    vctxt.error    = cb_dtd_error;
    vctxt.warning  = cb_dtd_warning;
    vctxt.userData = temp;

    temp->get_messages().clear();

    int     retCode = xmlValidateDtd(&vctxt, doc.pimpl_->doc_, pimpl_->dtd_);

    if (retCode == 0)
        return false;
    if (static_cast<error_messages*>(vctxt.userData)->has_errors())
        return false;
    if (static_cast<error_messages*>(vctxt.userData)->has_warnings()) {
        if (how == type_warnings_are_errors)
            return false;
    }
    return true;
}

dtd::~dtd() {
    if (pimpl_->owned_ && pimpl_->dtd_)
        xmlFreeDtd(pimpl_->dtd_);
    delete pimpl_;
}

void dtd::set_dtd_data (void *data) {
    pimpl_->owned_ = false;
    pimpl_->dtd_ = static_cast<xmlDtdPtr>(data);
}

// The document needs access to the raw dtd pointer to set external subset
void* dtd::get_raw_pointer (void) const {
    return pimpl_->dtd_;
}

const char* dtd::get_public_id (void) const {
    return reinterpret_cast<const char*>(pimpl_->dtd_->ExternalID);
}

const char* dtd::get_system_id (void) const {
    return reinterpret_cast<const char*>(pimpl_->dtd_->SystemID);
}

const char* dtd::get_name (void) const {
    return reinterpret_cast<const char*>(pimpl_->dtd_->name);
}

namespace {
    void register_error_helper (error_message::message_type mt,
                                void *v,
                                const std::string &message) {
        try {
            error_messages *p = static_cast<error_messages*>(v);
            if (p) {
                int     line = xmlLastError.line;
                if (line < 0)
                    line = 0;
                std::string     filename;
                if (xmlLastError.file != NULL)
                    filename = xmlLastError.file;
                p->get_messages().push_back(error_message(message, mt,
                                                          line, filename));
            }
        } catch (...) {}
    }

    extern "C" void cb_dtd_error (void *v, const char *message, ...) {
        std::string temporary;

        va_list ap;
        va_start(ap, message);
        printf2string(temporary, message, ap);
        va_end(ap);

        register_error_helper(error_message::type_error, v, temporary);
    }

    extern "C" void cb_dtd_warning (void *v, const char *message, ...) {
        std::string temporary;

        va_list ap;
        va_start(ap, message);
        printf2string(temporary, message, ap);
        va_end(ap);

        register_error_helper(error_message::type_warning, v, temporary);
    }
}

