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
 * Credits: Denis Vakatov, NCBI (API design)
 *
 */


/** @file
 * This file contains the definition of the xslt::exception class.
 */


#ifndef _xslt_exception_h_
#define _xslt_exception_h_

#include <misc/xmlwrapp/exception.hpp>

/// XML library namespace
namespace xslt
{
    /**
     * This exception class is thrown by xmlwrapp for all runtime XSLT-related
     * errors.
     *
     * @note C++ runtime may still throw other errors when used from xmlwrapp.
     *       Also, std::bad_alloc() is thrown in out-of-memory situations.
     */
    class exception : public xml::exception
    {
    public:
        explicit exception(const std::string& msg) : xml::exception(msg)
        {}
    };

} // namespace xslt

#endif // _xslt_exception_h_

