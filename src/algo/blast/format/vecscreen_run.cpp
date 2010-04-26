#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

/*
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

Author: Tom Madden

******************************************************************************/

/** @file vecscreen_run.cpp
 * Run vecscreen, produce output
*/

#include <ncbi_pch.hpp>
#include <algo/blast/format/blast_format.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <algo/blast/api/sseqloc.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>
#include <algo/blast/api/local_blast.hpp>

#include <objmgr/util/seq_loc_util.hpp>

#include <algo/blast/format/vecscreen_run.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
USING_SCOPE(sequence);
#endif

// static const string kGifLegend[] = {"Strong", "Moderate", "Weak", "Suspect"};

CVecscreenRun::CVecscreenRun(CRef<CSeq_loc> seq_loc, CRef<CScope> scope, const string & db)
 : m_SeqLoc(seq_loc), m_Scope(scope), m_DB(db)
{

   TSeqLocVector query;
   SSeqLoc ssl(*m_SeqLoc, *m_Scope);
   query.push_back(ssl);

   // Load blast query.
   CRef<IQueryFactory> query_factory(new CObjMgr_QueryFactory(query));

   // BLAST optiosn needed for vecscreen.
   CRef<CBlastOptionsHandle> opts(CBlastOptionsFactory::CreateTask("vecscreen"));

   // Sets Vecscreen database.
   const CSearchDatabase target_db(m_DB, CSearchDatabase::eBlastDbIsNucleotide);

   // Constructor for blast run.
   CLocalBlast blaster(query_factory, opts, target_db);

   // BLAST run.
   CRef<CSearchResultSet> results = blaster.Run();

   // The vecscreen stuff follows.
   m_Vecscreen = new CVecscreen(*((*results)[0].GetSeqAlign()), GetLength(*m_SeqLoc, m_Scope));

   // This actually does the vecscreen work.
   CRef<objects::CSeq_align_set> screen_aln = m_Vecscreen->ProcessSeqAlign();
}

list<CVecscreenRun::SVecscreenSummary>
CVecscreenRun::GetList() const
{
    list<CVecscreenRun::SVecscreenSummary> retval;

    const list<CVecscreen::AlnInfo*>* aln_info = m_Vecscreen->GetAlnInfoList();

    list<CVecscreen::AlnInfo*>::const_iterator itr=aln_info->begin();
    for ( ; itr != aln_info->end(); ++itr)
    {
       if ((*itr)->type == CVecscreen::eNoMatch)
          continue;
       SVecscreenSummary summary;
       summary.seqid = m_SeqLoc->GetId();
       summary.range = (*itr)->range;
       // summary.match_type = kGifLegend[(*itr)->type];
       summary.match_type = CVecscreen::GetStrengthString((*itr)->type);
       retval.push_back(summary);
    }

    return retval;
}
