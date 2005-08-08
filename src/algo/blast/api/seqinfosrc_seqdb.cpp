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
 * Author:  Ilya Dondoshansky
 *
 */

/** @file seqinfosrc_seqdb.cpp
 * Implementation of the concrete strategy for an IBlastSeqInfoSrc interface to
 * retrieve sequence identifiers and lengths from a BLAST database.
 */

#include <ncbi_pch.hpp>
#include <algo/blast/api/seqinfosrc_seqdb.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CSeqDbSeqInfoSrc::CSeqDbSeqInfoSrc(const string& dbname, bool is_protein)
{
    m_iSeqDb.Reset(new CSeqDB(dbname, (is_protein
                                       ? CSeqDB::eProtein
                                       : CSeqDB::eNucleotide)));
}

CSeqDbSeqInfoSrc::CSeqDbSeqInfoSrc(CSeqDB* seqdb)
{
    m_iSeqDb.Reset(seqdb);
}

CSeqDbSeqInfoSrc::~CSeqDbSeqInfoSrc()
{
}

list< CRef<CSeq_id> > CSeqDbSeqInfoSrc::GetId(Uint4 index) const
{
    return m_iSeqDb->GetSeqIDs(index);
}

Uint4 CSeqDbSeqInfoSrc::GetLength(Uint4 index) const
{
    return m_iSeqDb->GetSeqLength(index);
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.4  2005/08/08 16:29:36  dondosha
 * Added constructor from a CSeqDB instance
 *
 * Revision 1.3  2005/04/18 15:38:31  bealer
 * - Remove usage of deprecated SeqDB parts.
 *
 * Revision 1.2  2004/11/02 17:58:27  camacho
 * Add DOXYGEN_SKIP_PROCESSING to guard rcsid string
 *
 * Revision 1.1  2004/10/06 14:52:45  dondosha
 * Implementation of IBlastSeqInfoSrc with CSeqDB
 *
 *
 * ===========================================================================
 */
