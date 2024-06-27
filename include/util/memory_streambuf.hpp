#ifndef UTIL___MEMORY_STREAMBUF__HPP
#define UTIL___MEMORY_STREAMBUF__HPP

/* $Id$
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
 * Authors:  Sergiy Gotvyanskyy
 *           Anton Lavrentiev (Toolkit adaptation)
 *
 * File Description:
 *   Stream buffer based on a fixed size memory area
 *
 */

/// @file memory_streambuf.hpp

#include <corelib/ncbistre.hpp>


BEGIN_NCBI_SCOPE


class NCBI_XUTIL_EXPORT CMemory_Streambuf : public CNcbiStreambuf
{
public:
    CMemory_Streambuf(const char* area, size_t size);
    CMemory_Streambuf(char*       area, size_t size);

protected:
    CT_INT_TYPE overflow(CT_INT_TYPE c) override;
    streamsize  xsputn(const CT_CHAR_TYPE* buf, streamsize n) override;

    CT_INT_TYPE underflow(void) override;
    streamsize  xsgetn(CT_CHAR_TYPE* buf, streamsize n) override;
    streamsize  showmanyc(void) override;

    CT_POS_TYPE seekoff(CT_OFF_TYPE off, IOS_BASE::seekdir whence,
                        IOS_BASE::openmode which =
                        IOS_BASE::in | IOS_BASE::out) override;
    CT_POS_TYPE seekpos(CT_POS_TYPE pos,
                        IOS_BASE::openmode which =
                        IOS_BASE::in | IOS_BASE::out) override;

private:
    const char* m_begin;
    const char* m_end;
};


END_NCBI_SCOPE

#endif /* UTIL___MEMORY_STREAMBUF_HPP */
