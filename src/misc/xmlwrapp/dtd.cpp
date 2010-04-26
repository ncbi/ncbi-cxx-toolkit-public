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
    dtd_impl (void) : dtd_(NULL) {}

    xmlDtdPtr       dtd_;
    error_messages  dtd_parser_messages_;
    error_messages  dtd_validation_messages_;
};


dtd::dtd () : pimpl_(new dtd_impl)
{}


// There is a hope that libxml2 will be extended eventually and
// will provide the detailed errors and warnings for the parsed
// dtd file.
dtd::dtd (const char* filename,
          warnings_as_errors_type how) : pimpl_(NULL) {
    if (!filename)
        throw xml::exception("invalid file name");
    std::auto_ptr<dtd_impl> ap(pimpl_ = new dtd_impl);

    pimpl_->dtd_ = xmlParseDTD(0, reinterpret_cast<const xmlChar*>(filename));
    if (pimpl_->dtd_ == NULL) {
        // It is a common case that the file does not exist or cannot be
        // opened. libxml2 does not recognise it so make a test here to
        // have a better error message.
        FILE *test(fopen(filename, "r"));
        std::string message;
        if (test == NULL) {
            message = std::string("cannot open DTD ") + filename;
        }
        else {
            fclose(test);
            message = std::string("unable to parse DTD ") + filename;
        }
        error_message msg(message, error_message::type_error);
        pimpl_->dtd_parser_messages_.get_messages().push_back(msg);
        throw parser_exception(pimpl_->dtd_parser_messages_);
    }

    ap.release();
}

dtd::~dtd() {
    if (pimpl_->dtd_)
        xmlFreeDtd(pimpl_->dtd_);
    delete pimpl_;
}

bool dtd::validate (document& doc, warnings_as_errors_type how) {
    xmlValidCtxt    vctxt;

    memset(&vctxt, 0, sizeof(vctxt));
    vctxt.userData = pimpl_;
    vctxt.error    = cb_dtd_error;
    vctxt.warning  = cb_dtd_warning;

    // Clear old messages
    pimpl_->dtd_validation_messages_.get_messages().clear();

    int retCode(0);
    if (pimpl_->dtd_) {
        // External DTD case
        retCode = xmlValidateDtd(&vctxt, doc.pimpl_->doc_, pimpl_->dtd_);
    } else {
        // Internal DTD case
        retCode = xmlValidateDocument(&vctxt, doc.pimpl_->doc_);
    }

    if (retCode == 0)
        return false;
    if (pimpl_->dtd_validation_messages_.has_errors())
        return false;
    if (pimpl_->dtd_validation_messages_.has_warnings()) {
        if (how == type_warnings_are_errors)
            return false;
    }

    // Validation is OK. Update the external subset pointer if required
    if (pimpl_->dtd_ == NULL)
        return true;

    xmlDtdPtr   copy = xmlCopyDtd(pimpl_->dtd_);
    if (copy == NULL)
        throw xml::exception("Error copying DTD");

    if (doc.pimpl_->doc_->extSubset != 0) xmlFreeDtd(doc.pimpl_->doc_->extSubset);
    doc.pimpl_->doc_->extSubset = copy;
    return true;
}

const error_messages& dtd::get_dtd_parser_messages (void) const {
    return pimpl_->dtd_parser_messages_;
}

const error_messages& dtd::get_validation_messages(void) const {
    return pimpl_->dtd_validation_messages_;
}


namespace {
    void register_error_helper (error_message::message_type mt,
                                void *v,
                                const std::string &message) {
        try {
            impl::dtd_impl *p = static_cast<impl::dtd_impl*>(v);
            p->dtd_validation_messages_.get_messages().push_back(error_message(message, mt));
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

