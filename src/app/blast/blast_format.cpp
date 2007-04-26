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
#include <util/tables/raw_scoremat.h>
#include "blast_format.hpp"

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(blast);
USING_SCOPE(objects);
#endif

CBlastFormat::CBlastFormat(const CBlastOptions& options, const string& dbname, 
                 int format_type, bool db_is_aa,
                 bool believe_query, CNcbiOstream& outfile,
                 int num_summary, 
                 const char *matrix_name,
                 bool show_gi, bool is_html,
                 int qgencode, int dbgencode,
                 bool show_linked)
        : m_FormatType(format_type), m_IsHTML(is_html), 
          m_DbIsAA(db_is_aa), m_BelieveQuery(believe_query),
          m_Outfile(outfile), m_NumSummary(num_summary),
          m_Program(Blast_ProgramNameFromType(options.GetProgramType())), 
          m_DbName(dbname),
          m_QueryGenCode(qgencode), m_DbGenCode(dbgencode),
          m_ShowGi(show_gi), m_ShowLinkedSetSize(show_linked),
          m_MatrixSet(false)
{
    // blast formatter cannot handle these output types
    if (format_type == 5 || format_type == 6) {
        NCBI_THROW(blast::CBlastException, 
                   eInvalidArgument,
                   "blunt-end query anchored format not supported");
    }
    if (format_type == 7) {
        NCBI_THROW(blast::CBlastException, 
                   eInvalidArgument,
                   "XML output format not supported");
    }

    x_FillScoreMatrix(matrix_name);
    x_FillDbInfo();
}


CBlastFormat::~CBlastFormat()
{
    for (int i = 0; i < CDisplaySeqalign::ePMatrixSize; i++)
        delete [] m_Matrix[i];
}


void
CBlastFormat::x_FillScoreMatrix(const char *matrix_name)
{
    for (int i = 0; i < CDisplaySeqalign::ePMatrixSize; i++)
        m_Matrix[i] = new int[CDisplaySeqalign::ePMatrixSize];

    if (matrix_name == NULL)
        return;

    const char *letter_order = "ARNDCQEGHILKMFPSTWYVBZX";
    const SNCBIPackedScoreMatrix *packed_matrix = 0;

    if (strcmp(matrix_name, "BLOSUM45") == 0)
        packed_matrix = &NCBISM_Blosum45;
    else if (strcmp(matrix_name, "BLOSUM50") == 0)
        packed_matrix = &NCBISM_Blosum50;
    else if (strcmp(matrix_name, "BLOSUM62") == 0)
        packed_matrix = &NCBISM_Blosum62;
    else if (strcmp(matrix_name, "BLOSUM80") == 0)
        packed_matrix = &NCBISM_Blosum80;
    else if (strcmp(matrix_name, "BLOSUM90") == 0)
        packed_matrix = &NCBISM_Blosum90;
    else if (strcmp(matrix_name, "PAM30") == 0)
        packed_matrix = &NCBISM_Pam30;
    else if (strcmp(matrix_name, "PAM70") == 0)
        packed_matrix = &NCBISM_Pam70;
    else if (strcmp(matrix_name, "PAM250") == 0)
        packed_matrix = &NCBISM_Pam250;
    else if (m_Program != "blastn" && m_Program != "megablast") {
        NCBI_THROW(blast::CBlastException, 
                   eInvalidArgument,
                   "unsupported score matrix");
    }

    if (packed_matrix) {
        SNCBIFullScoreMatrix m;

        NCBISM_Unpack(packed_matrix, &m);

        for (int i = 0; i < CDisplaySeqalign::ePMatrixSize; i++) {
            for (int j = 0; j < CDisplaySeqalign::ePMatrixSize; j++) {
                m_Matrix[i][j] = m.s[(unsigned char)letter_order[i]]
                                    [(unsigned char)letter_order[j]];
            }
        }
        m_MatrixSet = true;
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

void 
CBlastFormat::PrintProlog()
{
    // no header for some output types
    if (m_FormatType >= 8)
        return;

    CBlastFormatUtil::BlastPrintVersionInfo(m_Program, m_IsHTML, 
                                            m_Outfile);
    m_Outfile << endl << endl;

    CBlastFormatUtil::BlastPrintReference(m_IsHTML, kFormatLineLength, 
                                          m_Outfile);

    CBlastFormatUtil::PrintDbReport(m_DbInfo, kFormatLineLength, 
                                    m_Outfile, true);
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
    if (kbp_ungap) {
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
CBlastFormat::PrintOneAlignSet(const CSearchResults& results,
                               CScope& scope,
                               unsigned int itr_num)
{
    const CSeq_align_set& aln_set = *results.GetSeqAlign();

    // ASN.1 formatting is straightforward
    if (m_FormatType == 10) {
        m_Outfile << MSerial_AsnText << aln_set;
        return;
    }
    else if (m_FormatType == 11) {
        m_Outfile << MSerial_AsnBinary << aln_set;
        return;
    }
#if 0
    if (m_FormatType == 10 || m_FormatType == 11) {
        CSeq_annot seqannot;
        seqannot.SetData().SetAlign() = aln_set.Get();
        if (m_FormatType == 10)
            m_Outfile << MSerial_AsnText << seqannot;
        else
            m_Outfile << MSerial_AsnBinary << aln_set;
        return;
    }
#endif

    // tabular formatting just prints each alignment in turn
    // (plus a header)
    if (m_FormatType == 8 || m_FormatType == 9) {
        CBlastTabularInfo tabinfo(m_Outfile);
        ITERATE(CSeq_align_set::Tdata, itr, aln_set.Get()) {
                const CSeq_align& s = **itr;
                tabinfo.SetFields(s, scope);
                tabinfo.Print();
        }
        return;
    }

    if (itr_num != numeric_limits<unsigned int>::max()) {
        m_Outfile << "Results from round " << itr_num << NcbiEndl;
    }

    // other output types will need a bioseq handle
    CBioseq_Handle bhandle = scope.GetBioseqHandle(*results.GetSeqId(),
                                                  CScope::eGetBioseq_All);
    CConstRef<CBioseq> bioseq = bhandle.GetCompleteBioseq();

    // print the preamble for this query

    m_Outfile << endl << endl;
    CBlastFormatUtil::AcknowledgeBlastQuery(*bioseq, kFormatLineLength,
                                            m_Outfile, m_BelieveQuery,
                                            m_IsHTML, false);

    // quit early if there are no hits
    if (aln_set.Get().empty())
    {
        m_Outfile << "\n\n ***** No hits found *****\n\n" << endl;
        x_PrintOneQueryFooter(*results.GetAncillaryData());
        return;
    }

    // set the alignment flags

    int flags;

    //-------------------------------------------------
    // print 1-line summaries
    m_Outfile << endl;

    CShowBlastDefline showdef(aln_set, scope, 
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
    CDisplaySeqalign display(aln_set, scope, &masklocs, NULL,
                             m_MatrixSet ? (const int(*)[23])m_Matrix : NULL);
    display.SetDbName(m_DbName);
    display.SetDbType(!m_DbIsAA);

    flags = CDisplaySeqalign::eShowBlastInfo;

    if (m_IsHTML)
        flags |= CDisplaySeqalign::eHtml;
    if (m_ShowGi)
        flags |= CDisplaySeqalign::eShowGi;

    if (m_FormatType >= 1 && m_FormatType <= 6) {
        flags |= CDisplaySeqalign::eMergeAlign;
    }
    else {
        flags |= CDisplaySeqalign::eShowBlastStyleId |
                 CDisplaySeqalign::eShowMiddleLine;
    }

    if (m_FormatType == 1 || m_FormatType == 3 || m_FormatType == 5) {
        flags |= CDisplaySeqalign::eShowIdentity;
    }
    if (m_FormatType == 1 || m_FormatType == 2) {
        flags |= CDisplaySeqalign::eMasterAnchored;
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
    if (m_FormatType >= 8)
        return;

    m_Outfile << endl << endl;
    CBlastFormatUtil::PrintDbReport(m_DbInfo, kFormatLineLength, 
                                    m_Outfile, false);

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
}
