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
* Authors:  Eugene Vasilchenko, Aleksey Grichenko, Denis Vakatov
*
* File Description:
*   Bio sequence data generator to test Object Manager
*/

#include <ncbi_pch.hpp>
#include "test_helper.hpp"
#include <corelib/ncbithr.hpp>
#include <objects/seqloc/Seq_point.hpp>
#include <serial/object.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objcopy.hpp>
#include <serial/objectinfo.hpp>
#include <serial/iterator.hpp>
#include <serial/objectiter.hpp>
#include <serial/serial.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/IUPACna.hpp>
#include <objects/seq/NCBIeaa.hpp>
#include <objects/seq/NCBI2na.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seg_ext.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqalign/Dense_diag.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_vector_ci.hpp>
#include <objmgr/seq_descr_ci.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/align_ci.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/general/Date.hpp>
// #include <objects/util/sequence.hpp>

#include <test/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

bool CDataGenerator::sm_DumpEntries = false;

/************************************************************************
    1.2.1.1. Bio sequences for testing
    Test entry = 1 top-level entry + 2 sub-entries (continuous sequence)
    TSE contains 1 description
    each sub-entry has
        two Seq_ids: local and GI, (local ids = 11+1000i, 12+1000i)
        one description,
        two annotations (Seq_feat)
            first - for an interval, local Seq_id:
                "plus" strand for one sub-entry,
                "minus" strand for another one
            second - for the whole seq, GI Seq_id
    one sub-entry has also an alignment annotation
************************************************************************/
CSeq_entry& CDataGenerator::CreateTestEntry1(int index)
{
    // create top level seq entry
    CRef<CSeq_entry> entry(new CSeq_entry);
    CBioseq_set& set = entry->SetSet();
    // class = nucleic acid and coded proteins
    set.SetClass(CBioseq_set::eClass_nuc_prot);
    // add description (name)
    list< CRef<CSeqdesc> >& descr = set.SetDescr().Set();
    CRef<CSeqdesc> desc(new CSeqdesc);
    desc->SetName("D1 from TSE1-"+NStr::IntToString(index));
    descr.push_back(desc);
    // list of sub-entries
    list< CRef<CSeq_entry> >& seq_set = set.SetSeq_set();

    // Sub-entry Seq 11
    {{
        CRef<CSeq_entry> sub_entry(new CSeq_entry);
        CBioseq& seq = sub_entry->SetSeq();
        CBioseq::TId& id_list = seq.SetId();

        // list of Ids (local + gi)
        CRef<CSeq_id> id(new CSeq_id);
        id->SetLocal().SetStr("seq"+NStr::IntToString(11+index*1000));
        id_list.push_back(id);
        id.Reset(new CSeq_id);
        id->SetGi(11+index*1000);
        id_list.push_back(id);

        // Description (name)
        list< CRef<CSeqdesc> >& descr1 = seq.SetDescr().Set();
        CRef<CSeqdesc> desc1(new CSeqdesc);
        desc1->SetName("D1 from BS11-"+NStr::IntToString(index));
        descr1.push_back(desc1);

        // Instance (sequence data)
        CSeq_inst& inst = seq.SetInst();
        // representation class = continuous sequence
        inst.SetRepr(CSeq_inst::eRepr_raw);
        // molecule class in living organism = dna
        inst.SetMol(CSeq_inst::eMol_dna);
        // length of sequence in residues
        inst.SetLength(40);
        // seq data in Iupacna
        inst.SetSeq_data().SetIupacna().Set(
            "CAGCAGCGGTACAGGAGGGTGAGACATCCCAGAGCGGTGC");
        // strandedness in living organism = double strand
        inst.SetStrand(CSeq_inst::eStrand_ds);

        // Annotations
        list< CRef<CSeq_annot> >& annot_list = seq.SetAnnot();
        CRef<CSeq_annot> annot(new CSeq_annot);
        // list of features
        list< CRef<CSeq_feat> >& ftable = annot->SetData().SetFtable();
        {{
            CRef<CSeq_feat> feat(new CSeq_feat);
            // feature Id
            feat->SetId().SetLocal().SetStr("F1: lcl|11");
            // the specific data
            CSeqFeatData& fdata = feat->SetData();
            CCdregion& cdreg = fdata.SetCdregion();
            cdreg.SetFrame(CCdregion::eFrame_one);
            // genetic code used
            list< CRef< CGenetic_code::C_E > >& gcode = cdreg.SetCode().Set();
            CRef< CGenetic_code::C_E > ce(new CGenetic_code::C_E);
            ce->SetId(111); // TSE=1; seq=1; feat=1
            gcode.push_back(ce);
            // feature location on the sequence: Seq_interval (local seq_Id)
            CSeq_interval& floc = feat->SetLocation().SetInt();
            floc.SetId().SetLocal().SetStr
                ("seq"+NStr::IntToString(11+index*1000));
            floc.SetFrom(20);
            floc.SetTo(30);
            ftable.push_back(feat);
        }}
        {{
            CRef<CSeq_feat> feat(new CSeq_feat);
            // feature Id
            feat->SetId().SetLocal().SetStr("F2: gi|11");
            // the specific data
            CSeqFeatData& fdata = feat->SetData();
            CCdregion& cdreg = fdata.SetCdregion();
            cdreg.SetFrame(CCdregion::eFrame_one);
            // genetic code used
            list< CRef< CGenetic_code::C_E > >& gcode = cdreg.SetCode().Set();
            CRef< CGenetic_code::C_E > ce(new CGenetic_code::C_E);
            ce->SetId(112); // TSE=1; seq=1; feat=2
            gcode.push_back(ce);
            // feature location on the sequence (seq_Id + "whole" sequence)
            feat->SetLocation().SetWhole().SetGi(11+index*1000);
            ftable.push_back(feat);
        }}
        annot_list.push_back(annot);

        // Add sub-entry
        seq_set.push_back(sub_entry);
    }}

    // Sub-entry Seq 12
    {{
        CRef<CSeq_entry> sub_entry(new CSeq_entry);
        CBioseq& seq = sub_entry->SetSeq();
        CBioseq::TId& id_list = seq.SetId();

        // list of Ids (local + gi)
        CRef<CSeq_id> id(new CSeq_id);
        id->SetLocal().SetStr("seq"+NStr::IntToString(12+index*1000));
        id_list.push_back(id);
        id.Reset(new CSeq_id);
        id->SetGi(12+index*1000);
        id_list.push_back(id);

        // Instance (sequence data)
        CSeq_inst& inst = seq.SetInst();
        // representation class = continuous sequence
        inst.SetRepr(CSeq_inst::eRepr_raw);
        // molecule class in living organism = dna
        inst.SetMol(CSeq_inst::eMol_dna);
        // length of sequence in residues
        inst.SetLength(40);
        // seq data in Iupacna
        inst.SetSeq_data().SetIupacna().Set(
            "CAATAACCTCAGCAGCAACAAGTGGCTTCCAGCGCCCTCC");
        // strandedness in living organism = double strand
        inst.SetStrand(CSeq_inst::eStrand_ds);

        // Annotations
        list< CRef<CSeq_annot> >& annot_list = seq.SetAnnot();
        {{
            CRef<CSeq_annot> annot(new CSeq_annot);
            // list of features
            list< CRef<CSeq_feat> >& ftable = annot->SetData().SetFtable();
            {{
                CRef<CSeq_feat> feat(new CSeq_feat);
                // feature Id
                feat->SetId().SetLocal().SetStr("F3: gi|12");
                // the specific data
                CSeqFeatData& fdata = feat->SetData();
                CCdregion& cdreg = fdata.SetCdregion();
                cdreg.SetFrame(CCdregion::eFrame_one);
                // genetic code used
                list< CRef< CGenetic_code::C_E > >& gcode =
                    cdreg.SetCode().Set();
                CRef< CGenetic_code::C_E > ce(new CGenetic_code::C_E);
                ce->SetId(123); // TSE=1; seq=2; feat=3
                gcode.push_back(ce);
                // feature location on the sequence: Seq_interval (gi seq_Id)
                CSeq_interval& floc = feat->SetLocation().SetInt();
                floc.SetId().SetGi(12+index*1000);
                floc.SetFrom(20);
                floc.SetTo(30);
                // minus strand
                floc.SetStrand(eNa_strand_minus);
                ftable.push_back(feat);
            }}
            {{
                CRef<CSeq_feat> feat(new CSeq_feat);
                // feature Id
                feat->SetId().SetLocal().SetStr("F4: lcl|12");
                // the specific data
                CSeqFeatData& fdata = feat->SetData();
                CCdregion& cdreg = fdata.SetCdregion();
                cdreg.SetFrame(CCdregion::eFrame_one);
                // genetic code used
                list< CRef< CGenetic_code::C_E > >& gcode =
                    cdreg.SetCode().Set();
                CRef< CGenetic_code::C_E > ce(new CGenetic_code::C_E);
                ce->SetId(124); // TSE=1; seq=2; feat=4
                gcode.push_back(ce);
                // feature location on the sequence (seq_Id + "whole" sequence)
                feat->SetLocation().SetWhole().SetLocal().SetStr
                    ("seq"+NStr::IntToString(12+index*1000));
                ftable.push_back(feat);
            }}
            annot_list.push_back(annot);
        }}
        {{
            CRef<CSeq_annot> annot(new CSeq_annot);
            // list of seq alignments
            list< CRef<CSeq_align> >& atable = annot->SetData().SetAlign();
            {{
                // CAGCAGC:
                // 11[0], 12[9]
                CRef<CSeq_align> align(new CSeq_align);
                align->SetType(CSeq_align::eType_not_set);
                // alignment data
                CSeq_align::C_Segs& segs = align->SetSegs();
                // for (multiway) diagonals
                CSeq_align::C_Segs::TDendiag& diag_list = segs.SetDendiag();
                CRef<CDense_diag> diag(new CDense_diag);
                // dimensionality = 2
                diag->SetDim(2);
                // list of Seq_ids (gi) (sequences in order)
                CDense_diag::TIds& idlist = diag->SetIds();
                CRef<CSeq_id> sid(new CSeq_id);
                sid->SetGi(11+index*1000);
                idlist.push_back(sid);
                sid.Reset(new CSeq_id);
                sid->SetGi(12+index*1000);
                idlist.push_back(sid);
                // start OFFSETS in ids order
                CDense_diag::TStarts& start_list = diag->SetStarts();
                start_list.push_back(0);
                start_list.push_back(9);
                diag->SetLen(7);
                diag_list.push_back(diag);
                atable.push_back(align);
            }}
            annot_list.push_back(annot);
        }}
        // Add sub-entry
        seq_set.push_back(sub_entry);
    }}

    if ( sm_DumpEntries ) {
        NcbiCout << "-------------------- "
            "TestEntry1 --------------------" << NcbiEndl;
        auto_ptr<CObjectOStream>
            out(CObjectOStream::Open(eSerial_AsnText, NcbiCout));
        *out << *entry;
    }
    return *entry.Release();
}


/************************************************************************
    1.2.1.2. Bio sequences for testing
    Test entry = 1 top-level entry + 2 sub-entries (continuous sequence)
    each sub-entry has
        two Seq_ids: local and GI, (local ids = 11+1000i, 12+1000i)
    No descriptions, No annotations
************************************************************************/
CSeq_entry& CDataGenerator::CreateTestEntry1a(int index)
{
    // create top level seq entry
    CRef<CSeq_entry> entry(new CSeq_entry);
    CBioseq_set& set = entry->SetSet();
    // class = nucleic acid and coded proteins
    set.SetClass(CBioseq_set::eClass_nuc_prot);
    list< CRef<CSeq_entry> >& seq_set = set.SetSeq_set();

    // Sub-entry Seq 11
    {{
        CRef<CSeq_entry> sub_entry(new CSeq_entry);
        CBioseq& seq = sub_entry->SetSeq();
        CBioseq::TId& id_list = seq.SetId();

        // list of Ids (local + gi)
        CRef<CSeq_id> id(new CSeq_id);
        id->SetLocal().SetStr("seq"+NStr::IntToString(11+index*1000));
        id_list.push_back(id);
        id.Reset(new CSeq_id);
        id->SetGi(11+index*1000);
        id_list.push_back(id);

        // Instance (sequence data)
        CSeq_inst& inst = seq.SetInst();
        // representation class = continuous sequence
        inst.SetRepr(CSeq_inst::eRepr_raw);
        // molecule class in living organism = dna
        inst.SetMol(CSeq_inst::eMol_dna);
        // length of sequence in residues
        inst.SetLength(40);
        // seq data in Iupacna
        inst.SetSeq_data().SetIupacna().Set(
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
        // strandedness in living organism = double strand
        inst.SetStrand(CSeq_inst::eStrand_ds);

        // Add sub-entry
        seq_set.push_back(sub_entry);
    }}

    // Sub-entry Seq 12
    {{
        CRef<CSeq_entry> sub_entry(new CSeq_entry);
        CBioseq& seq = sub_entry->SetSeq();
        CBioseq::TId& id_list = seq.SetId();

        // list of Ids (local + gi)
        CRef<CSeq_id> id(new CSeq_id);
        id->SetLocal().SetStr("seq"+NStr::IntToString(12+index*1000));
        id_list.push_back(id);
        id.Reset(new CSeq_id);
        id->SetGi(12+index*1000);
        id_list.push_back(id);

        // Instance (sequence data)
        CSeq_inst& inst = seq.SetInst();
        inst.SetRepr(CSeq_inst::eRepr_raw);
        inst.SetMol(CSeq_inst::eMol_dna);
        // length of sequence in residues
        inst.SetLength(40);
        // seq data in Iupacna
        inst.SetSeq_data().SetIupacna().Set(
            "TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT");
        inst.SetStrand(CSeq_inst::eStrand_ds);

        // Add sub-entry
        seq_set.push_back(sub_entry);
    }}

    if ( sm_DumpEntries ) {
        NcbiCout << "-------------------- "
            "TestEntry1a --------------------" << NcbiEndl;
        auto_ptr<CObjectOStream>
            out(CObjectOStream::Open(eSerial_AsnText, NcbiCout));
        *out << *entry;
    }
    return *entry.Release();
}


/************************************************************************
    1.2.1.3. Bio sequences for testing
    Test entry = 1 top-level entry + 3 sub-entries
    TSE
        one annotation (Seq_feat), which refers to ids 12+1000i and 21+1000i
    1 sub-entry
        two Seq_ids: local and GI, (local id = 21+1000i)
        one description
        segmented sequence: references to
            an interval, ( id= 11+1000i)
            whole seq,   ( id= 12+1000i)
            self         ( id= 21+1000i)
        two annotations (Seq_feat)
            both - for an interval,  local Seq_id = 11+1000i
    2 sub-entry
        two Seq_ids: local and GI, (local id = 22+1000i)
        continuous sequence
    3 sub-entry
        two Seq_ids: local and GI, (local id = 23+1000i)
        continuous sequence
        conversion from one seq.data format to another
************************************************************************/
CSeq_entry& CDataGenerator::CreateTestEntry2(int index)
{
    // create top level seq entry
    CRef<CSeq_entry> entry(new CSeq_entry);
    CBioseq_set& set = entry->SetSet();
    // class = nucleic acid and coded proteins
    set.SetClass(CBioseq_set::eClass_nuc_prot);
    list< CRef<CSeq_entry> >& seq_set = set.SetSeq_set();

    // Sub-entry Seq 21
    {{
        CRef<CSeq_entry> sub_entry(new CSeq_entry);
        CBioseq& seq = sub_entry->SetSeq();
        CBioseq::TId& id_list = seq.SetId();

        // list of Ids (local + gi)
        CRef<CSeq_id> id(new CSeq_id);
        id->SetLocal().SetStr("seq"+NStr::IntToString(21+index*1000));
        id_list.push_back(id);
        id.Reset(new CSeq_id);
        id->SetGi(21+index*1000);
        id_list.push_back(id);

        // Description (name)
        list< CRef<CSeqdesc> >& descr1 = seq.SetDescr().Set();
        CRef<CSeqdesc> desc1(new CSeqdesc);
        desc1->SetName("D3 from bioseq 21-"+NStr::IntToString(index));
        descr1.push_back(desc1);

        // Instance (sequence data)
        CSeq_inst& inst = seq.SetInst();
        // representation class = segmented sequence
        inst.SetRepr(CSeq_inst::eRepr_seg);
        // molecule class in living organism = dna
        inst.SetMol(CSeq_inst::eMol_dna);
        // segments = list of Seq_locs
        CSeg_ext::Tdata& seg_list = inst.SetExt().SetSeg();

        // Seq_interval (gi)
        CRef<CSeq_loc> loc(new CSeq_loc);
        CSeq_interval& ref_loc = loc->SetInt();
        ref_loc.SetId().SetGi(11+index*1000);
        ref_loc.SetFrom(0);
        ref_loc.SetTo(4);
        seg_list.push_back(loc);

        // whole sequence (gi)
        loc = new CSeq_loc;
        loc->SetWhole().SetGi(12+index*1000);
        seg_list.push_back(loc);

        // Seq_interval (gi)
        loc = new CSeq_loc;
        CSeq_interval& ref_loc2 = loc->SetInt();
        // "simple" self-reference
        ref_loc2.SetId().SetGi(21+index*1000);
        ref_loc2.SetFrom(0);
        ref_loc2.SetTo(9);
        seg_list.push_back(loc);

        // More complicated self-reference
        loc = new CSeq_loc;
        CSeq_interval& ref_loc3 = loc->SetInt();
        ref_loc3.SetId().SetGi(21+index*1000);
        ref_loc3.SetFrom(54);
        ref_loc3.SetTo(60);
        seg_list.push_back(loc);

        inst.SetStrand(CSeq_inst::eStrand_ds);

        // Annotations
        list< CRef<CSeq_annot> >& annot_list = seq.SetAnnot();
        CRef<CSeq_annot> annot(new CSeq_annot);
        // list of features
        list< CRef<CSeq_feat> >& ftable = annot->SetData().SetFtable();
        {{
            CRef<CSeq_feat> feat(new CSeq_feat);
            // feature Id
            feat->SetId().SetLocal().SetStr("F5: lcl|11");
            // the specific data
            CSeqFeatData& fdata = feat->SetData();
            CCdregion& cdreg = fdata.SetCdregion();
            cdreg.SetFrame(CCdregion::eFrame_one);
            // genetic code used
            list< CRef< CGenetic_code::C_E > >& gcode = cdreg.SetCode().Set();
            CRef< CGenetic_code::C_E > ce(new CGenetic_code::C_E);
            ce->SetId(215); // TSE=2; seq=1; feat=5
            gcode.push_back(ce);
            // feature location on the sequence: Seq_interval (local seq_Id)
            CSeq_interval& floc = feat->SetLocation().SetInt();
            floc.SetId().SetLocal().SetStr
                ("seq"+NStr::IntToString(11+index*1000));
            floc.SetFrom(20);
            floc.SetTo(30);
            // plus strand
            floc.SetStrand(eNa_strand_plus);
            ftable.push_back(feat);
        }}
        {{
            CRef<CSeq_feat> feat(new CSeq_feat);
            // feature Id
            feat->SetId().SetLocal().SetStr("F6: gi|11");
            // the specific data
            CSeqFeatData& fdata = feat->SetData();
            CCdregion& cdreg = fdata.SetCdregion();
            cdreg.SetFrame(CCdregion::eFrame_one);
            // genetic code used
            list< CRef< CGenetic_code::C_E > >& gcode = cdreg.SetCode().Set();
            CRef< CGenetic_code::C_E > ce(new CGenetic_code::C_E);
            ce->SetId(216); // TSE=2; seq=1; feat=6
            gcode.push_back(ce);
            // feature location on the sequence: Seq_interval (gi seq_Id)
            CSeq_interval& floc = feat->SetLocation().SetInt();
            floc.SetId().SetGi(11+index*1000);
            floc.SetFrom(5);
            floc.SetTo(15);
            ftable.push_back(feat);
        }}
        annot_list.push_back(annot);

        // Add sub-entry
        seq_set.push_back(sub_entry);
    }}

    // Sub-entry Seq 22
    {{
        CRef<CSeq_entry> sub_entry(new CSeq_entry);
        CBioseq& seq = sub_entry->SetSeq();
        CBioseq::TId& id_list = seq.SetId();

        // list of Ids (local + gi)
        CRef<CSeq_id> id(new CSeq_id);
        id->SetLocal().SetStr
            ("seq"+NStr::IntToString(22+index*1000));
        id_list.push_back(id);
        id.Reset(new CSeq_id);
        id->SetGi(22+index*1000);
        id_list.push_back(id);

        // Instance (sequence data)
        CSeq_inst& inst = seq.SetInst();
        // representation class = continuous sequence
        inst.SetRepr(CSeq_inst::eRepr_raw);
        // molecule class in living organism = aa (amino acid)
        inst.SetMol(CSeq_inst::eMol_aa);
        // length of sequence in residues
        inst.SetLength(20);
        // seq data in Ncbieaa
        inst.SetSeq_data().SetNcbieaa().Set("QGCGEQTMTLLAPTLAASRY");
        // single strand
        inst.SetStrand(CSeq_inst::eStrand_ss);

        // Add sub-entry
        seq_set.push_back(sub_entry);
    }}

    // Sub-entry Seq 23
    {{
        CRef<CSeq_entry> sub_entry(new CSeq_entry);
        CBioseq& seq = sub_entry->SetSeq();
        CBioseq::TId& id_list = seq.SetId();

        // list of Ids (local + gi)
        CRef<CSeq_id> id(new CSeq_id);
        id->SetLocal().SetStr
            ("seq"+NStr::IntToString(23+index*1000));
        id_list.push_back(id);
        id.Reset(new CSeq_id);
        id->SetGi(23+index*1000);
        id_list.push_back(id);

        // Instance (sequence data)
        CSeq_inst& inst = seq.SetInst();
        // representation class = continuous sequence
        inst.SetRepr(CSeq_inst::eRepr_raw);
        // molecule class in living organism = dna
        inst.SetMol(CSeq_inst::eMol_dna);
        // length of sequence in residues
        inst.SetLength(13);
        // seq data in Iupacna
        inst.SetSeq_data().SetIupacna().Set("ATGCAGCTGTACG");
        // double strand
        inst.SetStrand(CSeq_inst::eStrand_ds);

        CRef<CSeq_data> packed_data(new CSeq_data);
        // convert seq data to another format
        // (NCBI2na = 2 bit nucleic acid code)
        CSeqportUtil::Convert(inst.GetSeq_data(),
                              packed_data,
                              CSeq_data::e_Ncbi2na);
        inst.SetSeq_data(*packed_data);

        // Add sub-entry
        seq_set.push_back(sub_entry);
    }}

    // Annotations
    list< CRef<CSeq_annot> >& set_annot_list = set.SetAnnot();
    CRef<CSeq_annot> annot(new CSeq_annot);
    // list of features
    list< CRef<CSeq_feat> >& ftable = annot->SetData().SetFtable();
    {{
        CRef<CSeq_feat> feat(new CSeq_feat);
        // feature Id
        feat->SetId().SetLocal().SetStr("F7: lcl|21 + gi|12");
        // the specific data
        CSeqFeatData& fdata = feat->SetData();
        CCdregion& cdreg = fdata.SetCdregion();
        cdreg.SetFrame(CCdregion::eFrame_one);
        // genetic code used
        list< CRef< CGenetic_code::C_E > >& gcode = cdreg.SetCode().Set();
        CRef< CGenetic_code::C_E > ce(new CGenetic_code::C_E);
        ce->SetId(207); // TSE=2; no seq; feat=7
        gcode.push_back(ce);
        // feature location on the sequence: Seq_interval (local seq_Id)
        CSeq_interval& floc = feat->SetLocation().SetInt();
        floc.SetId().SetLocal().SetStr
            ("seq"+NStr::IntToString(21+index*1000));
        floc.SetFrom(1);
        floc.SetTo(20);
        // product of process (seq_loc = whole, gi)
        feat->SetProduct().SetWhole().SetGi(12+index*1000);
        ftable.push_back(feat);
    }}
    set_annot_list.push_back(annot);

    if ( sm_DumpEntries ) {
        NcbiCout << "-------------------- "
            "TestEntry2 --------------------" << NcbiEndl;
        auto_ptr<CObjectOStream>
            out(CObjectOStream::Open(eSerial_AsnText, NcbiCout));
        *out << *entry;
    }
    return *entry.Release();
}

/************************************************************************
    1.2.1.4. Bio sequences for testing
    Test entry = 1 top-level entry
    TSE
        only contains references to other sequences
************************************************************************/
CSeq_entry& CDataGenerator::CreateConstructedEntry(int idx, int index)
{
    CSeq_loc constr_loc;
    list< CRef<CSeq_interval> >& int_list = constr_loc.SetPacked_int();

    CRef<CSeq_interval> int_ref(new CSeq_interval);
    int_ref->SetId().SetGi(11+idx*1000);
    int_ref->SetFrom(5);
    int_ref->SetTo(10);
    if (index == 2) {
        int_ref->SetStrand(eNa_strand_minus);
    }
    int_list.push_back(int_ref);

    int_ref.Reset(new CSeq_interval);
    int_ref->SetId().SetGi(12+idx*1000);
    int_ref->SetFrom(0);
    int_ref->SetTo(20);
    int_list.push_back(int_ref);

    CRef<CBioseq> constr_seq(new CBioseq(constr_loc,
        "constructed"+NStr::IntToString(index)));
    CRef<CSeq_entry> constr_entry(new CSeq_entry);
    constr_entry->SetSeq(*constr_seq);
    if ( sm_DumpEntries ) {
        NcbiCout << "-------------------- "
            "ConstructedEntry --------------------" << NcbiEndl;
        auto_ptr<CObjectOStream>
            out(CObjectOStream::Open(eSerial_AsnText, NcbiCout));
        *out << *constr_entry;
    }
    return *constr_entry.Release();
}


/************************************************************************
    1.2.1.4. Bio sequences for testing
    Test entry = 1 top-level entry
    TSE
        Construct bioseq by excluding the seq-loc
************************************************************************/
/*
CSeq_entry& CDataGenerator::CreateConstructedExclusionEntry(int idx, int index)
{
    CSeq_loc loc;
    CSeq_loc_mix::Tdata& mix = loc.SetMix().Set();
    CRef<CSeq_loc> sl;

    sl.Reset(new CSeq_loc);
    sl->SetPnt().SetId().SetGi(11+idx*1000);
    sl->SetPnt().SetPoint(0);
    mix.push_back(sl);

    sl.Reset(new CSeq_loc);
    sl->SetInt().SetId().SetGi(11+idx*1000);
    sl->SetInt().SetFrom(0);
    sl->SetInt().SetTo(4);
    mix.push_back(sl);

    sl.Reset(new CSeq_loc);
    sl->SetInt().SetId().SetGi(11+idx*1000);
    sl->SetInt().SetFrom(10);
    sl->SetInt().SetTo(15);
    mix.push_back(sl);

    sl.Reset(new CSeq_loc);
    sl->SetInt().SetId().SetGi(11+idx*1000);
    sl->SetInt().SetFrom(10);
    sl->SetInt().SetTo(12);
    mix.push_back(sl);

    sl.Reset(new CSeq_loc);
    sl->SetInt().SetId().SetGi(11+idx*1000);
    sl->SetInt().SetFrom(13);
    sl->SetInt().SetTo(15);
    mix.push_back(sl);

    sl.Reset(new CSeq_loc);
    sl->SetInt().SetId().SetGi(11+idx*1000);
    sl->SetInt().SetFrom(8);
    sl->SetInt().SetTo(18);
    mix.push_back(sl);

    sl.Reset(new CSeq_loc);
    sl->SetPnt().SetId().SetGi(11+idx*1000);
    sl->SetPnt().SetPoint(20);
    mix.push_back(sl);

    sl.Reset(new CSeq_loc);
    sl->SetPnt().SetId().SetGi(11+idx*1000);
    sl->SetPnt().SetPoint(39);
    mix.push_back(sl);

    CRef<CBioseq> constr_ex_seq(&CBioseq::ConstructExcludedSequence(loc, 40,
        "construct_exclusion"+NStr::IntToString(index)));
    CRef<CSeq_entry> constr_ex_entry(new CSeq_entry);
    constr_ex_entry->SetSeq(*constr_ex_seq);
    return *constr_ex_entry.Release();
}
*/


CSeq_annot& CDataGenerator::CreateAnnotation1(int index)
{
    CRef<CSeq_annot> annot(new CSeq_annot);
    list< CRef<CSeq_feat> >& ftable = annot->SetData().SetFtable();
    {{
        CRef<CSeq_feat> feat(new CSeq_feat);
        feat->SetId().SetLocal().SetStr("attached feature");
        CSeqFeatData& fdata = feat->SetData();
        CCdregion& cdreg = fdata.SetCdregion();
        cdreg.SetFrame(CCdregion::eFrame_one);
        list< CRef< CGenetic_code::C_E > >& gcode = cdreg.SetCode().Set();
        CRef< CGenetic_code::C_E > ce(new CGenetic_code::C_E);
        ce->SetId(999);
        gcode.push_back(ce);
        feat->SetComment() = "Feature attached after indexing the entry";
        //feat->SetLocation().SetWhole().SetGi(11+index*1000);

        CSeq_interval& interval = feat->SetLocation().SetInt();
        interval.SetId().SetGi(11+index*1000);
        interval.SetFrom(1);
        interval.SetTo(9);
        interval.SetStrand(eNa_strand_unknown);

        ftable.push_back(feat);
    }}
    if ( sm_DumpEntries ) {
        NcbiCout << "-------------------- "
            "Annotation1 --------------------" << NcbiEndl;
        auto_ptr<CObjectOStream>
            out(CObjectOStream::Open(eSerial_AsnText, NcbiCout));
        *out << *annot;
    }
    return *annot.Release();
}


/////////////////////////////////////////////////////////////////////////////
/************************************************************************
    1.2.2. Test function
    Input: scope + seq_id
        other parameters - only to verify results.
    Operations:
        investigate SeqMap, verify lengths
        investigate SeqVector, verify seq.data (as strings)
        investigate SeqMap again - some references may be resolved now
        enumerate descriptions
        enumerate features for the whole sequence
        enumerate features for an interval
        enumerate alignments for the whole sequence
        enumerate alignments for an interval
************************************************************************/
#define CHECK_WRAP() \
    {{ \
        bool got_exception = false; \
        try {
#define CHECK_END2(MSG, have_errors) \
        } catch (exception) { \
            got_exception = true; \
            if ( !have_errors ) { \
                LOG_POST("Can not " MSG); throw; \
            } \
        } \
        if ( have_errors && !got_exception ) { \
            THROW1_TRACE(runtime_error, \
                         "Managed to " MSG " of erroneous sequence"); \
        } \
    }}
#define CHECK_END(MSG)         CHECK_END2(MSG, have_errors)
#define CHECK_END_ALWAYS(MSG)  CHECK_END2(MSG, false)


bool CTestHelper::sm_DumpFeatures = false;
#if defined _MT
bool CTestHelper::sm_TestRemoveEntry = false;
#else 
bool CTestHelper::sm_TestRemoveEntry = true;
#endif

void CTestHelper::ProcessBioseq(CScope& scope, CSeq_id& id,
                                TSeqPos seq_len,
                                string seq_str, string seq_str_compl,
                                int seq_desc_cnt,
                                int seq_feat_ra_cnt, // resolve-all method
                                int seq_feat_cnt, int seq_featrg_cnt,
                                int seq_align_cnt, int seq_alignrg_cnt,
                                size_t feat_annots_cnt,
                                size_t featrg_annots_cnt,
                                size_t align_annots_cnt,
                                size_t alignrg_annots_cnt,
                                bool tse_feat_test,
                                bool have_errors)
{
    CBioseq_Handle handle = scope.GetBioseqHandle(id);
    if ( !handle ) {
        LOG_POST("No seq-id found");
        return;
    }
    handle.GetTopLevelEntry();
#if 0 // build order issues
    CHECK_WRAP();
    sequence::GetTitle(handle);
    CHECK_END("get sequence title");
#endif

    CBioseq_Handle::TBioseqCore seq_core = handle.GetBioseqCore();
    CHECK_WRAP();
    CConstRef<CSeqMap> seq_map(&handle.GetSeqMap());
    TSeqPos len = 0;
    _TRACE("ProcessBioseq("<<id.AsFastaString()<<") seq_len="<<seq_len<<"):");
    // Iterate seq-map except the last element
    len = 0;
    CSeqMap::const_iterator seg = seq_map->begin(&scope);
    vector<CSeqMap::const_iterator> itrs;
    for ( ; seg != seq_map->end(&scope); ++seg ) {
        _ASSERT(seg);
        itrs.push_back(seg);
        switch (seg.GetType()) {
        case CSeqMap::eSeqData:
            _TRACE('@'<<len<<": seqData("<<seg.GetLength()<<")");
            len += seg.GetLength();
            break;
        case CSeqMap::eSeqRef:
            _TRACE('@'<<len<<": seqRef("<<seg.GetLength()<<
                   ", id="<<seg.GetRefSeqid().AsString()<<
                   ", pos="<<seg.GetRefPosition()<<
                   ", minus="<<seg.GetRefMinusStrand()<<")");
            len += seg.GetLength();
            break;
        case CSeqMap::eSeqGap:
            _TRACE('@'<<len<<": seqGap("<<seg.GetLength()<<")");
            len += seg.GetLength();
            break;
        case CSeqMap::eSeqEnd:
            _ASSERT("Unexpected END segment" && 0);
            break;
        default:
            _ASSERT("Unexpected segment type" && 0);
            break;
        }
    }
    _ASSERT(!seg);
    for ( int i = itrs.size()-1; i >= 0; --i ) {
        --seg;
        _ASSERT(seg);
        _ASSERT(seg == itrs[i]);
    }
    --seg;
    _ASSERT(!seg);

    _TRACE("ProcessBioseq("<<id.AsFastaString()<<") len="<<len<<")");
    _ASSERT(len == seq_len);
    CHECK_END("get sequence map");

    CHECK_WRAP();
    CConstRef<CSeqMap> seq_map(&handle.GetSeqMap());
    TSeqPos len = 0;
    _TRACE("ProcessBioseq("<<id.AsFastaString()<<") - depth-restricted");
    // Iterate seq-map except the last element
    len = 0;
    CSeqMap::const_iterator seg(seq_map, &scope, SSeqMapSelector()
                                .SetResolveCount(2));
    vector<CSeqMap::const_iterator> itrs;
    for ( ; seg != seq_map->end(&scope); ++seg ) {
        _ASSERT(seg);
        itrs.push_back(seg);
        switch (seg.GetType()) {
        case CSeqMap::eSeqData:
            _TRACE('@'<<len<<": seqData("<<seg.GetLength()<<")");
            len += seg.GetLength();
            break;
        case CSeqMap::eSeqRef:
            _TRACE('@'<<len<<": seqRef("<<seg.GetLength()<<
                   ", id="<<seg.GetRefSeqid().AsString()<<
                   ", pos="<<seg.GetRefPosition()<<
                   ", minus="<<seg.GetRefMinusStrand()<<")");
            len += seg.GetLength();
            break;
        case CSeqMap::eSeqGap:
            _TRACE('@'<<len<<": seqGap("<<seg.GetLength()<<")");
            len += seg.GetLength();
            break;
        case CSeqMap::eSeqEnd:
            _ASSERT("Unexpected END segment" && 0);
            break;
        default:
            _ASSERT("Unexpected segment type" && 0);
            break;
        }
    }
    _ASSERT(!seg);
    for ( int i = itrs.size()-1; i >= 0; --i ) {
        --seg;
        _ASSERT(seg);
        _ASSERT(seg == itrs[i]);
    }
    --seg;
    _ASSERT(!seg);
    CHECK_END("get restricted sequence map");

    CHECK_WRAP();
    CConstRef<CSeqMap> seq_map(&handle.GetSeqMap());
    TSeqPos len = 0;
    _TRACE("ProcessBioseq("<<id.AsFastaString()<<") - TSE-restricted");
    // Iterate seq-map except the last element
    len = 0;
    CSeqMap::const_iterator seg(seq_map, &scope, SSeqMapSelector()
                                .SetLimitTSE(handle.GetTopLevelEntry())
                                .SetResolveCount(kInvalidSeqPos)
                                .SetFlags(CSeqMap::fFindAny));
    vector<CSeqMap::const_iterator> itrs;
    for ( ; seg != seq_map->end(&scope); ++seg ) {
        _ASSERT(seg);
        itrs.push_back(seg);
        switch (seg.GetType()) {
        case CSeqMap::eSeqData:
            _TRACE('@'<<len<<": seqData("<<seg.GetLength()<<")");
            len += seg.GetLength();
            break;
        case CSeqMap::eSeqRef:
            _TRACE('@'<<len<<": seqRef("<<seg.GetLength()<<
                   ", id="<<seg.GetRefSeqid().AsString()<<
                   ", pos="<<seg.GetRefPosition()<<
                   ", minus="<<seg.GetRefMinusStrand()<<")");
            len += seg.GetLength();
            break;
        case CSeqMap::eSeqGap:
            _TRACE('@'<<len<<": seqGap("<<seg.GetLength()<<")");
            len += seg.GetLength();
            break;
        case CSeqMap::eSeqEnd:
            _ASSERT("Unexpected END segment" && 0);
            break;
        default:
            _ASSERT("Unexpected segment type" && 0);
            break;
        }
    }
    _ASSERT(!seg);
    for ( int i = itrs.size()-1; i >= 0; --i ) {
        --seg;
        _ASSERT(seg);
        _ASSERT(seg == itrs[i]);
    }
    --seg;
    _ASSERT(!seg);
    CHECK_END("get restricted sequence map");

    CHECK_WRAP();
    CConstRef<CSeqMap> seq_map(&handle.GetSeqMap());
    TSeqPos len = 0;
    _TRACE("ProcessBioseq("<<id.AsFastaString()<<
           ") seq_len="<<seq_len<<") resolved:");
    // Iterate seq-map except the last element
    len = 0;
    CSeqMap::const_iterator seg = seq_map->BeginResolved(&scope);
    vector<CSeqMap::const_iterator> itrs;
    for ( ; seg != seq_map->EndResolved(&scope); ++seg ) {
        _ASSERT(seg);
        itrs.push_back(seg);
        switch (seg.GetType()) {
        case CSeqMap::eSeqData:
            _TRACE('@'<<len<<": seqData("<<seg.GetLength()<<")");
            len += seg.GetLength();
            break;
        case CSeqMap::eSeqRef:
            _TRACE('@'<<len<<": seqRef("<<seg.GetLength()<<
                   ", id="<<seg.GetRefSeqid().AsString()<<
                   ", pos="<<seg.GetRefPosition()<<
                   ", minus="<<seg.GetRefMinusStrand()<<")");
            _ASSERT("Unexpected REF segment" && 0);
            //len += seg.GetLength();
            break;
        case CSeqMap::eSeqGap:
            _TRACE('@'<<len<<": seqGap("<<seg.GetLength()<<")");
            len += seg.GetLength();
            break;
        case CSeqMap::eSeqEnd:
            _ASSERT("Unexpected END segment" && 0);
            break;
        default:
            _ASSERT("Unexpected segment type" && 0);
            break;
        }
    }
    _ASSERT(!seg);
    for ( int i = itrs.size()-1; i >= 0; --i ) {
        --seg;
        _ASSERT(seg);
        _ASSERT(seg == itrs[i]);
    }
    --seg;
    _ASSERT(!seg);

    _TRACE("ProcessBioseq("<<id.AsFastaString()<<") len="<<len<<")");
    _ASSERT(len == seq_len);
    CHECK_END("get resolved sequence map");

    CSeqVector seq_vect;

    CHECK_WRAP();
    seq_vect = handle.GetSeqVector(CBioseq_Handle::eCoding_NotSet);
    string sout = "";
    {{
        CSeqVector_CI vit(seq_vect, 0);
        for ( ; vit.GetPos() < seq_vect.size(); ++vit) {
            sout += *vit;
        }
        _ASSERT(NStr::PrintableString(sout) == seq_str);
        seq_vect.SetCoding(CBioseq_Handle::eCoding_Ncbi);
        int pos = seq_vect.size() - 1;
        vit.SetPos(pos);
        for ( ; pos > 0; --vit, --pos) {
            _ASSERT(sout[vit.GetPos()] == *vit);
        }
        _ASSERT(sout[vit.GetPos()] == *vit);
        vit.GetSeqData(0, seq_vect.size(), sout);
        _ASSERT(NStr::PrintableString(sout) == seq_str);
        sout = "";
        seq_vect.SetCoding(CBioseq_Handle::eCoding_NotSet);
    }}
    for (TSeqPos i = 0; i < seq_vect.size(); i++) {
        sout += seq_vect[i];
    }
    _ASSERT(NStr::PrintableString(sout) == seq_str);
    CHECK_END("get seq vector");

    CHECK_WRAP();
    CSeq_interval interval;
    interval.SetId(id);
    interval.SetFrom(seq_str.size());
    interval.SetTo(seq_str.size()-1);
    CSeq_loc loc;
    loc.SetInt(interval);
    seq_vect = CSeqVector(loc, scope);
    string sout = "";
    for (TSeqPos i = 0; i < seq_vect.size(); i++) {
        sout += seq_vect[i];
    }
    _ASSERT(sout.empty());
    CHECK_END_ALWAYS("get seq view");

    if (seq_core->GetInst().IsSetStrand() &&
        seq_core->GetInst().GetStrand() == CSeq_inst::eStrand_ds) {
        CHECK_WRAP();
        seq_vect = handle.GetSeqVector(CBioseq_Handle::eCoding_NotSet,
                                       eNa_strand_minus);
        string sout_rev = "";
        for (TSeqPos i = seq_vect.size(); i> 0; i--) {
            sout_rev += seq_vect[i-1];
        }
        _ASSERT(NStr::PrintableString(sout_rev) == seq_str_compl);
        CHECK_END("get reverse seq vector");
    }
    else {
        _ASSERT(seq_str_compl.empty());
    }

    CConstRef<CBioseq> bioseq = handle.GetCompleteBioseq();

    int count = 0;
    CHECK_WRAP();
    // Test CSeq_descr iterator
    for (CSeq_descr_CI desc_it(handle); desc_it;  ++desc_it) {
        count++;
        //### _ASSERT(desc_it->
    }
    _ASSERT(count == seq_desc_cnt);
    CHECK_END_ALWAYS("count CSeq_descr");

    CHECK_WRAP();
    // Test CSeq_feat iterator, resolve-all method
    CSeq_loc loc;
    loc.SetWhole(id);
    count = 0;
    if ( !tse_feat_test ) {
        if ( sm_DumpFeatures ) {
            NcbiCout << "-------------------- CFeat_CI resolve-all over "<<
                id.AsFastaString()<<" --------------------" << NcbiEndl;
        }
        for ( CFeat_CI feat_it(scope, loc, SAnnotSelector().SetResolveAll());
              feat_it;  ++feat_it) {
            count++;
            if ( sm_DumpFeatures ) {
                auto_ptr<CObjectOStream>
                    out(CObjectOStream::Open(eSerial_AsnText, NcbiCout));
                *out << feat_it->GetMappedFeature();
            }
            //### _ASSERT(feat_it->
        }
        _ASSERT(count == seq_feat_ra_cnt);
    }
    CHECK_END("get annot set");

    CHECK_WRAP();
    // Test CSeq_feat iterator
    CSeq_loc loc;
    loc.SetWhole(id);
    count = 0;
    set<CSeq_annot_Handle> annot_set;
    if ( sm_DumpFeatures ) {
        NcbiCout << "-------------------- CFeat_CI over "<<
            id.AsFastaString()<<" --------------------" << NcbiEndl;
    }
    if ( !tse_feat_test ) {
        for ( CFeat_CI feat_it(scope, loc); feat_it;  ++feat_it) {
            count++;
            annot_set.insert(feat_it.GetAnnot());
            if ( sm_DumpFeatures ) {
                auto_ptr<CObjectOStream>
                    out(CObjectOStream::Open(eSerial_AsnText, NcbiCout));
                *out << feat_it->GetMappedFeature();
            }
            //### _ASSERT(feat_it->
        }
        if ( sm_DumpFeatures ) {
            NcbiCout << "-------------------- "
                "product CFeat_CI --------------------" << NcbiEndl;
        }
        // Get products
        for ( CFeat_CI feat_it(scope, loc, SAnnotSelector().SetByProduct());
              feat_it;  ++feat_it) {
            count++;
            annot_set.insert(feat_it.GetAnnot());
            if ( sm_DumpFeatures ) {
                auto_ptr<CObjectOStream>
                    out(CObjectOStream::Open(eSerial_AsnText, NcbiCout));
                *out << feat_it->GetMappedFeature();
            }
            //### _ASSERT(feat_it->
        }
    }
    else {
        if ( sm_DumpFeatures ) {
            NcbiCout << "-------------------- "
                "LimitTSE --------------------" << NcbiEndl;
        }
        for ( CFeat_CI feat_it(handle,
                               SAnnotSelector()
                               .SetLimitTSE(handle.GetTopLevelEntry()));
              feat_it;  ++feat_it) {
            count++;
            annot_set.insert(feat_it.GetAnnot());
            if ( sm_DumpFeatures ) {
                auto_ptr<CObjectOStream>
                    out(CObjectOStream::Open(eSerial_AsnText, NcbiCout));
                *out << feat_it->GetMappedFeature();
            }
            //### _ASSERT(feat_it->
        }
        if ( sm_DumpFeatures ) {
            NcbiCout << "-------------------- "
                "product CFeat_CI --------------------" << NcbiEndl;
        }
        // Get products
        for ( CFeat_CI feat_it(handle,
                               SAnnotSelector()
                               .SetLimitTSE(handle.GetTopLevelEntry())
                               .SetByProduct());
              feat_it;  ++feat_it) {
            count++;
            annot_set.insert(feat_it.GetAnnot());
            if ( sm_DumpFeatures ) {
                auto_ptr<CObjectOStream>
                    out(CObjectOStream::Open(eSerial_AsnText, NcbiCout));
                *out << feat_it->GetMappedFeature();
            }
            //### _ASSERT(feat_it->
        }
    }
    if ( sm_DumpFeatures ) {
        NcbiCout << "-------------------- "
            "end CFeat_CI --------------------" << NcbiEndl;
    }
    _ASSERT(count == seq_feat_cnt);
    _ASSERT(annot_set.size() == feat_annots_cnt);
    CHECK_END("get annot set");

    CHECK_WRAP();
    // Test CSeq_feat iterator for the specified range
    // Copy location seq-id
    count = 0;
    set<CSeq_annot_Handle> annot_set;
    CSeq_loc loc;
    loc.SetInt().SetId().Assign(id);
    loc.SetInt().SetFrom(0);
    loc.SetInt().SetTo(10);
    if ( !tse_feat_test ) {
        for ( CFeat_CI feat_it(scope, loc);
              feat_it;  ++feat_it) {
            count++;
            annot_set.insert(feat_it.GetAnnot());
            //### _ASSERT(feat_it->
        }
    }
    else {
        for ( CFeat_CI feat_it(scope, *handle.GetRangeSeq_loc(0, 10),
                               SAnnotSelector()
                               .SetLimitTSE(handle.GetTopLevelEntry()));
              feat_it;  ++feat_it) {
            count++;
            annot_set.insert(feat_it.GetAnnot());
            //### _ASSERT(feat_it->
        }
    }
    _ASSERT(count == seq_featrg_cnt);
    _ASSERT(annot_set.size() == featrg_annots_cnt);
    CHECK_END("get annot set");

    CHECK_WRAP();
    // Test CSeq_align iterator
    count = 0;
    set<CSeq_annot_Handle> annot_set;
    CSeq_loc loc;
    loc.SetWhole(id);
    if ( !tse_feat_test ) {
        for (CAlign_CI align_it(scope, loc);
             align_it;  ++align_it) {
            count++;
            annot_set.insert(align_it.GetAnnot());
            //### _ASSERT(align_it->
        }
    }
    else {
        for (CAlign_CI align_it(handle,
                                SAnnotSelector()
                                .SetLimitTSE(handle.GetTopLevelEntry()));
             align_it;  ++align_it) {
            count++;
            annot_set.insert(align_it.GetAnnot());
            //### _ASSERT(align_it->
        }
    }
    _ASSERT(count == seq_align_cnt);
    _ASSERT(annot_set.size() == align_annots_cnt);
    CHECK_END("get align set");

    CHECK_WRAP();
    // Test CSeq_align iterator for the specified range
    // Copy location seq-id
    count = 0;
    set<CSeq_annot_Handle> annot_set;
    CSeq_loc loc;
    loc.SetInt().SetId().Assign(id);
    loc.SetInt().SetFrom(10);
    loc.SetInt().SetTo(20);
    if ( !tse_feat_test ) {
        for (CAlign_CI align_it(scope, loc);
             align_it;  ++align_it) {
            count++;
            annot_set.insert(align_it.GetAnnot());
            //### _ASSERT(align_it->
        }
    }
    else {
        for (CAlign_CI align_it(scope, *handle.GetRangeSeq_loc(10, 20),
                                SAnnotSelector()
                                .SetLimitTSE(handle.GetTopLevelEntry()));
             align_it;  ++align_it) {
            count++;
            annot_set.insert(align_it.GetAnnot());
            //### _ASSERT(align_it->
        }
    }
    _ASSERT(count == seq_alignrg_cnt);
    _ASSERT(annot_set.size() == alignrg_annots_cnt);
    CHECK_END("get align set");
    if ( sm_TestRemoveEntry ) {
        CHECK_WRAP();
        CRef<CSeq_entry> tse
            (const_cast<CSeq_entry*>(
            handle.GetTopLevelEntry().GetCompleteSeq_entry().GetPointer()));
        
        CRef<CSeq_entry> entry(const_cast<CSeq_entry*>(
            handle.GetParentEntry().GetCompleteSeq_entry().GetPointer()));
        CSeq_entry* parent = entry->GetParentEntry();
        scope.RemoveEntry(*entry);
        handle = scope.GetBioseqHandle(id);
        _ASSERT(!handle  ||
            handle.GetParentEntry().GetCompleteSeq_entry() != entry);
        if ( parent ) {
            // Non-TSE
            scope.AttachEntry(*parent, *entry);
        }
        else {
            // TSE
            scope.AddTopLevelSeqEntry(*entry);
        }
        handle = scope.GetBioseqHandle(id);
        _ASSERT(handle);
        CHECK_END_ALWAYS("remove/attach seq-entry");
    }
}


void CTestHelper::TestDataRetrieval(CScope& scope, int idx,
                                    int delta)
{
    CSeq_id id;
    // find seq. by local id
    id.SetLocal().SetStr("seq" + NStr::IntToString(11+idx*1000));
    // iterate through the whole Scope
    ProcessBioseq(scope, id, 40,
        "CAGCAGCGGTACAGGAGGGTGAGACATCCCAGAGCGGTGC",
        "GTCGTCGCCATGTCCTCCCACTCTGTAGGGTCTCGCCACG",
        2, 4+delta, 4+delta, 2+delta, 1, 0, 2+delta, 2+delta, 1, 0);
    // iterate through the specific sequence only
    ProcessBioseq(scope, id, 40,
        "CAGCAGCGGTACAGGAGGGTGAGACATCCCAGAGCGGTGC",
        "GTCGTCGCCATGTCCTCCCACTCTGTAGGGTCTCGCCACG",
        2, -1, 2+delta, 1+delta, 1, 0, 1+delta, 1+delta, 1, 0, true);
    // find seq. by GI
    id.SetGi(12+idx*1000);
    ProcessBioseq(scope, id, 40,
        "CAATAACCTCAGCAGCAACAAGTGGCTTCCAGCGCCCTCC",
        "GTTATTGGAGTCGTCGTTGTTCACCGAAGGTCGCGGGAGG",
        1, 2, 3, 1, 1, 1, 2, 1, 1, 1); //1, 3, 2, 1, 1, 2, 2, 1, 1);
    // segmented sequence
    id.SetGi(21+idx*1000);
    ProcessBioseq(scope, id, 62,
        "CAGCACAATAACCTCAGCAGCAACAAGTGGCTTCCAGCGCCCTCCCAGCACAATAAAAAAAA",
        "GTCGTGTTATTGGAGTCGTCGTTGTTCACCGAAGGTCGCGGGAGGGTCGTGTTATTTTTTTT",
        1, 6+delta, 2, 1, 0, 0, 1, 1, 0, 0);
    id.SetGi(22+idx*1000);
    ProcessBioseq(scope, id, 20, "QGCGEQTMTLLAPTLAASRY", "",
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    // another seq.data format
    id.SetGi(23+idx*1000);
    ProcessBioseq(scope, id, 13,
        "\\x00\\x03\\x02\\x01\\x00\\x02\\x01\\x03\\x02\\x03\\x00\\x01\\x02",
        "\\x03\\x00\\x01\\x02\\x03\\x01\\x02\\x00\\x01\\x00\\x03\\x02\\x01",
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.62  2005/02/02 19:49:55  grichenk
* Fixed more warnings
*
* Revision 1.61  2004/12/06 17:54:10  grichenk
* Replaced calls to deprecated methods
*
* Revision 1.60  2004/11/01 19:33:09  grichenk
* Removed deprecated methods
*
* Revision 1.59  2004/08/25 15:03:56  grichenk
* Removed duplicate methods from CSeqMap
*
* Revision 1.58  2004/05/21 21:42:14  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.57  2004/04/01 20:18:12  grichenk
* Added initialization of m_MultiId member.
*
* Revision 1.56  2004/03/31 22:35:17  grichenk
* Fixed number of features found
*
* Revision 1.55  2004/03/24 18:30:30  vasilche
* Fixed edit API.
* Every *_Info object has its own shallow copy of original object.
*
* Revision 1.54  2004/03/23 15:14:01  vasilche
* Restored automatic call to Parentize() for Seq-entries in object manager.
*
* Revision 1.53  2004/03/18 16:30:24  grichenk
* Changed type of seq-align containers from list to vector.
*
* Revision 1.52  2004/03/16 15:47:28  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.51  2004/02/09 19:18:55  grichenk
* Renamed CDesc_CI to CSeq_descr_CI. Redesigned CSeq_descr_CI
* and CSeqdesc_CI to avoid using data directly.
*
* Revision 1.50  2004/02/03 17:58:50  vasilche
* Always test CScope::RemoveEntry() in single thread.
*
* Revision 1.49  2003/12/22 22:32:10  grichenk
* Enabled RemoveEntry/AttachEntry test in non-MT builds.
*
* Revision 1.48  2003/12/18 18:14:47  grichenk
* Removed test for CScope::RemoveEntry() since it breaks MT test.
*
* Revision 1.47  2003/12/18 16:38:07  grichenk
* Added CScope::RemoveEntry()
*
* Revision 1.46  2003/11/10 18:12:09  grichenk
* Removed extra EFlags declaration from seq_map_ci.hpp
*
* Revision 1.45  2003/11/04 16:21:37  grichenk
* Updated CAnnotTypes_CI to map whole features instead of splitting
* them by sequence segments.
*
* Revision 1.44  2003/09/30 16:22:05  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* Revision 1.43  2003/09/05 18:45:10  grichenk
* Fixed exceptions
*
* Revision 1.42  2003/07/14 21:13:27  grichenk
* Added possibility to resolve seq-map iterator withing a single TSE
* and to skip intermediate references during this resolving.
*
* Revision 1.41  2003/06/12 18:39:46  vasilche
* Chech CSeqVector assignment.
*
* Revision 1.40  2003/06/02 21:04:25  vasilche
* Fixed CSeqVector_CI test to not to go beyond begin().
*
* Revision 1.39  2003/06/02 16:06:39  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.37  2003/05/28 16:20:16  vasilche
* Added check for REF segments.
*
* Revision 1.36  2003/05/27 19:44:07  grichenk
* Added CSeqVector_CI class
*
* Revision 1.35  2003/05/23 16:32:54  vasilche
* Added tests for backward traversal of CSeqMap_CI.
*
* Revision 1.34  2003/05/12 19:17:42  vasilche
* Added initialization of mandatory field.
*
* Revision 1.33  2003/05/09 20:34:23  grichenk
* Reverted to 1.31
*
* Revision 1.32  2003/05/09 20:28:03  grichenk
* Changed warnings to info
*
* Revision 1.31  2003/04/24 16:12:39  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.30  2003/03/26 14:30:23  lavr
* Conform to modified representation of NUL char in NStr::PrintableString()
*
* Revision 1.29  2003/03/21 14:52:28  vasilche
* Modified testobjmgr1 to reflect changes in CFeat_CI and CAlign_CI
* constructors' behaviour.
*
* Revision 1.28  2003/03/05 20:56:44  vasilche
* SAnnotSelector now holds all parameters of annotation iterators.
*
* Revision 1.27  2003/03/04 16:43:53  grichenk
* +Test CFeat_CI with eResolve_All flag
*
* Revision 1.26  2003/02/28 16:37:47  vasilche
* Fixed expected feature count.
* Added optional flags to testobjmgr to dump generated data and found features.
*
* Revision 1.25  2003/01/24 20:13:38  vasilche
* Added check for empty CSeqVector from CBioseq_Handle::GetSequenceView().
*
* Revision 1.24  2003/01/22 20:11:55  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.23  2003/01/22 19:25:11  ucko
* Avoid use of xobjutil.
*
* Revision 1.22  2002/12/26 20:34:53  ucko
* Drop unused header <sstream>, which older compilers (such as GCC 2.9x) lack;
* move CVS log to end.
*
* Revision 1.21  2002/12/26 16:39:24  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.20  2002/12/20 20:54:25  grichenk
* Added optional location/product switch to CFeat_CI
*
* Revision 1.19  2002/12/19 20:18:57  grichenk
* Fixed test case for minus strand location
*
* Revision 1.18  2002/11/08 19:43:37  grichenk
* CConstRef<> constructor made explicit
*
* Revision 1.17  2002/09/10 19:55:54  grichenk
* Catch exceptions when resolving seq-maps; new arguments for GetSeqVector()
*
* Revision 1.16  2002/07/25 15:01:53  grichenk
* Replaced non-const GetXXX() with SetXXX()
*
* Revision 1.15  2002/07/03 14:40:46  ucko
* Use typedef rather than hard-coded type for Dense_diag.ids.
*
* Revision 1.14  2002/06/07 16:13:24  ucko
* GetTitle() is now in sequence::.
*
* Revision 1.13  2002/06/06 19:49:54  clausen
* Changed CBioseq_Handle::GetTitle() call to GetTitle()
*
* Revision 1.12  2002/05/24 14:57:14  grichenk
* SerialAssign<>() -> CSerialObject::Assign()
*
* Revision 1.11  2002/05/21 18:39:31  grichenk
* CBioseq_Handle::GetResolvedSeqMap() -> CreateResolvedSeqMap()
*
* Revision 1.10  2002/05/09 14:21:50  grichenk
* Turned GetTitle() test on, removed unresolved seq-map test
*
* Revision 1.9  2002/05/06 03:28:53  vakatov
* OM/OM1 renaming
*
* Revision 1.8  2002/05/03 21:28:12  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.7  2002/04/25 18:15:25  grichenk
* Adjusted tests to work with the updated CSeqVector
*
* Revision 1.6  2002/04/23 15:26:07  gouriano
* added test_assert.h
*
* Revision 1.5  2002/04/22 20:07:45  grichenk
* Commented calls to CBioseq::ConstructExcludedSequence()
*
* Revision 1.4  2002/04/22 18:33:48  gouriano
* corrected feature counts
*
* Revision 1.3  2002/03/28 21:22:38  grichenk
* More tests for location-exclusive bioseq
*
* Revision 1.2  2002/03/18 21:47:15  grichenk
* Moved most includes to test_helper.cpp
* Added test for CBioseq::ConstructExcludedSequence()
*
* Revision 1.1  2002/03/13 18:06:31  gouriano
* restructured MT test. Put common functions into a separate file
*
* ===========================================================================
*/
