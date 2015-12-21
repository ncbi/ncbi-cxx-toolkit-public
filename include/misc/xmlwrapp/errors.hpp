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

#ifndef _xmlwrapp_errors_hpp_
#define _xmlwrapp_errors_hpp_

// standard includes
#include <string>
#include <stdexcept>
#include <list>

namespace xml {

/**
 * The xml::error_message class is used to store a single error message
 * which may appear while parsing or validating an XML document.
 *
 * @author Sergey Satskiy, NCBI
**/
class error_message {
public:
    /// A type for different type of errors
    enum message_type {
        type_fatal_error,   ///< fatal error
        type_error,         ///< error
        type_warning        ///< warning
    };

    /**
     * Convert an error type to a string.
     *
     * @param mt The error type.
     * @return The string representation of the error type.
     * @author Sergey Satskiy, NCBI
    **/
    static std::string message_type_str(message_type mt);

    /**
     * Create a new xml::error_message object.
     *
     * @param message The error message.
     * @param msg_type The error type.
     * @param line file line realting to error message. 0 if none.
     * @param filename file name if available or an empty string.
     * @author Sergey Satskiy, NCBI
    **/
    error_message(const std::string& message,
                  message_type msg_type,
                  int line,
                  const std::string& filename);

    /**
     * Get the error message type.
     *
     * @return The error type.
     * @author Sergey Satskiy, NCBI
    **/
    message_type get_message_type (void) const;

    /**
     * Get the error message.
     *
     * @return The error message.
     * @author Sergey satskiy, NCBI
    **/
    std::string  get_message      (void) const;

    /**
     * Get the line number.
     *
     * @return The line number, 0 if not available.
    **/
    int get_line (void) const;

    /**
     * Get the file name.
     *
     * @return The file name, empty string if not available.
    **/
    std::string  get_filename (void) const;

private:
    message_type message_type_;
    std::string  message_;
    int          line_;
    std::string  filename_;
};



/**
 * The xml::error_messages class is used to store all the error message
 * which are collected while parsing or validating an XML document.
 *
 * @author Sergey Satskiy, NCBI
**/
class error_messages
{
public:
    /// A type to store multiple messages
    typedef std::list<error_message> error_messages_type;

    /**
     * Get the error messages.
     *
     * @return The error messages.
     * @author Sergey Satskiy, NCBI
    **/
    const error_messages_type& get_messages (void) const;

    /**
     * Get the error messages.
     *
     * @return The error messages.
     * @author Sergey Satskiy, NCBI
    **/
    error_messages_type& get_messages (void);

    /**
     * Check if there are warnings in the error messages.
     *
     * @return true if there is at least one warning in the error messages.
     *         It does not consider errors and fatal errors.
     * @author Sergey Satskiy, NCBI
    **/
    bool has_warnings (void) const;

    /**
     * Check if there are errors in the error messages.
     *
     * @return true if there is at least one error in the error messages.
    *          It does not consider fatal errors.
     * @author Sergey Satskiy, NCBI
    **/
    bool has_errors (void) const;

    /**
     * Check if there are fatal errors in the error messages.
     *
     * @return true if there is at least one fatal error in the error messages.
     * @author Sergey Satskiy, NCBI
    **/
    bool has_fatal_errors (void) const;

    /**
     * Convert error messages into a single printable string.
     *
     * @return string representation of the errors list ('\n' separated)
     * @author Sergey Satskiy, NCBI
    **/
    std::string print (void) const;

private:
    error_messages_type error_messages_;
    bool has_messages_of_type (error_message::message_type type) const;
};




/**
 * The xml::parser_exception class is used to store parsing and validating
 * exception information.
 *
 * @author Sergey Satskiy, NCBI
**/
class parser_exception : public std::exception
{
public:

    /**
     * Convert error messages into a printable C-style string.
     *
     * @return string representation of the errors list ('\n' separated).
     * @author Sergey Satskiy, NCBI
    **/
    virtual const char* what() const throw ();

    /**
     * Get error messages.
     *
     * @return The error messages.
     * @author Sergey Satskiy, NCBI
    **/
    const error_messages& get_messages(void) const;

    /**
     * Create a new object using the given list of error messages.
     *
     * @param msgs The error messages.
     * @author Sergey Satskiy, NCBI
    **/
    parser_exception(const error_messages& msgs);

    /**
     * Destroy the object.
     *
     * @author Sergey Satskiy, NCBI
    **/
    virtual ~parser_exception() throw () {}

private:
    error_messages          messages_;
    mutable std::string     buffer;     // used for list convertion in what()
};


/// A type for different approaches to process warnings
enum warnings_as_errors_type {
    type_warnings_are_errors,   ///< Treat warnings as errors
    type_warnings_not_errors    ///< Do not treat warnings as errors
};

} // xml namespace

#endif

