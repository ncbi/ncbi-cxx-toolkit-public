/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author:  Sergey Satskiy, NCBI
 * Credits: Denis Vakatov, NCBI (help with the original design)
 *
 */


/** @file
 * Various XML parser's and validator's errors related code for XmlWrapp.
**/

#include <misc/xmlwrapp/errors.hpp>
#include <misc/xmlwrapp/exception.hpp>

using namespace xml;


// xml::error_message class implementation
std::string error_message::message_type_str(message_type mt) {
    switch (mt) {
        case type_fatal_error:  return std::string("fatal error");
        case type_error:        return std::string("error");
        case type_warning:      return std::string("warning");
        default: ;
    }
    throw xml::exception("unknown message type");
}

error_message::error_message(const std::string& message,
                             message_type msg_type,
                             int line, const std::string& filename) :
    message_type_(msg_type), message_(message),
    line_(line), filename_(filename)
{}

error_message::message_type error_message::get_message_type (void) const {
    return message_type_;
}

std::string error_message::get_message (void) const {
    return message_;
}


int error_message::get_line (void) const {
    return line_;
}

std::string error_message::get_filename (void) const {
    return filename_;
}


// xml::error_messages class implementation
const error_messages::error_messages_type& error_messages::get_messages (void) const {
    return error_messages_;
}

error_messages::error_messages_type& error_messages::get_messages (void) {
    return error_messages_;
}

bool error_messages::has_messages_of_type (error_message::message_type type) const {
    error_messages_type::const_iterator  end(error_messages_.end());
    for (error_messages_type::const_iterator k(error_messages_.begin()); k != end; ++k)
        if (k->get_message_type() == type)
            return true;
    return false;
}

bool error_messages::has_warnings (void) const {
    return has_messages_of_type(error_message::type_warning);
}

bool error_messages::has_errors (void) const {
    return has_messages_of_type(error_message::type_error);
}

bool error_messages::has_fatal_errors (void) const {
    return has_messages_of_type(error_message::type_fatal_error);
}

std::string error_messages::print(void) const {
    std::string buffer;
    try {
        error_messages_type::const_iterator  begin(error_messages_.begin());
        error_messages_type::const_iterator  end(error_messages_.end());
        for (error_messages_type::const_iterator  k(begin); k != end; ++k) {
            if (k != begin) buffer += std::string( "\n" );
            buffer += error_message::message_type_str(k->get_message_type()) + ": " + k->get_message();
        }
    }
    catch (...)
    {}
    return buffer;
}


// xml::parser_exception class implementation
parser_exception::parser_exception(const error_messages& msgs) :
    messages_(msgs)
{}

const char* parser_exception::what() const throw() {
    buffer = messages_.print();
    return buffer.c_str();
}

const error_messages& parser_exception::get_messages(void) const {
    return messages_;
}

