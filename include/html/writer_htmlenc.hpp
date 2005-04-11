#ifndef HTML___WRITER_HTMLENC__HPP
#define HTML___WRITER_HTMLENC__HPP

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
 * Authors:  Aaron Ucko
 *
 */

/// @file writer_htmlenc.hpp
/// CWriter_HTMLEncoder -- HTML-encode supplied data on the fly before
/// passing it to a standard ostream.

#include <corelib/ncbistre.hpp>
#include <util/reader_writer.hpp>


/** @addtogroup HTMLStream
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class CWriter_HTMLEncoder : public IWriter
{
public:
    enum EFlags {
        /// Like CHTMLHelper::HTMLEncode, pass numeric entity
        /// specifications of the form &#...; through unmodified,
        /// rather than encoding the leading ampersand.
        fPassNumericEntities = 0x1
    };
    typedef int TFlags; ///< Binary OR of EFlags

    /// Wrap the supplied output stream (but do not own it).
    CWriter_HTMLEncoder(CNcbiOstream& o, TFlags flags = 0)
        : m_Stream(o), m_Flags(flags)
        { }
    ~CWriter_HTMLEncoder();

    // Implement IWriter's pure virtual methods

    ERW_Result Write(const void* buf, size_t count, size_t* bytes_written = 0);
    ERW_Result Flush(void);

private:
    enum EPrivateFlags {
        fTrailingAmpersand = 0x10000
    };

    CNcbiOstream& m_Stream;
    TFlags        m_Flags;
};


END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/04/11 17:59:46  ucko
 * CWriter_HTMLEncoder -- HTML-encode supplied data on the fly before
 * passing it to a standard ostream.  (Initial revision.)
 *
 * ===========================================================================
 */

#endif  /* HTML___WRITER_HTMLENC__HPP */
