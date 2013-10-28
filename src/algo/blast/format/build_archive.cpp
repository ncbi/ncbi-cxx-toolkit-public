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
* Author:  Tom Madden
*
* ===========================================================================
*/

/// @file build_archive.cpp
/// Builds archive format from BLAST results.

#include <ncbi_pch.hpp>
#include <corelib/ncbi_system.hpp>
#include <serial/iterator.hpp>
#include <algo/blast/format/build_archive.hpp>
#include <algo/blast/api/version.hpp>
#include <algo/blast/api/remote_blast.hpp>
#include <algo/blast/api/blast_options_builder.hpp>
#include <algo/blast/api/uniform_search.hpp>
#include <algo/blast/api/blast_results.hpp>
#include <algo/blast/api/search_strategy.hpp>

#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/blast/Blast4_ka_block.hpp>
#include <objects/blast/blast__.hpp>
#include <objects/blast/names.hpp>

#if defined(NCBI_OS_UNIX)
#include <unistd.h>
#endif

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)


static CRef<CBlast4_ka_block>
s_Convert_to_CBlast_ka_block(const Blast_KarlinBlk* kablk, bool gapped)
{
    CRef<CBlast4_ka_block> retval(new CBlast4_ka_block);
    if (kablk)
    {
       retval->SetLambda(kablk->Lambda);
       retval->SetK(kablk->K);
       retval->SetH(kablk->H);
    } else {
       retval->SetLambda(-1.0);
       retval->SetK(-1.0);
       retval->SetH(-1.0);
    }
    retval->SetGapped(gapped);
    return retval;
}

static CRef<objects::CBlast4_archive>
s_BuildArchiveAll(CRef<CExportStrategy>  export_strategy,
                     blast::CBlastOptionsHandle& options_handle,
                     const CSearchResultSet& results)
{
        CRef<objects::CBlast4_archive> archive(new objects::CBlast4_archive());

        CRef<CBlast4_request> net_request = export_strategy->GetSearchStrategy();
        CBlastVersion v;
        net_request->SetIdent(v.Print());

        archive->SetRequest(*net_request);
        bool isPsiblast = false;
        if(net_request->CanGetBody() && net_request->GetBody().IsQueue_search())
        {
        	if(net_request->GetBody().GetQueue_search().GetService() == "psi")
        		isPsiblast=true;
        }

        CRef<CSeq_align_set> seqalign_set(new CSeq_align_set);
         _ASSERT(seqalign_set.NotEmpty());

        CRef<objects::CBlast4_get_search_results_reply> net_results(new objects::CBlast4_get_search_results_reply());

        TSeqLocInfoVector mask_vector;

        list<CRef<CBlast4_mask> >& net_masks = net_results->SetMasks();

        bool first_time = true;
        Int8 effective_search_space = 0;
        Int8 length_adjustment = 0;
        ITERATE(CSearchResultSet, result, results) {
             CConstRef<CSeq_align_set> result_set =
                        (*result)->GetSeqAlign();
             if (result_set.NotEmpty() && !result_set->IsEmpty()) {
                 seqalign_set->Set().insert(seqalign_set->Set().end(),
                                                   result_set->Get().begin(),
                                                   result_set->Get().end());
             }
             if (first_time)
             {
                    CRef<CBlastAncillaryData> ancill_data = (*result)->GetAncillaryData();
                    list<CRef<CBlast4_ka_block> >& ka_list = net_results->SetKa_blocks();

                    ka_list.push_back(s_Convert_to_CBlast_ka_block(isPsiblast? ancill_data->GetPsiUngappedKarlinBlk():
                    														   ancill_data->GetUngappedKarlinBlk(), false));
                    ka_list.push_back(s_Convert_to_CBlast_ka_block(isPsiblast? ancill_data->GetPsiGappedKarlinBlk() :
                    		    											   ancill_data->GetGappedKarlinBlk(), true));
                    effective_search_space = ancill_data->GetSearchSpace();
                    length_adjustment = ancill_data->GetLengthAdjustment();
                    first_time = false;
             }
             TMaskedQueryRegions query_masks;
             (*result)->GetMaskedQueryRegions(query_masks);
             mask_vector.push_back(query_masks);
        }
        list<CRef<CBlast4_mask> > masks = 
                   CRemoteBlast::ConvertToRemoteMasks(mask_vector, options_handle.GetOptions().GetProgramType());
        net_masks.insert(net_masks.end(), masks.begin(), masks.end());

        list<string>& search_stats = net_results->SetSearch_stats();
        search_stats.push_back("Effective search space: " + NStr::Int8ToString(effective_search_space));
        search_stats.push_back("Effective search space used: " + NStr::Int8ToString(effective_search_space));
        search_stats.push_back("Length adjustment: " + NStr::Int8ToString(length_adjustment));

        net_results->SetAlignments(*seqalign_set);
        archive->SetResults(*net_results);

        return archive;
}


CRef<objects::CBlast4_archive>
BlastBuildArchive(blast::IQueryFactory& queries,
                     blast::CBlastOptionsHandle& options_handle,
                     const CSearchResultSet& results,
                     const string& dbname,
                     unsigned int num_iters)
{
        CSearchDatabase::EMoleculeType mol_type = CSearchDatabase::eBlastDbIsNucleotide;
        if (Blast_SubjectIsNucleotide(options_handle.GetOptions().GetProgramType()))
        	mol_type = CSearchDatabase::eBlastDbIsNucleotide;
	else
        	mol_type = CSearchDatabase::eBlastDbIsProtein;

       	CSearchDatabase db(dbname, mol_type);

        CRef<blast::IQueryFactory> iquery_ref(&queries);
        CRef<blast::CBlastOptionsHandle> options_ref(&options_handle);
        CRef<blast::CSearchDatabase> search_db(&db);
       	CRef<CExportStrategy> export_strategy;
        if(num_iters != 0)
        	export_strategy.Reset(new CExportStrategy(iquery_ref, options_ref, search_db, kEmptyStr, num_iters));
        else
        	export_strategy.Reset(new CExportStrategy(iquery_ref, options_ref, search_db));

        CRef<objects::CBlast4_archive> archive = s_BuildArchiveAll(export_strategy, options_handle, results);
        return archive;

}

CRef<objects::CBlast4_archive>
BlastBuildArchive(blast::IQueryFactory& queries,
                     blast::CBlastOptionsHandle& options_handle,
                     const CSearchResultSet& results,
                     blast::IQueryFactory& subjects)
{
        CRef<blast::IQueryFactory> iquery_ref(&queries);
        CRef<blast::IQueryFactory> isubject_ref(&subjects);
        CRef<blast::CBlastOptionsHandle> options_ref(&options_handle);
        CRef<CExportStrategy> export_strategy(new CExportStrategy(iquery_ref, options_ref, isubject_ref));

        return s_BuildArchiveAll(export_strategy, options_handle, results);
}

CRef<objects::CBlast4_archive>
BlastBuildArchive(objects::CPssmWithParameters & pssm,
                  blast::CBlastOptionsHandle& options_handle,
                  const CSearchResultSet& results,
                  const string& dbname,
                  unsigned int num_iters)
{
       	CSearchDatabase db(dbname, CSearchDatabase::eBlastDbIsProtein);

        CRef<objects::CPssmWithParameters> pssm_ref(&pssm);
        CRef<blast::CBlastOptionsHandle> options_ref(&options_handle);
        CRef<blast::CSearchDatabase> search_db(&db);
        CRef<CExportStrategy> export_strategy(new CExportStrategy(pssm_ref, options_ref, search_db, kEmptyStr, num_iters));

        CRef<objects::CBlast4_archive> archive = s_BuildArchiveAll(export_strategy, options_handle, results);
        return archive;

}
END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
