/* $Id$
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
* ===========================================================================
*
* Author:  Ilya Dondoshansky
*
* ===========================================================================
*/

/// @file: tabular.cpp
/// Formatting of pairwise sequence alignments in tabular form.
/// One line is printed for each alignment with tab-delimited fielalnVec. 

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objtools/blast_format/tabular.hpp>
#include <objtools/blast_format/blastfmtutil.hpp>
#include <objtools/blast_format/showdefline.hpp>
#include <serial/iterator.hpp>
#include <objects/general/Object_id.hpp>
#include <objmgr/util/sequence.hpp>

#include <objects/blastdb/Blast_def_line.hpp>
#include <objects/blastdb/Blast_def_line_set.hpp>

#include <sstream>
#include <map>

/** @addtogroup BlastFormatting
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

/// Structure to store the format specification strings, their description and
/// their corresponding enumeration
struct SFormatSpec {
    /// Format specification name
    string name;
    /// A description of what the above name represents
    string description;
    /// Enumeration that corresponds to this field
    CBlastTabularInfo::ETabularField field;

    /// Constructor
    /// @param n format specification name [in]
    /// @param d format specification description [in]
    /// @param f enumeration value [in]
    SFormatSpec(string n, string d, CBlastTabularInfo::ETabularField f)
        : name(n), description(d), field(f) {}
};

/// Static array of possible format specifications
static SFormatSpec sc_FormatSpecifiers[] = {
    SFormatSpec("qseqid",   
                "Query Seq-id",
                CBlastTabularInfo::eQuerySeqId),
    SFormatSpec("qgi",
                "Query GI",
                CBlastTabularInfo::eQueryGi),
    SFormatSpec("qacc",
                "Query accesion",
                CBlastTabularInfo::eQueryAccession),
    SFormatSpec("sseqid",
                "Subject Seq-id",
                CBlastTabularInfo::eSubjectSeqId),
    SFormatSpec("sallseqid",
                "All subject Seq-id(s), separated by a ';'", 
                CBlastTabularInfo::eSubjectAllSeqIds),
    SFormatSpec("sgi",
                "Subject GI", 
                CBlastTabularInfo::eSubjectGi),
    SFormatSpec("sallgi",
                "All subject GIs", 
                CBlastTabularInfo::eSubjectAllGis),
    SFormatSpec("sacc",
                "Subject accession", 
                CBlastTabularInfo::eSubjectAccession),
    SFormatSpec("sallacc", 
                "All subject accessions", 
                CBlastTabularInfo::eSubjectAllAccessions),
    SFormatSpec("qstart",
                "Start of alignment in query", 
                CBlastTabularInfo::eQueryStart),
    SFormatSpec("qend",
                "End of alignment in query", 
                CBlastTabularInfo::eQueryEnd),
    SFormatSpec("sstart", 
                "Start of alignment in subject", 
                CBlastTabularInfo::eSubjectStart),
    SFormatSpec("send",
                "End of alignment in subject", 
                CBlastTabularInfo::eSubjectEnd),
    SFormatSpec("qseq",
                "Aligned part of query sequence",
                CBlastTabularInfo::eQuerySeq),
    SFormatSpec("sseq",
                "Aligned part of subject sequence", 
                CBlastTabularInfo::eSubjectSeq),
    SFormatSpec("evalue", 
                "Expect value", 
                CBlastTabularInfo::eEvalue),
    SFormatSpec("bitscore", 
                "Bit score", 
                CBlastTabularInfo::eBitScore),
    SFormatSpec("score",
                "Raw score", 
                CBlastTabularInfo::eScore),
    SFormatSpec("length", 
                "Alignment length", 
                CBlastTabularInfo::eAlignmentLength),
    SFormatSpec("pident",
                "Percentage of identical matches", 
                CBlastTabularInfo::ePercentIdentical),
    SFormatSpec("nident",
                "Number of identical matches", 
                CBlastTabularInfo::eNumIdentical),
    SFormatSpec("mismatch",
                "Number of mismatches", 
                CBlastTabularInfo::eMismatches),
    SFormatSpec("positive", 
                "Number of positive-scoring matches", 
                CBlastTabularInfo::ePositives),
    SFormatSpec("gapopen", 
                "Number of gap openings", 
                CBlastTabularInfo::eGapOpenings),
    SFormatSpec("gaps",
                "Total number of gaps", 
                CBlastTabularInfo::eGaps),
    SFormatSpec("ppos",
                "Percentage of positive-scoring matches", 
                CBlastTabularInfo::ePercentPositives),
    SFormatSpec("frames",   
                "Query and subject frames separated by a '/'", 
                CBlastTabularInfo::eFrames),
    SFormatSpec("qframe", 
                "Query frame", 
                CBlastTabularInfo::eQueryFrame),
    SFormatSpec("sframe",   
                "Subject frame", 
                CBlastTabularInfo::eSubjFrame)
};
/// Number of elements in the array above
static const size_t kNumFormatSpecifiers = 
    (sizeof(sc_FormatSpecifiers)/sizeof(*sc_FormatSpecifiers));

// Definition for default format specification
const string
CBlastTabularInfo::kDefaultFormat =
    "qseqid sallseqid pident length mismatch gapopen qstart qend sstart send "
    "evalue bitscore";

/// This keyword is equivalent to specifying the kDefaultFormat string
static const string kDefaultKeyword("std");

string CBlastTabularInfo::DescribeFormatSpecifiers()
{
    ostringstream os;
    for (size_t i = 0; i < kNumFormatSpecifiers; i++) {
        os << "\t" << setw(10) << sc_FormatSpecifiers[i].name << " means ";
        os << sc_FormatSpecifiers[i].description << "\n";
    }
    os << "When not provided, the default value is:\n";
    os << "'" << CBlastTabularInfo::kDefaultFormat << "', which is equivalent ";
    os << "to the keyword '" << kDefaultKeyword << "'";
    return os.str();
}

void 
CBlastTabularInfo::x_AddDefaultFieldsToShow()
{
    vector<string> format_tokens;
    NStr::Tokenize(CBlastTabularInfo::kDefaultFormat, " ", format_tokens);
    ITERATE (vector<string>, iter, format_tokens) {
        _ASSERT(m_FieldMap.count(*iter) > 0);
        x_AddFieldToShow(m_FieldMap[*iter]);
    }
}

void CBlastTabularInfo::x_SetFieldsToShow(const string& format)
{
    for (size_t i = 0; i < kNumFormatSpecifiers; i++) {
        m_FieldMap.insert(make_pair(sc_FormatSpecifiers[i].name,
                                    sc_FormatSpecifiers[i].field));
    }
    
    vector<string> format_tokens;
    NStr::Tokenize(format, " ", format_tokens);

    if (format_tokens.empty())
        x_AddDefaultFieldsToShow();

    ITERATE (vector<string>, iter, format_tokens) {
        if (*iter == kDefaultKeyword)
            x_AddDefaultFieldsToShow();
        else if ((*iter)[0] == '-') {
            string field = (*iter).substr(1);
            if (m_FieldMap.count(field) > 0) 
                x_DeleteFieldToShow(m_FieldMap[field]);
        } else {
            if (m_FieldMap.count(*iter) > 0)
                x_AddFieldToShow(m_FieldMap[*iter]);
        }
    }

    if (m_FieldsToShow.empty()) {
        x_AddDefaultFieldsToShow();
    }
}

void CBlastTabularInfo::x_ResetFields()
{
    m_Score = m_AlignLength = m_NumGaps = m_NumGapOpens = m_NumIdent =
        m_NumPositives = m_QueryStart = m_QueryEnd = m_SubjectStart = 
        m_SubjectEnd = 0; 
    m_BitScore = NcbiEmptyString;
    m_Evalue = NcbiEmptyString;
    m_QuerySeq = NcbiEmptyString;
    m_SubjectSeq = NcbiEmptyString;
}

void CBlastTabularInfo::x_SetFieldDelimiter(EFieldDelimiter delim)
{
    switch (delim) {
    case eSpace: m_FieldDelimiter = ' '; break;
    case eComma: m_FieldDelimiter = ','; break;
    default: m_FieldDelimiter = '\t'; break; // eTab or unsupported value
    }
}

CBlastTabularInfo::CBlastTabularInfo(CNcbiOstream& ostr, const string& format,
                                     EFieldDelimiter delim, 
                                     bool parse_local_ids)
    : m_Ostream(ostr) 
{
    x_SetFieldsToShow(format);
    x_ResetFields();
    x_SetFieldDelimiter(delim);
    SetParseLocalIds(parse_local_ids);
}

CBlastTabularInfo::~CBlastTabularInfo()
{
    m_Ostream.flush();
}

static string 
s_GetSeqIdListString(const list<CRef<CSeq_id> >& id, 
                     CBlastTabularInfo::ESeqIdType id_type)
{
    string id_str = NcbiEmptyString;

    switch (id_type) {
    case CBlastTabularInfo::eFullId:
        id_str = CShowBlastDefline::GetSeqIdListString(id, true);
        break;
    case CBlastTabularInfo::eAccession:
    {
        CConstRef<CSeq_id> accid = FindBestChoice(id, CSeq_id::Score);
        accid->GetLabel(&id_str, CSeq_id::eContent, 0);
        break;
    }
    case CBlastTabularInfo::eGi:
        id_str = NStr::IntToString(FindGi(id));
        break;
    default: break;
    }

    if (id_str == NcbiEmptyString)
        id_str = "Unknown";

    return id_str;
}

void CBlastTabularInfo::x_PrintQuerySeqId()
{
    m_Ostream << s_GetSeqIdListString(m_QueryId, eFullId);
}

void CBlastTabularInfo::x_PrintQueryGi()
{
    m_Ostream << s_GetSeqIdListString(m_QueryId, eGi);
}

void CBlastTabularInfo::x_PrintQueryAccession()
{
    m_Ostream << s_GetSeqIdListString(m_QueryId, eAccession);
}

void CBlastTabularInfo::x_PrintSubjectSeqId()
{
    m_Ostream << s_GetSeqIdListString(m_SubjectIds[0], eFullId);
}

void CBlastTabularInfo::x_PrintSubjectAllSeqIds(void)
{
    ITERATE(vector<list<CRef<CSeq_id> > >, iter, m_SubjectIds) {
        if (iter != m_SubjectIds.begin())
            m_Ostream << ";";
        m_Ostream << s_GetSeqIdListString(*iter, eFullId); 
    }
}

void CBlastTabularInfo::x_PrintSubjectGi(void)
{
    m_Ostream << s_GetSeqIdListString(m_SubjectIds[0], eGi);
}

void CBlastTabularInfo::x_PrintSubjectAllGis(void)
{
    ITERATE(vector<list<CRef<CSeq_id> > >, iter, m_SubjectIds) {
        if (iter != m_SubjectIds.begin())
            m_Ostream << ";";
        m_Ostream << s_GetSeqIdListString(*iter, eGi); 
    }
}

void CBlastTabularInfo::x_PrintSubjectAccession(void)
{
    m_Ostream << s_GetSeqIdListString(m_SubjectIds[0], eAccession);
}

void CBlastTabularInfo::x_PrintSubjectAllAccessions(void)
{
    ITERATE(vector<list<CRef<CSeq_id> > >, iter, m_SubjectIds) {
        if (iter != m_SubjectIds.begin())
            m_Ostream << ";";
        m_Ostream << s_GetSeqIdListString(*iter, eAccession); 
    }
}

void CBlastTabularInfo::SetQueryId(const CBioseq_Handle& bh)
{
    m_QueryId.clear();

    // Create a new list of Seq-ids, substitute any local ids by new fake local 
    // ids, with label set to the first token of this Bioseq's title.
    ITERATE(CBioseq_Handle::TId, itr, bh.GetId()) {
        CRef<CSeq_id> next_id(new CSeq_id());

        string id_token;
        // Local ids are usually fake. If a title exists, use the first token
        // of the title instead of the local id. If no title or if the local id
        // should be parsed, use the local id, but without the "lcl|" prefix.
        if (itr->GetSeqId()->IsLocal()) {
            vector<string> title_tokens;
            title_tokens = 
                NStr::Tokenize(sequence::GetTitle(bh), " ", title_tokens);
            if(title_tokens.empty()){
                id_token = NcbiEmptyString;
            } else {
                id_token = title_tokens[0];
            }
            
            if (id_token == NcbiEmptyString || m_ParseLocalIds) {
                const CObject_id& obj_id = itr->GetSeqId()->GetLocal();
                if (obj_id.IsStr())
                    id_token = obj_id.GetStr();
                else 
                    id_token = NStr::IntToString(obj_id.GetId());
            }
            CObject_id* obj_id = new CObject_id();
            obj_id->SetStr(id_token);
            next_id->SetLocal(*obj_id);
        } else {
            next_id->Assign(*itr->GetSeqId());
        }
        m_QueryId.push_back(next_id);
    }
}

void CBlastTabularInfo::SetSubjectId(const CBioseq_Handle& bh)
{
    m_SubjectIds.clear();

    // Check if this Bioseq handle contains a Blast-def-line-set object.
    // If it does, retrieve Seq-ids from all redundant sequences, and 
    // print them separated by commas.
    // Retrieve the CBlast_def_line_set object and save in a CRef, preventing
    // its destruction; then extract the list of CBlast_def_line objects.
    const CRef<CBlast_def_line_set> bdlRef = 
        CBlastFormatUtil::GetBlastDefline(bh);
    const list< CRef< CBlast_def_line > >& bdl = bdlRef->Get();
    
    if (!bdl.empty()) {
        ITERATE(list<CRef<CBlast_def_line> >, defl_iter, bdl) {
            m_SubjectIds.push_back((*defl_iter)->GetSeqid());
        }
    } else {
        // Blast-def-line is not filled, hence retrieve all Seq-ids directly 
        // from the Bioseq handle's Seq-id.
        list<CRef<CSeq_id> > next_seqid_list;
        CShowBlastDefline::GetSeqIdList(bh, next_seqid_list);
        m_SubjectIds.push_back(next_seqid_list);
    }
}

int CBlastTabularInfo::SetFields(const CSeq_align& align, 
                                 CScope& scope, 
                                 CBlastFormattingMatrix* matrix)
{
    const int kQueryRow = 0;
    const int kSubjectRow = 1;

    int score, num_ident;
    double bit_score;
    double evalue;
    int sum_n;
    list<int> use_this_gi;
    
    // First reset all fields.
    x_ResetFields();

    CBlastFormatUtil::GetAlnScores(align, score, bit_score, evalue, sum_n, 
                                   num_ident, use_this_gi);
    SetScores(score, bit_score, evalue);

    bool query_is_na = false, subject_is_na = false;
    bool bioseqs_found = true;
    // Extract the full query id from the correspondintg Bioseq handle.
    try {
        const CBioseq_Handle& query_bh = 
            scope.GetBioseqHandle(align.GetSeq_id(0));
        SetQueryId(query_bh);
        query_is_na = query_bh.IsNa();
    } catch (const CException&) {
        list<CRef<CSeq_id> > query_ids;
        CRef<CSeq_id> id(new CSeq_id());
        id->Assign(align.GetSeq_id(0));
        query_ids.push_back(id);
        SetQueryId(query_ids);
        bioseqs_found = false;
    }

    // Extract the full list of subject ids
    try {
        const CBioseq_Handle& subject_bh = 
            scope.GetBioseqHandle(align.GetSeq_id(1));
        SetSubjectId(subject_bh);
        subject_is_na = subject_bh.IsNa();
    } catch (const CException&) {
        list<CRef<CSeq_id> > subject_ids;
        CRef<CSeq_id> id(new CSeq_id());
        id->Assign(align.GetSeq_id(1));
        subject_ids.push_back(id);
        SetSubjectId(subject_ids);
        bioseqs_found = false;
    }

    // If Bioseq has not been found for one or both of the sequences, all
    // subsequent computations cannot proceed. Hence don't set any of the other 
    // fields.
    if (!bioseqs_found)
        return -1;

    CRef<CSeq_align> finalAln(0);
   
    // Convert Std-seg and Dense-diag alignments to Dense-seg.
    // Std-segs are produced only for translated searches; Dense-diags only for 
    // ungapped, not translated searches.
    const bool kTranslated = align.GetSegs().IsStd();

    if (kTranslated) {
        CRef<CSeq_align> densegAln = align.CreateDensegFromStdseg();
        // When both query and subject are translated, i.e. tblastx, convert
        // to a special type of Dense-seg.
        if (query_is_na && subject_is_na)
            finalAln = densegAln->CreateTranslatedDensegFromNADenseg();
        else
            finalAln = densegAln;
    } else if (align.GetSegs().IsDendiag()) {
        finalAln = CBlastFormatUtil::CreateDensegFromDendiag(align);
    }

    const CDense_seg& ds = (finalAln ? finalAln->GetSegs().GetDenseg() :
                            align.GetSegs().GetDenseg());

    CAlnVec alnVec(ds, scope);
    
    int align_length, num_gaps, num_gap_opens;
    CBlastFormatUtil::GetAlignLengths(alnVec, align_length, num_gaps, 
                                      num_gap_opens);

    // Do not trust the identities count in the Seq-align, because if masking 
    // was used, then masked residues were not counted as identities. 
    // Hence retrieve the sequences present in the alignment and count the 
    // identities again.
    alnVec.SetGapChar('-');
    alnVec.GetWholeAlnSeqString(0, m_QuerySeq);
    alnVec.GetWholeAlnSeqString(1, m_SubjectSeq);

    // Do not trust the number of identities saved in the Seq-align.
    // Recalculate it again.
    num_ident = 0;
    int num_positives = 0;
    // The query and subject sequence strings must be the same size in a correct
    // alignment, but if alignment extends beyond the end of sequence because of
    // a bug, one of the sequence strings may be truncated, hence it is 
    // necessary to take a minimum here.
    /// @todo FIXME: Should an exception be thrown instead? 
    for (unsigned int i = 0; i < min(m_QuerySeq.size(), m_SubjectSeq.size()); 
         ++i) {
        if (m_QuerySeq[i] == m_SubjectSeq[i]) {
            ++num_ident;
            ++num_positives;
        } else if (matrix && 
                   (*matrix)(m_QuerySeq[i], m_SubjectSeq[i]) > 0) {
            ++num_positives;
        }
    }

   
    int q_start, q_end, s_start, s_end;

    // For translated search, for a negative query frame, reverse its start and
    // end offsets.
    if (kTranslated && ds.GetSeqStrand(kQueryRow) == eNa_strand_minus) {
        q_start = alnVec.GetSeqStop(kQueryRow) + 1;
        q_end = alnVec.GetSeqStart(kQueryRow) + 1;
    } else {
        q_start = alnVec.GetSeqStart(kQueryRow) + 1;
        q_end = alnVec.GetSeqStop(kQueryRow) + 1;
    }

    // If subject is on a reverse strand, reverse its start and end offsets.
    // Also do that for a nucleotide-nucleotide search, if query is on the 
    // reverse strand, because BLAST output always reverses subject, not query.
    if (ds.GetSeqStrand(kSubjectRow) == eNa_strand_minus ||
        (!kTranslated && ds.GetSeqStrand(kQueryRow) == eNa_strand_minus)) {
        s_end = alnVec.GetSeqStart(kSubjectRow) + 1;
        s_start = alnVec.GetSeqStop(kSubjectRow) + 1;
    } else {
        s_start = alnVec.GetSeqStart(kSubjectRow) + 1;
        s_end = alnVec.GetSeqStop(kSubjectRow) + 1;
    }
    
    int query_frame = 1, subject_frame = 1;
    if (kTranslated) {
        query_frame = CBlastFormatUtil::
            GetFrame (q_start - 1, ds.GetSeqStrand(kQueryRow), 
                      scope.GetBioseqHandle(align.GetSeq_id(0)));
    
        subject_frame = CBlastFormatUtil::
            GetFrame (s_start - 1, ds.GetSeqStrand(kSubjectRow), 
                      scope.GetBioseqHandle(align.GetSeq_id(1)));

    }
    SetCounts(num_ident, align_length, num_gaps, num_gap_opens, num_positives,
              query_frame, subject_frame);

    SetEndpoints(q_start, q_end, s_start, s_end);

    return 0;
}

void CBlastTabularInfo::Print() 
{
    ITERATE(list<ETabularField>, iter, m_FieldsToShow) {
        // Add tab in front of field, except for the first field.
        if (iter != m_FieldsToShow.begin())
            m_Ostream << m_FieldDelimiter;
        x_PrintField(*iter);
    }
    m_Ostream << "\n";
}

void CBlastTabularInfo::x_PrintFieldNames()
{
    m_Ostream << "# Fields: ";

    ITERATE(list<ETabularField>, iter, m_FieldsToShow) {
        if (iter != m_FieldsToShow.begin())
            m_Ostream << ", ";

        switch (*iter) {
        case eQuerySeqId:
            m_Ostream << "query id"; break;
        case eQueryGi:
            m_Ostream << "query gi"; break;
        case eQueryAccession:
            m_Ostream << "query acc."; break;
        case eSubjectSeqId:
            m_Ostream << "subject id"; break;
        case eSubjectAllSeqIds:
            m_Ostream << "subject ids"; break;
        case eSubjectGi:
            m_Ostream << "subject gi"; break;
        case eSubjectAllGis:
            m_Ostream << "subject gis"; break;
        case eSubjectAccession:
            m_Ostream << "subject acc."; break;
        case eSubjectAllAccessions:
            m_Ostream << "subject accs."; break;
        case eQueryStart:
            m_Ostream << "q. start"; break;
        case eQueryEnd:
            m_Ostream << "q. end"; break;
        case eSubjectStart:
            m_Ostream << "s. start"; break;
        case eSubjectEnd:
            m_Ostream << "s. end"; break;
        case eQuerySeq:
            m_Ostream << "query seq"; break;
        case eSubjectSeq:
            m_Ostream << "subject seq"; break;
        case eEvalue:
            m_Ostream << "evalue"; break;
        case eBitScore:
            m_Ostream << "bit score"; break;
        case eScore:
            m_Ostream << "score"; break;
        case eAlignmentLength:
            m_Ostream << "alignment length"; break;
        case ePercentIdentical:
            m_Ostream << "% identity"; break;
        case eNumIdentical:
            m_Ostream << "identical"; break;
        case eMismatches:
            m_Ostream << "mismatches"; break;
        case ePositives:
            m_Ostream << "positives"; break;
        case eGapOpenings:
            m_Ostream << "gap opens"; break;
        case eGaps:
            m_Ostream << "gaps"; break;
        case ePercentPositives:
            m_Ostream << "% positives"; break;
        case eFrames:
            m_Ostream << "query/sbjct frames"; break; 
        case eQueryFrame:
            m_Ostream << "query frame"; break; 
        case eSubjFrame:
            m_Ostream << "sbjct frame"; break; 
        default:
            break;
        }
    }

    m_Ostream << "\n";
}

void 
CBlastTabularInfo::PrintHeader(const string& program_in, 
       const CBioseq& bioseq, 
       const string& dbname, 
       const string& rid /* = kEmptyStr */,
       unsigned int iteration /* = numeric_limits<unsigned int>::max() */,
       const CSeq_align_set* align_set /* = 0 */,
       CConstRef<CBioseq> subj_bioseq /* = CConstRef<CBioseq>() */)
{
    m_Ostream << "# ";
    string program(program_in);
    CBlastFormatUtil::BlastPrintVersionInfo(NStr::ToUpper(program), false, 
                                            m_Ostream);

    if (iteration != numeric_limits<unsigned int>::max())
        m_Ostream << "# Iteration: " << iteration << "\n";

    const size_t kLineLength(0);
    const bool kBelieveQuery(false);
    const bool kHtmlFormat(false);
    const bool kTabularFormat(true);

    // Print the query defline with no html; there is no need to set the 
    // line length restriction, since it's ignored for the tabular case.
    CBlastFormatUtil::AcknowledgeBlastQuery(bioseq, kLineLength, m_Ostream, 
                                            kBelieveQuery, kHtmlFormat,
                                            kTabularFormat, rid);
    
    if (dbname != kEmptyStr) {
        m_Ostream << "\n# Database: " << dbname << "\n";
    } else {
        _ASSERT(subj_bioseq.NotEmpty());
        m_Ostream << "\n";
        CBlastFormatUtil::AcknowledgeBlastSubject(*subj_bioseq, kLineLength,
                                                  m_Ostream, kBelieveQuery,
                                                  kHtmlFormat, kTabularFormat);
        m_Ostream << "\n";
    }

    x_PrintFieldNames();
    
    // Print number of alignments found, but only if it has been set.
    if (align_set)
    {
       int num_hits = align_set->Get().size();
       m_Ostream << "# " << num_hits << " hits found" << "\n";
    }
}

void CBlastTabularInfo::PrintNumProcessed(int num_queries)
{
    m_Ostream << "# BLAST processed " << num_queries << " queries\n";
}


END_NCBI_SCOPE

/* @} */
