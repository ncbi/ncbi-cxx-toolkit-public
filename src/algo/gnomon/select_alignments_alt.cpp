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


bool CModelCompare::AreSimilar(const CGeneModel& a, const CGeneModel& b, int tolerance)
{
    if(a.Strand() != b.Strand()) 
        return false;

    if(a.ReadingFrame().NotEmpty() && b.ReadingFrame().NotEmpty()) {
        if(!a.ReadingFrame().IntersectingWith(b.ReadingFrame()) || a.GetCdsInfo().PStops() != b.GetCdsInfo().PStops())
           return false; 

        if(a.Exons().size() == 1 && b.Exons().size()==1) {         // both coding; reading frames intersecting
            TSignedSeqRange acds = a.GetCdsInfo().Cds();
            TSignedSeqRange bcds = b.GetCdsInfo().Cds();
            int common_point = (acds & bcds).GetFrom();
            if(a.FShiftedLen(acds.GetFrom(),common_point,false)%3 != b.FShiftedLen(bcds.GetFrom(),common_point,false)%3)  // different frames
                return false;
        }
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


bool CModelCompare::BadOverlapTest(const CGeneModel& a, const CGeneModel& b) {     // returns true for bad overlap
    if((!a.TrustedmRNA().empty() || !a.TrustedProt().empty()) && (!b.TrustedmRNA().empty() || !b.TrustedProt().empty()))
        return false;
    //    else if(a.Limits().IntersectingWith(b.Limits()) && (a.Exons().size() == 1 || b.Exons().size() == 1)) 
    //        return true;
    else 
        return CountCommonSplices(a,b) > 0;
}


bool CModelCompare::RangeNestedInIntron(TSignedSeqRange r, const CGeneModel& algn, bool check_in_holes) {
    for(int i = 1; i < (int)algn.Exons().size(); ++i) {
        if(check_in_holes || (algn.Exons()[i-1].m_ssplice && algn.Exons()[i].m_fsplice)) {
            TSignedSeqRange intron(algn.Exons()[i-1].GetTo()+1,algn.Exons()[i].GetFrom()-1);
            if(Include(intron, r)) 
                return true;
        }
    }
    return false;
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


void CGeneSelector::FilterGenes(TGeneModelList& chains, TGeneModelList& bad_aligns,
                                TGeneModelList& dest)
{
    ITERATE(TGeneModelList, it, chains) {
        if(it->Status()&CGeneModel::eSkipped) {
            bad_aligns.push_back(*it);
        } else {
            dest.push_back(*it);
        }
    }
}


TGeneModelList CGeneSelector::FilterGenes(TGeneModelList& chains, TGeneModelList& bad_aligns)
{
    TGeneModelList models;
    FilterGenes(chains, bad_aligns, models);
    return models;
}


TGeneModelList CGeneSelector::SelectGenes(TGeneModelList& chains, TGeneModelList& bad_aligns)
{        
    TGeneModelList models;

    int geneid = 0;
    map<int,int> old_to_new;
    ITERATE(TGeneModelList, it, chains) {
        if(it->Status()&CGeneModel::eSkipped) {
            bad_aligns.push_back(*it);
        } else {
            models.push_back(*it);
            int gid = old_to_new[models.back().GeneID()];
            if(gid == 0) {
                gid = ++geneid;
                old_to_new[models.back().GeneID()] = gid;
            }
            models.back().SetGeneID(gid);
        }
    }

    return models;
}


void CGeneSelector::RenumGenes(TGeneModelList& models, Int8& gennum, Int8 geninc)
{
    Int8 maxgeneid= gennum-geninc;
    NON_CONST_ITERATE(TGeneModelList, it, models) {
        if (it->GeneID()==0)
            continue;
        Int8 geneid = gennum+(it->GeneID()-1)*geninc;
        it->SetGeneID(geneid);
        if (maxgeneid < geneid)
            maxgeneid = geneid;
    }
    gennum = maxgeneid+geninc;
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
