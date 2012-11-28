/*  $Id$
  ===========================================================================
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
 * Authors:  Alexandre Souvorov
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <algo/gnomon/gnomon_model.hpp>
#include <algo/gnomon/aligncollapser.hpp>
#include <algo/gnomon/id_handler.hpp>


BEGIN_SCOPE(ncbi)
BEGIN_SCOPE(gnomon)


CAlignModel CAlignCommon::GetAlignment(const SAlignIndividual& ali, const deque<char>& target_id_pool) const {

    CGeneModel a(isPlus() ? ePlus : eMinus, 0, isEST() ? CGeneModel::eEST : CGeneModel::eSR);
    if(isPolyA()) 
        a.Status() |= CGeneModel::ePolyA;
    if(isCap())
        a.Status() |= CGeneModel::eCap;
    if(isUnknown()) 
        a.Status()|= CGeneModel::eUnknownOrientation;
    a.SetID(ali.m_align_id);
    a.SetWeight(ali.m_weight);

    if(m_introns.empty()) {
        a.AddExon(ali.m_range);
    } else {
        a.AddExon(TSignedSeqRange(ali.m_range.GetFrom(), m_introns.front().GetFrom()));
        for(int i = 0; i < (int)m_introns.size()-1; ++i) 
            a.AddExon(TSignedSeqRange(m_introns[i].GetTo(), m_introns[i+1].GetFrom()));
        a.AddExon(TSignedSeqRange(m_introns.back().GetTo(), ali.m_range.GetTo()));
    }

    CAlignMap amap(a.Exons(), a.FrameShifts(), a.Strand());
    CAlignModel align(a, amap);

    string target;
    for(int i = ali.m_target_id; target_id_pool[i] != 0; ++i)
        target.push_back(target_id_pool[i]);

    CRef<CSeq_id> target_id(CIdHandler::ToSeq_id(target));
    align.SetTargetId(*target_id);

    return align;
};

bool LeftAndLongFirstOrder(const SAlignIndividual& a, const SAlignIndividual& b) {  // left and long first
        if(a.m_range == b.m_range)
            return a.m_target_id < b.m_target_id;
        else if(a.m_range.GetFrom() != b.m_range.GetFrom())
            return a.m_range.GetFrom() < b.m_range.GetFrom();
        else
            return a.m_range.GetTo() > b.m_range.GetTo();
}

bool OriginalOrder(const SAlignIndividual& a, const SAlignIndividual& b) {  // the order in which alignmnets were added
    return a.m_target_id < b.m_target_id;
}



CAlignCommon::CAlignCommon(const CGeneModel& align) {

    m_flags = 0;
    if(align.Type()&CGeneModel::eSR)
        m_flags |= esr;
    if(align.Type()&CGeneModel::eEST)
        m_flags |= eest;
    if(align.Status()&CGeneModel::ePolyA)
        m_flags |= epolya;
    if(align.Status()&CGeneModel::eCap)
        m_flags |= ecap;

    if(align.Status()&CGeneModel::eUnknownOrientation) {
        m_flags |= eunknownorientation;
        m_flags |= eplus;
    } else if(align.Strand() == ePlus){
        m_flags |= eplus;
    } else {
        m_flags |= eminus;
    }

    for(int i = 1; i < (int)align.Exons().size(); ++i) {   // we assume it is continous
        m_introns.push_back(TSignedSeqRange(align.Exons()[i-1].GetTo(),align.Exons()[i].GetFrom()));
    }
}

struct SAlignExtended {
    SAlignExtended(SAlignIndividual& ali, const set<int>& left_exon_ends, const set<int>& right_exon_ends) : m_ali(&ali), m_initial_right_end(ali.m_range.GetTo()) {

        set<int>::iterator ri = right_exon_ends.lower_bound(m_ali->m_range.GetTo()); // leftmost compatible rexon
        m_rlimb =  numeric_limits<int>::max();
        if(ri != right_exon_ends.end())
            m_rlimb = *ri;
        m_rlima = -1;
        if(ri != right_exon_ends.begin())
            m_rlima = *(--ri);
        set<int>::iterator li = left_exon_ends.upper_bound(m_ali->m_range.GetFrom()); // leftmost not compatible lexon
        m_llimb = numeric_limits<int>::max() ;
        if(li != left_exon_ends.end())
            m_llimb = *li;
    }

    SAlignIndividual* m_ali;
    int m_initial_right_end;
    int m_rlimb;
    int m_rlima;
    int m_llimb;
};

void CAlignCollapser::GetCollapsedAlgnmnets(TAlignModelClusterSet& clsset, int oep, int max_extend) {
    ITERATE(Tidpool, i, m_target_id_pool) {
        ITERATE(CAlignCommon::Tintrons, k, i->first.GetIntrons()) {
            const TSignedSeqRange& intron = *k;
            m_right_exon_ends.insert(intron.GetFrom());
            m_left_exon_ends.insert(intron.GetTo());
        }
    }

    NON_CONST_ITERATE(Tdata, i, m_aligns) {
        const CAlignCommon& alc = i->first;
        const deque<char>& id_pool = m_target_id_pool[alc];
        deque<SAlignIndividual>& alideque = i->second;
        sort(alideque.begin(),alideque.end(),LeftAndLongFirstOrder);

        bool leftisfixed = (alc.isCap() && alc.isPlus()) || (alc.isPolyA() && alc.isMinus());
        bool rightisfixed = (alc.isPolyA() && alc.isPlus()) || (alc.isCap() && alc.isMinus());
        bool notspliced = alc.GetIntrons().empty();
        bool isest = alc.isEST();

        typedef list<SAlignExtended> TEA_List;
        TEA_List extended_aligns;

        NON_CONST_ITERATE(deque<SAlignIndividual>, k, alideque) {

            SAlignIndividual& aj = *k;
            bool collapsed = false;
            
            for(TEA_List::iterator itloop = extended_aligns.begin(); itloop != extended_aligns.end(); ) {
                TEA_List::iterator ita = itloop++;
                SAlignIndividual& ai = *ita->m_ali;

                if(aj.m_range.GetFrom() >= min((leftisfixed ? ai.m_range.GetFrom():ai.m_range.GetTo())+1,ita->m_llimb)) {  // extendent align is completed
                    CAlignModel align(alc.GetAlignment(ai, id_pool));
                    clsset.Insert(align);
                    extended_aligns.erase(ita);
                } else if(!collapsed) {
                    if(rightisfixed && ai.m_range.GetTo() != aj.m_range.GetTo())
                        continue;
                    if(notspliced && aj.m_range.GetTo() > ai.m_range.GetTo()) {
                        if(ai.m_range.GetTo()-aj.m_range.GetFrom()+1 < oep)
                            continue;
                        if(aj.m_range.GetTo()-ita->m_initial_right_end > max_extend)
                            continue;
                        if(aj.m_range.GetFrom()-ai.m_range.GetFrom() > max_extend)
                            continue;
                    }
                    if(aj.m_range.GetTo() > (isest ? ita->m_initial_right_end : ita->m_rlimb) || aj.m_range.GetTo() <= ita->m_rlima)
                        continue;
 
                    ai.m_weight += aj.m_weight;
                    if(aj.m_range.GetTo() > ai.m_range.GetTo())
                        ai.m_range.SetTo(aj.m_range.GetTo());
                    collapsed = true;
                }
            }

            if(!collapsed) 
                extended_aligns.push_back(SAlignExtended(aj,m_left_exon_ends,m_right_exon_ends));
        }

        ITERATE(TEA_List, ita, extended_aligns) {
            CAlignModel align(alc.GetAlignment(*ita->m_ali, id_pool));
            clsset.Insert(align);
        }
    }    
}

#define COLLAPS_CHUNK 500000
void CAlignCollapser::AddAlignment(const CAlignModel& align, bool includeincollaps) {

    if(!includeincollaps) {     // collect introns and leave it alone
        const CGeneModel::TExons& e = align.Exons();
        for(unsigned int l = 0; l < e.size(); ++l) {
            if(e[l].m_fsplice)
                m_left_exon_ends.insert(e[l].GetFrom());
            if(e[l].m_ssplice)
                m_right_exon_ends.insert(e[l].GetTo());
        }

        return;
    }
    
    CAlignCommon c(align);
    m_aligns[c].push_back(SAlignIndividual(align, m_target_id_pool[c]));
    if(++m_count%COLLAPS_CHUNK == 0) {
        CollapsIdentical();
    }
}

void CAlignCollapser::CollapsIdentical() {
    NON_CONST_ITERATE(Tdata, i, m_aligns) {
        deque<SAlignIndividual>& alideque = i->second;
        deque<char>& id_pool = m_target_id_pool[i->first];
        if(!alideque.empty()) {

            //remove identicals
            sort(alideque.begin(),alideque.end(),LeftAndLongFirstOrder);
            deque<SAlignIndividual>::iterator ali = alideque.begin();
            for(deque<SAlignIndividual>::iterator far = ali+1; far != alideque.end(); ++far) {
                _ASSERT(far > ali);
                if(far->m_range == ali->m_range) {
                    ali->m_weight += far->m_weight;
                    for(deque<char>::iterator p = id_pool.begin()+far->m_target_id; *p != 0; ++p) {
                        _ASSERT(p < id_pool.end());
                        *p = 0;
                    }
                } else {
                    *(++ali) = *far;
                }
            }
            _ASSERT(ali-alideque.begin()+1 <= (int)alideque.size());
            alideque.resize(ali-alideque.begin()+1);  // ali - last retained element

            
            
            //clean up id pool and reset shifts
            sort(alideque.begin(),alideque.end(),OriginalOrder);
            deque<char>::iterator id = id_pool.begin();
            int shift = 0;
            ali = alideque.begin();
            for(deque<char>::iterator far = id; far != id_pool.end(); ) {
                while(far != id_pool.end() && *far == 0) {
                    ++far;
                    ++shift;
                }
                if(far != id_pool.end()) {
                                                            
                    if(far-id_pool.begin() == ali->m_target_id) {
                        ali->m_target_id -= shift;
                        _ASSERT(ali->m_target_id >= 0);
                        ++ali;
                    }
                    

                    _ASSERT(far >= id);
                    while(*far != 0) {
                        *id++ = *far++;
                    }
                    *id++ = *far++;
                }
            }
            id_pool.resize(id-id_pool.begin());  // id - next after last retained element

            _ASSERT(ali == alideque.end());
        }    
    }
}

END_SCOPE(gnomon)
END_SCOPE(ncbi)


