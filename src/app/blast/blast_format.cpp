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
#include "blast_format.hpp"
#include "data4xmlformat.hpp"

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
#endif

const string CBlastFormat::kNoHitsFound("No hits found");

CBlastFormat::CBlastFormat(const CBlastOptions& options, const string& dbname, 
                 CFormattingArgs::EOutputFormat format_type, bool db_is_aa,
                 bool believe_query, CNcbiOstream& outfile,
                 int num_summary, 
                 int num_alignments, 
                 const char *matrix_name /* = BLAST_DEFAULT_MATRIX */,
                 bool show_gi /* = false */, 
                 bool is_html /* = false */,
                 int qgencode /* = BLAST_GENETIC_CODE */, 
                 int dbgencode /* = BLAST_GENETIC_CODE */,
                 bool show_linked /* = false */)
        : m_FormatType(format_type), m_IsHTML(is_html), 
          m_DbIsAA(db_is_aa), m_BelieveQuery(believe_query),
          m_Outfile(outfile), m_NumSummary(num_summary),
          m_NumAlignments(num_alignments),
          m_Program(Blast_ProgramNameFromType(options.GetProgramType())), 
          m_DbName(dbname),
          m_QueryGenCode(qgencode), m_DbGenCode(dbgencode),
          m_ShowGi(show_gi), m_ShowLinkedSetSize(show_linked),
          m_IsDbAvailable(dbname.length() > 0),
          m_IsUngappedSearch(!options.GetGappedMode()),
          m_MatrixName(matrix_name)
{
    if (m_IsDbAvailable) {
        x_FillDbInfo();
    }
    if (m_FormatType == CFormattingArgs::eXml) {
        m_AccumulatedQueries.Reset(new CBlastQueryVector());
    }
}


void
CBlastFormat::x_FillDbInfo()
{
    m_DbInfo.push_back(CBlastFormatUtil::SDbInfo());
    CBlastFormatUtil::SDbInfo& info = m_DbInfo.front();

    CSeqDB seqdb(m_DbName, m_DbIsAA ? CSeqDB::eProtein : CSeqDB::eNucleotide);

    info.is_protein = m_DbIsAA;
    info.name = seqdb.GetDBNameList();
    info.definition = seqdb.GetTitle();
    if (info.definition.empty())
        info.definition = info.name;
    info.date = seqdb.GetDate();
    info.total_length = seqdb.GetTotalLength();
    info.number_seqs = seqdb.GetNumSeqs();
    info.subset = false;
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
        m_Outfile << kHTML_Prefix << endl;
    }

    CBlastFormatUtil::BlastPrintVersionInfo(m_Program, m_IsHTML, 
                                            m_Outfile);
    m_Outfile << endl << endl;

    CBlastFormatUtil::BlastPrintReference(m_IsHTML, kFormatLineLength, 
                                          m_Outfile);

    if (m_IsDbAvailable)
    {
        CBlastFormatUtil::PrintDbReport(m_DbInfo, kFormatLineLength, 
                                        m_Outfile, true);
    }
}

void
CBlastFormat::x_PrintOneQueryFooter(const CBlastAncillaryData& summary)
{
    const Blast_KarlinBlk *kbp_ungap = summary.GetUngappedKarlinBlk();
    m_Outfile << endl;
    if (kbp_ungap) {
        CBlastFormatUtil::PrintKAParameters(kbp_ungap->Lambda, 
                                            kbp_ungap->K, kbp_ungap->H,
                                            kFormatLineLength, m_Outfile,
                                            false);
    }

    const Blast_KarlinBlk *kbp_gap = summary.GetGappedKarlinBlk();
    m_Outfile << endl;
    if (kbp_gap) {
        CBlastFormatUtil::PrintKAParameters(kbp_gap->Lambda, 
                                            kbp_gap->K, kbp_gap->H,
                                            kFormatLineLength, m_Outfile,
                                            true);
    }

    m_Outfile << endl;
    m_Outfile << "Effective search space used: " << 
                        summary.GetSearchSpace() << endl;
}

void
CBlastFormat::PrintOneResultSet(const CSearchResults& results,
                                CScope& scope,
                                CConstRef<CBlastQueryVector> queries,
                                unsigned int itr_num
                                /* = numeric_limits<unsigned int>::max() */)
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

    if (results.HasErrors()) {
        m_Outfile << endl << results.GetErrorStrings() << endl;
        return; // errors are deemed fatal
    }
    if (results.HasWarnings()) {
        m_Outfile << endl << results.GetWarningStrings() << endl;
    }

    // other output types will need a bioseq handle
    CBioseq_Handle bhandle = scope.GetBioseqHandle(*results.GetSeqId(),
                                                  CScope::eGetBioseq_All);

    // tabular formatting just prints each alignment in turn
    // (plus a header)
    if (m_FormatType == CFormattingArgs::eTabular || 
        m_FormatType == CFormattingArgs::eTabularWithComments) {
        CBlastTabularInfo tabinfo(m_Outfile);

        if (m_FormatType == CFormattingArgs::eTabularWithComments)
             tabinfo.PrintHeader(m_Program,
                                 *(bhandle.GetBioseqCore()),
                                 m_DbName, 0, aln_set);
                                 
        if (results.HasAlignments()) {
            ITERATE(CSeq_align_set::Tdata, itr, aln_set->Get()) {
                    const CSeq_align& s = **itr;
                    tabinfo.SetFields(s, scope);
                    tabinfo.Print();
            }
        }
        return;
    }

    if (itr_num != numeric_limits<unsigned int>::max()) {
        m_Outfile << "Results from round " << itr_num << NcbiEndl;
    }

    CConstRef<CBioseq> bioseq = bhandle.GetCompleteBioseq();

    // print the preamble for this query

    m_Outfile << endl << endl;
    CBlastFormatUtil::AcknowledgeBlastQuery(*bioseq, kFormatLineLength,
                                            m_Outfile, m_BelieveQuery,
                                            m_IsHTML, false);

    // quit early if there are no hits
    if ( !results.HasAlignments() ) {
        m_Outfile << endl << endl 
                  << "***** " << kNoHitsFound << " *****" << endl 
                  << endl << endl;
        x_PrintOneQueryFooter(*results.GetAncillaryData());
        return;
    }

    _ASSERT(results.HasAlignments());
    if (m_IsUngappedSearch) {
        aln_set.Reset(CDisplaySeqalign::PrepareBlastUngappedSeqalign(*aln_set));
    }

    // set the alignment flags

    int flags;

    //-------------------------------------------------
    // print 1-line summaries
    m_Outfile << endl;

    CShowBlastDefline showdef(*aln_set, scope, 
                              kFormatLineLength,
                              m_NumSummary);

    flags = 0;
    if (m_ShowLinkedSetSize)
        flags |= CShowBlastDefline::eShowSumN;
    if (m_IsHTML)
        flags |= CShowBlastDefline::eHtml;
    if (m_ShowGi)
        flags |= CShowBlastDefline::eShowGi;

    showdef.SetOption(flags);
    showdef.SetDbName(m_DbName);
    showdef.SetDbType(!m_DbIsAA);
    showdef.DisplayBlastDefline(m_Outfile);

    //-------------------------------------------------
    // print the alignments
    m_Outfile << endl;

    TMaskedQueryRegions masklocs;
    results.GetMaskedQueryRegions(masklocs);

    CSeq_align_set copy_aln_set;
    CBlastFormatUtil::PruneSeqalign(*aln_set, copy_aln_set, m_NumAlignments);

    CDisplaySeqalign display(copy_aln_set, scope, &masklocs, NULL,
                             m_MatrixName);
    display.SetDbName(m_DbName);
    display.SetDbType(!m_DbIsAA);

    flags = CDisplaySeqalign::eShowBlastInfo;

    if (m_IsHTML)
        flags |= CDisplaySeqalign::eHtml;
    if (m_ShowGi)
        flags |= CDisplaySeqalign::eShowGi;

    if (m_FormatType >= CFormattingArgs::eQueryAnchoredIdentities && 
        m_FormatType <= CFormattingArgs::eFlatQueryAnchoredNoIdentities) {
        flags |= CDisplaySeqalign::eMergeAlign;
    }
    else {
        flags |= CDisplaySeqalign::eShowBlastStyleId |
                 CDisplaySeqalign::eShowMiddleLine;
    }

    if (m_FormatType == CFormattingArgs::eQueryAnchoredIdentities || 
        m_FormatType == CFormattingArgs::eFlatQueryAnchoredIdentities) {
        flags |= CDisplaySeqalign::eShowIdentity;
    }
    if (m_FormatType == CFormattingArgs::eQueryAnchoredIdentities || 
        m_FormatType == CFormattingArgs::eQueryAnchoredNoIdentities) {
        flags |= CDisplaySeqalign::eMasterAnchored;
    }
    if (m_Program == "tblastx") {
        flags |= CDisplaySeqalign::eTranslateNucToNucAlignment;
    }
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

    // print the ancillary data for this query

    x_PrintOneQueryFooter(*results.GetAncillaryData());
}

void 
CBlastFormat::PrintEpilog(const CBlastOptions& options)
{
    // some output types don't have a footer
    if (m_FormatType >= CFormattingArgs::eTabular)
        return;

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

    m_Outfile << endl << endl;
    if (m_IsDbAvailable)
    {
        CBlastFormatUtil::PrintDbReport(m_DbInfo, kFormatLineLength, 
                                        m_Outfile, false);
    }

    if (m_Program == "blastn" || m_Program == "megablast") {
        m_Outfile << "\n\nMatrix: " << "blastn matrix " <<
                        options.GetMatchReward() << " " <<
                        options.GetMismatchPenalty() << endl;
    }
    else {
        m_Outfile << "\n\nMatrix: " << options.GetMatrixName() << endl;
    }

    if (options.GetGappedMode() == true) {
        m_Outfile << "Gap Penalties: Existence: "
                << options.GetGapOpeningCost() << ", Extension: "
                << options.GetGapExtensionCost() << endl;
    }
    if (options.GetWordThreshold()) {
        m_Outfile << "Neighboring words threshold: " <<
                        options.GetWordThreshold() << endl;
    }
    if (options.GetWindowSize()) {
        m_Outfile << "Window for multiple hits: " <<
                        options.GetWindowSize() << endl;
    }

    if (m_IsHTML) {
        m_Outfile << kHTML_Suffix << endl;
    }
}
