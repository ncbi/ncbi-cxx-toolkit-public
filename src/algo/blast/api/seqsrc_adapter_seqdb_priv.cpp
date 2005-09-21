#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] =
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */
/* ===========================================================================
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
 * Author:  Christiam Camacho
 *
 */

/** @file seqsrc_adapter_seqdb_priv.cpp
 * NOTE: This file contains work in progress and the APIs are likely to change,
 * please do not rely on them until this notice is removed.
 */


#include <ncbi_pch.hpp>
#include "seqsrc_adapter_seqdb_priv.hpp"
#include <algo/blast/api/seqsrc_seqdb.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(blast)

CSeqDbBSSAdapter::CSeqDbBSSAdapter(const CSearchDatabase& dbinfo)
    : IBlastSeqSrcAdapter(dbinfo)
{
}

BlastSeqSrc*
CSeqDbBSSAdapter::x_CalculateBlastSeqSrc(const CSearchDatabase& dbinfo)
{
    CSeqDB::ESeqType type =
        dbinfo.GetMoleculeType() == CSearchDatabase::eBlastDbIsProtein 
        ? CSeqDB::eProtein
        : CSeqDB::eNucleotide;

    // FIXME: refactor code in SplitDB/LibEntrezCacheEx.cpp ?
    if ( !dbinfo.GetEntrezQueryLimitation().empty() ) {
        NCBI_THROW(CException, eUnknown, "Unimplemented");
    }
    if ( !dbinfo.GetGiListLimitation().empty() ) {
        NCBI_THROW(CException, eUnknown, "Unimplemented");
    }

    /*
    CRef<CMySeqDbGiList> g(new
        CMySeqDbGiList(dbinfo.GetEntrezQueryLimitation(),
        dbinfo.GetGiListLimitation()));

    m_DbHandle.Reset(new CSeqDB(dbinfo.GetDatabaseName(), type, &*g));
    */
    m_DbHandle.Reset(new CSeqDB(dbinfo.GetDatabaseName(), type));
    return SeqDbBlastSeqSrcInit(m_DbHandle.GetNonNullPointer());
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
