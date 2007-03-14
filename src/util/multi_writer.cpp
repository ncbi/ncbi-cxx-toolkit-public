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
 * Author: Maxim Didenko
 *
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbimisc.hpp>
#include <util/util_exception.hpp>
#include <util/multi_writer.hpp>

BEGIN_NCBI_SCOPE

CMultiWriter::CMultiWriter(const list<CNcbiOstream*>& dest)
    : m_Dest(dest)
{
}


CMultiWriter::~CMultiWriter()
{
}


ERW_Result CMultiWriter::Write(const void* buf,
                               size_t      count,
                               size_t*     bytes_written)
{
    NON_CONST_ITERATE( list<CNcbiOstream*>, it, m_Dest) {
        (*it)->write((const char*)buf, count);        
    }
    if ( bytes_written ) *bytes_written = count;
    return eRW_Success;
}

ERW_Result CMultiWriter::Flush(void)
{
    NON_CONST_ITERATE( list<CNcbiOstream*>, it, m_Dest) {
        (*it)->flush();
    }
    return eRW_Success;

}

END_NCBI_SCOPE
