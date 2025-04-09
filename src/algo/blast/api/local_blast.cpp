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

/** @file local_blast.cpp
 * NOTE: This file contains work in progress and the APIs are likely to change,
 * please do not rely on them until this notice is removed.
 */

#include <ncbi_pch.hpp>
#include <algo/blast/api/local_blast.hpp>
#include <algo/blast/api/uniform_search.hpp>
#include <algo/blast/api/blast_seqinfosrc.hpp>
#include "blast_aux_priv.hpp"
#include <objects/scoremat/PssmWithParameters.hpp>
#include <algo/blast/api/seqinfosrc_seqdb.hpp>
#include <algo/blast/api/blast_dbindex.hpp>
#include <util/profile/rtprofile.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

size_t
SplitQuery_GetChunkSize(EProgram program)
{
    size_t retval = 0;

    // used for experimentation purposes
    char* chunk_sz_str = getenv("CHUNK_SIZE");
    if (chunk_sz_str && !NStr::IsBlank(chunk_sz_str)) {
        retval = NStr::StringToInt(chunk_sz_str);
        _TRACE("Using query chunk size from environment " << retval);
    } else {

        switch (program) {
        case eBlastn:
            retval = 1000000;
            break;
        case eMegablast:
        case eDiscMegablast:
        case eMapper:
            retval = 5000000;
            break;
        case eTblastn:
            retval = 20000;
            break;
        // if the query will be translated, round the chunk size up to the next
        // multiple of 3, that way, when the nucleotide sequence(s) get(s)
        // split, context N%6 in one chunk will have the same frame as context
        // N%6 in the next chunk
        case eBlastx:
        case eTblastx:
            // N.B.: the splitting is done on the nucleotide query sequences,
            // then each of these chunks is translated
            retval = 10002;
            break;
        case eVecScreen:
        	// Disable query splitting for vecscreen
        	retval =1;
        	break;
        case eBlastp:
        default:
            retval = 10000;
            break;
        }
    }

    const EBlastProgramType prog_type(EProgramToEBlastProgramType(program));
    if (Blast_QueryIsTranslated(prog_type) && !Blast_SubjectIsPssm(prog_type) &&
        (retval % CODON_LENGTH) != 0) {
        NCBI_THROW(CBlastException, eInvalidArgument, 
                   "Split query chunk size must be divisible by 3");
    }
    _TRACE("Returning query chunk size " << retval << " for " << EProgramToTaskName(program));
    return retval;
}

CLocalBlast::CLocalBlast(CRef<IQueryFactory> qf,
                         CRef<CBlastOptionsHandle> opts_handle,
                         const CSearchDatabase& dbinfo)
: m_QueryFactory    (qf),
  m_Opts            (const_cast<CBlastOptions*>(&opts_handle->GetOptions())),
  m_InternalData    (0),
  m_PrelimSearch    (new CBlastPrelimSearch(qf, m_Opts, dbinfo)),
  m_TbackSearch     (0)
{}

CLocalBlast::CLocalBlast(CRef<IQueryFactory> qf,
                         CRef<CBlastOptionsHandle> opts_handle,
                         CRef<CLocalDbAdapter> db)
: m_QueryFactory    (qf),
  m_Opts            (const_cast<CBlastOptions*>(&opts_handle->GetOptions())),
  m_InternalData    (0),
  m_PrelimSearch    (new CBlastPrelimSearch(qf, m_Opts, db)),
  m_TbackSearch     (0),
  m_LocalDbAdapter  (db.GetNonNullPointer())
{}

CLocalBlast::CLocalBlast(CRef<IQueryFactory> qf,
                         CRef<CBlastOptionsHandle> opts_handle,
                         BlastSeqSrc* seqsrc,
                         CRef<IBlastSeqInfoSrc> seqInfoSrc)
: m_QueryFactory    (qf),
  m_Opts            (const_cast<CBlastOptions*>(&opts_handle->GetOptions())),
  m_InternalData    (0),
  m_PrelimSearch    (new CBlastPrelimSearch(qf, m_Opts, seqsrc,
                                            CRef<CPssmWithParameters>())),
  m_TbackSearch     (0),
  m_SeqInfoSrc      (seqInfoSrc)
{}

/** FIXME: this should be removed as soon as we safely can
 * We will be able to do this once we are guaranteed that every
 * constructor to CLocalBlast takes or can construct a IBlastSeqInfoSrc
 * on it's own.
 * This function is dangerous as it assumes that the BlastSeqSrc
 * is based upon CSeqDB, which is not guaranteed.
 */
static IBlastSeqInfoSrc*
s_InitSeqInfoSrc(const BlastSeqSrc* seqsrc)
{
     string db_name;
     if (const char* seqsrc_name = BlastSeqSrcGetName(seqsrc)) {
         db_name.assign(seqsrc_name);
     }
     if (db_name.empty()) {
         NCBI_THROW(CBlastException, eNotSupported,
                    "BlastSeqSrc does not provide a name, probably it is not a"
                    " BLAST database");
     }
     bool is_prot = BlastSeqSrcGetIsProt(seqsrc) ? true : false;
     return new CSeqDbSeqInfoSrc(db_name, is_prot);
}

    
CRef<CSearchResultSet>
CLocalBlast::Run()
{
    _ASSERT(m_QueryFactory);
    _ASSERT(m_PrelimSearch);
    _ASSERT(m_Opts);
    BLAST_PROF_MARK2( m_batch_num_str + string("_BLAST.SETUP.START") );    
    // Note: we need to pass the search messages ...
    // filtered query regions should be masked in the BLAST_SequenceBlk
    // already.

    int status = m_PrelimSearch->CheckInternalData();
    if (status != 0)
    {
         // Search was not run, but we send back an empty CSearchResultSet.
         CRef<ILocalQueryData> local_query_data = m_QueryFactory->MakeLocalQueryData(m_Opts);
         // TSeqLocVector slv = m_QueryFactory.GetTSeqLocVector();
         vector< CConstRef<objects::CSeq_id> > seqid_vec;
         vector< CRef<CBlastAncillaryData> > ancill_vec;
         TSeqAlignVector sa_vec;
         size_t index;
         EResultType res_type = eDatabaseSearch;
         unsigned int num_subjects = 0;
         if (m_LocalDbAdapter.NotEmpty() && !m_LocalDbAdapter->IsBlastDb() && !m_LocalDbAdapter->IsDbScanMode()) {
        	 res_type = eSequenceComparison;
             IBlastSeqInfoSrc *  subject_infosrc = m_LocalDbAdapter->MakeSeqInfoSrc();
             if(subject_infosrc != NULL) {
            	 num_subjects = static_cast<unsigned int>(subject_infosrc->Size());
             }
         }
         TSearchMessages msg_vec;
         for (index=0; index<local_query_data->GetNumQueries(); index++)
         {
              CConstRef<objects::CSeq_id> query_id(local_query_data->GetSeq_loc(index)->GetId());
              TQueryMessages q_msg;
              local_query_data->GetQueryMessages(index, q_msg);
              msg_vec.push_back(q_msg);
              seqid_vec.push_back(query_id);
              CRef<objects::CSeq_align_set> tmp_align;
              sa_vec.push_back(tmp_align);
              pair<double, double> tmp_pair(-1.0, -1.0);
              CRef<CBlastAncillaryData>  tmp_ancillary_data(new CBlastAncillaryData(tmp_pair, tmp_pair, tmp_pair, 0));
              ancill_vec.push_back(tmp_ancillary_data);

              for(unsigned int i =1; i < num_subjects; i++)
              {
            	  TQueryMessages msg;
                  msg_vec.push_back(msg);
                  seqid_vec.push_back(query_id);
                  CRef<objects::CSeq_align_set> tmp_align;
                  sa_vec.push_back(tmp_align);
           	      CRef<CBlastAncillaryData>  tmp_ancillary_data(new CBlastAncillaryData(tmp_pair, tmp_pair, tmp_pair, 0));
           	      ancill_vec.push_back(tmp_ancillary_data);
              }
         }
         msg_vec.Combine(m_PrelimSearch->GetSearchMessages());

         CRef<CSearchResultSet> result_set(new CSearchResultSet(seqid_vec, sa_vec, msg_vec, ancill_vec, 0, res_type));
         return result_set;
    }
    
    BLAST_PROF_MARK2( m_batch_num_str + string("_BLAST.SETUP.STOP") );    
    BLAST_PROF_MARK2( m_batch_num_str + string("_BLAST.PRE.START") );    
	try {
	    m_PrelimSearch->SetNumberOfThreads(GetNumberOfThreads());
	    m_InternalData = m_PrelimSearch->Run();
	} catch( CIndexedDbException & e) {
        _TRACE(e.GetMsg());
	    throw;
	} catch (CBlastException & e) {
        _TRACE(e.GetMsg());
		if(e.GetErrCode() == CBlastException::eCoreBlastError) {
			throw;
		}
	}
	catch (CSeqDBException & e) {
		if (e.GetErrCode() == CSeqDBException::eTooManyOpenFiles) {
            _TRACE(e.GetMsg());  
			string err_msg = BlastErrorCode2String(BLASTERR_DB_TOO_MANY_OPEN_FILES);
			NCBI_THROW(CBlastException, eCoreBlastError, err_msg);
		}
		else {
            _TRACE(e.GetMsg());  
			string err_msg = BlastErrorCode2String(BLASTERR_DB_MEMORY_MAP);
			NCBI_THROW(CBlastException, eCoreBlastError, err_msg);
		}
	}
    catch (std::exception & e) {
        _TRACE(e.what());  
    	NCBI_THROW(CBlastException, eSystem, e.what());
    }

    //_ASSERT(m_InternalData);
    BLAST_PROF_MARK2( m_batch_num_str + string("_BLAST.PRE.STOP") );    
    BLAST_PROF_MARK2( m_batch_num_str + string("_BLAST.TB.START") );    
    
    TSearchMessages search_msgs = m_PrelimSearch->GetSearchMessages();
    
    CRef<IBlastSeqInfoSrc> seqinfo_src;
    
    if (m_SeqInfoSrc.NotEmpty())
    {
        // Use the SeqInfoSrc provided by the user during construction
        seqinfo_src = m_SeqInfoSrc;
    }
    else if (m_LocalDbAdapter.NotEmpty()) {
        // This path is preferred because it preserves the GI list
        // limitation if there is one.  DBs with both internal OID
        // filtering and user GI list filtering will not do complete
        // filtering during the traceback stage, which can cause
        // 'Unknown defline' errors during formatting.
        
        seqinfo_src.Reset(m_LocalDbAdapter->MakeSeqInfoSrc());
    } else {
        seqinfo_src.Reset(s_InitSeqInfoSrc(m_InternalData->m_SeqSrc->GetPointer()));
    }
    
    m_TbackSearch.Reset(new CBlastTracebackSearch(m_QueryFactory,
                                                  m_InternalData,
                                                  m_Opts,
                                                  seqinfo_src,
                                                  search_msgs));
    if (m_LocalDbAdapter.NotEmpty() && !m_LocalDbAdapter->IsBlastDb() 
	&& !m_LocalDbAdapter->IsDbScanMode()) {
        m_TbackSearch->SetResultType(eSequenceComparison);
    }
    m_TbackSearch->SetNumberOfThreads(GetNumberOfThreads());
    CRef<CSearchResultSet> retval = m_TbackSearch->Run();
    retval->SetFilteredQueryRegions(m_PrelimSearch->GetFilteredQueryRegions());
    m_Messages = m_TbackSearch->GetSearchMessages();

    BLAST_PROF_MARK2( m_batch_num_str + string("_BLAST.TB.STOP") );    
    return retval;
}

Int4 CLocalBlast::GetNumExtensions()
{
    Int4 retv = 0;
    if (m_InternalData) {
        BlastDiagnostics * diag = m_InternalData->m_Diagnostics->GetPointer();
        if (diag && diag->ungapped_stat) {
             retv = diag->ungapped_stat->good_init_extends;
        }
    }
    return retv;
}

BlastDiagnostics* CLocalBlast::GetDiagnostics()
{
    BlastDiagnostics* diag=NULL;
    
    if (m_InternalData) {
        diag = Blast_DiagnosticsCopy(m_InternalData->m_Diagnostics->GetPointer());
    }

    return diag;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
