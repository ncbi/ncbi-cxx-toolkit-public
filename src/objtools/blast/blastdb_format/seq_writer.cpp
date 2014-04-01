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

/** @file seq_writer.cpp
 *  Implementation of the CSeqFormatter class
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <objtools/blast/blastdb_format/seq_writer.hpp>
#include <objtools/blast/blastdb_format/blastdb_dataextract.hpp>
#include <objtools/blast/blastdb_format/invalid_data_exception.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <numeric>      // for std::accumulate
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

CSeqFormatter::CSeqFormatter(const string& format_spec, CSeqDB& blastdb,
                 CNcbiOstream& out, 
                 CSeqFormatterConfig config /* = CSeqFormatterConfig() */)
    : m_Out(out), m_FmtSpec(format_spec), m_BlastDb(blastdb),
      m_DataExtractor(blastdb, 
                      config.m_SeqRange,
                      config.m_Strand,
                      config.m_FiltAlgoId,
                      config.m_FmtAlgoId,
                      config.m_LineWidth,
                      config.m_TargetOnly,
                      config.m_UseCtrlA)
{
    // Validate the algo id
    if (config.m_FiltAlgoId >= 0 || config.m_FmtAlgoId >= 0) {
        vector<int> algo_ids;
        if (config.m_FiltAlgoId >= 0) 
            algo_ids.push_back(config.m_FiltAlgoId);
        if (config.m_FmtAlgoId >= 0) 
            algo_ids.push_back(config.m_FmtAlgoId);
        vector<int> invalid_algo_ids = 
            m_BlastDb.ValidateMaskAlgorithms(algo_ids);
        if ( !invalid_algo_ids.empty()) {
            NCBI_THROW(CInvalidDataException, eInvalidInput,
                "Invalid filtering algorithm ID.");
        }
    }

    // Record where the offsets where the replacements must occur
    for (SIZE_TYPE i = 0; i < m_FmtSpec.size(); i++) {
        if (m_FmtSpec[i] == '%' && m_FmtSpec[i+1] == '%') {
            // remove the escape character for '%'
            m_FmtSpec.erase(i++, 1);
            continue;
        }

        if (m_FmtSpec[i] == '%') {
            m_ReplOffsets.push_back(i);
            m_ReplTypes.push_back(m_FmtSpec[i+1]);
        }
    }

    if (m_ReplOffsets.empty() || m_ReplTypes.size() != m_ReplOffsets.size()) {
        NCBI_THROW(CInvalidDataException, eInvalidInput,
                   "Invalid format specification");
    }

    m_Fasta = (m_ReplTypes[0] == 'f');
}

bool CSeqFormatter::x_RequireData() const
{
	bool retval=false;

	ITERATE(vector<char>, fmt, m_ReplTypes) {
	switch (*fmt) {
		case 's':
		case 'h':
		case 'm':
		case 'e':
		case 'd':
		case 'b':
			retval = true;
			break;
	}
    }
    return retval;
}

void CSeqFormatter::x_Builder(vector<string>& data2write)
{
    data2write.reserve(m_ReplTypes.size());

    ITERATE(vector<char>, fmt, m_ReplTypes) {
        switch (*fmt) {

        case 's':
            data2write.push_back(m_DataExtractor.ExtractSeqData());
            break;

        case 'a':
            data2write.push_back(m_DataExtractor.ExtractAccession());
            break;

        case 'i':
            data2write.push_back(m_DataExtractor.ExtractSeqId());
            break;

        case 'g':
            data2write.push_back(m_DataExtractor.ExtractGi());
            break;

        case 'o':
            data2write.push_back(m_DataExtractor.ExtractOid());
            break;

        case 't':
            data2write.push_back(m_DataExtractor.ExtractTitle());
            break;

        case 'h':
            data2write.push_back(m_DataExtractor.ExtractHash());
            break;

        case 'l':
            data2write.push_back(m_DataExtractor.ExtractSeqLen());
            break;

        case 'T':
            data2write.push_back(m_DataExtractor.ExtractTaxId());
            break;

        case 'P':
            data2write.push_back(m_DataExtractor.ExtractPig());
            break;

        case 'L':
            data2write.push_back(m_DataExtractor.ExtractCommonTaxonomicName());
            break;

        case 'B':
            data2write.push_back(m_DataExtractor.ExtractBlastName());
            break;

        case 'K':
            data2write.push_back(m_DataExtractor.ExtractSuperKingdom());
            break;

        case 'S':
            data2write.push_back(m_DataExtractor.ExtractScientificName());
            break;

        case 'm':
            data2write.push_back(m_DataExtractor.ExtractMaskingData());
            break;

        case 'e':
            data2write.push_back(m_DataExtractor.ExtractMembershipInteger());
            break;

        case 'n':
            data2write.push_back(m_DataExtractor.ExtractLinksInteger());
            break;

        case 'd':
            data2write.push_back(m_DataExtractor.ExtractAsn1Defline());
            break;

        case 'b':
            data2write.push_back(m_DataExtractor.ExtractAsn1Bioseq());
            break;

        default:
            CNcbiOstrstream os;
            os << "Unrecognized format specification: '%" << *fmt << "'";
            NCBI_THROW(CInvalidDataException, eInvalidInput, 
                       CNcbiOstrstreamToString(os));
        }
    }
}

void CSeqFormatter::Write(CBlastDBSeqId& id)
{
    if (m_Fasta) {
        m_Out << m_DataExtractor.ExtractFasta(id);
        return;
    }

    bool get_data = x_RequireData();
    m_DataExtractor.SetSeqId(id, get_data);
    vector<string> data2write;
    x_Builder(data2write);
    m_Out << x_Replacer(data2write) << endl;
}

static string s_GetTitle(CConstRef<CBioseq> bioseq)
{
    ITERATE(CSeq_descr::Tdata, desc, bioseq->GetDescr().Get()) {
        if ((*desc)->Which() == CSeqdesc::e_Title) {
            return (*desc)->GetTitle();
        }
    }
    return string();
}

static void s_ReplaceCtrlAsInTitle(CRef<CBioseq> bioseq)
{
    static const string kTarget(" >gi|");
    static const string kCtrlA = string(1, '\001') + string("gi|");
    NON_CONST_ITERATE(CSeq_descr::Tdata, desc, bioseq->SetDescr().Set()) {
        if ((*desc)->Which() == CSeqdesc::e_Title) {
            NStr::ReplaceInPlace((*desc)->SetTitle(), kTarget, kCtrlA);
            break;
        }
    }
}

void CSeqFormatter::DumpAll(CSeqDB& blastdb, CSeqFormatterConfig config)
{
    CFastaOstream fasta(m_Out);
    fasta.SetWidth(config.m_LineWidth);
    fasta.SetAllFlags(CFastaOstream::fKeepGTSigns|CFastaOstream::fNoExpensiveOps);

    CRef<CBioseq> bioseq;
    for (int i=0; blastdb.CheckOrFindOID(i); i++) {
         bioseq.Reset(blastdb.GetBioseq(i));
         if (bioseq.Empty()) {
             continue;
         }
         // TODO: remove gnl|BL_ORD_ID
         CRef<CSeq_id> id(*(bioseq->GetId().begin()));
         if (id->IsGeneral() &&
             id->GetGeneral().GetDb() == "BL_ORD_ID") {
             m_Out << ">" << s_GetTitle(bioseq) << endl;
             CScope scope(*CObjectManager::GetInstance());
             fasta.WriteSequence(scope.AddBioseq(*bioseq));
             continue;
         }
         if (config.m_UseCtrlA) {
             s_ReplaceCtrlAsInTitle(bioseq);
         }
         fasta.Write(*bioseq, 0, true);
    }
}

/// Auxiliary functor to compute the length of a string
struct StrLenAdd : public binary_function<SIZE_TYPE, const string&, SIZE_TYPE>
{
    SIZE_TYPE operator() (SIZE_TYPE a, const string& b) const {
        return a + b.size();
    }
};

string
CSeqFormatter::x_Replacer(const vector<string>& data2write) const
{
    SIZE_TYPE data2write_size = accumulate(data2write.begin(), data2write.end(),
                                           0, StrLenAdd());

    string retval;
    retval.reserve(m_FmtSpec.size() + data2write_size -
                   (m_ReplTypes.size() * 2));

    SIZE_TYPE fmt_idx = 0;
    for (SIZE_TYPE i = 0, kSize = m_ReplOffsets.size(); i < kSize; i++) {
        retval.append(&m_FmtSpec[fmt_idx], &m_FmtSpec[m_ReplOffsets[i]]);
        retval.append(data2write[i]);
        fmt_idx = m_ReplOffsets[i] + 2;
    }
    if (fmt_idx <= m_FmtSpec.size()) {
        retval.append(&m_FmtSpec[fmt_idx], &m_FmtSpec[m_FmtSpec.size()]);
    }

    return retval;
}

void CSeqFormatter::SetConfig(TSeqRange range, objects::ENa_strand strand,
							  int filt_algo_id)
{
	m_DataExtractor.SetConfig(range, strand, filt_algo_id);
}


END_NCBI_SCOPE
