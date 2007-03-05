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
 * Authors:  Mike DiCuccio
 *
 * File Description: gnomon library parts requiring object manager support
 * to allow apps not using these to be linked w/o object manager libs
 *
 */

#include <ncbi_pch.hpp>
#include <algo/gnomon/gnomon.hpp>
#include <algo/gnomon/gnomon_exception.hpp>
#include "hmm.hpp"
#include "hmm_inlines.hpp"

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Packed_seqint.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objmgr/util/sequence.hpp>

#include <stdio.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(gnomon)
USING_SCOPE(ncbi::objects);



//
// helper function - convert a vector<TSignedSeqRange> into a compact CSeq_loc
//
namespace {
inline
CRef<CSeq_loc> s_ExonDataToLoc(const vector<TSignedSeqRange>& vec,
                               ENa_strand strand, CSeq_id& id)
{
    CRef<CSeq_loc> loc(new CSeq_loc());

    CPacked_seqint::Tdata data;
    ITERATE (vector<TSignedSeqRange>, iter, vec) {
        CRef<CSeq_interval> ival(new CSeq_interval());
        ival->SetId(id);
        ival->SetStrand(strand);
        ival->SetFrom(iter->GetFrom());
        ival->SetTo  (iter->GetTo());

        data.push_back(ival);
    }

    if (data.size() == 1) {
        loc->SetInt(*data.front());
    } else {
        loc->SetPacked_int().Set().swap(data);
    }

    return loc;
}

} //end unnamed namespace

void CGnomonEngine::GenerateSeqAnnot()
{
    list<CGene> genes = GetGenes();


    m_Annot.Reset(new CSeq_annot());
    m_Annot->AddName("GNOMON gene scan output");

    CRef<CSeq_id> id(new CSeq_id("lcl|gnomon"));

    CSeq_annot::C_Data::TFtable& ftable = m_Annot->SetData().SetFtable();

    int counter = 0;
    string locus_tag_base("GNOMON_");
    ITERATE (list<CGene>, it, genes) {
        const CGene& igene = *it;
        int strand = igene.Strand();

        vector<TSignedSeqRange> mrna_vec;
        vector<TSignedSeqRange> cds_vec;

        for (size_t j = 0;  j < igene.size();  ++j) {
            const CExonData& exon = igene[j];
            TSignedSeqPos a = exon.GetFrom();
            TSignedSeqPos b = exon.GetTo();

            if (j == 0  ||  igene[j-1].GetTo()+1 != a) {
                // new exon
                mrna_vec.push_back(TSignedSeqRange(a,b));
            } else {
                // attaching cds-part to utr-part
                mrna_vec.back().SetTo(b);
            } 

            if (exon.Type().find("Cds") != NPOS) {
                cds_vec.push_back(TSignedSeqRange(a,b));
            }
        }

        // stop-codon removal from cds
        if (cds_vec.size()  &&
            strand == ePlus  &&  igene.RightComplete()) {
            cds_vec.back().SetTo(cds_vec.back().GetTo() - 3);
        }
        if (cds_vec.size()  &&
            strand == eMinus  &&  igene.LeftComplete()) {
            cds_vec.front().SetFrom(cds_vec.back().GetFrom() + 3);
        }

        //
        // create our mRNA
        CRef<CSeq_feat> feat_mrna;
        if (mrna_vec.size()) {
            feat_mrna = new CSeq_feat();
            feat_mrna->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
            feat_mrna->SetLocation
                (*s_ExonDataToLoc(mrna_vec,
                 (strand == ePlus ? eNa_strand_plus : eNa_strand_minus), *id));
        } else {
            continue;
        }

        //
        // create the accompanying CDS
        CRef<CSeq_feat> feat_cds;
        if (cds_vec.size()) {
            feat_cds = new CSeq_feat();
            feat_cds->SetData().SetCdregion();

            feat_cds->SetLocation
                (*s_ExonDataToLoc(cds_vec,
                 (strand == ePlus ? eNa_strand_plus : eNa_strand_minus), *id));
        }

        //
        // create a dummy gene feature as well
        CRef<CSeq_feat> feat_gene(new CSeq_feat());

        char buf[32];
        sprintf(buf, "%04d", ++counter);
        string name(locus_tag_base);
        name += buf;
        feat_gene->SetData().SetGene().SetLocus_tag(name);
        feat_gene->SetLocation().SetInt()
            .SetFrom(feat_mrna->GetLocation().GetTotalRange().GetFrom());
        feat_gene->SetLocation().SetInt()
            .SetTo(feat_mrna->GetLocation().GetTotalRange().GetTo());
        feat_gene->SetLocation().SetInt().SetStrand
            (strand == ePlus ? eNa_strand_plus : eNa_strand_minus);

//        const CSeq_id& loc_id = sequence::GetId(feat_mrna->GetLocation(), 0);

        feat_gene->SetLocation().SetId
            (sequence::GetId(feat_mrna->GetLocation(), 0));

        ftable.push_back(feat_gene);
        ftable.push_back(feat_mrna);
        if (feat_cds) {
            ftable.push_back(feat_cds);
        }
    }
}

CRef<CSeq_annot> CGnomonEngine::GetAnnot(void)
{
    if (!m_Annot)
	GenerateSeqAnnot();
    return m_Annot;
}


//
//
// This function deduces the frame from 5p coordinate of the CDS which should be on the 5p codon boundary
//
// 
double CCodingPropensity::GetScore(const string& modeldatafilename, const CSeq_loc& cds, CScope& scope, int *const gccontent, double *const startscore)
{
    *gccontent = 0;
    const CSeq_id* seq_id = cds.GetId();
    if (seq_id == NULL)
	NCBI_THROW(CGnomonException, eGenericError, "CCodingPropensity: CDS has multiple ids or no id at all");
    
    // Need to know GC content in order to load correct models.
    // Compute on the whole transcript, not just CDS.
    CBioseq_Handle xcript_hand = scope.GetBioseqHandle(*seq_id);
    CSeqVector xcript_vec = xcript_hand.GetSeqVector();
    xcript_vec.SetIupacCoding();
    unsigned int gc_count = 0;
    CSeqVector_CI xcript_iter(xcript_vec);
    for( ;  xcript_iter;  ++xcript_iter) {
        if (*xcript_iter == 'G' || *xcript_iter == 'C') {
            ++gc_count;
        }
    }
    *gccontent = static_cast<unsigned int>(100.0 * gc_count / xcript_vec.size() + 0.5);
	
    // Load models from file
    CMC3_CodingRegion<5>   cdr(modeldatafilename, *gccontent);
    CMC_NonCodingRegion<5> ncdr(modeldatafilename, *gccontent);

    // Represent coding sequence as enum Nucleotides
    CSeqVector vec(cds, scope);
    vec.SetIupacCoding();
    CEResidueVec seq;
    seq.reserve(vec.size());
    CSeqVector_CI iter(vec);
    for( ;  iter;  ++iter) {
	seq.push_back(fromACGT(*iter));
    }

    // Sum coding and non-coding scores across coding sequence.
    // Don't include stop codon!
    double score = 0;
    for (unsigned int i = 5;  i < seq.size() - 3;  ++i)
        score += cdr.Score(seq, i, i % 3) - ncdr.Score(seq, i);

    if(startscore) {
        //
        // start evalustion needs 17 bases total (11 before ATG and 3 after)
        // if there is not enough sequence it will be substituted by Ns which will degrade the score
        //

        CWMM_Start start(modeldatafilename, *gccontent);

        int totallen = xcript_vec.size();
        int left, right;
        int extraNs5p = 0;
        int extrabases = start.Left()+2;            // (11) extrabases left of ATG needed for start (6) and noncoding (5) evaluation
        if(cds.GetStrand() == eNa_strand_plus) {
            int startposition = cds.GetTotalRange().GetFrom();
            if(startposition < extrabases) {
                left = 0;
                extraNs5p = extrabases-startposition;
            } else {
                left = startposition-extrabases;
            }
            right = min(startposition+2+start.Right(),totallen-1);
        } else {
            int startposition = cds.GetTotalRange().GetTo();
            if(startposition+extrabases >= totallen) {
                right = totallen-1;
                extraNs5p = startposition+extrabases-totallen+1;
            } else {
                right = startposition+extrabases;
            }
            left = max(0,startposition-2-start.Right());
        }


        CRef<CSeq_loc> startarea(new CSeq_loc());
        CSeq_interval& d = startarea->SetInt();
        d.SetStrand(cds.GetStrand());
        CRef<CSeq_id> id(new CSeq_id());
        id->Assign(*seq_id);
        d.SetId(*id);
        d.SetFrom(left);
        d.SetTo(right);
        
        CSeqVector sttvec(*startarea, scope);
        sttvec.SetIupacCoding();
        CEResidueVec sttseq;
        sttseq.resize(extraNs5p, enN);                         // fill with Ns if necessery
        for(unsigned int i = 0; i < sttvec.size(); ++i) {
            sttseq.push_back(fromACGT(sttvec[i]));
        }
        sttseq.resize(5+start.Left()+start.Right(), enN);      // add Ns if necessery
        
        *startscore = start.Score(sttseq, extrabases+2);
        if(*startscore != BadScore()) {
            for(unsigned int i = 5; i < sttseq.size(); ++i) {
                *startscore -= ncdr.Score(sttseq, i);
            }
        }
    }

    return score;
}

END_SCOPE(gnomon)
END_NCBI_SCOPE
