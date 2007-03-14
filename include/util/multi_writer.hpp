#ifndef UTIL___MULTI_WRITER__HPP
#define UTIL___MULTI_WRITER__HPP

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
 * Authors:  Maxim Didenko
 *
 * File Description:
 *
 */

/// @file transmissionrw.hpp
/// Reader writer with transmission checking
/// @sa IReader, IWriter

#include <corelib/ncbimisc.hpp>
#include <corelib/reader_writer.hpp>
#include <corelib/ncbidbg.hpp> // for _ASSERT


BEGIN_NCBI_SCOPE


/// IWriter which can write simultaneously to the different streams
///
class NCBI_XUTIL_EXPORT CMultiWriter : public IWriter
{
public:

    explicit CMultiWriter(const list<CNcbiOstream*>& streams);
    virtual ~CMultiWriter();

    virtual ERW_Result Write(const void* buf,
                             size_t      count,
                             size_t*     bytes_written = 0);

    virtual ERW_Result Flush(void);

private:
    CMultiWriter(const CMultiWriter&);
    CMultiWriter& operator=(CMultiWriter&);
private:
    list<CNcbiOstream*>    m_Dest;
};


END_NCBI_SCOPE

#endif /* UTIL___MULTI_WRITER__HPP */
