/* $Id$
* ===========================================================================
*
*                            public DOMAIN NOTICE                          
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
* Author:  Yuri Kapustin
*
* File Description:
*
*/

#include <ncbi_pch.hpp>
#include <algo/align/util/blast_tabular.hpp>
#include <algo/align/util/algo_align_util_exceptions.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/Object_id.hpp>
#include <corelib/ncbiutil.hpp>

#include <numeric>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//////////////////////////////////////////////////////////////////////////////
// explicit specializations


CBlastTabular::CBlastTabular(const CSeq_align& seq_align, bool save_xcript):
    TParent(seq_align, save_xcript)
{
    const CDense_seg &ds = seq_align.GetSegs().GetDenseg();
    const CDense_seg::TLens& lens = ds.GetLens();
    const CDense_seg::TStarts& starts = ds.GetStarts();
    TCoord spaces = 0, gaps = 0, aln_len = 0;
    for(size_t i = 0, dim = lens.size(); i < dim; ++i) {
        if(starts[2*i] == -1 || starts[2*i+1] == -1) {
            ++gaps;
            spaces += lens[i];
        }
        aln_len += lens[i];
    }

    SetLength(aln_len);
    SetGaps(gaps);

    double matches;
    if(seq_align.GetNamedScore("num_ident", matches) == false) {
        matches = aln_len - spaces; // just an estimate
    }
    SetIdentity(float(matches/aln_len));

    SetMismatches(aln_len - spaces - TCoord(matches));

    double score;
    if(seq_align.GetNamedScore("bit_score", score) == false) {
        score = 2.f * aln_len;
    }
    SetScore(float(score));

    double evalue;
    if(seq_align.GetNamedScore("e_value", evalue) == false) {
        evalue = 0;
    }
    SetEValue(evalue);
}


CBlastTabular::CBlastTabular(const TId& idquery, TCoord qstart, bool qstrand,
                             const TId& idsubj, TCoord sstart, bool sstrand,
                             const string& xcript):
    TParent(idquery, qstart, qstrand, idsubj, sstart, sstrand, xcript)
{
    m_Length = xcript.size();    
    m_Mismatches = m_Gaps = 0;
    bool diag (true);
    size_t matches (0);
    for(size_t i = 0; i < m_Length; ++i) {
        switch(xcript[i]) {
        case 'R': ++m_Mismatches; diag = true; break;
        case 'M': ++matches; diag = true; break;
        case 'I': case 'D': if(diag) {diag = false; ++m_Gaps; } break;
        }
    }

    SetIdentity(double(matches) / m_Length);

    m_EValue = 0;
    m_Score = 2 * matches;
}

namespace {
    class CLocalSeqIdCreator {
    public:
        CRef<CSeq_id> operator()(const string& strid)
        {
            CRef<CSeq_id> seqid;
            seqid.Reset(new CSeq_id);
            seqid->SetLocal().SetStr(strid);
            return seqid;
        }
    };

    class CLastFastaId {
    public:
        CRef<CSeq_id> operator()(const string& strid)
        {
            CRef<CSeq_id> seqid;
            CBioseq::TId ids;
            CSeq_id::ParseFastaIds(ids, strid);
            if(ids.size()) {
                seqid = ids.back();
            } else {
                seqid.Reset(NULL);
            }
            return seqid;
        }
    };

    class CBestSeqIdExtractor {
    public:
        CBestSeqIdExtractor(CBlastTabular::SCORE_FUNC score_func) :
            m_score_func(score_func) {}
        CRef<CSeq_id> operator()(const string& strid)
        {
            CRef<CSeq_id> seqid;
            CBioseq::TId ids;
            CSeq_id::ParseFastaIds(ids, strid);
            if(ids.size()) {
                seqid = FindBestChoice(ids, m_score_func);
            } else {
                seqid.Reset(NULL);
            }
            return seqid;
        }
    protected:
         CBlastTabular::SCORE_FUNC m_score_func;
    };
}

CBlastTabular::CBlastTabular(const char* m8, bool force_local_ids)
{
    if (force_local_ids)
        x_Deserialize(m8, CLocalSeqIdCreator());
    else
        x_Deserialize(m8, CLastFastaId());
}

CBlastTabular::CBlastTabular(const char* m8, SCORE_FUNC score_func)
{
    x_Deserialize(m8, CBestSeqIdExtractor(score_func));
}

template <class F>
void CBlastTabular::x_Deserialize(const char* m8, F seq_id_extractor)
{
    const char* p0 = m8, *p = p0;
    for(; *p && isspace((unsigned char)(*p)); ++p); // skip spaces
    for(p0 = p; *p && !isspace((unsigned char)(*p)); ++p); // get token
    if(*p) {
        const string id1 (p0, p - p0);
        m_Id.first = seq_id_extractor(id1);
    }

    for(; *p && isspace((unsigned char)(*p)); ++p); // skip spaces
    for(p0 = p; *p && !isspace((unsigned char)(*p)); ++p); // get token
    if(*p) {

        const string id2 (p0, p - p0);
        m_Id.second = seq_id_extractor(id2);
    }

    if(m_Id.first.IsNull() || m_Id.second.IsNull()) {
        NCBI_THROW(CAlgoAlignUtilException,
                   eFormat,
                   "Unable to recognize sequence IDs in "
                   + string(m8));
    }

    for(; *p && isspace((unsigned char)(*p)); ++p); // skip spaces

    x_PartialDeserialize(p);
}


//////////////////////////////////////////////////////////////////////////////
// getters and  setters


void CBlastTabular::SetLength(TCoord length)
{
    m_Length = length;
}



CBlastTabular::TCoord CBlastTabular::GetLength(void) const
{
    return m_Length;
}



void CBlastTabular::SetMismatches(TCoord mismatches)
{
    m_Mismatches = mismatches;
}



CBlastTabular::TCoord CBlastTabular::GetMismatches(void) const
{
    return m_Mismatches;
}



void CBlastTabular::SetGaps(TCoord gaps)
{
    m_Gaps = gaps;
}



CBlastTabular::TCoord CBlastTabular::GetGaps(void) const
{
    return m_Gaps;
}



void CBlastTabular::SetEValue(double EValue)
{
    m_EValue = EValue;
}



double CBlastTabular::GetEValue(void) const
{
    return m_EValue;
}



void CBlastTabular::SetScore(float score)
{
    m_Score = score;
}



float CBlastTabular::GetScore(void) const
{
    return m_Score;
}


void CBlastTabular::SetIdentity(float identity)
{
    m_Identity = identity;
}



float CBlastTabular::GetIdentity(void) const
{
    return m_Identity;
}


/////////////////////////////////////////////////////////////////////////////
// tabular serialization / deserialization


void CBlastTabular::x_PartialSerialize(CNcbiOstream& os) const
{
    os << 100.0 * GetIdentity() << '\t' << GetLength() << '\t'
       << GetMismatches() << '\t' << GetGaps() << '\t'
       << TParent::GetQueryStart() + 1 << '\t' 
       << TParent::GetQueryStop() + 1 << '\t'
       << TParent::GetSubjStart() + 1 << '\t' 
       << TParent::GetSubjStop() + 1 << '\t'
       << GetEValue() << '\t' << GetScore();
    if(m_Transcript.size() > 0) {
        os << '\t' << m_Transcript;
    }
}



void CBlastTabular::x_PartialDeserialize(const char* m8)
{
    CNcbiIstrstream iss (m8);
    double identity100, evalue, score;
    TCoord a, b, c, d;
    iss >> identity100 >> m_Length >> m_Mismatches >> m_Gaps
        >> a >> b >> c >> d >> evalue >> score;
    
    if(iss.fail() == false) {

        m_Identity = float(identity100 / 100.0);
        m_EValue = evalue;
        m_Score = float(score);

        if(a > 0 && b > 0 && c > 0 && d > 0) {

            SetQueryStart(a - 1);
            SetQueryStop(b - 1);
            SetSubjStart(c - 1);
            SetSubjStop(d - 1);
        }
        else {
            NCBI_THROW(CAlgoAlignUtilException, eFormat,
                      "Coordinates in m8 string are expected to be one-based: "
                       + string(m8));
        }

        m_Transcript.resize(0);
        if(iss.good())
            iss >> m_Transcript;
    }
    else {
        
        NCBI_THROW(CAlgoAlignUtilException, eFormat,
                   "Failed to init from m8 string: "
                   + string(m8));
    }
}


void CBlastTabular::Modify(Uint1 where, TCoord new_pos)
{
    const size_t trlen = GetTranscript().size();
    if(trlen > 0) {

        
 /* This is accurate but assumes that mismatches are included
    in the transcript, not just matches coding for generic diags.
    So keep it commented out for a while.

        const TCoord matches_old = TCoord(trlen * GetIdentity());
        TParent::Modify(where, new_pos);
        const TTranscript& tr_new = GetTranscript();

        TCoord gaps = 0;
        TCoord matches = 0;
        TCoord mismatches = 0;
        bool m = false, mm = false;
        TCoord mcnt = 0, mmcnt = 0;
        ITERATE(TTranscript, ii, tr_new) {

            char c = *ii;
            if(c == 'D' || c == 'I') {
                ++gaps;
                if(m) {
                    matches += mcnt == 0? 1: mcnt;
                    m = false;
                }
                if(mm) {
                    mismatches += mcnt == 0? 1: mmcnt;
                    mm = false;
                }
            }
            else if (c == 'M') {
                m = true;
                mcnt = 0;
            }
            else if (c == 'R') {
                mm = true;
                mmcnt = 0;
            }
            else if('0' <= c && c <= '9') {
                if(m) {
                    mcnt = 10*mcnt + c - '0';
                }
                if(mm) {
                    mmcnt = 10*mmcnt + c - '0';
                }
            }
            else {
                NCBI_THROW(CAlgoAlignUtilException, eInternal,
                           "Unexpected transcript symbol.");
            }
        }
        if(m) {
            matches += mcnt == 0? 1: mcnt;
        }
        if(mm) {
            mismatches += mmcnt == 0? 1: mmcnt;
        }
        SetMismatches(mismatches);
        SetGaps(gaps);
        SetLength(matches + mismatches + indels);
        SetScore(GetScore() * matches / double(matches_old));
 */

        const TCoord trlen_old = s_RunLengthDecode(GetTranscript()).size();
        TParent::Modify(where, new_pos);
        const TCoord trlen_new = s_RunLengthDecode(GetTranscript()).size();
        const double kq = double(trlen_new) / trlen_old;
        SetMismatches(TCoord(kq*GetMismatches()));
        SetGaps(TCoord(kq*GetGaps()));
        SetLength(trlen_new);
        SetScore(kq*GetScore());
    }
    else {
        const TCoord query_span_old = GetQuerySpan();
        TParent::Modify(where, new_pos);
        const TCoord query_span_new = GetQuerySpan();
        const double kq = double(query_span_new) / query_span_old;
        SetMismatches(TCoord(kq*GetMismatches()));
        SetGaps(TCoord(kq*GetGaps()));
        SetLength(TCoord(kq*GetLength()));
        SetScore(kq*GetScore());
    }
}

END_NCBI_SCOPE

