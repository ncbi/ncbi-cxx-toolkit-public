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
 * Author:  Ilya Dondoshansky
 *
 * ===========================================================================
 */

/// @file db_blast.cpp
/// Implemantation of a database BLAST class CDbBlast.

#include <ncbi_pch.hpp>

#ifdef Main
#undef Main
#endif
#include <corelib/ncbithr.hpp>

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objmgr/seq_vector.hpp> // For CSeqVectorException type

#include <algo/blast/api/db_blast.hpp>
#include <algo/blast/api/blast_options.hpp>
#include "blast_seqalign.hpp"
#include "blast_setup.hpp"
#include <algo/blast/api/blast_mtlock.hpp>
#include <algo/blast/api/seqinfosrc_seqdb.hpp>

// Core BLAST engine includes
#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_setup.h>
#include <algo/blast/core/lookup_wrap.h>
#include <algo/blast/core/blast_engine.h>
#include <algo/blast/core/blast_message.h>
#include <algo/blast/core/hspstream_collector.h>
#include <algo/blast/core/blast_traceback.h>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

///////////////////////////////////////////////////////////////////////
/// Definition and methods of the CDbBlastPrelim class,
/// performing preliminary stage of a BLAST search. 
///////////////////////////////////////////////////////////////////////

/// Runs the BLAST algorithm between a set of sequences and BLAST database
class NCBI_XBLAST_EXPORT CDbBlastPrelim : public CObject
{
public:

    /// Constructor using a CDbBlast object
    CDbBlastPrelim(CDbBlast& blaster);
    virtual ~CDbBlastPrelim();
    virtual int Run();

private:
    /// Data members coming from the client code and not owned by this object
    BlastHSPStream*     m_pHspStream;   /**< Placeholder for streaming HSP 
                                           lists out of the engine. */
    CRef<CBlastOptions> m_Options;      ///< Blast options
    BLAST_SequenceBlk*  m_pQueries;     ///< Structure for all queries
    LookupTableWrap*    m_pLookupTable; ///< Lookup table, one for all queries
    BlastDiagnostics*   m_pDiagnostics; ///< Diagnostic return structures
    BlastScoreBlk*      m_pScoreBlock;  ///< Statistical and scoring parameters

    /// Internal data members
    BlastSeqSrc*        m_ipSeqSrc;     ///< Subject sequences sorce

    CBlastQueryInfo     m_iclsQueryInfo;///< Data for all queries

    /// Prohibit copy constructor
    CDbBlastPrelim(const CDbBlastPrelim& rhs);
    /// Prohibit assignment operator
    CDbBlastPrelim& operator=(const CDbBlastPrelim& rhs);
};

/// Constructor from a full BLAST search object
CDbBlastPrelim::CDbBlastPrelim(CDbBlast& blaster)
{
    m_pHspStream = blaster.GetHSPStream();
    m_Options.Reset(&blaster.SetOptionsHandle().SetOptions());    
    m_pQueries = blaster.GetQueryBlk();
    m_pLookupTable = blaster.GetLookupTable();
    m_pDiagnostics = blaster.GetDiagnostics();
    m_pScoreBlock = blaster.GetScoreBlk();
    m_ipSeqSrc = BlastSeqSrcCopy(blaster.GetSeqSrc());
    m_iclsQueryInfo.Reset(BlastQueryInfoDup(blaster.GetQueryInfo()));
}

/// Destructor: needs to free local copy of the subject sequences source.
CDbBlastPrelim::~CDbBlastPrelim()
{ 
    BlastSeqSrcFree(m_ipSeqSrc);
}

/// Perform the preliminary stage of a BLAST search
int CDbBlastPrelim::Run()
{
    return 
        Blast_RunPreliminarySearch(m_Options->GetProgramType(),
            m_pQueries, m_iclsQueryInfo, m_ipSeqSrc,
            m_Options->GetScoringOpts(), m_pScoreBlock, m_pLookupTable, 
            m_Options->GetInitWordOpts(), m_Options->GetExtnOpts(), 
            m_Options->GetHitSaveOpts(), m_Options->GetEffLenOpts(), 
            m_Options->GetPSIBlastOpts(), m_Options->GetDbOpts(),
            m_pHspStream, m_pDiagnostics);

}

///////////////////////////////////////////////////////////////////////
/// Definition and methods of the CPrelimBlastThread class,
/// performing preliminary stage of a multi-threaded BLAST search. 
///////////////////////////////////////////////////////////////////////

/** Data structure containing all information necessary for production of the
 * thread output.
 */
class NCBI_XBLAST_EXPORT CPrelimBlastThread : public CThread 
{
public:
    CPrelimBlastThread(CDbBlast& blaster);
    ~CPrelimBlastThread();
protected:
    virtual void* Main(void);
    virtual void OnExit(void);
private:
    CRef<CDbBlastPrelim> m_iBlaster; ///< Object containing all search data
};

/// Constructor: creates a preliminary search object for internal use.
CPrelimBlastThread::CPrelimBlastThread(CDbBlast& blaster)
{
    // Copy the BLAST seach object
    m_iBlaster.Reset(new CDbBlastPrelim(blaster));
}

/// Destructor
CPrelimBlastThread::~CPrelimBlastThread()
{
}

/// Performs this thread's search by calling the CDbBlastPrelim class's Run 
/// method.
void* CPrelimBlastThread::Main(void) 
{
    long status = m_iBlaster->Run();
    return (void*) status;
}

/// Does nothing on exit
void CPrelimBlastThread::OnExit(void)
{ 
}

///////////////////////////////////////////////////////////////////////
/// Methods of the CDbBlast class
///////////////////////////////////////////////////////////////////////

/// Initializes internal pointers and fields. 
void CDbBlast::x_InitFields()
{
    m_ibQuerySetUpDone = false;
    m_ipScoreBlock = NULL;
    m_ipLookupTable = NULL;
    m_ipLookupSegments = NULL;
    m_ipFilteredRegions = NULL;
    m_ipResults = NULL;
    m_ipDiagnostics = NULL;
    m_ibLocalResults = false;
    m_ibTracebackOnly = false;
}

void CDbBlast::x_Blast_RPSInfoInit(BlastRPSInfo **ppinfo, 
                                   CMemoryFile **rps_mmap,
                                   CMemoryFile **rps_pssm_mmap,
                                   string dbname)
{
   BlastRPSInfo *info = new BlastRPSInfo;
   if (info == NULL) {
      NCBI_THROW(CBlastException, eOutOfMemory, 
                 "RPSInfo allocation failed");
   }

   vector<string> dbpath;
   CSeqDB::FindVolumePaths(dbname, 'p', dbpath);
   if (dbpath.empty()) {
       NCBI_THROW(CBlastException, eBadParameter,
                   "Cannot retrieve path to RPS database");
   }

   CMemoryFile *lut_mmap = new CMemoryFile(dbpath[0] + ".loo");
   if (lut_mmap == NULL) {
       NCBI_THROW(CBlastException, eBadParameter,
                   "Cannot map RPS BLAST lookup file");
   }
   info->lookup_header = (BlastRPSLookupFileHeader *)lut_mmap->GetPtr();

   CMemoryFile *pssm_mmap = new CMemoryFile(dbpath[0] + ".rps");
   if (pssm_mmap == NULL) {
       NCBI_THROW(CBlastException, eBadParameter,
                   "Cannot map RPS BLAST profile file");
   }
   info->profile_header = (BlastRPSProfileHeader *)pssm_mmap->GetPtr();

   CNcbiIfstream auxfile( (dbpath[0] + ".aux").c_str() );
   if (auxfile.bad() || auxfile.fail()) {
       NCBI_THROW(CBlastException, eBadParameter, 
                   "Cannot open RPS BLAST parameters file");
   }

   string matrix;
   auxfile >> matrix;
   info->aux_info.orig_score_matrix = strdup(matrix.c_str());

   auxfile >> info->aux_info.gap_open_penalty;
   auxfile >> info->aux_info.gap_extend_penalty;
   auxfile >> info->aux_info.ungapped_k;
   auxfile >> info->aux_info.ungapped_h;
   auxfile >> info->aux_info.max_db_seq_length;
   auxfile >> info->aux_info.db_length;
   auxfile >> info->aux_info.scale_factor;

   int num_db_seqs = info->profile_header->num_profiles;
   info->aux_info.karlin_k = new double[num_db_seqs];
   if (info->aux_info.karlin_k == NULL) {
      NCBI_THROW(CBlastException, eOutOfMemory, 
                  "RPS setup: karlin_k array allocation failed");
   }
   int i;

   for (i = 0; i < num_db_seqs && !auxfile.eof(); i++) {
      int seq_size;
      auxfile >> seq_size;  // not used
      auxfile >> info->aux_info.karlin_k[i];
   }

   if (i < num_db_seqs) {
       NCBI_THROW(CBlastException, eBadParameter,
                  "RPS setup: Aux file missing Karlin parameters");
   }

   *ppinfo = info;
   *rps_mmap = lut_mmap;
   *rps_pssm_mmap = pssm_mmap;
}

void CDbBlast::x_Blast_RPSInfoFree(BlastRPSInfo **ppinfo, 
                                   CMemoryFile **rps_mmap,
                                   CMemoryFile **rps_pssm_mmap)
{
    BlastRPSInfo *rps_info = *ppinfo;

    delete *rps_mmap;
    *rps_mmap = NULL;

    delete *rps_pssm_mmap;
    *rps_pssm_mmap = NULL;

    if (rps_info) {
        delete [] rps_info->aux_info.karlin_k;
        sfree(rps_info->aux_info.orig_score_matrix);
        delete rps_info;
        *ppinfo = NULL;
    }
}

void CDbBlast::x_InitRPSFields()
{
    EBlastProgramType program = m_OptsHandle->GetOptions().GetProgramType();
    if (program == eBlastTypeRpsBlast || program == eBlastTypeRpsTblastn) {
        string dbname(BLASTSeqSrcGetName(m_pSeqSrc));
        x_Blast_RPSInfoInit(&m_ipRpsInfo, &m_ipRpsMmap, 
                            &m_ipRpsPssmMmap, dbname);
        m_OptsHandle->SetOptions().SetMatrixName(m_ipRpsInfo->aux_info.orig_score_matrix);
        m_OptsHandle->SetOptions().SetGapOpeningCost(m_ipRpsInfo->aux_info.gap_open_penalty);
        m_OptsHandle->SetOptions().SetGapExtensionCost(m_ipRpsInfo->aux_info.gap_extend_penalty);
        /* Because of the database concatenation in the preliminary phase of 
           the search, sizes of hit lists should be equal to the total number 
           of sequences in the database. */
        m_OptsHandle->SetOptions().SetPrelimHitlistSize(
            m_ipRpsInfo->profile_header->num_profiles);
    } else {
        m_ipRpsInfo = NULL;
        m_ipRpsMmap = NULL;
        m_ipRpsPssmMmap = NULL;
    }
}

CDbBlast::CDbBlast(const TSeqLocVector& queries, BlastSeqSrc* seq_src,
                   EProgram p, BlastHSPStream* hsp_stream,
                   int nthreads)
    : m_tQueries(queries), m_pSeqSrc(seq_src), 
      m_pHspStream(hsp_stream), m_iNumThreads(nthreads)
{
    m_OptsHandle.Reset(CBlastOptionsFactory::Create(p));
    x_InitFields();
    x_InitRPSFields();
}

CDbBlast::CDbBlast(const TSeqLocVector& queries, BlastSeqSrc* seq_src, 
                   CBlastOptionsHandle& opts, 
                   BlastHSPStream* hsp_stream, int nthreads)
    : m_tQueries(queries), m_pSeqSrc(seq_src), 
      m_pHspStream(hsp_stream), m_iNumThreads(nthreads) 
{
    m_OptsHandle.Reset(&opts);    
    x_InitFields();
    x_InitRPSFields();
}

CDbBlast::~CDbBlast()
{ 
    x_ResetQueryDs();
    x_ResetResultDs();
    x_ResetRPSFields();
}

/// Resets query data structures
void
CDbBlast::x_ResetQueryDs()
{
    m_ibQuerySetUpDone = false;
    // should be changed if derived classes are created
    m_iclsQueries.Reset(NULL);
    m_iclsQueryInfo.Reset(NULL);
    m_ipScoreBlock = BlastScoreBlkFree(m_ipScoreBlock);
    m_ipLookupTable = LookupTableWrapFree(m_ipLookupTable);
    m_ipLookupSegments = BlastSeqLocFree(m_ipLookupSegments);
    m_ipFilteredRegions = BlastMaskLocFree(m_ipFilteredRegions);
}

/// Resets results data structures
void
CDbBlast::x_ResetResultDs()
{
    m_ipResults = Blast_HSPResultsFree(m_ipResults);
    
    m_ipDiagnostics = Blast_DiagnosticsFree(m_ipDiagnostics);
    NON_CONST_ITERATE(TBlastError, itr, m_ivErrors) {
        *itr = Blast_MessageFree(*itr);
    }
    // Only free the HSP stream, if it was allocated locally. That is true 
    // if and only if results are processed locally in this class.
    if (m_ibLocalResults)
        m_pHspStream = BlastHSPStreamFree(m_pHspStream);
}

/// Resets (frees) the fields allocated for the RPS database.
void 
CDbBlast::x_ResetRPSFields()
{
    x_Blast_RPSInfoFree(&m_ipRpsInfo, &m_ipRpsMmap, &m_ipRpsPssmMmap);
}

/// Initializes the HSP stream structure, if it has not been passed by the 
/// client.
void
CDbBlast::x_InitHSPStream()
{
    if (!m_pHspStream) {
        m_ibLocalResults = true;
        MT_LOCK lock = NULL;
        if (m_iNumThreads > 1)
            lock = Blast_CMT_LOCKInit();
        int num_results;

        num_results = (int) m_tQueries.size();

        const CBlastOptions& kOptions = GetOptionsHandle().GetOptions();

        m_pHspStream = 
            Blast_HSPListCollectorInitMT(kOptions.GetProgramType(), 
                                         kOptions.GetHitSaveOpts(), 
                                         num_results, TRUE, lock);
    }
}

void CDbBlast::SetupSearch()
{
    int status = 0;
    const CBlastOptions& kOptions = GetOptionsHandle().GetOptions();
    EBlastProgramType x_eProgram = kOptions.GetProgramType();

    // Check that query vector is not empty
    if (m_tQueries.size() == 0) {
        NCBI_THROW(CBlastException, eBadParameter, 
                   "Nothing to search: empty query vector"); 
    }
    
    // Check that sequence source exists
    if (!m_pSeqSrc) {
        NCBI_THROW(CBlastException, eBadParameter, 
                   "Subject sequence source (database) not provided"); 
    }
    
    // Check if subject sequence source is of correct molecule type
    bool seqsrc_is_prot = (BLASTSeqSrcGetIsProt(m_pSeqSrc) != FALSE);
    bool db_is_prot = (x_eProgram == eBlastTypeBlastp || 
                       x_eProgram == eBlastTypeBlastx ||
                       x_eProgram == eBlastTypeRpsBlast ||
                       x_eProgram == eBlastTypeRpsTblastn);
    if (seqsrc_is_prot != db_is_prot) {
        NCBI_THROW(CBlastException, eBadParameter, 
            "Database molecule does not correspond to BLAST program type");
    }

    x_ResetResultDs();

    if (!m_ibTracebackOnly) {
        // Initialize a new HSP stream, if necessary
        x_InitHSPStream();
        // Initialize a new diagnostics structure, with a mutex locking 
        // mechanism for a multi-threaded search, and without for 
        // single-threaded.
        if (m_iNumThreads > 1) {
           m_ipDiagnostics = 
              Blast_DiagnosticsInitMT(Blast_CMT_LOCKInit());
        } else {
           m_ipDiagnostics = Blast_DiagnosticsInit();
        }
    }

    if ( !m_ibQuerySetUpDone ) {
        double scale_factor;

        x_ResetQueryDs();

        SetupQueryInfo(m_tQueries, kOptions, &m_iclsQueryInfo);
        Blast_Message* blast_message = NULL;
        SetupQueries(m_tQueries, kOptions, m_iclsQueryInfo, &m_iclsQueries, 
                     &blast_message);
        if (blast_message) {
            m_ivErrors.push_back(blast_message);
            blast_message = NULL;
        }

        m_ipScoreBlock = 0;

        if (x_eProgram == eBlastTypeRpsBlast || 
            x_eProgram == eBlastTypeRpsTblastn)
            scale_factor = m_ipRpsInfo->aux_info.scale_factor;
        else
            scale_factor = 1.0;

        BlastMaskInformation maskInfo;

        status = 
            BLAST_MainSetUp(x_eProgram, 
                            kOptions.GetQueryOpts(),
                            kOptions.GetScoringOpts(),
                            kOptions.GetHitSaveOpts(),
                            m_iclsQueries, m_iclsQueryInfo,
                            scale_factor,
                            (m_ibTracebackOnly ? NULL : &m_ipLookupSegments),
                            &maskInfo, &m_ipScoreBlock, &blast_message);

        m_ipFilteredRegions = maskInfo.filter_slp;
        maskInfo.filter_slp = NULL;
        
        if (status != 0) {
            string msg = blast_message ? blast_message->message : 
                "BLAST_MainSetUp failed";
            Blast_MessageFree(blast_message);
            NCBI_THROW(CBlastException, eInternal, msg.c_str());
            // FIXME: shouldn't the error/warning also be saved in m_ivErrors?
        } else if (blast_message) {
            // Non-fatal error message; just save it.
            m_ivErrors.push_back(blast_message);
        }
        
        if (!m_ibTracebackOnly) {
            LookupTableWrapInit(m_iclsQueries, 
                                kOptions.GetLutOpts(), 
                                m_ipLookupSegments, m_ipScoreBlock, 
                                &m_ipLookupTable, m_ipRpsInfo);
        }

        // Fill the effective search space values in the BlastQueryInfo
        // structure, so it doesn't have to be done separately by each thread.
        Int8 total_length = BLASTSeqSrcGetTotLen(m_pSeqSrc);
        Int4 num_seqs = BLASTSeqSrcGetNumSeqs(m_pSeqSrc);
        BlastEffectiveLengthsParameters* eff_len_params = NULL;

        /* Initialize the effective length parameters with real values of
           database length and number of sequences */
        BlastEffectiveLengthsParametersNew(kOptions.GetEffLenOpts(),
                                           total_length, num_seqs, 
                                           &eff_len_params);
        status = 
            BLAST_CalcEffLengths(x_eProgram, kOptions.GetScoringOpts(), 
                                 eff_len_params, m_ipScoreBlock, 
                                 m_iclsQueryInfo);
        BlastEffectiveLengthsParametersFree(eff_len_params);
        if (status) {
            NCBI_THROW(CBlastException, eInternal, 
                       "BLAST_CalcEffLengths failed");
        }        

        m_ibQuerySetUpDone = true;
    }
}

void CDbBlast::PartialRun()
{
    GetOptionsHandle().Validate();
    SetupSearch();

    RunPreliminarySearch();
    x_RunTracebackSearch();
}

TSeqAlignVector
CDbBlast::Run()
{
    PartialRun();
    return x_Results2SeqAlign();
}

BlastHSPResults* 
CDbBlast::GetResults()
{
    // If results are not ready, extract them from the HSP stream
    if (!m_ipResults) {
        BlastHSPStream* hsp_stream = GetHSPStream();
        BlastHSPList* hsp_list = NULL;
        Int4 hitlist_size = GetOptionsHandle().GetHitlistSize();

        m_ipResults = Blast_HSPResultsNew((int) GetQueries().size());
        while (BlastHSPStreamRead(hsp_stream, &hsp_list) 
               != kBlastHSPStream_Eof) {
            Blast_HSPResultsInsertHSPList(m_ipResults, hsp_list, hitlist_size);
        }
    }
    return m_ipResults;
}

void CDbBlast::RunPreliminarySearch()
{
    int index;
    int retval = 0;

    typedef vector< CRef<CPrelimBlastThread> > TPrelimBlastThreads;

    if (m_iNumThreads > 1) {
        TPrelimBlastThreads prelim_blast_threads;
        prelim_blast_threads.reserve(m_iNumThreads);

        for (index = 0; index < m_iNumThreads; ++index) {
            CRef<CPrelimBlastThread> blast_thread(
                new CPrelimBlastThread(*this));
            prelim_blast_threads.push_back(blast_thread);
            blast_thread->Run();
        }
        // Join the threads
        NON_CONST_ITERATE(TPrelimBlastThreads, itr, prelim_blast_threads) {
            if (!retval) {
                (*itr)->Join(reinterpret_cast<void**>(&retval));
            } else {
                (*itr)->Detach();
            }
        }
        if (retval) {
            NCBI_THROW(CBlastException, eOutOfMemory, 
                       "One of the threads failed in preliminary search");
        }
    } else {
        CRef<CDbBlastPrelim> prelim_blaster(new CDbBlastPrelim(*this));
        if ((retval = prelim_blaster->Run()) != 0)
            NCBI_THROW(CBlastException, eOutOfMemory, 
                       "Preliminary search failed");
    }
}

TSeqAlignVector 
CDbBlast::RunTraceback()
{
    m_ibTracebackOnly = true;
    SetupSearch();
    x_RunTracebackSearch();
    return x_Results2SeqAlign();
}

void CDbBlast::x_RunTracebackSearch()
{
    Int2 status = 0;

    // If results are handled outside of the CDbBlast class, traceback stage 
    // cannot be performed, so just return. This condition is satisfied when 
    // HSP stream was passed to the CDbBlast object from outside, and it is 
    // not a traceback only search.
    if (!m_ibLocalResults && !m_ibTracebackOnly)
        return;

    const CBlastOptions& options = GetOptionsHandle().GetOptions();

    if ((status = 
         Blast_RunTracebackSearch(options.GetProgramType(), 
             m_iclsQueries, m_iclsQueryInfo, m_pSeqSrc, 
             options.GetScoringOpts(), options.GetExtnOpts(), 
             options.GetHitSaveOpts(), options.GetEffLenOpts(), 
             options.GetDbOpts(), options.GetPSIBlastOpts(), 
             m_ipScoreBlock, m_pHspStream, m_ipRpsInfo, &m_ipResults)) != 0)
        NCBI_THROW(CBlastException, eInternal, "Traceback failed"); 

    return;
}

TSeqAlignVector
CDbBlast::x_Results2SeqAlign()
{
    TSeqAlignVector retval;

    if (!m_ipResults)
        return retval;

    bool gappedMode = GetOptionsHandle().GetGappedMode();
    bool outOfFrameMode = GetOptionsHandle().GetOptions().GetOutOfFrameMode();
    string db_name(BLASTSeqSrcGetName(m_pSeqSrc));
    bool db_is_prot = (BLASTSeqSrcGetIsProt(m_pSeqSrc) ? true : false);

    // Create a source for retrieving sequence ids and lengths
    CSeqDbSeqInfoSrc seqinfo_src(db_name, db_is_prot);

    retval = BLAST_Results2CSeqAlign(m_ipResults, 
                 GetOptionsHandle().GetOptions().GetProgramType(),
                 m_tQueries, &seqinfo_src, 
                 gappedMode, outOfFrameMode);

    return retval;
}

END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.61  2005/04/06 23:29:04  dondosha
 * Doxygen fixes
 *
 * Revision 1.60  2005/04/06 21:06:18  dondosha
 * Use EBlastProgramType instead of EProgram in non-user-exposed functions
 *
 * Revision 1.59  2005/03/31 13:45:35  camacho
 * BLAST options API clean-up
 *
 * Revision 1.58  2005/03/29 17:15:51  dondosha
 * Use CBlastOptionsHandle instead of CBlastOptions to retrieve gapped mode
 *
 * Revision 1.57  2005/03/08 21:10:30  dondosha
 * BlastMaskLocProteinToDNA is now called inside BLAST_MainSetUp, so extra call is no longer needed
 *
 * Revision 1.56  2005/02/08 18:50:29  bealer
 * - Fix type truncation warnings.
 *
 * Revision 1.55  2005/01/28 21:39:37  ucko
 * CPrelimBlastThread::Main: use long rather than intptr_t, which isn't
 * as widely available as one might like.
 *
 * Revision 1.54  2005/01/28 14:32:38  coulouri
 * use intptr_t when casting to void pointer
 *
 * Revision 1.53  2005/01/21 15:38:44  papadopo
 * changed x_Blast_FillRPSInfo to x_Blast_RPSInfo{Init|Free}
 *
 * Revision 1.52  2005/01/14 18:00:59  papadopo
 * move FillRPSInfo into CDbBlast, to remove some xblast dependencies on SeqDB
 *
 * Revision 1.51  2005/01/11 17:50:44  dondosha
 * Removed TrimBlastHSPResults method: will be a static function in blastsrv4.REAL code
 *
 * Revision 1.50  2005/01/10 18:35:07  dondosha
 * BlastMaskLocDNAToProtein and BlastMaskLocProteinToDNA moved to core with changed signatures
 *
 * Revision 1.49  2005/01/06 15:42:02  camacho
 * Make use of modified signature to SetupQueries
 *
 * Revision 1.48  2005/01/04 16:55:51  dondosha
 * Adjusted TrimBlastHSPResults so it can be called after preliminary search
 *
 * Revision 1.47  2004/12/21 17:17:42  dondosha
 * Use Blast_RunPreliminarySearch and Blast_RunTracebackSearch core functions for respective stages; no longer need to branch code for RPS BLAST
 *
 * Revision 1.46  2004/11/30 17:01:14  dondosha
 * Use Blast_DiagnosticsInitMT for multi-threaded search; always perform traceback if m_ibTracebackOnly field is set
 *
 * Revision 1.45  2004/10/26 16:05:20  dondosha
 * Removed unused variable
 *
 * Revision 1.44  2004/10/26 15:31:41  dondosha
 * Removed RPSInfo argument from constructors;
 * RPSInfo is now initialized inside the CDbBlast class if RPS search is requested;
 * multiple queries are now allowed for RPS search
 *
 * Revision 1.43  2004/10/06 14:54:57  dondosha
 * Use IBlastSeqInfoSrc interface in x_Results2CSeqAlign; added RunTraceback method to perform a traceback only search, given the precomputed preliminary results, and return Seq-align; removed unused SetSeqSrc method
 *
 * Revision 1.42  2004/09/21 13:50:59  dondosha
 * Conversion of filtering locations from protein to nucleotide scale is now needed for RPS tblastn
 *
 * Revision 1.41  2004/09/13 14:14:20  dondosha
 * Minor fix in conversion from Boolean to bool
 *
 * Revision 1.40  2004/09/13 12:46:07  madden
 * Replace call to ListNodeFreeData with BlastSeqLocFree
 *
 * Revision 1.39  2004/09/07 19:56:02  dondosha
 * return statement not needed after exception is thrown
 *
 * Revision 1.38  2004/09/07 17:59:30  dondosha
 * CDbBlast class changed to support multi-threaded search
 *
 * Revision 1.37  2004/07/06 15:51:13  dondosha
 * Changes in preparation for implementation of multi-threaded search
 *
 * Revision 1.36  2004/06/28 13:40:51  madden
 * Use BlastMaskInformation rather than BlastMaskLoc in BLAST_MainSetUp
 *
 * Revision 1.35  2004/06/24 15:54:59  dondosha
 * Added doxygen file description
 *
 * Revision 1.34  2004/06/23 14:06:15  dondosha
 * Added MT_LOCK argument in constructors
 *
 * Revision 1.33  2004/06/15 18:49:07  dondosha
 * Added optional BlastHSPStream argument to constructors, to allow use of HSP
 * stream by a separate thread doing on-the-fly formatting
 *
 * Revision 1.32  2004/06/08 15:20:44  dondosha
 * Use BlastHSPStream interface
 *
 * Revision 1.31  2004/06/07 21:34:55  dondosha
 * Use 2 booleans for gapped and out-of-frame mode instead of scoring options in function arguments
 *
 * Revision 1.30  2004/06/07 18:26:29  dondosha
 * Bit scores are now filled in HSP lists, so BlastScoreBlk is no longer needed when results are converted to seqalign
 *
 * Revision 1.29  2004/06/02 15:57:06  bealer
 * - Isolate object manager dependent code.
 *
 * Revision 1.28  2004/05/21 21:41:02  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.27  2004/05/17 18:12:29  bealer
 * - Add PSI-Blast support.
 *
 * Revision 1.26  2004/05/14 17:16:12  dondosha
 * BlastReturnStat structure changed to BlastDiagnostics and refactored
 *
 * Revision 1.25  2004/05/07 15:28:41  papadopo
 * add scale factor to BlastMainSetup
 *
 * Revision 1.24  2004/05/05 15:28:56  dondosha
 * Renamed functions in blast_hits.h accordance with new convention Blast_[StructName][Task]
 *
 * Revision 1.23  2004/04/30 16:53:06  dondosha
 * Changed a number of function names to have the same conventional Blast_ prefix
 *
 * Revision 1.22  2004/04/21 17:33:46  madden
 * Rename BlastHSPFree to Blast_HSPFree
 *
 * Revision 1.21  2004/03/24 22:12:46  dondosha
 * Fixed memory leaks
 *
 * Revision 1.20  2004/03/17 14:51:33  camacho
 * Fix compiler errors
 *
 * Revision 1.19  2004/03/16 23:32:28  dondosha
 * Added capability to run RPS BLAST seach; added function x_InitFields; changed mi_ to m_i in member field names
 *
 * Revision 1.18  2004/03/16 14:45:28  dondosha
 * Removed doxygen comments for nonexisting parameters
 *
 * Revision 1.17  2004/03/15 19:57:00  dondosha
 * Merged TwoSequences and Database engines
 *
 * Revision 1.16  2004/03/10 17:37:36  papadopo
 * add (unused) RPS info pointer to LookupTableWrapInit()
 *
 * Revision 1.15  2004/02/24 20:31:39  dondosha
 * Typo fix; removed irrelevant CVS log comments
 *
 * Revision 1.14  2004/02/24 18:19:35  dondosha
 * Removed lookup options argument from call to BLAST_MainSetUp
 *
 * Revision 1.13  2004/02/23 15:45:09  camacho
 * Eliminate compiler warning about qsort
 *
 * Revision 1.12  2004/02/19 21:12:02  dondosha
 * Added handling of error messages; fill info message in TrimBlastHSPResults
 *
 * Revision 1.11  2004/02/18 23:49:08  dondosha
 * Added TrimBlastHSPResults method to remove extra HSPs if limit on total number is provided
 *
 * Revision 1.10  2004/02/13 20:47:20  madden
 * Throw exception rather than ERR_POST if setup fails
 *
 * Revision 1.9  2004/02/13 19:32:55  camacho
 * Removed unnecessary #defines
 *
 * Revision 1.8  2004/01/16 21:51:34  bealer
 * - Changes for Blast4 API
 *
 * Revision 1.7  2003/12/15 23:42:46  dondosha
 * Set database length and number of sequences options in constructor
 *
 * Revision 1.6  2003/12/15 15:56:42  dondosha
 * Added constructor with options handle argument
 *
 * Revision 1.5  2003/12/03 17:41:19  camacho
 * Fix incorrect member initializer list
 *
 * Revision 1.4  2003/12/03 16:45:03  dondosha
 * Use CBlastOptionsHandle class
 *
 * Revision 1.3  2003/11/26 18:36:45  camacho
 * Renaming blast_option*pp -> blast_options*pp
 *
 * Revision 1.2  2003/10/30 21:41:12  dondosha
 * Removed unneeded extra argument from call to BLAST_Results2CSeqAlign
 *
 * Revision 1.1  2003/10/29 22:37:36  dondosha
 * Database BLAST search class methods
 * ===========================================================================
 */
