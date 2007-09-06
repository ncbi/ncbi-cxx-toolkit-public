/*
*  $Id$
*
* =========================================================================
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
*  Government do not and cannt warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* =========================================================================
*
*  Author: Boris Kiryutin
*
* =========================================================================
*/

#include <ncbi_pch.hpp>

#include <algo/align/prosplign/prosplign_exception.hpp>

#include "AliUtil.hpp"
#include "Ali.hpp"
#include "NSeq.hpp"
#include "nucprot.hpp"
#include "intron.hpp"

#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(prosplign)

int CAliUtil::CutIntrons(CAli& new_ali, const CAli& ali, const CProSplignScaledScoring& scoring)
{
    int score = 0;
    vector<CAliPiece>::const_iterator it;
    vector<pair<int, int> > igi;
    int nulpos = 0;
    EAliPieceType prev = eSP;
    for(it = ali.m_ps.begin(); it != ali.m_ps.end(); ++it) {
        switch(it->m_type) {
            case eMP:
            case eHP:
                nulpos += it->m_len;
                break;
            case eSP:
                if(it->m_len < 2) NCBI_THROW(CProSplignException, eAliData, "intron length less than 2");
                if( (ali.cnseq->GetNuc(nulpos) == nG) && (ali.cnseq->GetNuc(nulpos+1) == nT) 
                    && (ali.cnseq->GetNuc(nulpos+it->m_len-2) == nA) && (ali.cnseq->GetNuc(nulpos+it->m_len-1) == nG) ) {
                    score -= scoring.sm_ICGT;
                } else if( (ali.cnseq->GetNuc(nulpos) == nG) && (ali.cnseq->GetNuc(nulpos+1) == nC) 
                    && (ali.cnseq->GetNuc(nulpos+it->m_len-2) == nA) && (ali.cnseq->GetNuc(nulpos+it->m_len-1) == nG) ) {
                    score -= scoring.sm_ICGC;
                } else if( (ali.cnseq->GetNuc(nulpos) == nA) && (ali.cnseq->GetNuc(nulpos+1) == nT) 
                    && (ali.cnseq->GetNuc(nulpos+it->m_len-2) == nA) && (ali.cnseq->GetNuc(nulpos+it->m_len-1) == nC) ) {
                    score -= scoring.sm_ICAT;
                } else score -= scoring.sm_ICANY;
                score -= it->m_len * scoring.ie;
                igi.push_back(make_pair(nulpos, it->m_len));
                nulpos += it->m_len;
            default :
                break;
        }
        if(it->m_type != eSP) {
            if(it->m_type == prev) new_ali.m_ps.back().m_len += it->m_len;
            else {
                new_ali.m_ps.push_back(*it);
                prev = it->m_type;
            }
        }
    }
    if(nulpos !=  ali.cnseq->size()) NCBI_THROW(CProSplignException, eAliData, "nuc. length count is wrong");
    new_ali.cnseq->Init(*ali.cnseq, igi);
    return score;
}


void CAliUtil::CheckValidity(const CAli& ali) //returns if valid, throws otherwise 
{
    size_t plen = 0, nlen = 0;
    vector<CAliPiece>::const_iterator it;
    int prev_type = -1;
    for(it = ali.m_ps.begin(); it != ali.m_ps.end(); ++it) {
        switch(it->m_type) {
            case eMP:
                if(prev_type == eMP) NCBI_THROW(CProSplignException, eAliData, "two matches(diags) in a row");
                prev_type = eMP;
                plen += it->m_len;
                nlen += it->m_len;
                break;
            case eVP:
                if(prev_type == eVP) NCBI_THROW(CProSplignException, eAliData, "two V-gaps in a row");
                prev_type = eVP;
                plen += it->m_len;
                break;
            case eHP:
                if(prev_type == eHP) NCBI_THROW(CProSplignException, eAliData, "two H-gaps in a row");
                prev_type = eHP;
                nlen += it->m_len;
                break;
            case eSP:
                if(prev_type == eSP) NCBI_THROW(CProSplignException, eAliData, "two introns in a row");
                prev_type = eSP;
                nlen += it->m_len;
                break;
            default :
                NCBI_THROW(CProSplignException, eAliData, "Unknown EAliPieceType");
        }
    }
    if(plen != ali.cpseq->seq.size()*3) NCBI_THROW(CProSplignException, eAliData, "protein length count is wrong");
    if(nlen != size_t(ali.cnseq->size())) NCBI_THROW(CProSplignException, eAliData, "nuc. length count is wrong");
}


//count scaled score ('IScore')
//divide by CScoring::sm_koef to get actual score
//mode 2 : gaps/framshifts at the end have 0 score (default)
//mode 1 : full scoring for gaps/framshifts in protein, 0 score for H-gaps/framshifts
//mode 0 : full scoring at the end
int CAliUtil::CountIScore(const CAli& ali, const CProSplignScaledScoring& scoring, int mode)
{
    //    CScoring::Init();
    CheckValidity(ali);
    CNSeq tseq;
    CAli tali(tseq, *ali.cpseq);
    //intron score
    int score = CutIntrons(tali, ali, scoring);
    return score + CountFrIScore(tali, scoring, mode);
}

//same as before, version without introns
//CHECKS THAT THERE ARE NO INTRONS 
int CAliUtil::CountFrIScore(const CAli& ali, const CProSplignScaledScoring& scoring, int mode, bool lgap, bool rgap)
{
    //    CScoring::Init();
    CheckValidity(ali);
    if(mode < 0 || mode > 2) mode = 2;
    // check for intron absence
    vector<CAliPiece>::const_iterator it;
    for(it = ali.m_ps.begin(); it != ali.m_ps.end(); ++it) {
        if(it->m_type == eSP) NCBI_THROW(CProSplignException, eAliData, "EAliPieceType::eSP found in alignment without introns");
    }
    //score
    int nulpos = 0, nultripos = 0;
    int skip, len, i, j, protpos;
    int score = 0;
    bool first = true;
    for(it = ali.m_ps.begin(); it != ali.m_ps.end(); ++it) {
        switch (it->m_type) {
            case eVP :
                if( !first || mode == 0 || mode == 1 || lgap) {
                    score -= scoring.sm_Ine * it->m_len;
                    if(it->m_len%3) score -= scoring.sm_If;
                    else score -= scoring.sm_Ig;
                }
                nultripos += it->m_len;
                break;
            case eHP :
                if(!first || mode == 0) {
                    score -= scoring.sm_Ine * it->m_len;
                    if(it->m_len%3) score -= scoring.sm_If;
                    else score -= scoring.sm_Ig;
                }
                nulpos += it->m_len;
                break;
            case eMP :
                //skip to frame
                len = it->m_len;
                skip = 3 - nultripos%3;
                if(skip == 3) skip = 0;
                if(len < skip) {
                    nultripos += len;
                    nulpos += len;
                    break;
                }
                len -= skip;
                nultripos += skip;
                _ASSERT(nultripos%3 == 0);
                nulpos += skip;
                i = len/3;
                skip = len%3;
                protpos = nultripos/3;
                //matches
                for(j=0;j<i;++j) {
                    score += matrix.MultScore(ali.cnseq->GetNuc(nulpos), ali.cnseq->GetNuc(nulpos+1), ali.cnseq->GetNuc(nulpos+2), ali.cpseq->seq[protpos++], scoring);
                    nulpos += 3;
                    nultripos += 3;
                    len -= 3;
                }
                //skip the rest
                len -= skip;
                _ASSERT(len == 0);
                nulpos += skip;
                nultripos += skip;
                break;
            case eSP : 
                NCBI_THROW(CProSplignException, eAliData, "EAliPieceType::eSP found in alignment without introns");
                break;        
            default : 
                NCBI_THROW(CProSplignException, eAliData, "unknown EAliPieceType found in alignment without introns");
                break;        
        }
        first = false;        
    }
    if(ali.m_ps.size() > 0) {//adjust score of the last gap
        if( (mode != 0 && ali.m_ps.back().m_type == eHP) ||
            (!rgap && mode == 2 && ali.m_ps.back().m_type == eVP ) ) {
                score += ali.m_ps.back().m_len * scoring.sm_Ine;
                    if(ali.m_ps.back().m_len%3) score += scoring.sm_If;
                    else score += scoring.sm_Ig;
        }
    }
    return score;
}



END_SCOPE(prosplign)
END_NCBI_SCOPE
