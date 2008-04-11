#ifndef CORELIB___RWUTILS__HPP
#define CORELIB___RWUTILS__HPP

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

/// @file rwutils.hpp
/// Reader-writer utilities.

#include <corelib/reader_writer.hpp>


/** @addtogroup Stream
 *
 * @{
 */


BEGIN_NCBI_SCOPE

/// Appends to the given string.
void ExtractReaderContents(IReader& reader, string& s);

class CStringReader : public IReader
{
public:
    explicit CStringReader(const string& s)
        : m_String(s), m_Position(0)
        { }

    ERW_Result Read(void* buf, size_t count, size_t* bytes_read);
    ERW_Result PendingCount(size_t* count);

private:
    string    m_String;
    SIZE_TYPE m_Position;
};

END_NCBI_SCOPE


/* @} */

#endif  /* CORELIB___RWUTILS__HPP */
