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
 * Author: Christiam Camacho
 *
 */

/** @file blastdb_dataextract.hpp
 *  Declares classes which extract data from a BLAST database
 */

#ifndef OBJTOOLS_BLASTDB_FORMAT___BLASTDB_DATAEXTRACT__HPP
#define OBJTOOLS_BLASTDB_FORMAT___BLASTDB_DATAEXTRACT__HPP

#include <objtools/blast/blastdb_format/blastdb_seqid.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <objtools/blast/seqdb_reader/linkoutdb.hpp>
#include <objmgr/util/sequence.hpp>
#include <corelib/ncbiapp.hpp>  // temporarily needed for CNcbiApplication
#include <sstream>

// Note: move this to corelib and define properly (see blastformat equivalent)
// #define NCBI_BLASTDB_FORMAT_EXPORT

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

/// Class to extract data from a BLAST database given an identifier
class NCBI_BLASTDB_FORMAT_EXPORT CBlastDBExtractor {

public:
    /// ctor
    CBlastDBExtractor(CSeqDB&             blastdb, 
                      TSeqRange           range = TSeqRange(),
                      objects::ENa_strand strand = objects::eNa_strand_both,
                      int                 filt_algo_id = -1, 
                      int                 fmt_algo_id = -1,
                      int                 line_width = 80,
                      bool                target_only = true,
                      bool                ctrl_a = false)
            : m_BlastDb(blastdb),
              m_SeqRange(range),
              m_Strand(strand),
              m_FiltAlgoId(filt_algo_id),
              m_FmtAlgoId(fmt_algo_id),
              m_LineWidth(line_width),
              m_TargetOnly(target_only),
              m_UseCtrlA(ctrl_a) 
    {
        m_UseLinkoutDB = false;
        CNcbiApplication* app = CNcbiApplication::Instance();
        if (app && !app->GetEnvironment().Get("LINKOUTDB").empty()) {
            m_UseLinkoutDB = true;
        }
    }

    /// Setting seqid
    /// @param id sequence identifier [in]
    void SetSeqId(const CBlastDBSeqId &seq_id, bool get_defline=true);
    string ExtractOid();
    string ExtractPig();
    string ExtractGi();
    string ExtractAccession();
    string ExtractTitle();
    string ExtractTaxId();
    string ExtractCommonTaxonomicName();
    string ExtractScientificName();
    string ExtractMaskingData();
    string ExtractSeqData();
    string ExtractSeqLen();
    string ExtractHash();
    string ExtractLinkoutInteger();
    string ExtractMembershipInteger();
    string ExtractLinkoutTokens();

    string ExtractFasta(const CBlastDBSeqId &seq_id);

protected:
    /// underlying Blast database
    CSeqDB& m_BlastDb;
    /// sequence range
    TSeqRange m_SeqRange;
    /// strand
    objects::ENa_strand m_Strand;
    /// filtering algorithsm for sequence
    int m_FiltAlgoId;
    /// filtering algorithsm for outfmt
    int m_FmtAlgoId;
    /// FASTA output line width
    int m_LineWidth;
    /// Should the record contain mutilple seqids? (used only with %f)
    bool m_TargetOnly;
    /// Replace with ctrl_a? (used only with %f)
    bool m_UseCtrlA;
    /// OID of the record
    int m_Oid;
    /// the target gi
    int m_Gi;
    /// bioseq
    CRef<CBioseq> m_Bioseq;

    /**** BEGIN: Support for printing linkouts */

    /// Cache the defline
    CRef<CBlast_def_line_set> m_Defline; 
    /// Convenience for printing strings of linkout types
    vector<CLinkoutDB::TLinkoutTypeString> m_LinkoutTypes;

    /// This replaces having to read and cache the defline (once all linkout
    /// bits have been ported to LinkoutDB, turned off for now but can be
    /// enabled via the LINKOUTDB environment variable
    bool m_UseLinkoutDB;

    /**** END: Support for printing linkouts */
private:
    void x_ExtractMaskingData(CSeqDB::TSequenceRanges &ranges, int algo_id);
    int x_ExtractTaxId();

    /// Initialize the data structures required to print the linkout information
    void x_InitLinkoutData() {
        if (m_LinkoutTypes.empty()) {
            CLinkoutDB::GetLinkoutTypes(m_LinkoutTypes);
        }
    }

    /// Initialize the cached defline
    void x_InitDefline() {
        if (m_Defline.NotEmpty()) {
            return;
        }
        m_Defline = m_BlastDb.GetHdr(m_Oid);
    }
};


END_NCBI_SCOPE

#endif /* OBJTOOLS_BLASTDB_FORMAT___BLASTDB_DATAEXTRACT__HPP */
