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
* Author:  Aaron Ucko, Aleksey Grichenko
*
* File Description:
*   Flat formatter for Sequence Alignment/Map (SAM).
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objtools/format/cigar_formatter.hpp>
#include <objtools/format/sam_formatter.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/general/Object_id.hpp>

#include <set>

#define NCBI_USE_ERRCODE_X   Objtools_Fmt_SAM

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CSAM_CIGAR_Formatter : public CCIGAR_Formatter
{
public:
    typedef CSAM_Formatter::TFlags TFlags;

    CSAM_CIGAR_Formatter(CNcbiOstream&      out,
                         const CSeq_align&  aln,
                         CScope&            scope,
                         TFlags             flags);
    virtual ~CSAM_CIGAR_Formatter(void) {}

    void SetSkipHeader(bool value = true) { m_SkipHead = value; }

protected:
    virtual void StartAlignment(void);
    virtual void EndAlignment(void);
    virtual void AddRow(const string& cigar);
    virtual void AddSegment(CNcbiOstream& cigar,
                            char seg_type,
                            TSeqPos seg_len);
private:
    enum EReadFlags {
        fRead_Default       = 0x0000,
        fRead_Reverse       = 0x0010  // minus strand
    };
    typedef unsigned int TReadFlags;

    typedef list<string> TLines;

    string x_GetRefIdString(void) const;
    string x_GetTargetIdString(void) const;
    void x_AddLines(const TLines& lines);

    CNcbiOstream&   m_Out;
    TFlags          m_Flags;
    TLines          m_Head;
    TLines          m_Rows;
    bool            m_SkipHead; // Skip headers when batch processing
    int             m_NumDif;   // count differences

    set<CBioseq_Handle> m_KnownRefSeqs; // refseqs already in the header
};


CSAM_CIGAR_Formatter::CSAM_CIGAR_Formatter(CNcbiOstream&      out,
                                           const CSeq_align&  aln,
                                           CScope&            scope,
                                           TFlags             flags)
    : CCIGAR_Formatter(aln, &scope),
      m_Out(out),
      m_Flags(flags),
      m_SkipHead(false),
      m_NumDif(0)
{
}


inline
string CSAM_CIGAR_Formatter::x_GetRefIdString(void) const
{
    // ???
    return GetRefId().AsFastaString();
}


inline
string CSAM_CIGAR_Formatter::x_GetTargetIdString(void) const
{
    // ???
    return GetTargetId().AsFastaString();
}


void CSAM_CIGAR_Formatter::x_AddLines(const TLines& lines)
{
    ITERATE(TLines, it, lines) {
        m_Out << *it << '\n';
    }
}


void CSAM_CIGAR_Formatter::StartAlignment(void)
{
    m_Head.push_back("@HD\tVN:1.2\tGO:query");
}


void CSAM_CIGAR_Formatter::EndAlignment(void)
{
    if ( !m_SkipHead ) {
        x_AddLines(m_Head);
    }
    x_AddLines(m_Rows);
    m_Head.clear();
    m_Rows.clear();
}


void CSAM_CIGAR_Formatter::AddSegment(CNcbiOstream& cigar,
                                      char seg_type,
                                      TSeqPos seg_len)
{
    if (seg_type != 'M') {
        m_NumDif += seg_len;
    }
    CCIGAR_Formatter::AddSegment(cigar, seg_type, seg_len);
}


static int GetIntScore(const CScore& score)
{
    if ( score.GetValue().IsInt() ) {
        return score.GetValue().GetInt();
    }
    return int(score.GetValue().GetReal());
}


static double GetFloatScore(const CScore& score)
{
    if ( score.GetValue().IsInt() ) {
        return score.GetValue().GetInt();
    }
    return score.GetValue().GetReal();
}


void CSAM_CIGAR_Formatter::AddRow(const string& cigar)
{
    CBioseq_Handle  refseq = GetScope()->GetBioseqHandle(GetRefId());
    if (m_KnownRefSeqs.find(refseq) == m_KnownRefSeqs.end()) {
        m_Head.push_back("@SQ\tSN:" + x_GetRefIdString() +
            "\tLN:" + NStr::UInt8ToString(refseq.GetBioseqLength()));
        m_KnownRefSeqs.insert(refseq);
    }

    string id = x_GetTargetIdString();

    TReadFlags flags = fRead_Default;
    if ( GetTargetSign() < 0 ) {
        flags |= fRead_Reverse;
    }

    const TRange& ref_rg = GetRefRange();
    const TRange& tgt_rg = GetTargetRange();
    string clip_front, clip_back;
    if (tgt_rg.GetFrom() > 0) {
        clip_front = NStr::UInt8ToString(tgt_rg.GetFrom()) + "H";
    }

    //string seq_data;
    CBioseq_Handle h;
    h = GetScope()->GetBioseqHandle(GetTargetId());
    if ( h ) {
        //CSeqVector vect = h.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
        //vect.GetSeqData(tgt_rg.GetFrom(), tgt_rg.GetTo(), seq_data);
        if ( TSeqPos(tgt_rg.GetToOpen()) < h.GetBioseqLength() ) {
            clip_back = NStr::UInt8ToString(
                h.GetBioseqLength() - tgt_rg.GetToOpen()) + "H";
        }
    }
    /*
    else {
        seq_data = string(tgt_rg.GetLength(), 'N'); // ???
    }
    */

    // Add tags
    string AS; // alignment score, int
    string EV; // expectation value, float
    string PI; // percentage identity, float
    string BS; // bit-score, int?
    const CSeq_align& aln = GetCurrentSeq_align();
    if ( aln.IsSetScore() ) {
        ITERATE(CSeq_align::TScore, score, aln.GetScore()) {
            if (!(*score)->IsSetId()  ||  !(*score)->GetId().IsStr()) continue;
            const string& id = (*score)->GetId().GetStr();
            if (m_Flags & CSAM_Formatter::fSAM_AlignmentScore) {
                if (AS.empty()  &&  id == "score") {
                    AS = "\tAS:i:" + NStr::IntToString(GetIntScore(**score));
                }
            }
            if (m_Flags & CSAM_Formatter::fSAM_ExpectationValue) {
                if (EV.empty()  &&  id == "e_value") {
                    EV = "\tEV:f:" + NStr::DoubleToString(GetFloatScore(**score));
                }
            }
            if (m_Flags & CSAM_Formatter::fSAM_BitScore) {
                if (BS.empty()  &&  id == "bit_score") {
                    BS = "\tBS:f:" + NStr::DoubleToString(GetFloatScore(**score));
                }
            }
            if (m_Flags & CSAM_Formatter::fSAM_PercentageIdentity) {
                if (PI.empty()  &&  id == "num_ident") {
                    int len = aln.GetAlignLength(false);
                    int ni = GetIntScore(**score);
                    double pi = 100.0;
                    if (ni != len) {
                        pi = min(99.99, 100.0 * ((double)ni)/((double)len));
                    }
                    PI = "\tPI:f:" + NStr::DoubleToString(pi, 2);
                }
            }
        }
    }
    string NM;
    if (m_Flags & CSAM_Formatter::fSAM_NumNucDiff) {
        NM = "\tNM:i:" + NStr::IntToString(m_NumDif);
    }
    m_Rows.push_back(
        id + "\t" +
        NStr::UIntToString(flags) + "\t" +
        x_GetRefIdString() + "\t" +
        NStr::UInt8ToString(ref_rg.GetFrom() + 1) + "\t" + // position, 1-based
        "255\t" + // ??? mapping quality
        clip_front + cigar + clip_back + "\t" +
        "*\t" + // ??? mate reference sequence
        "0\t" + // mate position, 1-based
        "0\t" + // inferred insert size
        /*seq_data + */ "*\t" +
        "*" + // query quality
        AS + EV + NM + PI + BS // tags
        );
}


CSAM_Formatter::CSAM_Formatter(CNcbiOstream& out,
                               CScope&       scope,
                               TFlags        flags)
    : m_Out(&out),
      m_Scope(&scope),
      m_Flags(flags)
{
}


CSAM_Formatter& CSAM_Formatter::Print(const CSeq_align&  aln,
                                      const CSeq_id&     query_id)
{
    CSAM_CIGAR_Formatter fmt(*m_Out, aln, *m_Scope, m_Flags);
    fmt.FormatByTargetId(query_id);
    return *this;
}


CSAM_Formatter& CSAM_Formatter::Print(const CSeq_align&  aln,
                                      CSeq_align::TDim   query_row)
{
    CSAM_CIGAR_Formatter fmt(*m_Out, aln, *m_Scope, m_Flags);
    fmt.FormatByTargetRow(query_row);
    return *this;
}


CSAM_Formatter& CSAM_Formatter::Print(const CSeq_align_set& aln_set,
                                      const CSeq_id&        query_id)
{
    CSeq_align disc;
    disc.SetType(CSeq_align::eType_disc);
    disc.SetSegs().SetDisc().Assign(aln_set);
    Print(disc, query_id);
    return *this;
}


CSAM_Formatter& CSAM_Formatter::Print(const CSeq_align_set& aln_set,
                                      CSeq_align::TDim      query_row)
{
    CSeq_align disc;
    disc.SetType(CSeq_align::eType_disc);
    disc.SetSegs().SetDisc().Assign(aln_set);
    Print(disc, query_row);
    return *this;
}


END_SCOPE(objects)
END_NCBI_SCOPE
