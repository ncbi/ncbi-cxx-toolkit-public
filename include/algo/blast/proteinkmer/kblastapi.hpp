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
 * Author: Tom Madden
 *
 * File Description:
 *   Class to run BLAST search based on KMER preprocessing.
 *
 */

#include <objmgr/object_manager.hpp>
#include <algo/blast/api/local_db_adapter.hpp>
#include <algo/blast/api/blastp_kmer_options.hpp>
#include <algo/blast/api/blastp_kmer_options.hpp>
#include <math.h>

#include <algo/blast/proteinkmer/blastkmer.hpp>
#include <algo/blast/proteinkmer/blastkmerresults.hpp>
#include <algo/blast/proteinkmer/blastkmeroptions.hpp>

#ifndef BLAST_KMER_API_HPP
#define BLAST_KMER_API_HPP


USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(blast);

/////////////////////////////////////////////////////////////////////////////
/// Threading class for BlastKmer searches.
/// Each thread runs a batch of input sequences
/// through KMER lookup and then through BLAST.
////////////
class NCBI_XBLAST_EXPORT CBlastKmerSearch : public CObject
{
public:
    CBlastKmerSearch(CRef<IQueryFactory> queryFactory,
                     CRef<CBlastpKmerOptionsHandle> optsHndl,
                     CRef<CLocalDbAdapter> db)
    : m_QueryFactory(queryFactory),
      m_OptsHandle(optsHndl),
      m_Database(db)
   { }

    CBlastKmerSearch(CRef<CBlastpKmerOptionsHandle> optsHndl,
                     CRef<CLocalDbAdapter> db)
    : m_OptsHandle(optsHndl),
      m_Database(db)
   { }

   /// Sets the queries.  Overrides any queries already set.
   void SetQuery(CRef<IQueryFactory> queryFactory) {m_QueryFactory = queryFactory;}

   /// Limits output by GILIST
   /// @param list CRef<CSeqDBGiList> to limit by [in]
   void SetGiListLimit(CRef<CSeqDBGiList> list) {m_GIList=list;}

   /// Limits output by negative GILIST
   /// @param list CRef<CSeqDBNegativeList> to limit by [in]
   void SetGiListLimit(CRef<CSeqDBNegativeList> list) {m_NegGIList=list;}

   /// Run a KMER and then BLAST search
   CRef<CSearchResultSet> Run(void);

private:

    /// Holds the query seqloc and scope
    CRef<IQueryFactory> m_QueryFactory;

    /// Options for KMER search
    CRef<CBlastpKmerOptionsHandle> m_OptsHandle;

    /// Database to search.
    CRef<CLocalDbAdapter> m_Database;

    /// GIList to limit search by.
    CRef<CSeqDBGiList> m_GIList;

    /// Negative GIList to limit search by.
    /// Only one of the gilist or negative GIlist should be set.
    CRef<CSeqDBNegativeList> m_NegGIList;
};

#endif /* BLAST_KMER_API_HPP */
