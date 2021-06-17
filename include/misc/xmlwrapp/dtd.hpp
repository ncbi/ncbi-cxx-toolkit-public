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

// for NCBI_DEPRECATED
#include <ncbiconf.h>

// standard includes
#include <string>

// xmlwrapp includes
#include <misc/xmlwrapp/errors.hpp>

namespace xml {

// forward declarations
class document;
namespace impl {
struct dtd_impl;
struct doc_impl;
}


/**
 * The xml::dtd class represents an XML dtd from a file.
**/
class dtd
{
public:
    typedef std::size_t size_type;

    /**
     * Create a new xml::dtd object by parsing the given XML dtd file.
     *
     * @param filename The XML dtd file name.
     * @param messages A pointer to the object where all the warnings are
     *                 collected. If NULL then no messages will be collected.
     * @param how How to treat warnings (default: warnings are not treated as
     *            errors). If warnings are treated as errors then an exception
     *            is thrown in case of both errors and/or warnings. If warnings
     *            are not treated as errors then an exception will be thrown
     *            only when there are errors.
     * @exception Throws xml::parser_exception in case of dtd parsing errors
     *            and std::exception in case of other problems.
    **/
    dtd (const char* filename, error_messages* messages,
         warnings_as_errors_type how = type_warnings_not_errors);

    /**
     * Validate the given XML document.
     *
     * @param doc XML document.
     * @param messages A pointer to the object where all the warnings and error
     *                 messages are collected. If NULL then no messages will be
     *                 collected.
     * @param how How to treat warnings (default: warnings are treated as
     *            errors). If warnings are treated as errors false is
     *            returned in case of both errors and/or warnings. If warnings
     *            are not treated as errors then false is returned
     *            only when there are errors.
     * @return true if the document is valid, false otherwise.
     * @exception Throws std::exception in case of problems.
    **/
    bool validate (const document& doc, error_messages* messages,
                   warnings_as_errors_type how = type_warnings_are_errors) const;

    /**
     * Destroy the object.
    **/
    virtual ~dtd();

    /**
     * Get the public ID.
     *
     * @return Public ID or NULL if not available
    **/
    const char* get_public_id (void) const;

    /**
     * Get the system ID.
     *
     * @return System ID or NULL if not available
    **/
    const char* get_system_id (void) const;

    /**
     * Get the name.
     *
     * @return Name or NULL if not available
    **/
    const char* get_name (void) const;

    /**
     * Moving constructor.
     * @param other The other dtd.
    **/
    dtd (dtd &&other);

    /**
     * Moving assignment.
     * @param other The other dtd.
    **/
    dtd& operator= (dtd &&other);

private:
    impl::dtd_impl *pimpl_;
    void set_dtd_data (void *data);
    void* get_raw_pointer (void) const;

    // Required by xml::document
    dtd ();

    // prohibited
    dtd (const dtd&);
    dtd& operator= (const dtd&);

    friend class document;
    friend struct impl::doc_impl;
};

} // xml namespace

#endif

