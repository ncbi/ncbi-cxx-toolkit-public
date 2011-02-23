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
#include <algo/blast/format/blast_format.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/util/sequence.hpp>
#include <algo/blast/core/blast_stat.h>
#include <corelib/ncbiutil.hpp>                 // for FindBestChoice
#include <algo/blast/api/sseqloc.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>

#include <algo/blast/format/blastxml_format.hpp>
#include <algo/blast/format/data4xmlformat.hpp>       /* NCBI_FAKE_WARNING */
#include <algo/blast/format/build_archive.hpp>
#include <serial/objostrxml.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
USING_SCOPE(align_format);
#endif

CBlastFormat::CBlastFormat(const blast::CBlastOptions& options, 
                           blast::CLocalDbAdapter& db_adapter,
                 blast::CFormattingArgs::EOutputFormat format_type, 
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
                 int dbfilt_algorithm /* = -1 */,
                 const string& custom_output_format /* = kEmptyStr */,
                 bool is_megablast /* = false */,
                 bool is_indexed /* = false */,
                 const blast::CIgBlastOptions *ig_opts /* = NULL */)
        : m_FormatType(format_type), m_IsHTML(is_html), 
          m_DbIsAA(db_adapter.IsProtein()), m_BelieveQuery(believe_query),
          m_Outfile(outfile), m_NumSummary(num_summary),
          m_NumAlignments(num_alignments),
          m_Program(Blast_ProgramNameFromType(options.GetProgramType())), 
          m_DbName(kEmptyStr),
          m_QueryGenCode(qgencode), m_DbGenCode(dbgencode),
          m_ShowGi(show_gi), m_ShowLinkedSetSize(false),
          m_IsUngappedSearch(!options.GetGappedMode()),
          m_MatrixName(matrix_name),
          m_Scope(& scope),
          m_IsBl2Seq(false),
          m_IsRemoteSearch(is_remote_search),
          m_QueriesFormatted(0),
          m_Megablast(is_megablast),
          m_IndexedMegablast(is_indexed), 
          m_CustomOutputFormatSpec(custom_output_format),
          m_IgOptions(ig_opts)
{
    m_DbName = db_adapter.GetDatabaseName();
    m_IsBl2Seq = (m_DbName == kEmptyStr ? true : false);
    if (m_IsBl2Seq) {
        m_SeqInfoSrc.Reset(db_adapter.MakeSeqInfoSrc());
    } else {
        CBlastFormatUtil::GetBlastDbInfo(m_DbInfo, m_DbName, m_DbIsAA,
                                   dbfilt_algorithm, is_remote_search);
    }
    if (m_FormatType == CFormattingArgs::eXml) {
        m_AccumulatedQueries.Reset(new CBlastQueryVector());
    }
    if (use_sum_statistics && m_IsUngappedSearch) {
        m_ShowLinkedSetSize = true;
    }
    if ( m_Program == "blastn" &&
         options.GetMatchReward() == 0 &&
         options.GetMismatchPenalty() == 0 )
    {
       /* This combination is an indicator that we have used matrices
        * solely to develop the hsp score.  Also for the time being it
        * indicates that KA stats are not available. -RMH- 
        */
        m_DisableKAStats = true;
    }
    else
    {
        m_DisableKAStats = false;
    }

    CAlignFormatUtil::GetAsciiProteinMatrix(m_MatrixName, m_ScoringMatrix);
}

CBlastFormat::CBlastFormat(const blast::CBlastOptions& opts, 
                 const vector< CBlastFormatUtil::SDbInfo >& dbinfo_list,
                 blast::CFormattingArgs::EOutputFormat format_type, 
                 bool believe_query, CNcbiOstream& outfile,
                 int num_summary, 
                 int num_alignments,
                 CScope& scope,
                 bool show_gi, 
                 bool is_html, 
                 bool is_remote_search,
                 const string& custom_output_format)
        : m_FormatType(format_type),
          m_IsHTML(is_html), 
          m_DbIsAA(!Blast_SubjectIsNucleotide(opts.GetProgramType())),
          m_BelieveQuery(believe_query),
          m_Outfile(outfile),
          m_NumSummary(num_summary),
          m_NumAlignments(num_alignments),
          m_Program(Blast_ProgramNameFromType(opts.GetProgramType())), 
          m_DbName(kEmptyStr),
          m_QueryGenCode(opts.GetQueryGeneticCode()),
          m_DbGenCode(opts.GetDbGeneticCode()),
          m_ShowGi(show_gi),
          m_ShowLinkedSetSize(false),
          m_IsUngappedSearch(!opts.GetGappedMode()),
          m_MatrixName(opts.GetMatrixName()),
          m_Scope(&scope),
          m_IsBl2Seq(false),
          m_IsRemoteSearch(is_remote_search),
          m_QueriesFormatted(0),
          m_Megablast(opts.GetProgram() == eMegablast ||
                      opts.GetProgram() == eDiscMegablast),
          m_IndexedMegablast(opts.GetMBIndexLoaded()), 
          m_CustomOutputFormatSpec(custom_output_format)
{
    m_DbInfo.assign(dbinfo_list.begin(), dbinfo_list.end());
    vector< CBlastFormatUtil::SDbInfo >::const_iterator itInfo;
    for (itInfo = m_DbInfo.begin(); itInfo != m_DbInfo.end(); itInfo++)
    {
        m_DbName += itInfo->name + " ";
    }

    m_IsBl2Seq = false;

    if (m_FormatType == CFormattingArgs::eXml) {
        m_AccumulatedQueries.Reset(new CBlastQueryVector());
    }

    if (opts.GetSumStatisticsMode() && m_IsUngappedSearch) {
        m_ShowLinkedSetSize = true;
    }
    CAlignFormatUtil::GetAsciiProteinMatrix(m_MatrixName, m_ScoringMatrix);
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
    // Make sure no-one confuses us with the standard BLASTN 
    // algorithm.  -RMH-
    if ( m_Program == "blastn" &&
         m_DisableKAStats == true )
    {
      CBlastFormatUtil::BlastPrintVersionInfo("rmblastn", m_IsHTML,
                                              m_Outfile);
      m_Outfile << "\n\n";
      m_Outfile << "Reference: Robert M. Hubley, Arian Smit\n";
      m_Outfile << "RMBlast - RepeatMasker Search Engine\n";
      m_Outfile << "2010 <http://www.repeatmasker.org>";
    }else
    {
      CBlastFormatUtil::BlastPrintVersionInfo(m_Program, m_IsHTML,
                                              m_Outfile);
    }

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
    /* Skip printing KA parameters if the program is rmblastn -RMH- */
    if ( m_DisableKAStats )
      return;

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
        CSeq_id& subj_id =
            const_cast<CSeq_id&>((*alignment)->GetSeq_id(kSubjRow));
        if (prev_seqids.find(CRef<CSeq_id>(&subj_id)) != prev_seqids.end()) {
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
    bool html, bool showgi, bool isbl2seq)
{
   // set the alignment flags
    int flags = CDisplaySeqalign::eShowBlastInfo;

    if ( isbl2seq ) {
        flags |= CDisplaySeqalign::eShowNoDeflineInfo;
    }
    
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

// Ported from blastutl.c's BlastGetTypes and BlastGetProgramNumber (CVS
// revision 6.471), using program number definitions from blastdef.h (CVS
// revision 6.169)
pair<string, int> 
CBlastFormat::x_ComputeBlastTypePair() const
{
    pair<string, int> retval("unknown", 0);
    if (NStr::CompareNocase(m_Program, "blastn") == 0 ||
        NStr::CompareNocase(m_Program, "phiblastn") == 0) {
        retval = make_pair<string, int>("BLASTN", 1);
    } else if (NStr::CompareNocase(m_Program, "blastp") == 0 ||
               NStr::CompareNocase(m_Program, "psiblast") == 0 ||
               NStr::CompareNocase(m_Program, "rpsblast") == 0 ||
               NStr::CompareNocase(m_Program, "phiblastp") == 0) {
        retval = make_pair<string, int>("BLASTP", 2);
    } else if (NStr::CompareNocase(m_Program, "blastx") == 0 ||
               NStr::CompareNocase(m_Program, "rpstblastn") == 0) {
        retval = make_pair<string, int>("BLASTX", 3);
    } else if (NStr::CompareNocase(m_Program, "tblastn") == 0) {
        retval = make_pair<string, int>("TBLASTN", 4);
    } else if (NStr::CompareNocase(m_Program, "tblastx") == 0) {
        retval = make_pair<string, int>(kEmptyStr, 5);
    } else if (NStr::CompareNocase(m_Program, "psitblastn") == 0) {
        retval = make_pair<string, int>("TBLASTN", 6);
    } 
    return retval;
}

// Port of jzmisc.c's AddAlignInfoToSeqAnnotEx (CVS revision 6.11)
CRef<objects::CSeq_annot>
CBlastFormat::x_WrapAlignmentInSeqAnnot(CConstRef<objects::CSeq_align_set> alnset) const
{
    _ASSERT(alnset.NotEmpty());
    CRef<CSeq_annot> retval(new CSeq_annot);
    static const string kHistSeqalign("Hist Seqalign");
    static const string kBlastType("Blast Type");

    CRef<CUser_object> hist_align_obj(new CUser_object);
    hist_align_obj->AddField(kHistSeqalign, true);
    hist_align_obj->SetType().SetStr(kHistSeqalign);
    retval->AddUserObject(*hist_align_obj);

    pair<string, int> blast_type_pair(x_ComputeBlastTypePair());
    CRef<CUser_object> blast_type(new CUser_object);
    blast_type->AddField(blast_type_pair.first, blast_type_pair.second);
    if (blast_type_pair.first == kEmptyStr) {
        // For backwards compatibility with C toolkit BLAST output
        blast_type->SetData().back()->SetLabel().SetId(0);
    }
    blast_type->SetType().SetStr(kBlastType);
    retval->AddUserObject(*blast_type);

    ITERATE(CSeq_align_set::Tdata, itr, alnset->Get()) {
        retval->SetData().SetAlign().push_back(*itr);
    }

    return retval;
}

void 
CBlastFormat::x_PrintStructuredReport(const blast::CSearchResults& results,
              CConstRef<blast::CBlastQueryVector> queries)
{
   CConstRef<CSeq_align_set> aln_set = results.GetSeqAlign();

    // ASN.1 formatting is straightforward
    if (m_FormatType == CFormattingArgs::eAsnText) {
        if (results.HasAlignments()) {
            m_Outfile << MSerial_AsnText << *x_WrapAlignmentInSeqAnnot(aln_set);
        }
        return;
    } else if (m_FormatType == CFormattingArgs::eAsnBinary) {
        if (results.HasAlignments()) {
            m_Outfile << MSerial_AsnBinary <<
                *x_WrapAlignmentInSeqAnnot(aln_set);
        }
        return;
    } else if (m_FormatType == CFormattingArgs::eXml) {
        CRef<CSearchResults> res(const_cast<CSearchResults*>(&results));
        m_AccumulatedResults.push_back(res);
        CConstRef<CSeq_id> query_id = results.GetSeqId();
        // FIXME: this can be a bottleneck with large numbers of queries
        ITERATE(CBlastQueryVector, itr, *queries) {
            if (query_id->Match(*(*itr)->GetQueryId())) {
                m_AccumulatedQueries->push_back(*itr);
                break;
            }
        }
        return;
    }
}

void
CBlastFormat::x_PrintTabularReport(const blast::CSearchResults& results, 
                                   unsigned int itr_num)
{
    CConstRef<CSeq_align_set> aln_set = results.GetSeqAlign();
    if (m_IsUngappedSearch && results.HasAlignments()) {
        aln_set.Reset(CDisplaySeqalign::PrepareBlastUngappedSeqalign(*aln_set));
    }
    // other output types will need a bioseq handle
    CBioseq_Handle bhandle = m_Scope->GetBioseqHandle(*results.GetSeqId(),
                                                      CScope::eGetBioseq_All);

    // tabular formatting just prints each alignment in turn
    // (plus a header)
    if (m_FormatType == CFormattingArgs::eTabular ||
        m_FormatType == CFormattingArgs::eTabularWithComments ||
        m_FormatType == CFormattingArgs::eCommaSeparatedValues) {
        const CBlastTabularInfo::EFieldDelimiter kDelim =
            (m_FormatType == CFormattingArgs::eCommaSeparatedValues
             ? CBlastTabularInfo::eComma : CBlastTabularInfo::eTab);
        CBlastTabularInfo tabinfo(m_Outfile, m_CustomOutputFormatSpec, kDelim);
        tabinfo.SetParseLocalIds(m_BelieveQuery);

        if (m_FormatType == CFormattingArgs::eTabularWithComments) {
            string strProgVersion =
                NStr::ToUpper(m_Program) + " " + blast::CBlastVersion().Print();
            CConstRef<CBioseq> subject_bioseq = x_CreateSubjectBioseq();
            tabinfo.PrintHeader(strProgVersion, *(bhandle.GetBioseqCore()),
                                m_DbName, results.GetRID(), itr_num, aln_set,
                                subject_bioseq);
        }

    	CSeq_align_set copy_aln_set;
        CBlastFormatUtil::PruneSeqalign(*aln_set, copy_aln_set, MIN(m_NumSummary, m_NumAlignments));

        if (results.HasAlignments()) {
            ITERATE(CSeq_align_set::Tdata, itr, copy_aln_set.Get()) {
                    const CSeq_align& s = **itr;
                    tabinfo.SetFields(s, *m_Scope, &m_ScoringMatrix);
                    tabinfo.Print();
            }
        }
        return;
    }
}

void
CBlastFormat::x_PrintIgTabularReport(const blast::CIgBlastResults& results)
{
    CConstRef<CSeq_align_set> aln_set = results.GetSeqAlign();
    /* TODO do we support ungapped Igblast search?
    if (m_IsUngappedSearch && results.HasAlignments()) {
        aln_set.Reset(CDisplaySeqalign::PrepareBlastUngappedSeqalign(*aln_set));
    } */
    // other output types will need a bioseq handle
    CBioseq_Handle bhandle = m_Scope->GetBioseqHandle(*results.GetSeqId(),
                                                      CScope::eGetBioseq_All);

    // tabular formatting just prints each alignment in turn
    // (plus a header)
    if (m_FormatType != CFormattingArgs::eTabular &&
        m_FormatType != CFormattingArgs::eTabularWithComments &&
        m_FormatType != CFormattingArgs::eCommaSeparatedValues) return;

    const CBlastTabularInfo::EFieldDelimiter kDelim =
            (m_FormatType == CFormattingArgs::eCommaSeparatedValues
             ? CBlastTabularInfo::eComma : CBlastTabularInfo::eTab);

    CIgBlastTabularInfo tabinfo(m_Outfile, m_CustomOutputFormatSpec, kDelim);
    tabinfo.SetParseLocalIds(m_BelieveQuery);

    if (m_FormatType == CFormattingArgs::eTabularWithComments) {
        string strProgVersion =
                NStr::ToUpper(m_Program) + " " + blast::CBlastVersion().Print();
        CConstRef<CBioseq> subject_bioseq = x_CreateSubjectBioseq();
        tabinfo.PrintHeader(strProgVersion, *(bhandle.GetBioseqCore()),
                                m_DbName, results.GetRID(), 
                                numeric_limits<unsigned int>::max(),
                                aln_set, subject_bioseq);
    }

    // print the master alignment
    if (results.HasAlignments()) {
        CSeq_align_set::Tdata::const_iterator itr = aln_set->Get().begin();
        tabinfo.SetMasterFields(**itr, *m_Scope, "NA", &m_ScoringMatrix);
        tabinfo.SetIgAnnotation(results.GetIgAnnotation(), m_IgOptions->m_IsProtein);
        tabinfo.PrintMasterAlign();        
 
        for (; itr != aln_set->Get().end(); ++itr) {
            tabinfo.SetFields(**itr, *m_Scope, "NA", &m_ScoringMatrix);
            tabinfo.Print();
        }
    }
    // else TODO print something like this is not ig sequence?
}

CConstRef<objects::CBioseq> CBlastFormat::x_CreateSubjectBioseq()
{
    if ( !m_IsBl2Seq ) {
        return CConstRef<CBioseq>();
    }

    _ASSERT(m_IsBl2Seq);
    _ASSERT(m_SeqInfoSrc);
    static Uint4 subj_index = 0;

    list< CRef<CSeq_id> > ids = m_SeqInfoSrc->GetId(subj_index++);
    CRef<CSeq_id> id = FindBestChoice(ids, CSeq_id::BestRank);
    CBioseq_Handle bhandle = m_Scope->GetBioseqHandle(*id,
                                                      CScope::eGetBioseq_All);
    // If this assertion fails, we're not able to get the subject, possibly a
    // programming error (see @note in this function's declaration - was the
    // order of calls altered?)
    _ASSERT(bhandle);

    // reset the subject index if necessary
    if (subj_index >= m_SeqInfoSrc->Size()) {
        subj_index = 0;
    }
    return bhandle.GetBioseqCore();
}

void 
CBlastFormat::WriteArchive(blast::IQueryFactory& queries,
                           blast::CBlastOptionsHandle& options_handle,
                           const CSearchResultSet& results)
{
    if (m_IsBl2Seq)
    {
	CRef<CBlastQueryVector> query_vector(new CBlastQueryVector);
	for (unsigned int i=0; i<m_SeqInfoSrc->Size(); i++)
        {
		list< CRef<CSeq_id> > ids = m_SeqInfoSrc->GetId(i);
                CRef<CSeq_id> id = FindBestChoice(ids, CSeq_id::BestRank);
                CRef<CSeq_loc> seq_loc(new CSeq_loc);
                seq_loc->SetWhole(*id);
                CRef<CBlastSearchQuery> search_query(new CBlastSearchQuery(*seq_loc, *m_Scope));
                query_vector->AddQuery(search_query);
        }
        CObjMgr_QueryFactory subjects(*query_vector);
        m_Outfile << MSerial_AsnText << *(BlastBuildArchive(queries, options_handle, results, subjects));
        
    }
    else
       m_Outfile << MSerial_AsnText << *(BlastBuildArchive(queries, options_handle, results,  m_DbName));
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
        ERR_POST(Error << results.GetErrorStrings());
        return; // errors are deemed fatal
    }
    if (results.HasWarnings()) {
        ERR_POST(Warning << results.GetWarningStrings());
    }

    if (m_FormatType == CFormattingArgs::eTabular ||
        m_FormatType == CFormattingArgs::eTabularWithComments ||
        m_FormatType == CFormattingArgs::eCommaSeparatedValues) {
        x_PrintTabularReport(results, itr_num);
        return;
    }
    const bool kIsTabularOutput = false;

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
                                            m_IsHTML, kIsTabularOutput,
                                            results.GetRID());
    if (m_IsBl2Seq) {
        m_Outfile << "\n";
        // FIXME: this might be configurable in the future
        const bool kBelieveSubject = false; 
        CConstRef<CBioseq> subject_bioseq = x_CreateSubjectBioseq();
        CBlastFormatUtil::AcknowledgeBlastSubject(*subject_bioseq, 
                                                  kFormatLineLength, 
                                                  m_Outfile, kBelieveSubject, 
                                                  m_IsHTML, kIsTabularOutput);
    }

    // quit early if there are no hits
    if ( !results.HasAlignments() ) {
        m_Outfile << "\n\n" 
              << "***** " << CBlastFormatUtil::kNoHitsFound << " *****" << "\n" 
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
    // Also disable when program is rmblastn.  At this time
    // we do not want summary bit scores/evalues for this 
    // program. -RMH-
    if ( !m_IsBl2Seq && !m_DisableKAStats ) {
        x_DisplayDeflines(aln_set, itr_num, prev_seqids);
    }

    //-------------------------------------------------
    // print the alignments
    m_Outfile << "\n";

    TMaskedQueryRegions masklocs;
    results.GetMaskedQueryRegions(masklocs);

    CSeq_align_set copy_aln_set;
    CBlastFormatUtil::PruneSeqalign(*aln_set, copy_aln_set, m_NumAlignments);

    int flags = s_SetFlags(m_Program, m_FormatType, m_IsHTML, m_ShowGi,
                           m_IsBl2Seq);

    /* Display the raw score and disable the display of bitscore and 
     * evalue. -RMH- 
     */
    if ( m_DisableKAStats ) {
        flags |= CDisplaySeqalign::eShowRawScoreOnly;
    }

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
CBlastFormat::PrintOneResultSet(blast::CIgBlastResults& results,
                        CConstRef<blast::CBlastQueryVector> queries)
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
        ERR_POST(Error << results.GetErrorStrings());
        return; // errors are deemed fatal
    }
    if (results.HasWarnings()) {
        ERR_POST(Warning << results.GetWarningStrings());
    }

    if (results.GetIgAnnotation()->m_MinusStrand) {
        x_ReverseQuery(results);
    }

    if (m_FormatType == CFormattingArgs::eTabular ||
        m_FormatType == CFormattingArgs::eTabularWithComments ||
        m_FormatType == CFormattingArgs::eCommaSeparatedValues) {
        x_PrintIgTabularReport(results);
        return;
    }
    const bool kIsTabularOutput = false;

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
                                            m_IsHTML, kIsTabularOutput,
                                            results.GetRID());
    if (m_IsBl2Seq) {
        m_Outfile << "\n";
        // FIXME: this might be configurable in the future
        const bool kBelieveSubject = false; 
        CConstRef<CBioseq> subject_bioseq = x_CreateSubjectBioseq();
        CBlastFormatUtil::AcknowledgeBlastSubject(*subject_bioseq, 
                                                  kFormatLineLength, 
                                                  m_Outfile, kBelieveSubject, 
                                                  m_IsHTML, kIsTabularOutput);
    }

    // quit early if there are no hits
    if ( !results.HasAlignments() ) {
        m_Outfile << "\n\n" 
              << "***** " << CBlastFormatUtil::kNoHitsFound << " *****" << "\n" 
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
        CPsiBlastIterationState::TSeqIds prev_ids = CPsiBlastIterationState::TSeqIds();
        x_DisplayDeflines(aln_set, numeric_limits<unsigned int>::max(), prev_ids);
    }

    //-------------------------------------------------
    // print the alignments
    m_Outfile << "\n";

    TMaskedQueryRegions masklocs;
    results.GetMaskedQueryRegions(masklocs);

    CSeq_align_set copy_aln_set;
    CBlastFormatUtil::PruneSeqalign(*aln_set, copy_aln_set, m_NumAlignments);

    int flags = s_SetFlags(m_Program, m_FormatType, m_IsHTML, m_ShowGi,
                           m_IsBl2Seq);

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
CBlastFormat::x_ReverseQuery(blast::CIgBlastResults& results)
{
    // create a temporary seq_id
    CConstRef<CSeq_id> qid = results.GetSeqId();
    string new_id = qid->AsFastaString() + "_reversed";
    
    // create a bioseq
    CBioseq_Handle q_bh = m_Scope->GetBioseqHandle(*qid);
    int len = q_bh.GetBioseqLength();
    CSeq_loc loc(*(const_cast<CSeq_id *>(&*qid)), 0, len-1, eNa_strand_minus);
    CRef<CBioseq> q_new(new CBioseq(loc, new_id));
    CConstRef<CSeq_id> new_qid = m_Scope->AddBioseq(*q_new).GetSeqId();
    if (qid->IsLocal()) {
        string title = sequence::GetTitle(q_bh);
        if (title != "") {
            CRef<CSeqdesc> des(new CSeqdesc());
            des->SetTitle("reversed|" + title);
            m_Scope->GetBioseqEditHandle(*q_new).SetDescr().Set().push_back(des);
        }
    }

    // set up the mapping
    CSeq_loc new_loc(*(const_cast<CSeq_id *>(&*new_qid)), 0, len-1, eNa_strand_plus);
    CSeq_loc_Mapper mapper(loc, new_loc, &*m_Scope);

    // replace the alignment with the new query 
    CRef<CSeq_align_set> align_set(new CSeq_align_set());
    ITERATE(CSeq_align_set::Tdata, align, results.GetSeqAlign()->Get()) {
        CRef<CSeq_align> new_align = mapper.Map(**align, 0);
        align_set->Set().push_back(new_align);
    }
    results.SetSeqAlign().Reset(&*align_set);

    // reverse IgAnnotations
    CRef<CIgAnnotation> &annots = results.SetIgAnnotation();
    for (int i=0; i<6; i+=2) {
        int start = annots->m_GeneInfo[i];
        if (start >= 0) {
            annots->m_GeneInfo[i] = len - annots->m_GeneInfo[i+1];
            annots->m_GeneInfo[i+1] = len - start;
        }
    }
    for (int i=0; i<12; ++i) {
        // TODO
    }
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
        m_FormatType == CFormattingArgs::eTabularWithComments ||
        m_FormatType == CFormattingArgs::eCommaSeparatedValues) {
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
              << "***** " << CBlastFormatUtil::kNoHitsFound << " *****" << "\n" 
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


    int flags = s_SetFlags(m_Program, m_FormatType, m_IsHTML, m_ShowGi,
                           m_IsBl2Seq);

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
        CBlastTabularInfo tabinfo(m_Outfile, m_CustomOutputFormatSpec);
        if (m_IsBl2Seq) {
            _ASSERT(m_SeqInfoSrc);
            m_QueriesFormatted /= m_SeqInfoSrc->Size();
        }
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
                                               m_QueryGenCode, m_DbGenCode,
                                               m_IsRemoteSearch);
        objects::CBlastOutput xml_output;
        BlastXML_FormatReport(xml_output, &report_data);

	CObjectOStreamXml l_xml_stream( m_Outfile, false );
	l_xml_stream.SetDefaultDTDFilePrefix("http://www.ncbi.nlm.nih.gov/dtd/");
	l_xml_stream.WriteFileHeader(ObjectType( xml_output ));
        l_xml_stream.WriteObject( ObjectInfo( xml_output ));

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
        double gap_extension = (double) options.GetGapExtensionCost();
        if ((m_Program == "megablast" || m_Program == "blastn") && options.GetGapExtensionCost() == 0)
        { // Formula from PMID 10890397 applies if both gap values are zero.
               gap_extension = -2*options.GetMismatchPenalty() + options.GetMatchReward();
               gap_extension /= 2.0;
        }
        m_Outfile << "Gap Penalties: Existence: "
                << options.GetGapOpeningCost() << ", Extension: "
                << gap_extension << "\n";
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
    // Do not reset the scope for BLAST2Sequences or else we'll loose the
    // sequence data! (see x_CreateSubjectBioseq)
    if (m_IsBl2Seq) {
        return;
    }

    // Our current XML/ASN.1 libraries do not have provisions for
    // incremental object input/output, so with XML output format we
    // need to accumulate the whole document before writing any data.
    
    // This means that XML output requires more memory than other
    // output formats.
    
    if (m_FormatType != CFormattingArgs::eXml) {
        m_Scope->ResetHistory();
    }
}

