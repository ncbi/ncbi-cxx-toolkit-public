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

#include <string.h>
#include <libxml/xmlIO.h>

#include <misc/xmlwrapp/exception.hpp>
#include <connect/ncbi_socket.h>
#include <connect/ncbi_gnutls.h>
#include <connect/ncbi_conn_stream.hpp>

#include "https_input_impl.hpp"




using namespace ncbi;


namespace {

    struct https_context {
        std::string             uri_;
        CConn_HttpStream *      stream_;
    };

    // See xmlInputMatchCallback
    // uri: filename or URI
    // return: 1 if matched, 0 if another input module should be used
    extern "C" int     https_input_match(const char *  uri);


    // See xmlInputOpenCallback
    // uri: filename or URI
    // return: https input context or NULL in case of errors
    extern "C" void *  https_input_open(const char *  uri);


    // See xmlInputReadCallback
    // context: https input context
    // buffer: the buffer to store data
    // len: the length of the buffer
    // return: the number of bytes read or -1 in case of error
    extern "C" int  https_input_read(void *  context, char *  buffer, int  len);


    // See xmlInputCloseCallback
    // context: https input context
    // return: 0 if OK, -1 in case of errors
    extern "C" int  https_input_close(void *  context);


    // There is no good way for the callbacks above to tell about the
    // errors or warnings. So this is a storage for the custom messages.
    #if defined(_MSC_VER)
        __declspec(thread) xml::error_messages    https_messages;
    #else
        // On Apple C-lang no messages
        #if defined(__APPLE__)  &&  defined(__clang__)
            xml::error_messages    https_messages;
        #else
            thread_local   xml::error_messages    https_messages;
        #endif
    #endif
}



namespace xml {
    namespace impl {
        // Registers xml input
        void    register_https_input(void)
        {
            if (xmlRegisterInputCallbacks(https_input_match,
                                          https_input_open,
                                          https_input_read,
                                          https_input_close) == -1)
                throw xml::exception("Error registering https input");
        }

        void clear_https_messages(void)
        {
            #if !defined(__APPLE__)  ||  !defined(__clang__)
            https_messages.get_messages().clear();
            #endif
        }

        const error_messages &  get_https_messages(void)
        {
            return https_messages;
        }

        void append_https_message(const std::string &             msg,
                                  error_message::message_type     msg_type,
                                  int                             line,
                                  const std::string &             fname)
        {
            #if !defined(__APPLE__)  ||  !defined(__clang__)
            https_messages.get_messages().push_back(
                                    error_message(msg, msg_type,
                                                  line, fname));
            #endif
        }

    }   // namespace impl
}   // namespace xml


namespace {

    extern "C" int     https_input_match(const char *  uri)
    {
        if (!xmlStrncasecmp(BAD_CAST uri, BAD_CAST "https://", 8))
            return 1;
        return 0;
    }


    extern "C" void *  https_input_open(const char *  uri)
    {
        https_context *  context = NULL;

        try {
            context = new https_context;
        } catch (...) {
            xml::impl::append_https_message("Cannot allocate memory for"
                                            " an https IO context",
                                            xml::error_message::type_warning,
                                            0, "");
            return NULL;
        }

        try {
            context->uri_ = std::string(uri);
            context->stream_ = new CConn_HttpStream(uri);
        } catch (const std::exception &  exc) {
            xml::impl::append_https_message(
                "Error creating https stream for URI " + std::string(uri) +
                ": " + std::string(exc.what()),
                xml::error_message::type_warning, 0, "");

            delete context;
            return NULL;
        } catch (...) {
            xml::impl::append_https_message(
                "Unknown error creating https stream for URI " +
                std::string(uri),
                xml::error_message::type_warning, 0, "");

            delete context;
            return NULL;
        }

        return context;
    }


    extern "C" int  https_input_read(void *  context, char *  buffer, int  len)
    {
        https_context *     ctxt = (https_context *) context;

        if (ctxt->stream_->eof())
            return 0;

        try {
            // Note: the eof bit is set together with the fail bit
            // Cases:
            // - good URI, size of content < size of buffer =>
            //   eof == 1, fail == 1, status == 200
            // - broken URI =>
            //   eof == 1, fail == 1, status == 404
            // So it was decided to stick on status codes
            ctxt->stream_->read(buffer, len);
            if (ctxt->stream_->GetStatusCode() != 200) {
                char    status_buffer[64];

                sprintf(status_buffer, "%d", ctxt->stream_->GetStatusCode());
                xml::impl::append_https_message(
                    "Error reading from URI " +
                    ctxt->uri_ + ". Last status: " +
                    std::string(status_buffer) + " (" +
                    ctxt->stream_->GetStatusText() + ")",
                    xml::error_message::type_warning, 0, "");

                return -1;
            }
        } catch (const std::exception &  exc) {
            xml::impl::append_https_message(
                "Error reading from URI " +
                ctxt->uri_ + ": " + std::string(exc.what()),
                xml::error_message::type_warning, 0, "");
            return -1;
        } catch (...) {
            xml::impl::append_https_message(
                "Unknown error reading from URI " +
                std::string(ctxt->uri_),
                xml::error_message::type_warning, 0, "");
            return -1;
        }

        return ctxt->stream_->gcount();
    }


    extern "C" int  https_input_close(void *  context)
    {
        https_context *     ctxt = (https_context *) context;

        if (ctxt) {
            delete ctxt->stream_;
            delete ctxt;
        }
        return 0;
    }
}

