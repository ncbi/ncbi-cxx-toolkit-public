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
 * Authors:  Alexandre Souvorov
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include <algo/gnomon/gnomon_model.hpp>
#include <algo/gnomon/annot.hpp>

#include <map>
#include <list>
#include <sstream>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)

CModelFilters::CModelFilters()
{
}

void CModelFilters::FilterOutSingleExonEST(TGeneModelList& chains)
{
      for (TGeneModelList::iterator it = chains.begin(); it != chains.end();) {
        const CGeneModel& chain(*it);
        if (chain.Exons().size() == 1 && (chain.Type() & (CAlignModel::eProt|CAlignModel::emRNA))==0) {
            it = chains.erase(it);
        } else {
            ++it;
        }
    }
}


class CAltSplice : public list<const CGeneModel*>
{
public:
    CAltSplice() : m_maxscore(BadScore()), m_nested(false) {}
    typedef list<CGeneModel>::iterator TIt;
    typedef list<CGeneModel>::const_iterator TConstIt;
    TSignedSeqRange Limits() const { return m_limits; }
    TSignedSeqRange RealCdsLimits() const { return m_real_cds_limits; }
    bool IsAlternative(const ncbi::gnomon::CGeneModel&) const;
    bool IsAllowedAlternative(const ncbi::gnomon::CGeneModel&, int maxcomposite) const;
    void Insert(const CGeneModel& a);
    double MaxScore() const { return m_maxscore; }
    bool Nested() const { return m_nested; }
    bool& Nested() { return m_nested; }

    bool BadOverlapTest(const CGeneModel& a) const;

private:
    TSignedSeqRange m_limits, m_real_cds_limits;
    double m_maxscore;
    bool m_nested;
};

bool CModelCompare::CanBeConnectedIntoOne(const CGeneModel& a, const CGeneModel& b)
{
    if (a.Strand() != b.Strand())
        return false;
    if (Precede(a.Limits(),b.Limits())) {
        if(a.OpenRightEnd() && b.OpenLeftEnd())
            return true;
    } else if (Precede(b.Limits(),a.Limits())) {
        if(b.OpenRightEnd() && a.OpenLeftEnd())
            return true;
    }
    return false;
}

void CAltSplice::Insert(const CGeneModel& a)
{
    push_back(&a);
    m_limits += a.Limits();
    m_real_cds_limits += a.RealCdsLimits();
    m_maxscore = max(m_maxscore,a.Score());
}


size_t CModelCompare::CountCommonSplices(const CGeneModel& a, const CGeneModel& b) {
    size_t commonspl = 0;
    if(a.Strand() != b.Strand() || !a.IntersectingWith(b)) return commonspl;
    for(size_t i = 1; i < a.Exons().size(); ++i) {
        for(size_t j = 1; j < b.Exons().size(); ++j) {
            if(a.Exons()[i-1].GetTo() == b.Exons()[j-1].GetTo())
                ++commonspl;
            if(a.Exons()[i].GetFrom() == b.Exons()[j].GetFrom() )
                ++commonspl;
        }
    }

    return commonspl;
}

int HasRetainedIntron(const CGeneModel& under_test, const CGeneModel& control_model)
{
    int num = 0;
    for(int i = 1; i < (int)control_model.Exons().size(); ++i) {
        if(control_model.Exons()[i-1].m_ssplice && control_model.Exons()[i].m_fsplice) {
            TSignedSeqRange intron(control_model.Exons()[i-1].GetTo()+1,control_model.Exons()[i].GetFrom()-1);
            ITERATE(CGeneModel::TExons, test_exon, under_test.Exons()) {
                if(Include(test_exon->Limits(), intron))
                    ++num;
            }
        }
    } 
    return num;
}

bool CAltSplice::IsAllowedAlternative(const CGeneModel& a, int maxcomposite) const
{
    if(a.Exons().size() > 1 && (a.Status()&CGeneModel::ecDNAIntrons) == 0 && a.TrustedmRNA().empty() && a.TrustedProt().empty()) {
        return false;
    }

    if (a.Support().empty()) {
        return false;
    }

    int composite = 0;
    ITERATE(CSupportInfoSet, s, a.Support()) {
        if(s->IsCore() && ++composite > maxcomposite) return false;
    }

    if(a.PStop() || !a.FrameShifts().empty())
        return false;
    if(front()->PStop() || !front()->FrameShifts().empty())
        return false;

    ITERATE(CAltSplice, it, *this) {
        const CGeneModel& b = **it;
        set<TSignedSeqRange> b_introns;
        for(int i = 1; i < (int)b.Exons().size(); ++i) {
            if(b.Exons()[i-1].m_ssplice && b.Exons()[i].m_fsplice) {
                TSignedSeqRange intron(b.Exons()[i-1].GetTo()+1,b.Exons()[i].GetFrom()-1);
                b_introns.insert(intron);
            }
        }

        bool a_has_new_intron = false;
        for(int i = 1; i < (int)a.Exons().size(); ++i) {
            if(a.Exons()[i-1].m_ssplice && a.Exons()[i].m_fsplice) {
                TSignedSeqRange intron(a.Exons()[i-1].GetTo()+1,a.Exons()[i].GetFrom()-1);
                if(b_introns.insert(intron).second) {
                    a_has_new_intron = true;
                    continue;
                }
            }
        }
 
       if(a_has_new_intron) {
            continue;
       } else if(a.RealCdsLen() <= b.RealCdsLen()){
            return false;
       }
    }

    return true;
    

    /* Include everything version
    typedef vector<TSignedSeqRange> TRVec;
    TRVec a_introns;
    for(int i = 1; i < (int)a.Exons().size(); ++i) {
        if(a.Exons()[i-1].m_ssplice && a.Exons()[i].m_fsplice) {
            TSignedSeqRange intron(a.Exons()[i-1].GetTo()+1,a.Exons()[i].GetFrom()-1);
            a_introns.push_back(intron);
        }
    }
    
    ITERATE(CAltSplice, it, *this) {
        const CGeneModel& b = *it;
        TRVec b_introns;
        for(int i = 1; i < (int)b.Exons().size(); ++i) {
            if(b.Exons()[i-1].m_ssplice && b.Exons()[i].m_fsplice) {
                TSignedSeqRange intron(b.Exons()[i-1].GetTo()+1,b.Exons()[i].GetFrom()-1);
                b_introns.push_back(intron);
            }
        }
        if(a_introns.empty() && b_introns.empty())  //already have not-spliced model
            return false;

        bool b_contains_all_introns = false;
        
        if(!a_introns.empty()) {
            TRVec::iterator it = find(b_introns.begin(),b_introns.end(),a_introns.front());
            if(it != b_introns.end() && b_introns.size()-(it-b_introns.begin()) >= a_introns.size() && equal(a_introns.begin(),a_introns.end(),it)) 
                b_contains_all_introns = true;
        }

        if(a_introns.empty() || b_contains_all_introns) { // b exactly contains all spliced part of a (if any)
            bool new_pattern = false;
            ITERATE(TRVec, it, b_introns) {
                if(Include(a.Exons().front(),*it) || Include(a.Exons().back(),*it))
                    new_pattern = true; 
            }
            if(!new_pattern)
                return false;
        }
    }

    return true;
    */

    /*  Old version
    ITERATE(CAltSplice, it, *this) {
        const CGeneModel& b = *it;
        if (HasRetainedIntron(a, b) || HasRetainedIntron(b, a))
            return false;
    }

    //check for models with partial retained introns
    if(a.Exons().size() == 1) {
        return false;
    } else {
        ITERATE(CAltSplice, it, *this) {
            const CGeneModel& b = *it;
            if(CModelCompare::CountCommonSplices(a,b) == 2*(a.Exons().size()-1)) {  //all a-splices are common with b
                for(int i = 1; i < (int)b.Exons().size(); ++i) {
                    if(b.Exons()[i-1].m_ssplice && b.Exons()[i].m_fsplice) {
                        TSignedSeqRange intron(b.Exons()[i-1].GetTo()+1,b.Exons()[i].GetFrom()-1);
                        if(Include(intron,a.Limits().GetFrom())) {  // left end in intron
                            if(a.Strand() == ePlus && (a.Status()&CGeneModel::eCap) == 0)
                                return false;
                            if(a.Strand() == eMinus && (a.Status()&CGeneModel::ePolyA) == 0)
                                return false;
                        }
                        if(Include(intron,a.Limits().GetTo())) {  // right end in intron
                            if(a.Strand() == ePlus && (a.Status()&CGeneModel::ePolyA) == 0)
                                return false;
                            if(a.Strand() == eMinus && (a.Status()&CGeneModel::eCap) == 0)
                                return false;
                        }
                    }
                }
            }
        }
    }

    return true;
    */
}

bool CAltSplice::IsAlternative(const CGeneModel& a) const
{
    _ASSERT( size()>0 );

    if (a.Strand() != front()->Strand())
        return false;

    ITERATE(CAltSplice, it, *this) {
        if(CModelCompare::CountCommonSplices(**it, a) > 0)       // has common splice
            return true;
    }

    if(a.ReadingFrame().NotEmpty() && RealCdsLimits().NotEmpty()) { 
        CAlignMap amap(a.Exons(), a.FrameShifts(), a.Strand(), a.GetCdsInfo().Cds());
        TIVec acds_map(amap.FShiftedLen(a.GetCdsInfo().Cds()),0);
        for(unsigned int j = 0; j < a.Exons().size(); ++j) {
            for(TSignedSeqPos k = max(a.Exons()[j].GetFrom(),a.GetCdsInfo().Cds().GetFrom()); k <= min(a.Exons()[j].GetTo(),a.GetCdsInfo().Cds().GetTo()); ++k) {
                TSignedSeqPos p =  amap.MapOrigToEdited(k);
                _ASSERT(p < (int)acds_map.size());
                if(p >= 0)
                    acds_map[p] = k;
            }
        }
    
        ITERATE(CAltSplice, it, *this) {
            CAlignMap gmap((*it)->Exons(), (*it)->FrameShifts(), (*it)->Strand(), (*it)->GetCdsInfo().Cds());
            TIVec cds_map(gmap.FShiftedLen((*it)->GetCdsInfo().Cds()),0);
            for(unsigned int j = 0; j < (*it)->Exons().size(); ++j) {
                for(TSignedSeqPos k = max((*it)->Exons()[j].GetFrom(),(*it)->GetCdsInfo().Cds().GetFrom()); k <= min((*it)->Exons()[j].GetTo(),(*it)->GetCdsInfo().Cds().GetTo()); ++k) {
                    TSignedSeqPos p =  gmap.MapOrigToEdited(k);
                    _ASSERT(p < (int)cds_map.size());
                    if(p >= 0)
                        cds_map[p] = k;
                }
            }
        
            for(unsigned int i = 0; i < acds_map.size(); ) {
                unsigned int j = 0;
                for( ; j < cds_map.size() && (acds_map[i] != cds_map[j] || i%3 != j%3); ++j);
                if(j == cds_map.size()) {
                    ++i;
                    continue;
                }
            
                int count = 0;
                for( ; j < cds_map.size() && i < acds_map.size() && acds_map[i] == cds_map[j]; ++j, ++i, ++count);
            
                if(count > 30)        // has common cds
                    return true;
            }
        }
    }

    return false;
}

bool CModelCompare::AreSimilar(const CGeneModel& a, const CGeneModel& b, int tolerance)
{
    if(a.Strand() != b.Strand() || !a.ReadingFrame().IntersectingWith(b.ReadingFrame())) return false;

    if(a.ReadingFrame().NotEmpty() && b.ReadingFrame().NotEmpty() && a.Exons().size() == 1 && b.Exons().size()==1) {         // both coding; reading frames intersecting
        int common_point = (a.ReadingFrame() & b.ReadingFrame()).GetFrom();
        if(a.FShiftedLen(a.ReadingFrame().GetFrom(),common_point,false)%3 != b.FShiftedLen(b.ReadingFrame().GetFrom(),common_point,false)%3)  // different frames
            return false;
    }

    TSignedSeqRange intersection = a.Limits() & b.Limits();
    TSignedSeqPos mutual_min = intersection.GetFrom();
    TSignedSeqPos mutual_max = intersection.GetTo();
        
    int amin = 0;
    while(amin < (int)a.Exons().size() && a.Exons()[amin].GetTo() < mutual_min) ++amin;
    if(amin == (int)a.Exons().size()) return false;
    
    int amax = (int)a.Exons().size()-1;
    while(amax >=0 && a.Exons()[amax].GetFrom() > mutual_max) --amax;
    if(amax < 0) return false;
    
    int bmin = 0;
    while(bmin < (int)b.Exons().size() && b.Exons()[bmin].GetTo() < mutual_min) ++bmin;
    if(bmin == (int)b.Exons().size()) return false;

    int bmax = (int)b.Exons().size()-1;
    while(bmax >=0 && b.Exons()[bmax].GetFrom() > mutual_max) --bmax;
    if(bmax < 0) return false;
    
    if(amax-amin != bmax-bmin) return false;

//  head-to-tail overlap
    if (amin != 0 || size_t(amax) != a.Exons().size()-1 || bmin != 0 || size_t(bmax) != b.Exons().size()-1)
        return false;
    
    for( ; amin <= amax; ++amin, ++bmin) {
        if(abs(max(mutual_min,a.Exons()[amin].GetFrom())-max(mutual_min,b.Exons()[bmin].GetFrom())) >= tolerance)
            return false;
        if(abs(min(mutual_max,a.Exons()[amin].GetTo())-min(mutual_max,b.Exons()[bmin].GetTo())) >= tolerance)
            return false;
    }
    
    return true;
}

bool DescendingModelOrder(const CGeneModel& a, const CGeneModel& b)
{
    if (!a.Support().empty() && b.Support().empty())
        return true;
    else if (a.Support().empty() && !b.Support().empty())
        return false;


    bool atrusted = !a.TrustedmRNA().empty() || !a.TrustedProt().empty();
    bool btrusted = !b.TrustedmRNA().empty() || !b.TrustedProt().empty();
    if(atrusted && !btrusted) {                                     // trusted gene is always better
        return true;
    } else if(btrusted && !atrusted) {
        return false;
    } else if(a.ReadingFrame().NotEmpty() && b.ReadingFrame().Empty()) {       // coding is always better
        return true;
    } else if(b.ReadingFrame().NotEmpty() && a.ReadingFrame().Empty()) {
        return false;
    } else if(a.ReadingFrame().NotEmpty()) {     // both coding
        int acdslen = a.FShiftedLen(a.GetCdsInfo().Cds(),true);
        int bcdslen = b.FShiftedLen(b.GetCdsInfo().Cds(),true);
        if(acdslen > 1.5*bcdslen)   // much longer cds is better
            return true;
        else if(bcdslen > 1.5*acdslen)
            return false;

        double ds = 0.025*(fabs(b.Score())+fabs(a.Score()));
        
        double as = a.Score();
        if((a.Status()&CGeneModel::ecDNAIntrons) != 0)
            as += 2*ds;
        if((a.Status()&CGeneModel::ePolyA) != 0)
            as += ds; 
        if((a.Status()&CGeneModel::eCap) != 0)
            as += ds; 
        if(a.isNMD())
            as -= ds;
        
        double bs = b.Score();
        if((b.Status()&CGeneModel::ecDNAIntrons) != 0)
            bs += 2*ds;
        if((b.Status()&CGeneModel::ePolyA) != 0)
            bs += ds; 
        if((b.Status()&CGeneModel::eCap) != 0)
            bs += ds; 
        if(b.isNMD())
            bs -= ds;
        
        if(as > bs)    // better score
            return true;
        else if(bs > as)
            return false;
        else if(HasRetainedIntron(a, b) < HasRetainedIntron(b,a)) // less retained introns is better
            return true;
        else if(HasRetainedIntron(a, b) > HasRetainedIntron(b,a))
            return false;
        else if(a.Weight() > b.Weight())       // more alignments is better
            return true;
        else if(a.Weight() < b.Weight()) 
            return false;
        else 
            return (a.Limits().GetLength() < b.Limits().GetLength());   // everything else equal prefer compact model
    } else {                       // both noncoding
        double asize = a.Weight();
        double bsize = b.Weight();
        double ds = 0.025*(asize+bsize);
        
        if((a.Status()&CGeneModel::ePolyA) != 0)
            asize += ds; 
        if((a.Status()&CGeneModel::eCap) != 0)
            asize += ds; 
        if(a.isNMD())
            asize -= ds;
        
        if((b.Status()&CGeneModel::ePolyA) != 0)
            bsize += ds; 
        if((b.Status()&CGeneModel::eCap) != 0)
            bsize += ds; 
        if(b.isNMD())
            bsize -= ds;
        
        if(asize > bsize)     
            return true;
        else if(bsize > asize)
            return false;
        else 
            return (a.Limits().GetLength() < b.Limits().GetLength());   // everything else equal prefer compact model
    }
}

void CModelFilters::FilterOutSimilarsWithLowerScore(TGeneModelList& cls, int tolerance, TGeneModelList& bad_aligns)
{
    cls.sort(DescendingModelOrder);
    for(TGeneModelList::iterator it = cls.begin(); it != cls.end(); ++it) {
        const CGeneModel& ai(*it);
        TGeneModelList::iterator jt_loop = it;
        for(++jt_loop; jt_loop != cls.end();) {
            TGeneModelList::iterator jt = jt_loop++;
            const CGeneModel& aj(*jt);
            if (CModelCompare::AreSimilar(ai,aj,tolerance)) {
                CNcbiOstrstream ost;
                jt->Status() |= CGeneModel::eSkipped;
                ost << "Trumped by similar chain " << it->ID();
                jt->AddComment(CNcbiOstrstreamToString(ost));
                bad_aligns.push_back(*jt);
                cls.erase(jt);
            }
        }
    }
}

void  CModelFilters::FilterOutLowSupportModels(TGeneModelList& cls, int minsupport, int minCDS, bool allow_partialgenes, TGeneModelList& bad_aligns)
{
    for(TGeneModelList::iterator jt_loop = cls.begin(); jt_loop != cls.end();) {
        TGeneModelList::iterator jt = jt_loop++;
        const CGeneModel& aj(*jt);
        if (aj.TrustedmRNA().empty() && aj.TrustedProt().empty() && (int)aj.Support().size() < minsupport && (aj.ReadingFrame().Empty() || aj.RealCdsLen() < minCDS || (allow_partialgenes && !aj.CompleteCds()))) {
            jt->Status() |= CGeneModel::eSkipped;
            jt->AddComment("Low support chain");
            bad_aligns.push_back(*jt);
            cls.erase(jt);
        }
    }
}



bool CModelCompare::BadOverlapTest(const CGeneModel& a, const CGeneModel& b) {     // returns true for bad overlap
    if((!a.TrustedmRNA().empty() || !a.TrustedProt().empty()) && (!b.TrustedmRNA().empty() || !b.TrustedProt().empty()))
        return false;
    //    else if(a.Limits().IntersectingWith(b.Limits()) && (a.Exons().size() == 1 || b.Exons().size() == 1)) 
    //        return true;
    else 
        return CountCommonSplices(a,b) > 0;
}

bool CAltSplice::BadOverlapTest(const CGeneModel& a) const {
    ITERATE(CAltSplice, it, *this) {
        if(CModelCompare::BadOverlapTest(**it,a)) return true;
    }
    return false;
}

bool CModelCompare::RangeNestedInIntron(TSignedSeqRange r, const CGeneModel& algn) {
    for(int i = 1; i < (int)algn.Exons().size(); ++i) {
        TSignedSeqRange intron(algn.Exons()[i-1].GetTo()+1,algn.Exons()[i].GetFrom()-1);
        if(Include(intron, r)) 
            return true;
    }
    return false;
}

CGeneSelector::ECompat CGeneSelector::CheckCompatibility(const CAltSplice& gene, const CGeneModel& algn)
{
    TSignedSeqRange gene_lim_with_margins( gene.Limits().GetFrom() - minIntergenic, gene.Limits().GetTo() + minIntergenic );
    TSignedSeqRange gene_cds = (gene.size() > 1 || gene.front()->CompleteCds()) ? gene.RealCdsLimits() : gene.front()->MaxCdsLimits();
    bool gene_good_enough_to_be_annotation = allow_partialalts || gene.front()->GoodEnoughToBeAnnotation(); 
    TSignedSeqRange gene_lim_for_nested = gene_cds.Empty() ? gene.Limits() : gene_cds;

    TSignedSeqRange algn_lim = algn.Limits();
    //    TSignedSeqRange algn_lim_with_margins(algn_lim.GetFrom() - minIntergenic, algn_lim.GetTo() + minIntergenic );
    TSignedSeqRange algn_cds = algn.CompleteCds() ? algn.RealCdsLimits() : algn.MaxCdsLimits();
    bool algn_good_enough_to_be_annotation = allow_partialalts || algn.GoodEnoughToBeAnnotation();
    TSignedSeqRange algn_lim_for_nested = algn_cds.Empty() ? algn_lim : algn_cds;

    TSignedSeqRange gene_cds_with_margins = gene_cds;
    if(gene_cds.NotEmpty() && (!gene_good_enough_to_be_annotation || !algn_good_enough_to_be_annotation))
        gene_cds_with_margins = TSignedSeqRange( gene_cds.GetFrom() - minIntergenic, gene_cds.GetTo() + minIntergenic );


    if(!gene_lim_with_margins.IntersectingWith(algn_lim))             // distance > MinIntergenic
        return eOtherGene;
    else if(gene_good_enough_to_be_annotation && CModelCompare::RangeNestedInIntron(gene_lim_for_nested, algn))    // gene is nested in align's intron
        return eExternal;

    if(gene.IsAlternative(algn)) {   // has common splice or common CDS
        if (gene.IsAllowedAlternative(algn, composite) &&
            ( !algn.TrustedmRNA().empty() || !algn.TrustedProt().empty()    // trusted gene
              //              || (algn.AlignLen() > altfrac/100*gene.front().AlignLen() && (algn.ReadingFrame().Empty() || algn.Score() > altfrac/100*gene.front().Score())) // long enough and noncoding or good score
              || (algn.AlignLen() > altfrac/100*gene.front()->AlignLen() && (algn.ReadingFrame().Empty() || algn.RealCdsLen() > altfrac/100*gene.front()->RealCdsLen())) // long enough and noncoding or long enough cds
            )
         && gene_good_enough_to_be_annotation && algn_good_enough_to_be_annotation)       // complete or allowpartials
            return eAlternative;
        else 
            return eNotCompatible;
    }
    
    bool nested = true;
    ITERATE(CAltSplice, it, gene) {
        if(algn_lim.IntersectingWith((*it)->Limits()) && !CModelCompare::RangeNestedInIntron(algn_lim_for_nested, **it)) {
            nested = false;
            break;
        }
    }
    if(nested) {               // algn is nested in gene's intron
        if(algn_good_enough_to_be_annotation)
            return eNested;
        else
            return eNotCompatible;
    }


    if(!algn_cds.Empty() && !gene_cds.Empty()) {                          // both coding
        if (!gene_cds_with_margins.IntersectingWith(algn_cds)) {          // distance > MinIntergenic 
            if(gene.BadOverlapTest(algn)) {
                return eNotCompatible;
            } else {
                return eOtherGene;
            }
        }
    }
    
    if(gene_good_enough_to_be_annotation && algn_good_enough_to_be_annotation) {
        if(gene.front()->Strand() != algn.Strand() && allow_opposite_strand && 
           ((algn.Status()&CGeneModel::eBestPlacement) || (algn.Exons().size() > 1 && gene.front()->Exons().size() > 1)))
            return eOtherGene; 
        else if(algn.Status() & CGeneModel::eBestPlacement && (algn.Exons().size() == 1 || (algn.Status()&CGeneModel::ecDNAIntrons))) {
#ifdef _DEBUG 
            const_cast<CGeneModel&>(algn).AddComment("Best placement overlap");
#endif    
            return eOtherGene;
        }
    }
    
    return eNotCompatible;
}

void CGeneSelector::FindGeneSeeds(const TGeneModelList& cls, list<CAltSplice>& alts, list<const CGeneModel*>& not_placed_yet) {
    ITERATE(TGeneModelList, it, cls) {
        const CGeneModel& algn(*it);

        if(algn.Score() == BadScore()) {             // postpone noncoding models
            not_placed_yet.push_back(&algn);
            continue;
        }

        list<CAltSplice*> possibly_nested;

        bool good_model = true;
        for(list<CAltSplice>::iterator itl = alts.begin(); good_model && itl != alts.end(); ++itl) {
            ECompat cmp = CheckCompatibility(*itl, algn);

            switch(cmp) {
            case eExternal:
                possibly_nested.push_back(&(*itl));  // already created gene is nested in this model
            case eOtherGene:
                break;
            default:
                good_model = false;
                break;
            }                
        }

        if(good_model) {
            ITERATE(list<CAltSplice*>, itl, possibly_nested) {
                (*itl)->Nested() = true;
            }
            alts.push_back(CAltSplice());
            alts.back().Insert(algn);
        } else {
            not_placed_yet.push_back(&algn);
        }
    }
}

void CGeneSelector::FindAltsForGeneSeeds(list<CAltSplice>& alts, list<const CGeneModel*>& not_placed_yet) {
    for(list<const CGeneModel*>::iterator itloop = not_placed_yet.begin(); itloop != not_placed_yet.end(); ) {
        list<const CGeneModel*>::iterator it = itloop++;
        const CGeneModel& algn(**it);

        list<list<CAltSplice>::iterator> included_in;
        list<CAltSplice*> possibly_nested;

        bool good_model = true;
        for(list<CAltSplice>::iterator itl = alts.begin(); good_model && itl != alts.end(); ++itl) {
            ECompat cmp = CheckCompatibility(*itl, algn);

            switch(cmp) {
            case eExternal:
                possibly_nested.push_back(&(*itl));  // already created gene is nested in this model
            case eOtherGene:
                break;
            case eAlternative:
                included_in.push_back(itl);
                break;
            default:
                good_model = false;
                break;
            }
        }

        if(!good_model ||included_in.empty()) {
            continue;
        } else if(included_in.size() == 1) {    // alternative to only one seed
            ITERATE(list<CAltSplice*>, itl, possibly_nested) {
                (*itl)->Nested() = true;
            }
            included_in.front()->Insert(algn);
            not_placed_yet.erase(it);
        } else if(!algn.TrustedmRNA().empty() || !algn.TrustedProt().empty()) {   // connects seeds but trusted
            bool cds_overlap = true;
            ITERATE(list<list<CAltSplice>::iterator>, k, included_in) {
                if(!algn.RealCdsLimits().IntersectingWith((*k)->Limits())) {
                    cds_overlap = false;
                    break;
                }
            }
            if(cds_overlap) {
                ITERATE(list<CAltSplice*>, itl, possibly_nested) {
                    (*itl)->Nested() = true;
                }
                included_in.front()->Insert(algn);
                ITERATE(list<list<CAltSplice>::iterator>, k, included_in) {
                    if(k != included_in.begin()) {
                        ITERATE(CAltSplice, l, **k) {
                            if(itloop == not_placed_yet.end() || !DescendingModelOrder(**itloop, **l)) {  // next is not better
                                if(CheckCompatibility(*included_in.front(), **l) == eAlternative) {  // check that the thresholds are met
                                    included_in.front()->Insert(**l);
                                } else {
                                    not_placed_yet.push_back(*l); // position doesn't matter - will go to 'bad' models
                                }
                            } else {
                                list<const CGeneModel*>::iterator idest = itloop;
                                for( ;idest != not_placed_yet.end() && DescendingModelOrder(**idest, **l); ++idest);
                                not_placed_yet.insert(idest, *l);
                            }
                        }
                        alts.erase(*k);
                    }
                }
                not_placed_yet.erase(it);
            }
        }
    }
}

void CGeneSelector::PlaceAllYouCan(list<CAltSplice>& alts, list<const CGeneModel*>& not_placed_yet, TGeneModelList& rejected) {
    ITERATE(list<const CGeneModel*>, it, not_placed_yet) {
        const CGeneModel& algn(**it);
        bool nested = false;
        list<CAltSplice>::iterator included_in(alts.end());
        list<CAltSplice*> possibly_nested;

        bool good_model = true;
        for(list<CAltSplice>::iterator itl = alts.begin(); good_model && itl != alts.end(); ++itl) {
            ECompat cmp = CheckCompatibility(*itl, algn);
            CNcbiOstrstream ost;
            switch(cmp) {
            case eNotCompatible:
                rejected.push_back(algn);
                rejected.back().Status() |= CGeneModel::eSkipped;
                ost << "Trumped by another model " << itl->front()->ID();
                rejected.back().AddComment(CNcbiOstrstreamToString(ost));
                good_model = false;
                break;
            case eAlternative:
                if(included_in == alts.end()) {
                    included_in = itl;
                } else {  // tries to connect two different genes
                    good_model = false;
                    rejected.push_back(algn);
                    rejected.back().Status() |= CGeneModel::eSkipped;
                    ost << "Connects two genes " << itl->front()->ID() << " " << included_in->front()->ID();
                    rejected.back().AddComment(CNcbiOstrstreamToString(ost));
                }
                break;
            case eNested:
                nested = true;
                break;
            case eExternal:
                possibly_nested.push_back(&(*itl));  // already created gene is nested in this model
                break;
            case eOtherGene:
                break;
            }
        }
        if(good_model) {
            ITERATE(list<CAltSplice*>, itl, possibly_nested) {
                (*itl)->Nested() = true;
            }
            if(included_in != alts.end()) {
                included_in->Insert(algn);
            } else {
                alts.push_back(CAltSplice());
                alts.back().Insert(algn);
                alts.back().Nested() = nested;
            }
        }
    }
}

void CGeneSelector::FindGenesPass1(const TGeneModelList& cls, list<CAltSplice>& alts,
                                   list<const CGeneModel*>& possibly_alternative, TGeneModelList& rejected)
{
    ITERATE(TGeneModelList, it, cls) {
        const CGeneModel& algn(*it);
        bool nested = false;
        list<CAltSplice>::iterator included_in(alts.end());
        list<CAltSplice*> possibly_nested;

        bool good_model = true;
        for(list<CAltSplice>::iterator itl = alts.begin(); good_model && itl != alts.end(); ++itl) {
            ECompat cmp = CheckCompatibility(*itl, algn);
            CNcbiOstrstream ost;
            switch(cmp) {
            case eNotCompatible:
                rejected.push_back(algn);
                rejected.back().Status() |= CGeneModel::eSkipped;
                ost << "Trumped by another model pass1 " << itl->front()->ID();
                rejected.back().AddComment(CNcbiOstrstreamToString(ost));
                good_model = false;
                break;
            case eAlternative:
                included_in = itl;
                break;
            case eNested:
                nested = true;
                break;
            case eExternal:
                possibly_nested.push_back(&(*itl));
                break;
            case eOtherGene:
                break;
            }
        }

        if(good_model) {
            if(included_in != alts.end()) {
                possibly_alternative.push_back(&algn);
            } else {
                ITERATE(list<CAltSplice*>, itl, possibly_nested) {
                    (*itl)->Nested() = true;
                }
                alts.push_back(CAltSplice());
                alts.back().Insert(algn);
                alts.back().Nested() = nested;
            }
        }
    }
}

void CGeneSelector::FindGenesPass2(const list<const CGeneModel*>& possibly_alternative, list<CAltSplice>& alts,
                                   TGeneModelList& bad_aligns)
{
    ITERATE(list<const CGeneModel*>, it, possibly_alternative) {
        const CGeneModel& algn(**it);
        list<CAltSplice>::iterator included_in(alts.end());
        list<CAltSplice*> possibly_nested;

        bool good_model = true;
        for(list<CAltSplice>::iterator itl = alts.begin(); good_model && itl != alts.end(); ++itl) {
            ECompat cmp = CheckCompatibility(*itl, algn);
            CNcbiOstrstream ost;
            switch(cmp) {
            case eNotCompatible:
                bad_aligns.push_back(algn);
                bad_aligns.back().Status() |= CGeneModel::eSkipped;
                ost << "Trumped by another model pass2 " << itl->front()->ID();
                bad_aligns.back().AddComment(CNcbiOstrstreamToString(ost));
                good_model = false;
                break;
            case eAlternative:
                if(included_in == alts.end()) {
                    included_in = itl;
                } else if(Include(included_in->Limits(),itl->Limits()) || Include(itl->Limits(),included_in->Limits())) {   // connects nested to external
                    itl->Nested() = (Include(included_in->Limits(),itl->Limits()) && included_in->Nested()) || (Include(itl->Limits(),included_in->Limits()) && itl->Nested());
                    ITERATE(CAltSplice, i, *included_in) {
                        itl->Insert(**i);
                    }                        
                    possibly_nested.remove(&(*included_in));
                    alts.erase(included_in);
                    included_in = itl;
                } else {
                    bad_aligns.push_back(algn);
                    bad_aligns.back().Status() |= CGeneModel::eSkipped;
                    ost << "Connects two genes " << itl->front()->ID() << " " << included_in->front()->ID();
                    bad_aligns.back().AddComment(CNcbiOstrstreamToString(ost));
                    good_model = false;            // tries to connect two different genes
                }
                break;
            case eNested:
                break;
            case eExternal:
                possibly_nested.push_back(&(*itl));
                break;
            case eOtherGene:
                break;
            }
        }

        if(good_model) {
            _ASSERT(included_in != alts.end());
            ITERATE(list<CAltSplice*>, itl, possibly_nested) {
                (*itl)->Nested() = true;
            }
            if(included_in != alts.end()) {
                //                _ASSERT(algn.Score() <= included_in->back().Score()); // chains should be sorted by score desc
                included_in->Insert(algn);
            }
        }
    }
}

void CGeneSelector::FindGenesPass3(const TGeneModelList& rejected, list<CAltSplice>& alts,
                                   TGeneModelList& bad_aligns)
{
    ITERATE(TGeneModelList, it, rejected) {
        const CGeneModel& algn(*it);
        list<CAltSplice>::iterator included_in(alts.end());
        list<CAltSplice*> possibly_nested;

        bool good_model = true;
        for(list<CAltSplice>::iterator itl = alts.begin(); good_model && itl != alts.end(); ++itl) {
            ECompat cmp = CheckCompatibility(*itl, algn);
            switch(cmp) {
            case eOtherGene:
                break;
            default:
                good_model = false;
                break;
            }
        }

        if(good_model) {
            alts.push_back(CAltSplice());
            alts.back().Insert(algn);
            alts.back().Nested() = false;
        } else {
            bad_aligns.push_back(algn);
        }
    }
}

void CGeneSelector::FindAllCompatibleGenes(TGeneModelList& cls, list<CAltSplice>& alts, TGeneModelList& bad_aligns)
{
    list<const CGeneModel*> not_placed_yet;

    cls.sort(DescendingModelOrder);
    FindGeneSeeds(cls, alts, not_placed_yet);
    FindAltsForGeneSeeds(alts, not_placed_yet);
    PlaceAllYouCan(alts, not_placed_yet, bad_aligns);

    /*
    list<const CGeneModel*> possibly_alternative;
    TGeneModelList rejected;
    TGeneModelList rejected;

    cls.sort(DescendingModelOrder);
    FindGenesPass1(cls, alts,
                   possibly_alternative, rejected);
    FindGenesPass2(possibly_alternative, alts,
                   bad_aligns);
    FindGenesPass3(rejected, alts,
                   bad_aligns);
    */
}

bool CModelCompare::HaveCommonExonOrIntron(const CGeneModel& a, const CGeneModel& b) {
    if(a.Strand() != b.Strand() || !a.IntersectingWith(b)) return false;

    for(unsigned int i = 0; i < a.Exons().size(); ++i) {
        for(unsigned int j = 0; j < b.Exons().size(); ++j) {
            if(a.Exons()[i] == b.Exons()[j]) 
                return true;
        }
    }

    for(unsigned int i = 1; i < a.Exons().size(); ++i) {
        TSignedSeqRange introna(a.Exons()[i-1].GetTo()+1,a.Exons()[i].GetFrom()-1);
        for(unsigned int j = 1; j < b.Exons().size(); ++j) {
            TSignedSeqRange intronb(b.Exons()[j-1].GetTo()+1,b.Exons()[j].GetFrom()-1);
            if(introna == intronb) 
                return true;
        }
    }

    return false;
}



void CModelFilters::FilterOutTandemOverlap(TGeneModelList&cls, double fraction, TGeneModelList& bad_aligns)
{
    cls.sort(DescendingModelOrder);
    for(TGeneModelList::iterator it_loop = cls.begin(); it_loop != cls.end();) {
        TGeneModelList::iterator it = it_loop++;
        const CGeneModel& ai(*it);
        if(!ai.TrustedmRNA().empty() || !ai.TrustedProt().empty() || ai.ReadingFrame().Empty())
            continue;
        int cds_len = ai.RealCdsLen();

        vector<const CGeneModel*> candidates;
        for(TGeneModelList::iterator jt = cls.begin(); jt != cls.end(); ++jt) {
            const CGeneModel& aj(*jt);
            if(!aj.HasStart() || !aj.HasStop() || aj.Score() < fraction/100*ai.Score() || aj.RealCdsLen() < fraction/100*cds_len || !CModelCompare::HaveCommonExonOrIntron(ai,aj)) 
                continue;
            candidates.push_back(&aj);
        }

        bool alive = true;
        for (size_t i = 0; alive && i < candidates.size(); ++i) {
            for (size_t j = i+1; alive && j < candidates.size(); ++j) {
                if(!candidates[i]->Limits().IntersectingWith(candidates[j]->Limits())) {
                    CNcbiOstrstream ost;
                    it->Status() |= CGeneModel::eSkipped;
                    ost << "Overlapping tandem " << candidates[i]->ID() << " " << candidates[j]->ID();
                    it->AddComment(CNcbiOstrstreamToString(ost));
                    bad_aligns.push_back(*it);
                    cls.erase(it);
                    alive = false;
                }
            }
        }
    }
}


TGeneModelList CGeneSelector::SelectGenes(TGeneModelList& chains, TGeneModelList& bad_aligns)
{        
    TGeneModelList models;

    list<CAltSplice> alts_clean;
    FindAllCompatibleGenes(chains, alts_clean, bad_aligns);

    int geneid = 1;
    ITERATE(list<CAltSplice>, itl, alts_clean) {
        int rank = 0;
        ITERATE(CAltSplice, ita, *itl) {
            models.push_back(**ita);
            CGeneModel& align = models.back();
            
            if (itl->Nested()) {
                align.SetType(align.Type() | CGeneModel::eNested);
            }
            align.SetGeneID(geneid);
            align.SetRankInGene(++rank);
        }
        ++geneid;
    }
    return models;
}
            
void CGeneSelector::ReselectGenes(TGeneModelList& models, TGeneModelList& bad_aligns)
{
    TGeneModelClusterSet model_clusters;
    ITERATE(TGeneModelList, m, models)
        model_clusters.Insert(*m);
    models.clear();
    int geneid = 1;
    NON_CONST_ITERATE(TGeneModelClusterSet, it_cl, model_clusters) {
        TGeneModelCluster& cls = const_cast<TGeneModelCluster&>(*it_cl);
        TGeneModelList models_tmp = SelectGenes(cls, bad_aligns);
        RenumGenes(models_tmp, geneid, 1);
        models.splice(models.end(), models_tmp);
    }
}

void CGeneSelector::RenumGenes(TGeneModelList& models, int& gennum, int geninc)
{
    int maxgeneid= gennum-geninc;
    NON_CONST_ITERATE(TGeneModelList, it, models) {
        if (it->GeneID()==0)
            continue;
        int geneid = gennum+(it->GeneID()-1)*geninc;
        it->SetGeneID(geneid);
        if (maxgeneid < geneid)
            maxgeneid = geneid;
    }
    gennum = maxgeneid+geninc;
}

END_SCOPE(gnomon)
END_NCBI_SCOPE
