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
 * XML dtd API for XmlWrapp.
**/

#ifndef _xmlwrapp_dtd_hpp_
#define _xmlwrapp_dtd_hpp_

// standard includes
#include <string>

// xmlwrapp includes
#include <misc/xmlwrapp/errors.hpp>

namespace xml {

// forward declarations
class document;
namespace impl {
struct dtd_impl;
}


/**
 * The xml::dtd class represents an XML dtd from a file.
**/
class dtd
{
public:
    typedef std::size_t size_type;

    /**
     * Create a new empty xml::dtd object.
     * The object created this way is for validating a document with a DTD which
     * resides in the documents.
    **/
    dtd ();

    /**
     * Create a new xml::dtd object by parsing the given XML dtd file.
     *
     * @param filename The XML dtd file name.
     * @param how How to treat warnings (default: warnings are not treated as
     *            errors). If warnings are treated as errors then an exception
     *            is thrown in case of both errors and/or warnings. If warnings
     *            are not treated as errors then an exception will be thrown
     *            only when there are errors.
     * @exception Throws xml::parser_exception in case of dtd parsing errors
     *            and std::exception in case of other problems.
     * @author Sergey Satskiy, NCBI
    **/
    dtd (const char* filename,
         warnings_as_errors_type how = type_warnings_not_errors);

    /**
     * Destroy the object.
     *
     * @author Sergey Satskiy, NCBI
    **/
    ~dtd();

    /**
     * Get the XML dtd parsing error messages.
     *
     * @return XML dtd parsing error messages.
     * @author Sergey Satskiy, NCBI
    **/
    const error_messages& get_dtd_parser_messages (void) const;

    /**
     * Validate the given XML document.
     *
     * If the dtd was constructed from a file then after the successfull
     * document validation the parsed DTD will be added to the document as the
     * external subset. If there is already an external DTD attached to the
     * document it will be removed and deleted.
     *
     * @param doc XML document.
     * @param how How to treat warnings (default: warnings are treated as
     *            errors). If warnings are treated as errors false is
     *            returned in case of both errors and/or warnings. If warnings
     *            are not treated as errors then false is returned
     *            only when there are errors. The full list of warnings and
     *            errors can be retrieved by calling the
     *            get_validation_messages() member function.
     * @return true if the document is valid, false otherwise.
     * @exception Throws std::exception in case of problems.
     * @author Sergey Satskiy, NCBI
    **/
    bool validate (document& doc,
                   warnings_as_errors_type how = type_warnings_are_errors);

    /**
     * Get the XML document validating error messages.
     *
     * @return XML dtd validating error messages.
     * @author Sergey Satskiy, NCBI
    **/
    const error_messages& get_validation_messages (void) const;

    /**
     * Get the public ID.
     *
     * @return Public ID or NULL if not available
     * @author Sergey Satskiy, NCBI
    **/
    const char* get_public_id (void) const;

    /**
     * Get the system ID.
     *
     * @return System ID or NULL if not available
     * @author Sergey Satskiy, NCBI
    **/
    const char* get_system_id (void) const;

    /**
     * Get the name.
     *
     * @return Name or NULL if not available
     * @author Sergey Satskiy, NCBI
    **/
    const char* get_name (void) const;

private:
    impl::dtd_impl *pimpl_;
    void set_dtd_data (void *data);

    // prohibited
    dtd (const dtd&);
    dtd& operator= (const dtd&);

    friend class document;
};

} // xml namespace

#endif

