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

Author: Jason Papadopoulos

******************************************************************************/

/** @file blast_format.cpp
 * Produce formatted blast output
*/

#include <ncbi_pch.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objtools/readers/seqdb/seqdb.hpp>
#include <algo/blast/core/blast_stat.h>
#include <objtools/blast_format/blastxml_format.hpp>
#include <algo/blast/api/remote_services.hpp>   // for CRemoteServices
#include "blast_format.hpp"
#include "data4xmlformat.hpp"

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
#endif

const string CBlastFormat::kNoHitsFound("No hits found");

CBlastFormat::CBlastFormat(const blast::CBlastOptions& options, 
                 const string& dbname, 
                 blast::CFormattingArgs::EOutputFormat format_type, 
                 bool db_is_aa,
                 bool believe_query, CNcbiOstream& outfile,
                 int num_summary, 
                 int num_alignments, 
                 CScope & scope,
                 const char *matrix_name /* = BLAST_DEFAULT_MATRIX */,
                 bool show_gi /* = false */, 
                 bool is_html /* = false */,
                 int qgencode /* = BLAST_GENETIC_CODE */, 
                 int dbgencode /* = BLAST_GENETIC_CODE */,
                 bool use_sum_statistics /* = false */,
                 bool is_remote_search /* = false */,
                 const vector<int>& dbfilt_algorithms /* = vector<int>() */,
                 bool is_megablast /* = false */,
                 bool is_indexed /* = false */)
        : m_FormatType(format_type), m_IsHTML(is_html), 
          m_DbIsAA(db_is_aa), m_BelieveQuery(believe_query),
          m_Outfile(outfile), m_NumSummary(num_summary),
          m_NumAlignments(num_alignments),
          m_Program(Blast_ProgramNameFromType(options.GetProgramType())), 
          m_DbName(dbname),
          m_QueryGenCode(qgencode), m_DbGenCode(dbgencode),
          m_ShowGi(show_gi), m_ShowLinkedSetSize(false),
          m_IsUngappedSearch(!options.GetGappedMode()),
          m_MatrixName(matrix_name),
          m_Scope(& scope),
          m_IsBl2Seq(false),
          m_IsRemoteSearch(is_remote_search),
          m_QueriesFormatted(0),
          m_Megablast(is_megablast),
          m_IndexedMegablast(is_indexed)
{
    m_IsBl2Seq = static_cast<bool>(dbname == kEmptyStr);
    if ( !m_IsBl2Seq ) {
        x_FillDbInfo(dbfilt_algorithms);
    }
    if (m_FormatType == CFormattingArgs::eXml) {
        m_AccumulatedQueries.Reset(new CBlastQueryVector());
    }
    if (use_sum_statistics && m_IsUngappedSearch) {
        m_ShowLinkedSetSize = true;
    }
}

bool 
CBlastFormat::x_FillDbInfoRemotely(const string& dbname,
                                   CBlastFormatUtil::SDbInfo& info) const
{
    CRef<CBlast4_database> blastdb(new CBlast4_database);
    blastdb->SetName(dbname);
    blastdb->SetType() = m_DbIsAA
        ? eBlast4_residue_type_protein : eBlast4_residue_type_nucleotide;
    CRef<CBlast4_database_info> dbinfo = 
        CRemoteServices().GetDatabaseInfo(blastdb);

    info.name = dbname;
    if ( !dbinfo ) {
        return false;
    }
    info.definition = dbinfo->GetDescription();
    if (info.definition.empty())
        info.definition = info.name;
    info.date = dbinfo->GetLast_updated();
    info.total_length = dbinfo->GetTotal_length();
    info.number_seqs = static_cast<int>(dbinfo->GetNum_sequences());
    return true;
}

bool
CBlastFormat::x_FillDbInfoLocally(const string& dbname,
                                  CBlastFormatUtil::SDbInfo& info, 
                                  const vector<int>& dbfilt_algorithms) const
{
    CRef<CSeqDB> seqdb(new CSeqDB(dbname, m_DbIsAA 
                          ? CSeqDB::eProtein : CSeqDB::eNucleotide));
    if ( !seqdb ) {
        return false;
    }
    info.name = seqdb->GetDBNameList();
    info.definition = seqdb->GetTitle();
    if (info.definition.empty())
        info.definition = info.name;
    info.date = seqdb->GetDate();
    info.total_length = seqdb->GetTotalLength();
    info.number_seqs = seqdb->GetNumSeqs();

    // Process the filtering algorithm IDs
    info.algorithm_names.clear();
    info.detailed_masking_info.clear();
    if (dbfilt_algorithms.empty()) {
        return true;
    }

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    EBlast_filter_program filtering_algorithm;
    string opts, filt_algo_name;
    seqdb->GetMaskAlgorithmDetails(dbfilt_algorithms.front(), 
                                   filtering_algorithm, filt_algo_name, opts);
    info.algorithm_names += filt_algo_name;
    size_t index = 1;
    info.detailed_masking_info += 
        NStr::IntToString(index) + ". " + filt_algo_name + ". Options: '" 
        + opts + "'\n";
    for (; index < dbfilt_algorithms.size(); index++) {
        seqdb->GetMaskAlgorithmDetails(dbfilt_algorithms[index],
                                       filtering_algorithm, filt_algo_name,
                                       opts);
        info.algorithm_names += ", " + filt_algo_name;
        info.detailed_masking_info += "     " +
            NStr::IntToString(index+1) + ". " + filt_algo_name + 
            ". Options: '" + opts + "\n";
    }
#endif
    return true;
}

void
CBlastFormat::x_FillDbInfo(const vector<int>& dbfilt_algorithms)
{
    vector<string> dbnames;
    NStr::Tokenize(m_DbName, " ", dbnames);

    m_DbInfo.reserve(dbnames.size());
    ITERATE(vector<string>, dbname, dbnames) {
        CBlastFormatUtil::SDbInfo info;
        info.is_protein = m_DbIsAA;
        bool success = false;

        if (m_IsRemoteSearch) {
            success = x_FillDbInfoRemotely(*dbname, info);
        } else {
            success = x_FillDbInfoLocally(*dbname, info, dbfilt_algorithms);
        }
        if (success) {
            m_DbInfo.push_back(info);
        } else {
            string msg;
            msg += *dbname;
            if (m_IsRemoteSearch)
                msg += string("' not found on server.\n");
            else
                msg += string("' not found.\n");
            NCBI_THROW(CSeqDBException, eFileErr, msg);
        }
    }

    return;
}

static const string kHTML_Prefix =
"<HTML>\n"
"<TITLE>BLAST Search Results</TITLE>\n"
"<BODY BGCOLOR=\"#FFFFFF\" LINK=\"#0000FF\" VLINK=\"#660099\" ALINK=\"#660099\">\n"
"<PRE>\n";

static const string kHTML_Suffix =
"</PRE>\n"
"</BODY>\n"
"</HTML>";

void 
CBlastFormat::PrintProlog()
{
    // no header for some output types
    if (m_FormatType >= CFormattingArgs::eXml)
        return;

    if (m_IsHTML) {
        m_Outfile << kHTML_Prefix << "\n";
    }

    CBlastFormatUtil::BlastPrintVersionInfo(m_Program, m_IsHTML, 
                                            m_Outfile);
    if (m_IsBl2Seq) {
        return;
    }

    m_Outfile << "\n\n";
    if (m_Megablast)
        CBlastFormatUtil::BlastPrintReference(m_IsHTML, kFormatLineLength, 
                                          m_Outfile, CReference::eMegaBlast);
    else
        CBlastFormatUtil::BlastPrintReference(m_IsHTML, kFormatLineLength, 
                                          m_Outfile);

    if (m_Megablast && m_IndexedMegablast)
    {
        m_Outfile << "\n\n";
        CBlastFormatUtil::BlastPrintReference(m_IsHTML, kFormatLineLength, 
                              m_Outfile, CReference::eIndexedMegablast);
    }

    if (m_Program == "psiblast") {
        m_Outfile << "\n\n";
        CBlastFormatUtil::BlastPrintReference(m_IsHTML, kFormatLineLength, 
                              m_Outfile, CReference::eCompAdjustedMatrices);
    }
    if (m_Program == "psiblast" || m_Program == "blastp") {
        m_Outfile << "\n\n";
        CBlastFormatUtil::BlastPrintReference(m_IsHTML, kFormatLineLength, 
                              m_Outfile, CReference::eCompBasedStats,
                              (bool)(m_Program == "psiblast"));
    }

    m_Outfile << "\n\n";
    CBlastFormatUtil::PrintDbReport(m_DbInfo, kFormatLineLength, 
                                    m_Outfile, true);
}

void
CBlastFormat::x_PrintOneQueryFooter(const blast::CBlastAncillaryData& summary)
{
    const Blast_KarlinBlk *kbp_ungap = (m_Program == "psiblast") 
        ? summary.GetPsiUngappedKarlinBlk() 
        : summary.GetUngappedKarlinBlk();
    m_Outfile << "\n";
    if (kbp_ungap) {
        CBlastFormatUtil::PrintKAParameters(kbp_ungap->Lambda, 
                                            kbp_ungap->K, kbp_ungap->H,
                                            kFormatLineLength, m_Outfile,
                                            false);
    }

    const Blast_KarlinBlk *kbp_gap = (m_Program == "psiblast") 
        ? summary.GetPsiGappedKarlinBlk()
        : summary.GetGappedKarlinBlk();
    m_Outfile << "\n";
    if (kbp_gap) {
        CBlastFormatUtil::PrintKAParameters(kbp_gap->Lambda, 
                                            kbp_gap->K, kbp_gap->H,
                                            kFormatLineLength, m_Outfile,
                                            true);
    }

    m_Outfile << "\n";
    m_Outfile << "Effective search space used: " << 
                        summary.GetSearchSpace() << "\n";
}

/// Auxialiary function to determine if there are local IDs in the identifiers
/// of the query sequences
/// @param queries query sequence(s) [in]
static bool 
s_HasLocalIDs(CConstRef<CBlastQueryVector> queries)
{
    bool retval = false;
    ITERATE(CBlastQueryVector, itr, *queries) {
        if (blast::IsLocalId((*itr)->GetQuerySeqLoc()->GetId())) {
            retval = true;
            break;
        }
    }
    return retval;
}

void 
CBlastFormat::x_ConfigCShowBlastDefline(CShowBlastDefline& showdef)
{
    int flags = 0;
    if (m_ShowLinkedSetSize)
        flags |= CShowBlastDefline::eShowSumN;
    if (m_IsHTML)
        flags |= CShowBlastDefline::eHtml;
    if (m_ShowGi)
        flags |= CShowBlastDefline::eShowGi;

    showdef.SetOption(flags);
    showdef.SetDbName(m_DbName);
    showdef.SetDbType(!m_DbIsAA);
}

/** Auxiliary class to sort the CPsiBlastIterationState::TSeqIds */
class CSeqIdComparer {
public:
    /// Construct with Seq-id target
    /// @param target_seqid Seq-id to be searched [in]
    CSeqIdComparer(const CSeq_id& target_seqid)
        : m_Target(target_seqid) {}

    /// Returns true if the candidate_seqid matches the target Seq-id provided
    /// when creating this object
    /// @param candidate_seqid Candidate Seq-id for comparison [in]
    bool operator() (CRef<CSeq_id> candidate_seqid) const {
        bool retval = false;
        if (m_Target.Match(*candidate_seqid)) {
            retval = true;
        }
        return retval;
    }

private:
    const CSeq_id& m_Target;    ///< The target Seq-id of the search
};

void
CBlastFormat::x_SplitSeqAlign(CConstRef<CSeq_align_set> full_alignment,
                       CSeq_align_set& repeated_seqs,
                       CSeq_align_set& new_seqs,
                       blast::CPsiBlastIterationState::TSeqIds& prev_seqids)
{
    static const CSeq_align::TDim kSubjRow = 1;
    _ASSERT( !prev_seqids.empty() );
    _ASSERT( !full_alignment->IsEmpty() );
    _ASSERT(repeated_seqs.IsEmpty());
    _ASSERT(new_seqs.IsEmpty());

    ITERATE(CSeq_align_set::Tdata, alignment, full_alignment->Get()) {
        const CSeq_id& subj_id = (*alignment)->GetSeq_id(kSubjRow);

        CPsiBlastIterationState::TSeqIds::iterator pos =
            find_if(prev_seqids.begin(), prev_seqids.end(), 
                    CSeqIdComparer(subj_id));
        if (pos != prev_seqids.end()) { 
            // if found among previously seen Seq-ids...
            repeated_seqs.Set().push_back(*alignment);
        } else {
            // ... else add them as new
            new_seqs.Set().push_back(*alignment);
        }
    }
}


void
CBlastFormat::x_DisplayDeflines(CConstRef<CSeq_align_set> aln_set, 
                        unsigned int itr_num,
                        blast::CPsiBlastIterationState::TSeqIds& prev_seqids)
{
    if (itr_num != numeric_limits<unsigned int>::max() && 
        !prev_seqids.empty()) {
        // Split seq-align-set
        CSeq_align_set repeated_seqs, new_seqs;
        x_SplitSeqAlign(aln_set, repeated_seqs, new_seqs, prev_seqids);

        // Show deflines for 'repeat' sequences
        {{
            CShowBlastDefline showdef(repeated_seqs, *m_Scope, 
                                      kFormatLineLength,
                                  min(m_NumSummary, (int)prev_seqids.size()));
            x_ConfigCShowBlastDefline(showdef);
            showdef.SetupPsiblast(NULL, CShowBlastDefline::eRepeatPass);
            showdef.DisplayBlastDefline(m_Outfile);
        }}
        m_Outfile << "\n";

        // Show deflines for 'new' sequences
        {{
            CShowBlastDefline showdef(new_seqs, *m_Scope, kFormatLineLength,
                              max(0, m_NumSummary-(int)prev_seqids.size()));
            x_ConfigCShowBlastDefline(showdef);
            showdef.SetupPsiblast(NULL, CShowBlastDefline::eNewPass);
            showdef.DisplayBlastDefline(m_Outfile);
        }}

    } else {
        CShowBlastDefline showdef(*aln_set, *m_Scope, 
                                  kFormatLineLength,
                                  m_NumSummary);
        x_ConfigCShowBlastDefline(showdef);
        showdef.DisplayBlastDefline(m_Outfile);
    }
    m_Outfile << "\n";
}

int
s_SetFlags(string& program, 
    blast::CFormattingArgs::EOutputFormat format_type,
    bool html, bool showgi)
{
   // set the alignment flags
    int flags = CDisplaySeqalign::eShowBlastInfo;
    
    if (html)
        flags |= CDisplaySeqalign::eHtml;
    if (showgi)
        flags |= CDisplaySeqalign::eShowGi;

    if (format_type >= CFormattingArgs::eQueryAnchoredIdentities &&
        format_type <= CFormattingArgs::eFlatQueryAnchoredNoIdentities) {
        flags |= CDisplaySeqalign::eMergeAlign;
    }
    else {
        flags |= CDisplaySeqalign::eShowBlastStyleId |
                 CDisplaySeqalign::eShowMiddleLine;
    }

    if (format_type == CFormattingArgs::eQueryAnchoredIdentities ||
        format_type == CFormattingArgs::eFlatQueryAnchoredIdentities) {
        flags |= CDisplaySeqalign::eShowIdentity;
    }
    if (format_type == CFormattingArgs::eQueryAnchoredIdentities ||
        format_type == CFormattingArgs::eQueryAnchoredNoIdentities) {
        flags |= CDisplaySeqalign::eMasterAnchored;
    }
    if (program == "tblastx") {
        flags |= CDisplaySeqalign::eTranslateNucToNucAlignment;
    }
    return flags;
}

void 
CBlastFormat::x_PrintStructuredReport(const blast::CSearchResults& results,
              CConstRef<blast::CBlastQueryVector> queries)
{
   CConstRef<CSeq_align_set> aln_set = results.GetSeqAlign();

    // ASN.1 formatting is straightforward
    if (m_FormatType == CFormattingArgs::eAsnText) {
        if (results.HasAlignments()) {
            m_Outfile << MSerial_AsnText << *aln_set;
        }
        return;
    } else if (m_FormatType == CFormattingArgs::eAsnBinary) {
        if (results.HasAlignments()) {
            m_Outfile << MSerial_AsnBinary << *aln_set;
        }
        return;
    } else if (m_FormatType == CFormattingArgs::eXml) {
        // Prepare for XML formatting
        if (results.HasAlignments()) {
            CRef<CSearchResults> res(const_cast<CSearchResults*>(&results));
            m_AccumulatedResults.push_back(res);
        }
        ITERATE(CBlastQueryVector, itr, *queries) {
            m_AccumulatedQueries->push_back(*itr);
        }
        return;
    }
}

void
CBlastFormat::x_PrintTabularReport(const blast::CSearchResults& results, unsigned int itr_num)
{
    CConstRef<CSeq_align_set> aln_set = results.GetSeqAlign();
    // other output types will need a bioseq handle
    CBioseq_Handle bhandle = m_Scope->GetBioseqHandle(*results.GetSeqId(),
                                                      CScope::eGetBioseq_All);

    // tabular formatting just prints each alignment in turn
    // (plus a header)
    if (m_FormatType == CFormattingArgs::eTabular ||
        m_FormatType == CFormattingArgs::eTabularWithComments) {
        CBlastTabularInfo tabinfo(m_Outfile);
        tabinfo.SetParseLocalIds(m_BelieveQuery);

        if (m_FormatType == CFormattingArgs::eTabularWithComments)
             tabinfo.PrintHeader(m_Program, *(bhandle.GetBioseqCore()),
                                 m_DbName, results.GetRID(), itr_num, aln_set);

        if (results.HasAlignments()) {
            ITERATE(CSeq_align_set::Tdata, itr, aln_set->Get()) {
                    const CSeq_align& s = **itr;
                    tabinfo.SetFields(s, *m_Scope);
                    tabinfo.Print();
            }
        }
        return;
    }
}

void
CBlastFormat::PrintOneResultSet(const blast::CSearchResults& results,
                        CConstRef<blast::CBlastQueryVector> queries,
                        unsigned int itr_num
                        /* = numeric_limits<unsigned int>::max() */,
                        blast::CPsiBlastIterationState::TSeqIds prev_seqids
                        /* = CPsiBlastIterationState::TSeqIds() */)
{
    // For remote searches, we don't retrieve the sequence data for the query
    // sequence when initially sending the request to the BLAST server (if it's
    // a GI/accession/TI), so we flush the scope so that it can be retrieved
    // (needed if a self-hit is found) again. This is not applicable if the
    // query sequence(s) are specified as FASTA (will be identified by local
    // IDs).
    if (m_IsRemoteSearch && !s_HasLocalIDs(queries)) {
        ResetScopeHistory();
    }

    // Used with tabular output to print number of searches formatted at end.
    m_QueriesFormatted++;

    if (m_FormatType == CFormattingArgs::eAsnText 
      || m_FormatType == CFormattingArgs::eAsnBinary 
      || m_FormatType == CFormattingArgs::eXml )
    {
        x_PrintStructuredReport(results, queries);
        return;
    }

    if (results.HasErrors()) {
            m_Outfile << "\n" << results.GetErrorStrings() << "\n";
            return; // errors are deemed fatal
    }
    if (results.HasWarnings()) {
            m_Outfile << "\n" << results.GetWarningStrings() << "\n";
    }

    if (m_FormatType == CFormattingArgs::eTabular ||
        m_FormatType == CFormattingArgs::eTabularWithComments) {
        x_PrintTabularReport(results, itr_num);
        return;
    }

    if (itr_num != numeric_limits<unsigned int>::max()) {
        m_Outfile << "Results from round " << itr_num << "\n";
    }

    // other output types will need a bioseq handle
    CBioseq_Handle bhandle = m_Scope->GetBioseqHandle(*results.GetSeqId(),
                                                      CScope::eGetBioseq_All);
    // If this assertion fails, we're not able to get the query, most likely a
    // bug
    _ASSERT(bhandle);
    CConstRef<CBioseq> bioseq = bhandle.GetBioseqCore();

    // print the preamble for this query

    m_Outfile << "\n\n";
    CBlastFormatUtil::AcknowledgeBlastQuery(*bioseq, kFormatLineLength,
                                            m_Outfile, m_BelieveQuery,
                                            m_IsHTML, false,
                                            results.GetRID());

    // quit early if there are no hits
    if ( !results.HasAlignments() ) {
        m_Outfile << "\n\n" 
                  << "***** " << kNoHitsFound << " *****" << "\n" 
                  << "\n\n";
        x_PrintOneQueryFooter(*results.GetAncillaryData());
        return;
    }

    CConstRef<CSeq_align_set> aln_set = results.GetSeqAlign();
    _ASSERT(results.HasAlignments());
    if (m_IsUngappedSearch) {
        aln_set.Reset(CDisplaySeqalign::PrepareBlastUngappedSeqalign(*aln_set));
    }

    //-------------------------------------------------
    // print 1-line summaries
    if ( !m_IsBl2Seq ) {
        x_DisplayDeflines(aln_set, itr_num, prev_seqids);
    }

    //-------------------------------------------------
    // print the alignments
    m_Outfile << "\n";

    TMaskedQueryRegions masklocs;
    results.GetMaskedQueryRegions(masklocs);

    CSeq_align_set copy_aln_set;
    CBlastFormatUtil::PruneSeqalign(*aln_set, copy_aln_set, m_NumAlignments);

    int flags = s_SetFlags(m_Program, m_FormatType, m_IsHTML, m_ShowGi);

    CDisplaySeqalign display(copy_aln_set, *m_Scope, &masklocs, NULL, m_MatrixName);
    display.SetDbName(m_DbName);
    display.SetDbType(!m_DbIsAA);

    // set the alignment flags
    display.SetAlignOption(flags);

    if (m_Program == "blastn" || m_Program == "megablast") {
            display.SetMiddleLineStyle(CDisplaySeqalign::eBar);
            display.SetAlignType(CDisplaySeqalign::eNuc);
    }
    else {
            display.SetMiddleLineStyle(CDisplaySeqalign::eChar);
            display.SetAlignType(CDisplaySeqalign::eProt);
    }

    display.SetMasterGeneticCode(m_QueryGenCode);
    display.SetSlaveGeneticCode(m_DbGenCode);
    display.SetSeqLocChar(CDisplaySeqalign::eLowerCase);
    TSeqLocInfoVector subj_masks;
    results.GetSubjectMasks(subj_masks);
    display.SetSubjectMasks(subj_masks);
    display.DisplaySeqalign(m_Outfile);

    // print the ancillary data for this query

    x_PrintOneQueryFooter(*results.GetAncillaryData());
}

void
CBlastFormat::PrintPhiResult(const blast::CSearchResultSet& result_set,
                        CConstRef<blast::CBlastQueryVector> queries,
                        unsigned int itr_num
                        /* = numeric_limits<unsigned int>::max() */,
                        blast::CPsiBlastIterationState::TSeqIds prev_seqids
                        /* = CPsiBlastIterationState::TSeqIds() */)
{
    // For remote searches, we don't retrieve the sequence data for the query
    // sequence when initially sending the request to the BLAST server (if it's
    // a GI/accession/TI), so we flush the scope so that it can be retrieved
    // (needed if a self-hit is found) again. This is not applicable if the
    // query sequence(s) are specified as FASTA (will be identified by local
    // IDs).
    if (m_IsRemoteSearch && !s_HasLocalIDs(queries)) {
        ResetScopeHistory();
    }

    if (m_FormatType == CFormattingArgs::eAsnText 
      || m_FormatType == CFormattingArgs::eAsnBinary 
      || m_FormatType == CFormattingArgs::eXml )
    {
        ITERATE(CSearchResultSet, result, result_set) {
           x_PrintStructuredReport(**result, queries);
        }
        return;
    }

    ITERATE(CSearchResultSet, result, result_set) {
        if ((**result).HasErrors()) {
            m_Outfile << "\n" << (**result).GetErrorStrings() << "\n";
            return; // errors are deemed fatal
        }
        if ((**result).HasWarnings()) {
            m_Outfile << "\n" << (**result).GetWarningStrings() << "\n";
        }
    }

    if (m_FormatType == CFormattingArgs::eTabular ||
        m_FormatType == CFormattingArgs::eTabularWithComments) {
        ITERATE(CSearchResultSet, result, result_set) {
           x_PrintTabularReport(**result, itr_num);
        }
        return;
    }

    const CSearchResults& first_results = result_set[0];

    if (itr_num != numeric_limits<unsigned int>::max()) {
        m_Outfile << "Results from round " << itr_num << "\n";
    }

    CBioseq_Handle bhandle = m_Scope->GetBioseqHandle(*first_results.GetSeqId(),
                                                      CScope::eGetBioseq_All);
    CConstRef<CBioseq> bioseq = bhandle.GetBioseqCore();

    // print the preamble for this query

    m_Outfile << "\n\n";
    CBlastFormatUtil::AcknowledgeBlastQuery(*bioseq, kFormatLineLength,
                                            m_Outfile, m_BelieveQuery,
                                            m_IsHTML, false,
                                            first_results.GetRID());

    const SPHIQueryInfo *phi_query_info = first_results.GetPhiQueryInfo();

    if (phi_query_info)
    {
        vector<int> offsets;
        for (int index=0; index<phi_query_info->num_patterns; index++)
            offsets.push_back(phi_query_info->occurrences[index].offset);

        CBlastFormatUtil::PrintPhiInfo(phi_query_info->num_patterns,
                                   string(phi_query_info->pattern), 
                                   phi_query_info->probability,
                                   offsets, m_Outfile);
    }

    // quit early if there are no hits
    if ( !first_results.HasAlignments() ) {
        m_Outfile << "\n\n" 
                  << "***** " << kNoHitsFound << " *****" << "\n" 
                  << "\n\n";
        x_PrintOneQueryFooter(*first_results.GetAncillaryData());
        return;
    }

    _ASSERT(first_results.HasAlignments());
    //-------------------------------------------------

    ITERATE(CSearchResultSet, result, result_set)
    {
        CConstRef<CSeq_align_set> aln_set = (**result).GetSeqAlign();
        x_DisplayDeflines(aln_set, itr_num, prev_seqids);
    }

    //-------------------------------------------------
    // print the alignments
    m_Outfile << "\n";


    int flags = s_SetFlags(m_Program, m_FormatType, m_IsHTML, m_ShowGi);

    if (phi_query_info)
    {
        SPHIPatternInfo *occurrences = phi_query_info->occurrences;
        int index;
        for (index=0; index<phi_query_info->num_patterns; index++)
        {
           list <CDisplaySeqalign::FeatureInfo*> phiblast_pattern;
           CSeq_id* id = new CSeq_id;
           id->Assign(*(result_set[index]).GetSeqId());
           CDisplaySeqalign::FeatureInfo*  feature_info = new CDisplaySeqalign::FeatureInfo;
           feature_info->seqloc = new CSeq_loc(*id, (TSeqPos) occurrences[index].offset,
                  (TSeqPos) (occurrences[index].offset + occurrences[index].length - 1));
           feature_info->feature_char = '*';
           feature_info->feature_id = "pattern";
           phiblast_pattern.push_back(feature_info);

           m_Outfile << "\nSignificant alignments for pattern occurrence " << index+1
                 << " at position " << 1+occurrences[index].offset << "\n\n";

           TMaskedQueryRegions masklocs;
           result_set[index].GetMaskedQueryRegions(masklocs);
           CConstRef<CSeq_align_set> aln_set = result_set[index].GetSeqAlign();
           CSeq_align_set copy_aln_set;
           CBlastFormatUtil::PruneSeqalign(*aln_set, copy_aln_set, m_NumAlignments);

           CDisplaySeqalign display(copy_aln_set, *m_Scope, &masklocs, &phiblast_pattern,
                             m_MatrixName);

           display.SetDbName(m_DbName);
           display.SetDbType(!m_DbIsAA);

           // set the alignment flags
           display.SetAlignOption(flags);

           if (m_Program == "blastn" || m_Program == "megablast") {
               display.SetMiddleLineStyle(CDisplaySeqalign::eBar);
               display.SetAlignType(CDisplaySeqalign::eNuc);
           }
           else {
               display.SetMiddleLineStyle(CDisplaySeqalign::eChar);
               display.SetAlignType(CDisplaySeqalign::eProt);
           }

           display.SetMasterGeneticCode(m_QueryGenCode);
           display.SetSlaveGeneticCode(m_DbGenCode);
           display.SetSeqLocChar(CDisplaySeqalign::eLowerCase);
           display.DisplaySeqalign(m_Outfile);
           m_Outfile << "\n";

           NON_CONST_ITERATE(list<CDisplaySeqalign::FeatureInfo*>, itr, phiblast_pattern) {
               delete *itr;
           }
        }
    }

    // print the ancillary data for this query

    x_PrintOneQueryFooter(*first_results.GetAncillaryData());
}
void 
CBlastFormat::PrintEpilog(const blast::CBlastOptions& options)
{
    if (m_FormatType == CFormattingArgs::eTabularWithComments) {
        CBlastTabularInfo tabinfo(m_Outfile);
        tabinfo.PrintNumProcessed(m_QueriesFormatted);
        return;
    } else if (m_FormatType >= CFormattingArgs::eTabular) 
        return;  // No footer for these.

    // XML can only be printed once (i.e.: not in batches), so we print it as
    // the epilog of the report
    if (m_FormatType == CFormattingArgs::eXml) {
        CCmdLineBlastXMLReportData report_data(m_AccumulatedQueries, 
                                               m_AccumulatedResults,
                                               options, m_DbName, m_DbIsAA,
                                               m_QueryGenCode, m_DbGenCode);
        objects::CBlastOutput xml_output;
        BlastXML_FormatReport(xml_output, &report_data);
        m_Outfile << MSerial_Xml << xml_output;
        m_AccumulatedResults.clear();
        m_AccumulatedQueries->clear();
        return;
    }

    m_Outfile << "\n\n";
    if ( !m_IsBl2Seq ) {
        CBlastFormatUtil::PrintDbReport(m_DbInfo, kFormatLineLength, 
                                        m_Outfile, false);
    }

    if (m_Program == "blastn" || m_Program == "megablast") {
        m_Outfile << "\n\nMatrix: " << "blastn matrix " <<
                        options.GetMatchReward() << " " <<
                        options.GetMismatchPenalty() << "\n";
    }
    else {
        m_Outfile << "\n\nMatrix: " << options.GetMatrixName() << "\n";
    }

    if (options.GetGappedMode() == true) {
        m_Outfile << "Gap Penalties: Existence: "
                << options.GetGapOpeningCost() << ", Extension: "
                << options.GetGapExtensionCost() << "\n";
    }
    if (options.GetWordThreshold()) {
        m_Outfile << "Neighboring words threshold: " <<
                        options.GetWordThreshold() << "\n";
    }
    if (options.GetWindowSize()) {
        m_Outfile << "Window for multiple hits: " <<
                        options.GetWindowSize() << "\n";
    }

    if (m_IsHTML) {
        m_Outfile << kHTML_Suffix << "\n";
    }
}

void CBlastFormat::ResetScopeHistory()
{
    // Our current XML/ASN.1 libraries do not have provisions for
    // incremental object input/output, so with XML output format we
    // need to accumulate the whole document before writing any data.
    
    // This means that XML output requires more memory than other
    // output formats.
    
    if (m_FormatType != CFormattingArgs::eXml) {
        m_Scope->ResetHistory();
    }
}

