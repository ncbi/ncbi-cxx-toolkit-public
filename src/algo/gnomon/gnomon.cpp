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
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <algo/gnomon/gnomon.hpp>
#include "gene_finder.hpp"

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
USING_SCOPE(ncbi::objects);


CGnomon::CGnomon()
: m_Repeats(false)
, m_ScanRange(CRange<TSeqPos>::GetWhole())
{
}


CGnomon::~CGnomon()
{
}


//
// set our sequence information
//
void CGnomon::SetSequence(const vector<char>& seq)
{
    m_Seq = seq;
}


void CGnomon::SetSequence(const string& str)
{
    m_Seq.clear();
    m_Seq.reserve(str.length());
    ITERATE (string, iter, str) {
        m_Seq.push_back(*iter);
    }
}


void CGnomon::SetSequence(const CSeqVector& vec)
{
    CSeqVector my_vec(vec);
    my_vec.SetCoding(CSeq_data::e_Iupacna);

    m_Seq.clear();
    m_Seq.reserve(my_vec.size());
    CSeqVector_CI iter(my_vec);
    for ( ;  iter;  ++iter) {
        m_Seq.push_back(*iter);
    }
}


//
// set the model data file
// we save this as a string, as it is read multiple times
//
void CGnomon::SetModelData(const string& data_file)
{
    m_ModelFile = data_file;
}


//
// set the a priori information
//
void CGnomon::SetAprioriInfo(const string& file)
{
    m_Clusters.reset(new CClusterSet());
    if (file.empty()) {
        return;
    }

    CNcbiIfstream from(file.c_str());
    from >> *m_Clusters;
}



//
// set the frame shift information
//
void CGnomon::SetFrameShiftInfo(const string& file)
{
    m_Fshifts.reset(new TFrameShifts());
    if (file.empty()) {
        return;
    }

    CNcbiIfstream from(file.c_str());
    int loc;
    while(from >> loc)
    {
        char is_i;
        char c = 'N';
        from >> is_i;
        if(is_i == '+') {
            from >> c;
        }
        m_Fshifts->push_back(CFrameShiftInfo(loc, (is_i == '+'), c));
    }
}


//
// set the repeats flag
//
void CGnomon::SetRepeats(bool b)
{
    m_Repeats = b;
}


//
// set our scan range
//
void CGnomon::SetScanRange(const CRange<TSeqPos>& range)
{
    m_ScanRange = range;
}


//
// set the scan start position
//
void CGnomon::SetScanFrom(TSeqPos from)
{
    m_ScanRange.SetFrom(from);
}


//
// set the scan stop position
//
void CGnomon::SetScanTo(TSeqPos to)
{
    m_ScanRange.SetTo(to);
}

//
// helper function - convert a vector<IPair> into a compact CSeq_loc
//
static inline
CRef<CSeq_loc> s_ExonDataToLoc(const vector<IPair>& vec,
                               ENa_strand strand, CSeq_id& id)
{
    CRef<CSeq_loc> loc(new CSeq_loc());

    CPacked_seqint::Tdata data;
    ITERATE (vector<IPair>, iter, vec) {
        CRef<CSeq_interval> ival(new CSeq_interval());
        ival->SetId(id);
        ival->SetStrand(strand);
        ival->SetFrom(iter->first);
        ival->SetTo  (iter->second);

        data.push_back(ival);
    }

    if (data.size() == 1) {
        loc->SetInt(*data.front());
    } else {
        loc->SetPacked_int().Set().swap(data);
    }

    return loc;
}


//
// Run() performs all the housekeeping necessary for GNOMON to work
//
void CGnomon::Run(void)
{
    // compute the GC content of the sequence
    TSeqPos gc_content = 0;
    ITERATE (vector<char>, iter, m_Seq) {
        int c = toupper(*iter);
        if (c == 'C'  ||  c == 'G') {
            ++gc_content;
        }
    }
    gc_content = static_cast<int>
        (gc_content*100.0 / (m_Seq.size()) + 0.4999999);

    //
    // determine our scan range
    //
    TSeqPos from = m_ScanRange.GetFrom();
    TSeqPos to   = m_ScanRange.GetTo();

    if (m_ScanRange == CRange<TSeqPos>::GetWhole()) {
        from = 0;
        to   = m_Seq.size();
    }

    // TSeqPos is unsigned, so from is always > 0
    to = min(to, (TSeqPos)m_Seq.size());
    if (from > to) {
        swap (from, to);
    }

    //
    // GNOMON setup
    //
    auto_ptr<Terminal> donorp;
    if(m_ModelFile.find("Human.inp") == m_ModelFile.length() - 9) {
        donorp.reset(new MDD_Donor(m_ModelFile, gc_content));
    } else {
        donorp.reset(new WAM_Donor<2>(m_ModelFile,gc_content));
    }

    WAM_Acceptor<2>       acceptor      (m_ModelFile, gc_content);
    WMM_Start             start         (m_ModelFile, gc_content);
    WAM_Stop              stop          (m_ModelFile, gc_content);
    MC3_CodingRegion<5>   cdr           (m_ModelFile, gc_content);
    MC_NonCodingRegion<5> intron_reg    (m_ModelFile, gc_content);
    MC_NonCodingRegion<5> intergenic_reg(m_ModelFile, gc_content);

    Intron::Init    (m_ModelFile, gc_content, to - from + 1);
    Intergenic::Init(m_ModelFile, gc_content, to - from + 1);
    Exon::Init      (m_ModelFile, gc_content);

    bool leftwall = true;
    bool rightwall = true;

    if ( !m_Clusters.get() ) {
        m_Clusters.reset(new CClusterSet());
    }

    if ( !m_Fshifts.get() ) {
        m_Fshifts.reset(new TFrameShifts());
    }

    SeqScores ss(acceptor, *donorp, start, stop, cdr, intron_reg, 
                 intergenic_reg, m_Seq, from, to - from + 1,
                 *m_Clusters, *m_Fshifts, m_Repeats,
                 leftwall, rightwall, m_ModelFile);
    HMM_State::SetSeqScores(ss);

    m_Annot.Reset(new CSeq_annot());
    m_Annot->AddName("GNOMON gene scan output");

    Parse parse(ss);
    list<Gene> genes = parse.GetGenes();

    CRef<CSeq_id> id(new CSeq_id("lcl|gnomon"));

    CSeq_annot::C_Data::TFtable& ftable = m_Annot->SetData().SetFtable();

    size_t counter = 0;
    string locus_tag_base("GNOMON_");
    ITERATE (list<Gene>, it, genes) {
        const Gene& igene = *it;
        int strand = igene.Strand();

        vector<IPair> mrna_vec;
        vector<IPair> cds_vec;

        for (size_t j = 0;  j < igene.size();  ++j) {
            const ExonData& exon = igene[j];
            int a = exon.Start();
            int b = exon.Stop();

            if (j == 0  ||  igene[j-1].Stop()+1 != a) {
                // new exon
                mrna_vec.push_back(IPair(a,b));
            } else {
                // attaching cds-part to utr-part
                mrna_vec.back().second = b;
            } 

            if (exon.Type().find("Cds") == 0) {
                cds_vec.push_back(IPair(a,b));
            }
        }

        // stop-codon removal from cds
        if (cds_vec.size()  &&
            strand == Plus  &&  igene.RightComplete()) {
            cds_vec.back().second -= 3;
        }
        if (cds_vec.size()  &&
            strand == Minus  &&  igene.LeftComplete()) {
            cds_vec.front().first += 3;
        }

        //
        // create our mRNA
        CRef<CSeq_feat> feat_mrna;
        if (mrna_vec.size()) {
            feat_mrna = new CSeq_feat();
            feat_mrna->SetData().SetRna().SetType(CRNA_ref::eType_mRNA);
            feat_mrna->SetLocation
                (*s_ExonDataToLoc(mrna_vec,
                 (strand == Plus ? eNa_strand_plus : eNa_strand_minus), *id));
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
                 (strand == Plus ? eNa_strand_plus : eNa_strand_minus), *id));
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
            (strand == Plus ? eNa_strand_plus : eNa_strand_minus);

        const CSeq_id& loc_id = sequence::GetId(feat_mrna->GetLocation(), 0);

        feat_gene->SetLocation().SetId
            (sequence::GetId(feat_mrna->GetLocation(), 0));

        ftable.push_back(feat_gene);
        ftable.push_back(feat_mrna);
        if (feat_cds) {
            ftable.push_back(feat_cds);
        }
    }
}

CRef<CSeq_annot> CGnomon::GetAnnot(void) const
{
    return m_Annot;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.10  2005/04/26 17:28:26  dicuccio
 * Fix compiler warning
 *
 * Revision 1.9  2004/11/18 21:27:40  grichenk
 * Removed default value for scope argument in seq-loc related functions.
 *
 * Revision 1.8  2004/11/12 17:39:36  dicuccio
 * Temporary fix: don't segfault if no CDS features can be created
 *
 * Revision 1.7  2004/07/28 12:33:19  dicuccio
 * Sync with Sasha's working tree
 *
 * Revision 1.6  2004/06/17 00:47:03  ucko
 * Remember to #include <stdio.h> due to use of sprintf().
 * Indent with spaces, not tabs.
 *
 * Revision 1.5  2004/06/16 12:00:57  dicuccio
 * Create dummy gene features to go with GNOMON mRNA features
 *
 * Revision 1.4  2004/05/21 21:41:03  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.3  2003/11/06 00:51:05  ucko
 * Adjust usage of min for platforms with 64-bit size_t.
 *
 * Revision 1.2  2003/11/05 21:15:24  ucko
 * Fixed usage of ITERATE.  (m_Seq is a vector, not a string.)
 *
 * Revision 1.1  2003/10/24 15:07:25  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */
