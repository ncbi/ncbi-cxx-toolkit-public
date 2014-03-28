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
 * This file contains the implementation of the xml::schema class.
**/

// xmlwrapp includes
#include <misc/xmlwrapp/schema.hpp>
#include <misc/xmlwrapp/document.hpp>
#include <misc/xmlwrapp/exception.hpp>
#include "document_impl.hpp"
#include "utility.hpp"

// standard includes
#include <stdexcept>
#include <string.h>
#include <memory>

// libxml includes
#include <libxml/xmlschemas.h>


using namespace xml;
using namespace xml::impl;


namespace {
    extern "C" void cb_schema_error (void *v, const char *message, ...);
    extern "C" void cb_schema_warning (void *v, const char *message, ...);
}


struct xml::impl::schema_impl {
    schema_impl (void) : schema_(NULL) {}

    xmlSchemaPtr schema_;
};


schema::schema (const char* filename,
                error_messages* messages,
                warnings_as_errors_type how) : pimpl_(NULL) {
    if (!filename)
        throw xml::exception("invalid file name");

    std::auto_ptr<schema_impl> ap(pimpl_ = new schema_impl);
    error_messages *    temp(messages);
    std::auto_ptr<error_messages>   msgs;
    if (!messages)
        msgs.reset(temp = new error_messages);

    construct(filename, (size_type)(-1), temp, how);
    ap.release();
}

schema::schema (const char* data, size_type size,
                error_messages* messages,
                warnings_as_errors_type how) : pimpl_(NULL) {
    if (!data)
        throw xml::exception("invalid data pointer");

    std::auto_ptr<schema_impl> ap(pimpl_ = new schema_impl);
    error_messages *    temp(messages);
    std::auto_ptr<error_messages>   msgs;
    if (!messages)
        msgs.reset(temp = new error_messages);

    construct(data, size, temp, how);
    ap.release();
}

bool schema::validate (const document& doc,
                       error_messages* messages,
                       warnings_as_errors_type how) const
{
    xmlSchemaValidCtxtPtr ctxt;
    if ((ctxt = xmlSchemaNewValidCtxt(pimpl_->schema_)) == 0) {
        throw std::bad_alloc();
    }

    error_messages* temp(messages);
    std::auto_ptr<error_messages>   msgs;
    if (!messages)
        msgs.reset(temp = new error_messages);
    else
        messages->get_messages().clear();
    xmlSchemaSetValidErrors(ctxt, cb_schema_error,
                                  cb_schema_warning,
                                  temp);

    int retCode = xmlSchemaValidateDoc(ctxt, doc.pimpl_->doc_);
    xmlSchemaFreeValidCtxt(ctxt);

    if (retCode == -1)
        throw xml::exception("internal libxml2 API error");

    // There are errors
    if (temp->has_errors())
        return false;

    // There are warnings and they are treated as errors
    if (temp->has_warnings()) {
        if (how == type_warnings_are_errors)
            return false;
    }

    return true;
}

// Helper constructor.
// Two public constructors bodies differ only in the way of creating the parser
// context. So this function is introduced.
void schema::construct (const char* file_or_data, size_type size,
                        error_messages* messages,
                        warnings_as_errors_type how) {
    xmlSchemaParserCtxtPtr ctxt;

    // Create a context depending on where data come from - a file or memory
    if (size == (size_type)(-1)) {
        // This is a file parsing request
        if ((ctxt = xmlSchemaNewParserCtxt(file_or_data)) == NULL) {
            throw std::bad_alloc();
        }
    } else {
        // This is a memory parsing request
        ctxt = xmlSchemaNewMemParserCtxt(file_or_data,
                                         size_t_to_int_conversion(size,
                                            "memory buffer is too large"));
        if (ctxt == NULL)
            throw std::bad_alloc();
    }

    messages->get_messages().clear();
    xmlSchemaSetParserErrors(ctxt, cb_schema_error,
                                   cb_schema_warning,
                                   messages);
    pimpl_->schema_ = xmlSchemaParse(ctxt);
    xmlSchemaFreeParserCtxt(ctxt);

    // Fatal errors are impossible here. They may appear for document parser
    // only.
    if (messages->has_errors())
        throw parser_exception(*messages);

    if ((how == type_warnings_are_errors) && messages->has_warnings())
        throw parser_exception(*messages);

    // To be 100% sure that schema was created successfully
    if (pimpl_->schema_ == NULL)
        throw xml::exception("unknown schema parsing error");
}

schema::~schema() {
    if (pimpl_->schema_)
        xmlSchemaFree(pimpl_->schema_);
    delete pimpl_;
}


namespace {
    void register_error_helper (error_message::message_type mt,
                                void *v,
                                const std::string &message) {
        try {
            error_messages *p = static_cast<error_messages*>(v);
            if (p)
                p->get_messages().push_back(error_message(message, mt));
        } catch (...) {}
    }

    extern "C" void cb_schema_error (void *v, const char *message, ...) {
        std::string temporary;

        va_list ap;
        va_start(ap, message);
        printf2string(temporary, message, ap);
        va_end(ap);

        register_error_helper(error_message::type_error, v, temporary);
    }

    extern "C" void cb_schema_warning (void *v, const char *message, ...) {
        std::string temporary;

        va_list ap;
        va_start(ap, message);
        printf2string(temporary, message, ap);
        va_end(ap);

        register_error_helper(error_message::type_warning, v, temporary);
    }
}

