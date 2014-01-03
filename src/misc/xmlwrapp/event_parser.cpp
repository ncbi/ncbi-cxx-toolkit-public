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

/** @file
 * This file contains the implementation of the xml::event_parser class for
 * the libxml XML parser.
**/

// make MSVC6 shutup about long template names
#if defined(_MSC_VER)
#  pragma warning(disable : 4786)
#endif

// xmlwrapp includes
#include <misc/xmlwrapp/event_parser.hpp>
#include <misc/xmlwrapp/node.hpp>
#include <misc/xmlwrapp/exception.hpp>
#include "utility.hpp"

// libxml includes
#include <libxml/parser.h>
#include <libxml/xmlversion.h>

// standard includes
#include <new>
#include <cstring>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <memory>

using namespace xml;
using namespace xml::impl;

//####################################################################
/*
 * This is a hack to fix a problem with a change in the libxml2 API for
 * versions starting at 2.6.0
 */
#if LIBXML_VERSION >= 20600
#   define initxmlDefaultSAXHandler xmlSAX2InitDefaultSAXHandler
#   include <libxml/SAX2.h>
#endif
//####################################################################
namespace {
    const std::size_t const_buffer_size = 4096;
}

//####################################################################
struct xml::impl::epimpl {
public:
    epimpl (event_parser &parent, event_parser::sax_handlers_mask mask);
    ~epimpl (void);
    void create_context(void);
    void destroy_context(void);
    void recreate_context(void);

    xmlSAXHandler       sax_handler_;
    xmlParserCtxt *     parser_context_;

    bool                parser_status_; // true - OK, can continue.
                                        // false - user interrupted or failed
    error_messages *    errors_;

    void event_start_document (void);
    void event_end_document (void);
    void event_start_element (const xmlChar *tag, const xmlChar **props);
    void event_end_element (const xmlChar *tag);
    void event_text (const xmlChar *text, int length);
    void event_pi (const xmlChar *target, const xmlChar *data);
    void event_comment (const xmlChar *text);
    void event_cdata (const xmlChar *text, int length);
    void event_notation_declaration (const xmlChar *name,
                                     const xmlChar *public_id,
                                     const xmlChar *system_id);
    void event_entity_declaration (const xmlChar * name,
                                   int type,
                                   const xmlChar * publicId,
                                   const xmlChar * systemId,
                                   xmlChar * content);
    void event_unparsed_entity_declaration (const xmlChar *name,
                                            const xmlChar *public_id,
                                            const xmlChar *system_id,
                                            const xmlChar *notation_name);
    void event_external_subset_declaration (const xmlChar *name,
                                            const xmlChar *external_id,
                                            const xmlChar *system_id);
    void event_internal_subset_declaration (const xmlChar *name,
                                            const xmlChar *external_id,
                                            const xmlChar *system_id);
    void event_attribute_declaration (const xmlChar *element_name,
                                      const xmlChar *attribute_name,
                                      int attr_type,
                                      int default_type,
                                      const xmlChar *default_value,
                                      xmlEnumeration *default_values);
    void event_element_declaration (const xmlChar *element_name,
                                    int type,
                                    xmlElementContent *content);
    void event_entity_reference (const xmlChar *name);
    void event_warning (const std::string &message);
    void event_error (const std::string &message);
    void event_fatal_error (const std::string &message);
private:
    event_parser &parent_;

    epimpl (const epimpl&);
    epimpl& operator= (const epimpl&);
};

extern "C"
{
    //####################################################################
    static void cb_start_document (void *parser)
    { static_cast<epimpl*>(parser)->event_start_document(); }
    //####################################################################
    static void cb_end_document (void *parser)
    { static_cast<epimpl*>(parser)->event_end_document(); }
    //####################################################################
    static void cb_start_element (void *parser, const xmlChar *tag, const xmlChar **props)
    { static_cast<epimpl*>(parser)->event_start_element(tag, props); }
    //####################################################################
    static void cb_end_element (void *parser, const xmlChar *tag)
    { static_cast<epimpl*>(parser)->event_end_element(tag); }
    //####################################################################
    static void cb_text (void *parser, const xmlChar *text, int length)
    { static_cast<epimpl*>(parser)->event_text(text, length); }
    //####################################################################
    static void cb_pi (void *parser, const xmlChar *target, const xmlChar *data)
    { static_cast<epimpl*>(parser)->event_pi(target, data); }
    //####################################################################
    static void cb_comment (void *parser, const xmlChar *text)
    { static_cast<epimpl*>(parser)->event_comment(text); }
    //####################################################################
    static void cb_cdata (void *parser, const xmlChar *text, int length)
    { static_cast<epimpl*>(parser)->event_cdata(text, length); }
    //####################################################################
    static void cb_notation_declaration (void *parser, const xmlChar *name,
                                         const xmlChar *public_id,
                                         const xmlChar *system_id)
    { static_cast<epimpl*>(parser)->event_notation_declaration(name,
                                                               public_id,
                                                               system_id); }
    //####################################################################
    static void cb_entity_declaration (void *parser, const xmlChar *name,
                                       int type, const xmlChar *public_id,
                                       const xmlChar *system_id,
                                       xmlChar *content)
    { static_cast<epimpl*>(parser)->event_entity_declaration(name,
                                                             type,
                                                             public_id,
                                                             system_id,
                                                             content); }
    //####################################################################
    static void cb_unparsed_entity_declaration (void *parser,
                                                const xmlChar *name,
                                                const xmlChar *public_id,
                                                const xmlChar *system_id,
                                                const xmlChar *notation_name)
    { static_cast<epimpl*>(parser)->event_unparsed_entity_declaration(name,
                                                                      public_id,
                                                                      system_id,
                                                                      notation_name); }
    //####################################################################
    static void cb_external_subset_declaration (void *parser,
                                                const xmlChar *name,
                                                const xmlChar *external_id,
                                                const xmlChar *system_id)
    { static_cast<epimpl*>(parser)->event_external_subset_declaration(name,
                                                                      external_id,
                                                                      system_id); }
    //####################################################################
    static void cb_internal_subset_declaration (void *parser,
                                                const xmlChar *name,
                                                const xmlChar *external_id,
                                                const xmlChar *system_id)
    { static_cast<epimpl*>(parser)->event_internal_subset_declaration(name,
                                                                      external_id,
                                                                      system_id); }
    //####################################################################
    static void cb_attribute_declaration (void *parser,
                                          const xmlChar *element_name,
                                          const xmlChar *attribute_name,
                                          int attr_type,
                                          int default_type,
                                          const xmlChar *default_value,
                                          xmlEnumeration *default_values)
    { static_cast<epimpl*>(parser)->event_attribute_declaration(element_name,
                                                                attribute_name,
                                                                attr_type,
                                                                default_type,
                                                                default_value,
                                                                default_values); }
    //####################################################################
    static void cb_element_declaration (void *parser,
                                        const xmlChar *element_name,
                                        int type,
                                        xmlElementContent *content)
    { static_cast<epimpl*>(parser)->event_element_declaration(element_name,
                                                              type,
                                                              content); }
    //####################################################################
    static void cb_entity_reference (void *parser, const xmlChar *name)
    { static_cast<epimpl*>(parser)->event_entity_reference(name); }
    //####################################################################
    static void cb_warning (void *parser, const char *message, ...) {
        std::string complete_message;

        va_list ap;
        va_start(ap, message);
        printf2string(complete_message, message, ap);
        va_end(ap);

        static_cast<epimpl*>(parser)->event_warning(complete_message);
    }
    //####################################################################
    static void cb_error (void *parser, const char *message, ...) {
        std::string complete_message;

        va_list ap;
        va_start(ap, message);
        printf2string(complete_message, message, ap);
        va_end(ap);

        static_cast<epimpl*>(parser)->event_error(complete_message);
    }
    //####################################################################
    static void cb_fatal_error (void *parser, const char *message, ...) {
        std::string complete_message;

        va_list ap;
        va_start(ap, message);
        printf2string(complete_message, message, ap);
        va_end(ap);

        static_cast<epimpl*>(parser)->event_fatal_error(complete_message);
    }
    //####################################################################
    static void cb_ignore (void*, const xmlChar*, int) {
        return;
    }
} // extern "C"

//####################################################################
xml::event_parser::event_parser (sax_handlers_mask mask) : parse_finished_(true) {
    pimpl_ = new epimpl(*this, mask);
}
//####################################################################
xml::event_parser::~event_parser (void) {
    delete pimpl_;
}
//####################################################################
bool event_parser::parse_file (const char *filename, error_messages* messages,
                               warnings_as_errors_type how) {
    if (!parse_finished_)
        parse_finish(messages, how);

    if (messages)
        messages->get_messages().clear();
    pimpl_->parser_status_ = true;

    std::ifstream file(filename);
    if (!file)
    {
        pimpl_->parser_status_ = false;
        if (messages)
        {
            std::string message("Cannot open file" + std::string(filename));
            messages->get_messages().push_back(error_message(message,
                                                             error_message::type_error));
        }
        return false;
    }
    return parse_stream(file, messages, how);
}
//####################################################################
bool event_parser::parse_stream (std::istream &stream, error_messages* messages,
                                 warnings_as_errors_type how) {
    char buffer[const_buffer_size];
    error_messages* temp(messages);
    std::auto_ptr<error_messages>   msgs;
    if (!messages)
        msgs.reset(temp = new error_messages);

    if (!parse_finished_)
        parse_finish(temp, how);

    temp->get_messages().clear();
    pimpl_->parser_status_ = true;

    if (stream && (stream.eof() || stream.peek() == std::istream::traits_type::eof()))
    {
        pimpl_->parser_status_ = false;
        temp->get_messages().push_back(error_message("empty xml document",
                                                     error_message::type_error));
        return false;
    }

    // Allocate a new parser context. Existing one is destroyed if needed.
    pimpl_->recreate_context();

    parse_finished_ = false;
    while (pimpl_->parser_status_ &&
           (stream.read(buffer, const_buffer_size) || stream.gcount()))
        pimpl_->parser_status_ = parse_chunk(buffer,
                                             (size_t)stream.gcount(),
                                             temp, how);

    if (!stream && !stream.eof())
    {
        parse_finish(temp, how);
        return false;
    }
    return parse_finish(temp, how);
}
//####################################################################
bool event_parser::parse_chunk (const char *chunk, size_type length,
                                error_messages* messages,
                                warnings_as_errors_type how) {
    error_messages* temp(messages);
    std::auto_ptr<error_messages>   msgs;
    if (!messages)
        msgs.reset(temp = new error_messages);
    else
        if (parse_finished_)
            // This is first call of the parse_chunk() after parse_finished()
            messages->get_messages().clear();

    parse_finished_ = false;
    pimpl_->errors_ = temp;

    if (pimpl_->parser_context_ == NULL)
        pimpl_->create_context();   // This is the first call of the
                                    // parse_chunk so create a context
    else
    {
        // Not first call, check that the callbacks are enabled
        if (pimpl_->parser_context_->disableSAX != 0)
            throw xml::exception("parse_finish(...) was not called after "
                                 "an error occured or the user "
                                 "stopped the parser");
        if (pimpl_->parser_context_->instate == XML_PARSER_EOF)
            throw xml::exception("parse_finish(...) was not called "
                                 "after the parser has finished");
    }

    xmlParseChunk(pimpl_->parser_context_, chunk, static_cast<int>(length), 0);
    if (!pimpl_->parser_status_)
        return false;
    if (is_failure(temp, how))
        return false;
    return true;
}
//####################################################################
bool event_parser::parse_finish (error_messages* messages,
                                 warnings_as_errors_type how) {
    xmlParseChunk(pimpl_->parser_context_, 0, 0, 1);

    parse_finished_ = true;

    // There was an error while parsing or the user interrupted parsing
    bool        ret_val = true;
    if (!pimpl_->parser_status_)
        ret_val= false;
    else
    {
        if (messages)
            if (is_failure(messages, how))
                ret_val= false;
    }

    // The parser context is not needed any more
    pimpl_->destroy_context();
    return ret_val;
}
//####################################################################
bool xml::event_parser::is_failure (error_messages* messages,
                                    warnings_as_errors_type how) const {
    // if there are fatal errors or errors it is a failure
    if (messages->has_errors() ||
        messages->has_fatal_errors())
        return true;
    if ((how == type_warnings_are_errors) &&
         messages->has_warnings())
        return true;
    return false;
}
//####################################################################
bool xml::event_parser::start_document () {
    return true;
}
//####################################################################
bool xml::event_parser::end_document () {
    return true;
}
//####################################################################
bool xml::event_parser::processing_instruction (const std::string&, const std::string&) {
    return true;
}
//####################################################################
bool xml::event_parser::comment (const std::string&) {
    return true;
}
//####################################################################
bool xml::event_parser::cdata (const std::string &contents) {
    return text(contents);
}
//####################################################################
bool xml::event_parser::notation_declaration (const std::string &name,
                                              const std::string &public_id,
                                              const std::string &system_id) {
    return true;
}
//####################################################################
bool xml::event_parser::unparsed_entity_declaration (const std::string &name,
                                                     const std::string &public_id,
                                                     const std::string &system_id,
                                                     const std::string &notation_name) {
    return true;
}
//####################################################################
bool xml::event_parser::external_subset_declaration (const std::string &name,
                                                     const std::string &external_id,
                                                     const std::string &system_id) {
    return true;
}
//####################################################################
bool xml::event_parser::internal_subset_declaration (const std::string &name,
                                                     const std::string &external_id,
                                                     const std::string &system_id) {
    return true;
}
//####################################################################
bool xml::event_parser::entity_declaration (const std::string &name,
                                            entity_type        type,
                                            const std::string &public_id,
                                            const std::string &system_id,
                                            const std::string &content) {
    return true;
}
//####################################################################
bool xml::event_parser::attribute_declaration (const std::string &element_name,
                                               const std::string &attribute_name,
                                               attribute_type attr_type,
                                               attribute_default_type default_type,
                                               const std::string &default_value,
                                               const values_type &default_values) {
    return true;
}
//####################################################################
bool xml::event_parser::element_declaration (const std::string &name,
                                             element_content_type type,
                                             _xmlElementContent *content) {
    return true;
}
//####################################################################
bool xml::event_parser::entity_reference (const std::string &name) {
    return true;
}
//####################################################################
bool xml::event_parser::warning (const std::string&) {
    return true;
}
//####################################################################
bool xml::event_parser::error (const std::string&) {
    return true;
}
//####################################################################
xml::event_parser::entity_type xml::event_parser::get_entity_type (int type) {
    switch (type) {
        case XML_INTERNAL_GENERAL_ENTITY:           return type_internal_general_entity;
        case XML_EXTERNAL_GENERAL_PARSED_ENTITY:    return type_external_general_parsed_entity;
        case XML_EXTERNAL_GENERAL_UNPARSED_ENTITY:  return type_external_general_unparsed_entity;
        case XML_INTERNAL_PARAMETER_ENTITY:         return type_internal_parameter_entity;
        case XML_EXTERNAL_PARAMETER_ENTITY:         return type_external_parameter_entity;
        case XML_INTERNAL_PREDEFINED_ENTITY:        return type_internal_predefined_entity;
        default: ;
    }
    throw xml::exception("Unknown entity type");
}
//####################################################################
xml::event_parser::attribute_type xml::event_parser::get_attribute_type (int type) {
    switch (type) {
        case XML_ATTRIBUTE_CDATA:       return type_attribute_cdata;
        case XML_ATTRIBUTE_ID:          return type_attribute_id;
        case XML_ATTRIBUTE_IDREF:       return type_attribute_idref;
        case XML_ATTRIBUTE_IDREFS:      return type_attribute_idrefs;
        case XML_ATTRIBUTE_ENTITY:      return type_attribute_entity;
        case XML_ATTRIBUTE_ENTITIES:    return type_attribute_entities;
        case XML_ATTRIBUTE_NMTOKEN:     return type_attribute_nmtoken;
        case XML_ATTRIBUTE_NMTOKENS:    return type_attribute_nmtokens;
        case XML_ATTRIBUTE_ENUMERATION: return type_attribute_enumeration;
        case XML_ATTRIBUTE_NOTATION:    return type_attribute_notation;
        default: ;
    }
    throw xml::exception("Unknown attribute type");
}
//####################################################################
xml::event_parser::attribute_default_type xml::event_parser::get_attribute_default_type (int type) {
    switch (type) {
        case XML_ATTRIBUTE_NONE:        return type_attribute_none;
        case XML_ATTRIBUTE_REQUIRED:    return type_attribute_required;
        case XML_ATTRIBUTE_IMPLIED:     return type_attribute_implied;
        case XML_ATTRIBUTE_FIXED:       return type_attribute_fixed;
        default: ;
    }
    throw xml::exception("Unknown attribute default type");
}
//####################################################################
xml::event_parser::element_content_type xml::event_parser::get_element_content_type (int type) {
    switch (type) {
        case XML_ELEMENT_TYPE_UNDEFINED:    return type_undefined;
        case XML_ELEMENT_TYPE_EMPTY:        return type_empty;
        case XML_ELEMENT_TYPE_ANY:          return type_any;
        case XML_ELEMENT_TYPE_MIXED:        return type_mixed;
        case XML_ELEMENT_TYPE_ELEMENT:      return type_element;
        default: ;
    }
    throw xml::exception("Unknown element type");
}
//####################################################################
epimpl::epimpl (event_parser &parent, event_parser::sax_handlers_mask mask)
    : parser_context_(NULL),
      parser_status_(true),
      errors_(NULL),
      parent_(parent)
{
    std::memset(&sax_handler_, 0, sizeof(sax_handler_));

    // Error handlers are set unconditionally
    sax_handler_.warning                = cb_warning;
    sax_handler_.error                  = cb_error;
    sax_handler_.fatalError             = cb_fatal_error;

    // The rest of SAX handlers depends on the user provided mask
    if (mask & event_parser::start_document_handler)
        sax_handler_.startDocument = cb_start_document;

    if (mask & event_parser::end_document_handler)
        sax_handler_.endDocument = cb_end_document;

    if (mask & event_parser::start_element_handler)
        sax_handler_.startElement = cb_start_element;

    if (mask & event_parser::end_element_handler)
        sax_handler_.endElement = cb_end_element;

    if (mask & event_parser::characters_handler)
        sax_handler_.characters = cb_text;

    if (mask & event_parser::pi_handler)
        sax_handler_.processingInstruction = cb_pi;

    if (mask & event_parser::comment_handler)
        sax_handler_.comment = cb_comment;

    if (mask & event_parser::cdata_handler)
        sax_handler_.cdataBlock = cb_cdata;

    if (mask & event_parser::notation_decl_handler)
        sax_handler_.notationDecl = cb_notation_declaration;

    if (mask & event_parser::entity_decl_handler)
        sax_handler_.entityDecl = cb_entity_declaration;

    if (mask & event_parser::unparsed_entity_decl_handler)
        sax_handler_.unparsedEntityDecl = cb_unparsed_entity_declaration;

    if (mask & event_parser::external_subset_handler)
        sax_handler_.externalSubset = cb_external_subset_declaration;

    if (mask & event_parser::internal_subset_handler)
        sax_handler_.internalSubset = cb_internal_subset_declaration;

    if (mask & event_parser::attribute_decl_handler)
        sax_handler_.attributeDecl = cb_attribute_declaration;

    if (mask & event_parser::element_decl_handler)
        sax_handler_.elementDecl = cb_element_declaration;

    if (mask & event_parser::reference_handler)
        sax_handler_.reference = cb_entity_reference;


    if ((xmlKeepBlanksDefaultValue != 0) && (mask & event_parser::characters_handler))
        sax_handler_.ignorableWhitespace = cb_text;
    else
        sax_handler_.ignorableWhitespace = cb_ignore;

    // Note: parser context is not allocated here.
    //       A new one is allocated each time a new parsing is requested
}

epimpl::~epimpl (void) {
    /* Do I need this?                     */
    /* xmlFreeDoc(parser_context_->myDoc); */
    destroy_context();
}

void epimpl::create_context(void) {
    parser_context_ = xmlCreatePushParserCtxt(&sax_handler_, this, 0, 0, 0);
    if (parser_context_ == NULL)
        throw std::bad_alloc();
}

void epimpl::destroy_context(void) {
    if (parser_context_)
        xmlFreeParserCtxt(parser_context_);
    parser_context_ = NULL;
}

void epimpl::recreate_context(void) {
    destroy_context();
    create_context();
}

void epimpl::event_start_element (const xmlChar *tag, const xmlChar **props) {
    if (!parser_status_) return;

    try {
        event_parser::attrs_type attrs;
        const xmlChar **attrp;

        for (attrp = props; attrp && *attrp; attrp += 2) {
            attrs[reinterpret_cast<const char*>(*attrp)] = reinterpret_cast<const char*>(*(attrp+1));
        }

        std::string name = reinterpret_cast<const char*>(tag);
        parser_status_ = parent_.start_element(name, attrs);
    } catch (const std::exception &ex) {
        event_fatal_error(ex.what());
        return;
    } catch ( ... ) {
        event_fatal_error("user exception in start_element handler");
        return;
    }
    if (!parser_status_) xmlStopParser(parser_context_);
}

void epimpl::event_end_element (const xmlChar *tag) {
    if (!parser_status_) return;

    try {
        std::string name = reinterpret_cast<const char*>(tag);
        parser_status_ = parent_.end_element(name);
    } catch (const std::exception &ex) {
        event_fatal_error(ex.what());
        return;
    } catch ( ... ) {
        event_fatal_error("user exception in end_element handler");
        return;
    }
    if (!parser_status_) xmlStopParser(parser_context_);
}
//####################################################################
void epimpl::event_text (const xmlChar *text, int length) {
    if (!parser_status_) return;

    try {
        std::string contents(reinterpret_cast<const char*>(text), static_cast<std::string::size_type>(length));
        parser_status_ = parent_.text(contents);
    } catch (const std::exception &ex) {
        event_fatal_error(ex.what());
        return;
    } catch ( ... ) {
        event_fatal_error("user exception in text handler");
        return;
    }
    if (!parser_status_) xmlStopParser(parser_context_);
}
//####################################################################
void epimpl::event_start_document () {
    if (!parser_status_) return;

    try {
        parser_status_ = parent_.start_document();
    } catch (const std::exception &ex) {
        event_fatal_error(ex.what());
        return;
    } catch ( ... ) {
        event_fatal_error("user exception in start_document handler");
        return;
    }
    if (!parser_status_) xmlStopParser(parser_context_);
}
//####################################################################
void epimpl::event_end_document () {
    if (!parser_status_) return;

    try {
        parser_status_ = parent_.end_document();
    } catch (const std::exception &ex) {
        event_fatal_error(ex.what());
        return;
    } catch ( ... ) {
        event_fatal_error("user exception in end_document handler");
        return;
    }
    if (!parser_status_) xmlStopParser(parser_context_);
}
//####################################################################
void epimpl::event_pi (const xmlChar *target, const xmlChar *data) {
    if (!parser_status_) return;

    try {
        std::string s_target = reinterpret_cast<const char*>(target);
        std::string s_data = reinterpret_cast<const char*>(data);
        parser_status_ = parent_.processing_instruction(s_target, s_data);
    } catch (const std::exception &ex) {
        event_fatal_error(ex.what());
        return;
    } catch ( ... ) {
        event_fatal_error("user exception in processing_instruction handler");
        return;
    }
    if (!parser_status_) xmlStopParser(parser_context_);
}
//####################################################################
void epimpl::event_comment (const xmlChar *text) {
    if (!parser_status_) return;

    try {
        std::string contents = reinterpret_cast<const char*>(text);
        parser_status_ = parent_.comment(contents);
    } catch (const std::exception &ex) {
        event_fatal_error(ex.what());
        return;
    } catch ( ... ) {
        event_fatal_error("user exception in comment handler");
        return;
    }
    if (!parser_status_) xmlStopParser(parser_context_);
}
//####################################################################
void epimpl::event_cdata (const xmlChar *text, int length) {
    if (!parser_status_) return;

    try {
        std::string contents(reinterpret_cast<const char*>(text), static_cast<std::string::size_type>(length));
        parser_status_ = parent_.cdata(contents);
    } catch (const std::exception &ex) {
        event_fatal_error(ex.what());
        return;
    } catch ( ... ) {
        event_fatal_error("user exception in cdata handler");
        return;
    }
    if (!parser_status_) xmlStopParser(parser_context_);
}
//####################################################################
void epimpl::event_notation_declaration (const xmlChar *name,
                                         const xmlChar *public_id,
                                         const xmlChar *system_id) {
    if (!parser_status_) return;

    try {
        std::string     notation_name( name ? reinterpret_cast<const char*>(name) : "" );
        std::string     notation_public_id( public_id ? reinterpret_cast<const char*>(public_id) : "" );
        std::string     notation_system_id( system_id ? reinterpret_cast<const char*>(system_id) : "" );
        parser_status_ = parent_.notation_declaration(notation_name,
                                                      notation_public_id,
                                                      notation_system_id);
    } catch (const std::exception &ex) {
        event_fatal_error(ex.what());
        return;
    } catch ( ... ) {
        event_fatal_error("user exception in notation_declaration handler");
        return;
    }
    if (!parser_status_) xmlStopParser(parser_context_);
}
//####################################################################
void epimpl::event_entity_declaration (const xmlChar *name,
                                       int type,
                                       const xmlChar *public_id,
                                       const xmlChar *system_id,
                                       xmlChar * content) {
    if (!parser_status_) return;

    try {
        std::string     entity_name( name ? reinterpret_cast<const char*>(name) : "" );
        std::string     entity_public_id( public_id ? reinterpret_cast<const char*>(public_id) : "" );
        std::string     entity_system_id( system_id ? reinterpret_cast<const char*>(system_id) : "" );
        std::string     entity_content( content ? reinterpret_cast<const char*>(content) : "" );
        parser_status_ = parent_.entity_declaration(entity_name, parent_.get_entity_type(type),
                                                    entity_public_id, entity_system_id,
                                                    entity_content);
    } catch (const std::exception &ex) {
        event_fatal_error(ex.what());
        return;
    } catch ( ... ) {
        event_fatal_error("user exception in entity_declaration handler");
        return;
    }
    if (!parser_status_) xmlStopParser(parser_context_);
}
//####################################################################
void epimpl::event_unparsed_entity_declaration (const xmlChar *name,
                                                const xmlChar *public_id,
                                                const xmlChar *system_id,
                                                const xmlChar *notation_name) {
    if (!parser_status_) return;

    try {
        std::string     entity_name( name ? reinterpret_cast<const char*>(name) : "" );
        std::string     entity_public_id( public_id ? reinterpret_cast<const char*>(public_id) : "" );
        std::string     entity_system_id( system_id ? reinterpret_cast<const char*>(system_id) : "" );
        std::string     entity_notation_name( notation_name ? reinterpret_cast<const char*>(notation_name) : "" );
        parser_status_ = parent_.unparsed_entity_declaration(entity_name,
                                                             entity_public_id, entity_system_id,
                                                             entity_notation_name);
    } catch (const std::exception &ex) {
        event_fatal_error(ex.what());
        return;
    } catch ( ... ) {
        event_fatal_error("user exception in unparsed_entity_declaration handler");
        return;
    }
    if (!parser_status_) xmlStopParser(parser_context_);
}
//####################################################################
void epimpl::event_external_subset_declaration (const xmlChar *name,
                                                const xmlChar *external_id,
                                                const xmlChar *system_id) {
    if (!parser_status_) return;

    try {
        std::string     root_element_name( name ? reinterpret_cast<const char*>(name) : "" );
        std::string     ext_id( external_id ? reinterpret_cast<const char*>(external_id) : "" );
        std::string     sys_id( system_id ? reinterpret_cast<const char*>(system_id) : "" );
        parser_status_ = parent_.external_subset_declaration(root_element_name,
                                                             ext_id,
                                                             sys_id);
    } catch (const std::exception &ex) {
        event_fatal_error(ex.what());
        return;
    } catch ( ... ) {
        event_fatal_error("user exception in external_subset_declaration handler");
        return;
    }
    if (!parser_status_) xmlStopParser(parser_context_);
}
//####################################################################
void epimpl::event_internal_subset_declaration (const xmlChar *name,
                                                const xmlChar *external_id,
                                                const xmlChar *system_id) {
    if (!parser_status_) return;

    try {
        std::string     root_element_name( name ? reinterpret_cast<const char*>(name) : "" );
        std::string     ext_id( external_id ? reinterpret_cast<const char*>(external_id) : "" );
        std::string     sys_id( system_id ? reinterpret_cast<const char*>(system_id) : "" );
        parser_status_ = parent_.internal_subset_declaration(root_element_name,
                                                             ext_id,
                                                             sys_id);
    } catch (const std::exception &ex) {
        event_fatal_error(ex.what());
        return;
    } catch ( ... ) {
        event_fatal_error("user exception in internal_subset_declaration handler");
        return;
    }
    if (!parser_status_) xmlStopParser(parser_context_);
}
//####################################################################
void epimpl::event_attribute_declaration (const xmlChar *element_name,
                                          const xmlChar *attribute_name,
                                          int attr_type,
                                          int default_type,
                                          const xmlChar *default_value,
                                          xmlEnumeration *default_values) {
    if (!parser_status_) return;

    try {
        std::string     element( element_name ? reinterpret_cast<const char*>(element_name) : "" );
        std::string     attr_name( attribute_name ? reinterpret_cast<const char*>(attribute_name) : "" );
        std::string     default_val( default_value ? reinterpret_cast<const char*>(default_value) : "" );

        xml::event_parser::values_type  def_vals;
        while (default_values) {
            if (default_values->name)
                def_vals.push_back(std::string(reinterpret_cast<const char*>(default_values->name)));
            default_values = default_values->next;
        }
        parser_status_ = parent_.attribute_declaration(element, attr_name,
                                                       parent_.get_attribute_type(attr_type),
                                                       parent_.get_attribute_default_type(default_type),
                                                       default_val,
                                                       def_vals);
    } catch (const std::exception &ex) {
        event_fatal_error(ex.what());
        return;
    } catch ( ... ) {
        event_fatal_error("user exception in attribute_declaration handler");
        return;
    }
    if (!parser_status_) xmlStopParser(parser_context_);
}
//####################################################################
void epimpl::event_element_declaration (const xmlChar *element_name,
                                        int type,
                                        xmlElementContent *content) {
    if (!parser_status_) return;

    try {
        std::string     element( element_name ? reinterpret_cast<const char*>(element_name) : "" );

        parser_status_ = parent_.element_declaration(element,
                                                     parent_.get_element_content_type(type),
                                                     content);
    } catch (const std::exception &ex) {
        event_fatal_error(ex.what());
        return;
    } catch ( ... ) {
        event_fatal_error("user exception in element_declaration handler");
        return;
    }
    if (!parser_status_) xmlStopParser(parser_context_);
}
//####################################################################
void epimpl::event_entity_reference (const xmlChar *name) {
    if (!parser_status_) return;

    try {
        std::string     reference_name( name ? reinterpret_cast<const char*>(name) : "" );

        parser_status_ = parent_.entity_reference(reference_name);
    } catch (const std::exception &ex) {
        event_fatal_error(ex.what());
        return;
    } catch ( ... ) {
        event_fatal_error("user exception in entity_reference handler");
        return;
    }
    if (!parser_status_) xmlStopParser(parser_context_);
}
//####################################################################
void epimpl::event_warning (const std::string &message) {
    if (!parser_status_) return;

    errors_->get_messages().push_back(error_message(message,
                                                    error_message::type_warning));
    try {
        parser_status_ = parent_.warning(message);
    } catch (const std::exception &ex) {
        event_fatal_error(ex.what());
        return;
    } catch ( ... ) {
        event_fatal_error("user exception in warning handler");
        return;
    }
    if (!parser_status_) xmlStopParser(parser_context_);
}
//####################################################################
void epimpl::event_error (const std::string &message) {
    if (!parser_status_) return;

    errors_->get_messages().push_back(error_message(message,
                                                    error_message::type_error));
    try {
        parser_status_ = parent_.error(message);
    } catch (const std::exception &ex) {
        event_fatal_error(ex.what());
        return;
    }
    catch ( ... ) {
       event_fatal_error("user exception in error handler");
       return;
    }
    if (!parser_status_) xmlStopParser(parser_context_);
}
//####################################################################
void epimpl::event_fatal_error (const std::string &message) {
    if (!parser_status_) return;

    errors_->get_messages().push_back(error_message(message,
                                                    error_message::type_fatal_error));
    parser_status_ = false;
    xmlStopParser(parser_context_);
}

