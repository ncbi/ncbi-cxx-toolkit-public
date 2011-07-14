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
 * This file contains the definition of the xml::tree_parser class.
**/

#ifndef _xmlwrapp_tree_parser_h_
#define _xmlwrapp_tree_parser_h_

// for NCBI_DEPRECATED
#include <ncbiconf.h>

// xmlwrapp includes
#include <misc/xmlwrapp/xml_init.hpp>
#include <misc/xmlwrapp/errors.hpp>

// standard includes
#include <cstddef>
#include <string>


// NB: The following macro definitions have been copied from ncbimisc.hpp.
//     If they are changed in the first place they should be updated here as
//     well.

/// Macro used to mark a constructor as deprecated.
///
/// The correct syntax for this varies from compiler to compiler:
/// older versions of GCC (prior to 3.4) require NCBI_DEPRECATED to
/// follow any relevant constructor declarations, but some other
/// compilers (Microsoft Visual Studio 2005, IBM Visual Age / XL)
/// require it to precede any relevant declarations, whether or not
/// they are for constructors.
#if defined(NCBI_COMPILER_MSVC) || defined(NCBI_COMPILER_VISUALAGE)
#  define NCBI_DEPRECATED_CTOR(decl) NCBI_DEPRECATED decl
#else
#  define NCBI_DEPRECATED_CTOR(decl) decl NCBI_DEPRECATED
#endif

/// Macro used to mark a class as deprecated.
///
/// @sa NCBI_DEPRECATED_CTOR
#define NCBI_DEPRECATED_CLASS NCBI_DEPRECATED_CTOR(class)





namespace xml {

// forward declarations
class document;
namespace impl {
struct tree_impl;
}

/**
 * The xml::tree_parser class is used to parse an XML document and generate
 * a tree like structure of xml::node objects. After constructing a
 * tree_parser, with either a file to parse or some in memory data to parse,
 * you can walk the tree using the xml::node interface.
**/
NCBI_DEPRECATED_CLASS tree_parser {
public:
    typedef std::size_t size_type;

    //####################################################################
    /**
     * Create a new xml::tree_parser object by parsing the given XML file.
     *
     * @param filename The XML file name.
     * @param messages A pointer to the object where all the warnings are
     *                 collected. If NULL then no messages will be collected.
     * @param how How to treat warnings (default: warnings are not treated as
     *            errors). If warnings are treated as errors then an exception
     *            is thrown in case of both errors and/or warnings. If warnings
     *            are not treated as errors then an exception will be thrown
     *            only when there are errors.
     * @exception Throws xml::parser_exception in case of schema parsing errors
     *            and std::exception in case of other problems.
     * @author Sergey Satskiy, NCBI
    **/
    tree_parser (const char* filename, error_messages* messages = NULL,
                 warnings_as_errors_type how = type_warnings_not_errors);

    //####################################################################
    /**
     * Create a new xml::tree_parser object by parsing the given XML from a
     * memory buffer.
     *
     * @param data The XML memory buffer.
     * @param size Size of the memory buffer.
     * @param messages A pointer to the object where all the warnings are
     *                 collected. If NULL then no messages will be collected.
     * @param how How to treat warnings (default: warnings are not treated as
     *            errors). If warnings are treated as errors then an exception
     *            is thrown in case of both errors and/or warnings. If warnings
     *            are not treated as errors then an exception will be thrown
     *            only when there are errors.
     * @exception Throws xml::parser_exception in case of schema parsing errors
     *            and std::exception in case of other problems.
     * @author Sergey Satskiy, NCBI
    **/
    tree_parser (const char* data, size_type size,
                 error_messages* messages,
                 warnings_as_errors_type how = type_warnings_not_errors);

    //####################################################################
    /**
     * xml::tree_parser class destructor.
     *
     * @author Peter Jones
    **/
    //####################################################################
    virtual ~tree_parser (void);

    //####################################################################
    /**
     * Get a reference to the xml::document that was generated during the
     * XML parsing. You should make sure to only use a reference to the
     * document to avoid a deep copy.
     *
     * @return A reference to the xml::document.
     * @author Peter Jones
    **/
    //####################################################################
    xml::document& get_document (void);

    //####################################################################
    /**
     * Get a const reference to the xml::document that was generate during
     * the XML parsing. You should make sure to only use a reference to the
     * document to avoid a deep copy.
     *
     * @return A const reference to the xml::document.
     * @author Peter Jones
    **/
    //####################################################################
    const xml::document& get_document (void) const;

    //####################################################################
    /**
     * xml::tree_parser class constructor. Given the name of a file, this
     * constructor will parse that file.
     *
     * There are two options for dealing with XML parsing errors. The
     * default it to throw an exception (xml::exception, xml::parser_exception
     * or std::bad_alloc). The other option is to pass false for the
     * allow_exceptions flag. This will prevent an exception from being thrown,
     * instead, a flag will be set that you can test with the operator!
     * member function.
     *
     * No matter what option you choose, this constructor may still throw
     * exceptions for memory failure or other non-parsing related failures.
     *
     * @deprecated
     *  Use the tree_parser(const char* filename, warnings_as_errors_type how)
     *  instead.
     * @param filename The name of the file to parse.
     * @param allow_exceptions Whether or not you want an exception for parsing errors.
     * @deprecated
     * @author Peter Jones
    **/
    //####################################################################
    NCBI_DEPRECATED
    tree_parser (const char *filename, bool allow_exceptions);

    //####################################################################
    /**
     * xml::tree_parser class constructor. Given some data and the size of
     * that data, parse that data as XML. To see if the data was parsed
     * successfully use operator!.
     *
     * @deprecated
     *  Use the tree_parser(const char* data, size_type size,
     *                      warnings_as_errors_type how)
     *  instead.
     * @param data The XML data to parse.
     * @param size The size of the XML data to parse.
     * @param allow_exceptions Whether or not you want an exception for parsing errors.
     * @deprecated
     * @author Peter Jones
    **/
    //####################################################################
    NCBI_DEPRECATED
    tree_parser (const char *data, size_type size, bool allow_exceptions);

    //####################################################################
    /**
     * Create a new xml::tree_parser object by parsing the given XML from a
     * memory buffer.
     *
     * @param data The XML memory buffer.
     * @param size Size of the memory buffer.
     * @param how How to treat warnings (default: warnings are not treated as
     *            errors). If warnings are treated as errors then an exception
     *            is thrown in case of both errors and/or warnings. If warnings
     *            are not treated as errors then an exception will be thrown
     *            only when there are errors.
     * @exception Throws xml::parser_exception in case of schema parsing errors
     *            and std::exception in case of other problems.
     * @deprecated
     * @author Sergey Satskiy, NCBI
    **/
    NCBI_DEPRECATED
    tree_parser (const char* data, size_type size,
                 warnings_as_errors_type how = type_warnings_not_errors);

    //####################################################################
    /**
     * Check to see if a xml::tree_parser class is vaild. That is, check to
     * see if parsing XML data was successful and the tree_parser holds a
     * good XML node tree.
     *
     * @deprecated Use new constructors which throw exceptions in case
     *             of problems and allow considering warnings as errors.
     * @return True if the tree_parser is NOT VAILD; false if it is vaild.
     * @deprecated
     * @author Peter Jones
    **/
    //####################################################################
    NCBI_DEPRECATED
    bool operator! (void) const;

    //####################################################################
    /**
     * If operator! indicates that there was an error parsing your XML data,
     * you can use this member function to get the error message that was
     * generated durring parsing.
     *
     * @deprecated Use get_parser_messages() instead.
     * @return The error message generated durring XML parsing.
     * @deprecated
     * @author Peter Jones
    **/
    //####################################################################
    NCBI_DEPRECATED
    const std::string& get_error_message (void) const;

    //####################################################################
    /**
     * Get the XML document parsing error messages. This may include
     * warnings, errors and fatal errors.
     *
     * @return XML document parser error messages.
     * @deprecated
     * @author Sergey Satskiy, NCBI
    **/
    NCBI_DEPRECATED
    const error_messages& get_parser_messages (void) const;

    //####################################################################
    /**
     * Check to see if there were any warnings from the parser while
     * processing the given XML data. If there were, you may want to send
     * the same document through xmllint or the event_parser to catch and
     * review the warning messages.
     *
     * @deprecated Use get_parser_messages() instead to get complete
     *             information about all the collected messages.
     * @return True if there were any warnings.
     * @return False if there were no warnings.
     * @deprecated
     * @author Peter Jones
    **/
    //####################################################################
    NCBI_DEPRECATED
    bool had_warnings (void) const;

private:
    impl::tree_impl *pimpl_; // private implementation

    bool is_failure (error_messages* messages,
                     warnings_as_errors_type how) const;
    /*
     * Don't allow anyone to copy construct a xml::tree_parser or allow the
     * assignment operator to be called. It is not very useful to copy a
     * parser that has already parsed half a document.
     */
    tree_parser (const tree_parser&);
    tree_parser& operator= (const tree_parser&);
}; // end xml::tree_parser class

} // end xml namespace
#endif
