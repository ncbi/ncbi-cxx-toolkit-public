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
*   Test the functionality of C++ object manager in MT mode
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.14  2002/02/25 21:05:31  grichenk
* Removed seq-data references caching. Increased MT-safety. Fixed typos.
*
* Revision 1.13  2002/02/21 19:27:09  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.12  2002/02/07 21:27:36  grichenk
* Redesigned CDataSource indexing: seq-id handle -> TSE -> seq/annot
*
* Revision 1.11  2002/02/06 21:46:11  gouriano
* *** empty log message ***
*
* Revision 1.10  2002/02/05 21:46:28  gouriano
* added FindSeqid function, minor tuneup in CSeq_id_mapper
*
* Revision 1.9  2002/02/04 21:16:27  gouriano
* minor changes to make it run correctly on Solaris
*
* Revision 1.8  2002/01/30 22:09:28  gouriano
* changed CSeqMap interface
*
* Revision 1.7  2002/01/29 17:47:33  grichenk
* Removed commented part
*
* Revision 1.6  2002/01/28 19:44:50  gouriano
* changed the interface of BioseqHandle: two functions moved from Scope
*
* Revision 1.5  2002/01/23 21:59:34  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.4  2002/01/18 17:06:30  gouriano
* renamed CScope::GetSequence to CScope::GetSeqVector
*
* Revision 1.3  2002/01/16 18:56:30  grichenk
* Removed CRef<> argument from choice variant setter, updated sources to
* use references instead of CRef<>s
*
* Revision 1.2  2002/01/16 16:28:46  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:28  gouriano
* restructured objmgr
*
* Revision 1.31  2001/12/20 20:00:30  grichenk
* CObjectManager::ConstructBioseq(CSeq_loc) -> CBioseq::CBioseq(CSeq_loc ...)
*
* Revision 1.30  2001/12/12 22:39:11  grichenk
* Added test for minus-strand intervals in constructed bioseqs
*
* Revision 1.29  2001/12/12 17:48:45  grichenk
* Fixed code using generated classes to work with the updated datatool
*
* Revision 1.28  2001/12/07 19:08:44  grichenk
* Updated Object manager test
*
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbitime.hpp>

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
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Genetic_code.hpp>
#include <objects/seqalign/Dense_diag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seqalign/Seq_align.hpp>

#include <objects/objmgr1/object_manager.hpp>
#include <objects/objmgr1/bioseq_handle.hpp>
#include <objects/objmgr1/scope.hpp>
#include <objects/objmgr1/seq_vector.hpp>
#include <objects/objmgr1/desc_ci.hpp>
#include <objects/objmgr1/feat_ci.hpp>
#include <objects/objmgr1/align_ci.hpp>

#include <objects/seq/seqport_util.hpp>


#include <objects/general/Date.hpp>

BEGIN_NCBI_SCOPE
using namespace objects;


/////////////////////////////////////////////////////////////////////////////
//
//  Test application
//

class CTestApp : public CNcbiApplication
{
    typedef CNcbiApplication CParent;
private:
    virtual int  Run (void);

    static CRef<CSeq_entry> CreateTestEntry1(int index);
    static CRef<CSeq_entry> CreateTestEntry2(int index);
    static CRef<CSeq_entry> CreateTestEntry1a(int index);

    void ProcessBioseq(CScope& scope, CSeq_id& id,
        int seq_len_unresolved, int seq_len_resolved,
        string seq_str, string seq_str_compl,
        int seq_desc_cnt,
        int seq_feat_cnt, int seq_featrg_cnt,
        int seq_align_cnt, int seq_alignrg_cnt);

    CRef<CObjectManager> m_ObjMgr;
};


CRef<CSeq_entry> CTestApp::CreateTestEntry1(int index)
{
    // TSE
    CRef<CSeq_entry> entry(new CSeq_entry);
    CBioseq_set& set = entry->SetSet();
    set.SetClass(CBioseq_set::eClass_nuc_prot);
    list< CRef<CSeqdesc> >& descr = set.SetDescr().Set();
    CRef<CSeqdesc> desc(new CSeqdesc);
    desc->SetName("D1 from TSE1-"+NStr::IntToString(index));
    descr.push_back(desc);
    list< CRef<CSeq_entry> >& seq_set = set.SetSeq_set();

    // Seq 11
    {{
        CRef<CSeq_entry> sub_entry(new CSeq_entry);
        CBioseq& seq = sub_entry->SetSeq();
        CBioseq::TId& id_list = seq.SetId();

        // Id
        CRef<CSeq_id> id(new CSeq_id);
        id->SetLocal().SetStr("seq"+NStr::IntToString(11+index*1000));
        id_list.push_back(id);
        id.Reset(new CSeq_id);
        id->SetGi(11+index*1000);
        id_list.push_back(id);

        // Descr
        list< CRef<CSeqdesc> >& descr1 = seq.SetDescr().Set();
        CRef<CSeqdesc> desc1(new CSeqdesc);
        desc1->SetName("D1 from BS11-"+NStr::IntToString(index));
        descr1.push_back(desc1);

        // Inst
        CSeq_inst& inst = seq.SetInst();
        inst.SetRepr(CSeq_inst::eRepr_raw);
        inst.SetMol(CSeq_inst::eMol_dna);
        inst.SetLength(40);
        inst.SetSeq_data().SetIupacna().Set(
            "CAGCAGCGGTACAGGAGGGTGAGACATCCCAGAGCGGTGC");
        inst.SetStrand(CSeq_inst::eStrand_ds);

        // Annot
        list< CRef<CSeq_annot> >& annot_list = seq.SetAnnot();
        CRef<CSeq_annot> annot(new CSeq_annot);
        list< CRef<CSeq_feat> >& ftable = annot->SetData().SetFtable();
        {{
            CRef<CSeq_feat> feat(new CSeq_feat);
            feat->SetId().SetLocal().SetStr("F1: lcl|11");
            CSeqFeatData& fdata = feat->SetData();
            CCdregion& cdreg = fdata.SetCdregion();
            cdreg.SetFrame(CCdregion::eFrame_one);
            list< CRef< CGenetic_code::C_E > >& gcode = cdreg.SetCode().Set();
            CRef< CGenetic_code::C_E > ce(new CGenetic_code::C_E);
            ce->SetId(111); // TSE=1; seq=1; feat=1
            gcode.push_back(ce);
            CSeq_interval& floc = feat->SetLocation().SetInt();
            floc.SetId().SetLocal().SetStr
                ("seq"+NStr::IntToString(11+index*1000));
            floc.SetFrom(20);
            floc.SetTo(30);
            ftable.push_back(feat);
        }}
        {{
            CRef<CSeq_feat> feat(new CSeq_feat);
            feat->SetId().SetLocal().SetStr("F2: gi|11");
            CSeqFeatData& fdata = feat->SetData();
            CCdregion& cdreg = fdata.SetCdregion();
            cdreg.SetFrame(CCdregion::eFrame_one);
            list< CRef< CGenetic_code::C_E > >& gcode = cdreg.SetCode().Set();
            CRef< CGenetic_code::C_E > ce(new CGenetic_code::C_E);
            ce->SetId(112); // TSE=1; seq=1; feat=2
            gcode.push_back(ce);
            feat->SetLocation().SetWhole().SetGi(11+index*1000);
            ftable.push_back(feat);
        }}
        annot_list.push_back(annot);

        // Add sub-entry
        seq_set.push_back(sub_entry);
    }}

    // Seq 12
    {{
        CRef<CSeq_entry> sub_entry(new CSeq_entry);
        CBioseq& seq = sub_entry->SetSeq();
        CBioseq::TId& id_list = seq.SetId();

        // Id
        CRef<CSeq_id> id(new CSeq_id);
        id->SetLocal().SetStr("seq"+NStr::IntToString(12+index*1000));
        id_list.push_back(id);
        id.Reset(new CSeq_id);
        id->SetGi(12+index*1000);
        id_list.push_back(id);

        // Inst
        CSeq_inst& inst = seq.SetInst();
        inst.SetRepr(CSeq_inst::eRepr_raw);
        inst.SetMol(CSeq_inst::eMol_dna);
        inst.SetLength(40);
        inst.SetSeq_data().SetIupacna().Set(
            "CAATAACCTCAGCAGCAACAAGTGGCTTCCAGCGCCCTCC");
        inst.SetStrand(CSeq_inst::eStrand_ds);

        // Annot
        list< CRef<CSeq_annot> >& annot_list = seq.SetAnnot();
        {{
            CRef<CSeq_annot> annot(new CSeq_annot);
            list< CRef<CSeq_feat> >& ftable = annot->SetData().SetFtable();
            {{
                CRef<CSeq_feat> feat(new CSeq_feat);
                feat->SetId().SetLocal().SetStr("F3: gi|12");
                CSeqFeatData& fdata = feat->SetData();
                CCdregion& cdreg = fdata.SetCdregion();
                cdreg.SetFrame(CCdregion::eFrame_one);
                list< CRef< CGenetic_code::C_E > >& gcode = cdreg.SetCode().Set();
                CRef< CGenetic_code::C_E > ce(new CGenetic_code::C_E);
                ce->SetId(123); // TSE=1; seq=2; feat=3
                gcode.push_back(ce);
                CSeq_interval& floc = feat->SetLocation().SetInt();
                floc.SetId().SetGi(12+index*1000);
                floc.SetFrom(20);
                floc.SetTo(30);
                floc.SetStrand(eNa_strand_minus);
                ftable.push_back(feat);
            }}
            {{
                CRef<CSeq_feat> feat(new CSeq_feat);
                feat->SetId().SetLocal().SetStr("F4: lcl|12");
                CSeqFeatData& fdata = feat->SetData();
                CCdregion& cdreg = fdata.SetCdregion();
                cdreg.SetFrame(CCdregion::eFrame_one);
                list< CRef< CGenetic_code::C_E > >& gcode =
                    cdreg.SetCode().Set();
                CRef< CGenetic_code::C_E > ce(new CGenetic_code::C_E);
                ce->SetId(124); // TSE=1; seq=2; feat=4
                gcode.push_back(ce);
                feat->SetLocation().SetWhole().SetLocal().SetStr
                    ("seq"+NStr::IntToString(12+index*1000));
                ftable.push_back(feat);
            }}
            annot_list.push_back(annot);
        }}
        {{
            CRef<CSeq_annot> annot(new CSeq_annot);
            list< CRef<CSeq_align> >& atable = annot->SetData().SetAlign();
            {{
                // CAGCAGC:
                // 11[0], 12[9]
                CRef<CSeq_align> align(new CSeq_align);
                CSeq_align::C_Segs& segs = align->SetSegs();
                CSeq_align::C_Segs::TDendiag& diag_list = segs.SetDendiag();
                CRef<CDense_diag> diag(new CDense_diag);
                diag->SetDim(2);
                list< CRef<CSeq_id> >& id_list = diag->SetIds();
                CRef<CSeq_id> id(new CSeq_id);
                id->SetGi(11+index*1000);
                id_list.push_back(id);
                id.Reset(new CSeq_id);
                id->SetGi(12+index*1000);
                id_list.push_back(id);
                list<int>& start_list = diag->SetStarts();
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

    return entry;
}


CRef<CSeq_entry> CTestApp::CreateTestEntry1a(int index)
{
    // TSE
    CRef<CSeq_entry> entry(new CSeq_entry);
    CBioseq_set& set = entry->SetSet();
    set.SetClass(CBioseq_set::eClass_nuc_prot);
    list< CRef<CSeq_entry> >& seq_set = set.SetSeq_set();

    // Seq 11
    {{
        CRef<CSeq_entry> sub_entry(new CSeq_entry);
        CBioseq& seq = sub_entry->SetSeq();
        CBioseq::TId& id_list = seq.SetId();

        // Id
        CRef<CSeq_id> id(new CSeq_id);
        id->SetLocal().SetStr("seq"+NStr::IntToString(11+index*1000));
        id_list.push_back(id);
        id.Reset(new CSeq_id);
        id->SetGi(11+index*1000);
        id_list.push_back(id);

        // Inst
        CSeq_inst& inst = seq.SetInst();
        inst.SetRepr(CSeq_inst::eRepr_raw);
        inst.SetMol(CSeq_inst::eMol_dna);
        inst.SetLength(40);
        inst.SetSeq_data().SetIupacna().Set(
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
        inst.SetStrand(CSeq_inst::eStrand_ds);

        // Add sub-entry
        seq_set.push_back(sub_entry);
    }}

    // Seq 12
    {{
        CRef<CSeq_entry> sub_entry(new CSeq_entry);
        CBioseq& seq = sub_entry->SetSeq();
        CBioseq::TId& id_list = seq.SetId();

        // Id
        CRef<CSeq_id> id(new CSeq_id);
        id->SetLocal().SetStr("seq"+NStr::IntToString(12+index*1000));
        id_list.push_back(id);
        id.Reset(new CSeq_id);
        id->SetGi(12+index*1000);
        id_list.push_back(id);

        // Inst
        CSeq_inst& inst = seq.SetInst();
        inst.SetRepr(CSeq_inst::eRepr_raw);
        inst.SetMol(CSeq_inst::eMol_dna);
        inst.SetLength(40);
        inst.SetSeq_data().SetIupacna().Set(
            "TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT");
        inst.SetStrand(CSeq_inst::eStrand_ds);

        // Add sub-entry
        seq_set.push_back(sub_entry);
    }}

    return entry;
}


CRef<CSeq_entry> CTestApp::CreateTestEntry2(int index)
{
    // TSE
    CRef<CSeq_entry> entry(new CSeq_entry);
    CBioseq_set& set = entry->SetSet();
    set.SetClass(CBioseq_set::eClass_nuc_prot);
    list< CRef<CSeq_entry> >& seq_set = set.SetSeq_set();

    // Seq 21
    {{
        CRef<CSeq_entry> sub_entry(new CSeq_entry);
        CBioseq& seq = sub_entry->SetSeq();
        CBioseq::TId& id_list = seq.SetId();

        // Id
        CRef<CSeq_id> id(new CSeq_id);
        id->SetLocal().SetStr("seq"+NStr::IntToString(21+index*1000));
        id_list.push_back(id);
        id.Reset(new CSeq_id);
        id->SetGi(21+index*1000);
        id_list.push_back(id);

        // Descr
        list< CRef<CSeqdesc> >& descr1 = seq.SetDescr().Set();
        CRef<CSeqdesc> desc1(new CSeqdesc);
        desc1->SetName("D3 from bioseq 21-"+NStr::IntToString(index));
        descr1.push_back(desc1);

        // Inst
        CSeq_inst& inst = seq.SetInst();
        inst.SetRepr(CSeq_inst::eRepr_seg);
        inst.SetMol(CSeq_inst::eMol_dna);
        CSeg_ext::Tdata& seg_list = inst.SetExt().SetSeg();
        CRef<CSeq_loc> loc(new CSeq_loc);
        CSeq_interval& ref_loc = loc->SetInt();

        ref_loc.SetId().SetGi(11+index*1000);
        ref_loc.SetFrom(0);
        ref_loc.SetTo(4);
        seg_list.push_back(loc);
        loc = new CSeq_loc;
        loc->SetWhole().SetGi(12+index*1000);
        seg_list.push_back(loc);

        loc = new CSeq_loc;
        CSeq_interval& ref_loc2 = loc->SetInt();
        ref_loc2.SetId().SetGi(21+index*1000); // self-reference
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

        // Annot
        list< CRef<CSeq_annot> >& annot_list = seq.SetAnnot();
        CRef<CSeq_annot> annot(new CSeq_annot);
        list< CRef<CSeq_feat> >& ftable = annot->SetData().SetFtable();
        {{
            CRef<CSeq_feat> feat(new CSeq_feat);
            feat->SetId().SetLocal().SetStr("F5: lcl|11");
            CSeqFeatData& fdata = feat->SetData();
            CCdregion& cdreg = fdata.SetCdregion();
            cdreg.SetFrame(CCdregion::eFrame_one);
            list< CRef< CGenetic_code::C_E > >& gcode = cdreg.SetCode().Set();
            CRef< CGenetic_code::C_E > ce(new CGenetic_code::C_E);
            ce->SetId(215); // TSE=2; seq=1; feat=5
            gcode.push_back(ce);
            CSeq_interval& floc = feat->SetLocation().SetInt();
            floc.SetId().SetLocal().SetStr
                ("seq"+NStr::IntToString(11+index*1000));
            floc.SetFrom(20);
            floc.SetTo(30);
            floc.SetStrand(eNa_strand_plus);
            ftable.push_back(feat);
        }}
        {{
            CRef<CSeq_feat> feat(new CSeq_feat);
            feat->SetId().SetLocal().SetStr("F6: gi|11");
            CSeqFeatData& fdata = feat->SetData();
            CCdregion& cdreg = fdata.SetCdregion();
            cdreg.SetFrame(CCdregion::eFrame_one);
            list< CRef< CGenetic_code::C_E > >& gcode = cdreg.SetCode().Set();
            CRef< CGenetic_code::C_E > ce(new CGenetic_code::C_E);
            ce->SetId(216); // TSE=2; seq=1; feat=6
            gcode.push_back(ce);
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

    // Seq 22
    {{
        CRef<CSeq_entry> sub_entry(new CSeq_entry);
        CBioseq& seq = sub_entry->SetSeq();
        CBioseq::TId& id_list = seq.SetId();

        // Id
        CRef<CSeq_id> id(new CSeq_id);
        id->SetLocal().SetStr
            ("seq"+NStr::IntToString(22+index*1000));
        id_list.push_back(id);
        id.Reset(new CSeq_id);
        id->SetGi(22+index*1000);
        id_list.push_back(id);

        // Inst
        CSeq_inst& inst = seq.SetInst();
        inst.SetRepr(CSeq_inst::eRepr_raw);
        inst.SetMol(CSeq_inst::eMol_aa);
        inst.SetLength(20);
        inst.SetSeq_data().SetNcbieaa().Set("QGCGEQTMTLLAPTLAASRY");
        inst.SetStrand(CSeq_inst::eStrand_ss);

        // Add sub-entry
        seq_set.push_back(sub_entry);
    }}

    // Seq 23
    {{
        CRef<CSeq_entry> sub_entry(new CSeq_entry);
        CBioseq& seq = sub_entry->SetSeq();
        CBioseq::TId& id_list = seq.SetId();

        // Id
        CRef<CSeq_id> id(new CSeq_id);
        id->SetLocal().SetStr
            ("seq"+NStr::IntToString(23+index*1000));
        id_list.push_back(id);
        id.Reset(new CSeq_id);
        id->SetGi(23+index*1000);
        id_list.push_back(id);

        // Inst
        CSeq_inst& inst = seq.SetInst();
        inst.SetRepr(CSeq_inst::eRepr_raw);
        inst.SetMol(CSeq_inst::eMol_dna);
        inst.SetLength(13);
        inst.SetSeq_data().SetIupacna().Set("ATGCAGCTGTACG");
        inst.SetStrand(CSeq_inst::eStrand_ds);

        CRef<CSeq_data> packed_data(new CSeq_data);
        CSeqportUtil::Convert(inst.GetSeq_data(),
                              packed_data,
                              CSeq_data::e_Ncbi2na);
        inst.SetSeq_data(*packed_data);

        // Add sub-entry
        seq_set.push_back(sub_entry);
    }}

    // Annot
    list< CRef<CSeq_annot> >& set_annot_list = set.SetAnnot();
    CRef<CSeq_annot> annot(new CSeq_annot);
    list< CRef<CSeq_feat> >& ftable = annot->SetData().SetFtable();
    {{
        CRef<CSeq_feat> feat(new CSeq_feat);
        feat->SetId().SetLocal().SetStr("F7: lcl|21 + gi|12");
        CSeqFeatData& fdata = feat->SetData();
        CCdregion& cdreg = fdata.SetCdregion();
        cdreg.SetFrame(CCdregion::eFrame_one);
        list< CRef< CGenetic_code::C_E > >& gcode = cdreg.SetCode().Set();
        CRef< CGenetic_code::C_E > ce(new CGenetic_code::C_E);
        ce->SetId(207); // TSE=2; no seq; feat=7
        gcode.push_back(ce);
        CSeq_interval& floc = feat->SetLocation().SetInt();
        floc.SetId().SetLocal().SetStr
            ("seq"+NStr::IntToString(21+index*1000));
        floc.SetFrom(1);
        floc.SetTo(20);
        feat->SetProduct().SetWhole().SetGi(12+index*1000);
        ftable.push_back(feat);
    }}
    set_annot_list.push_back(annot);

    return entry;
}


void CTestApp::ProcessBioseq(CScope& scope, CSeq_id& id,
                             int seq_len_unresolved, int seq_len_resolved,
                             string seq_str, string seq_str_compl,
                             int seq_desc_cnt,
                             int seq_feat_cnt, int seq_featrg_cnt,
                             int seq_align_cnt, int seq_alignrg_cnt)
{
    CBioseq_Handle handle = scope.GetBioseqHandle(id);
    if ( !handle ) {
        LOG_POST("No seq-id found");
        return;
    }

    handle.GetTopLevelSeqEntry();
//    handle.GetTitle();
    CBioseq_Handle::TBioseqCore seq_core = handle.GetBioseqCore();
    {{
        CSeqMap seq_map = handle.GetSeqMap();
        // Iterate seq-map except the last element
        int len = 0;
        for (size_t i = 0; i < seq_map.size(); i++) {
            switch (seq_map[i].GetType()) {
            case CSeqMap::eSeqData:
                len += seq_map[i].GetLength();
                break;
            case CSeqMap::eSeqRef:
                len += seq_map[i].GetLength();
                break;
            case CSeqMap::eSeqGap:
                len += seq_map[i].GetLength();
                break;
            case CSeqMap::eSeqEnd:
                break;
            default:
                break;
            }
        }
        _ASSERT(seq_map[seq_map.size()-1].GetType() ==
                CSeqMap::eSeqEnd);
        _ASSERT(len == seq_len_unresolved);
    }}

    {{
        CSeqVector seq_vect = handle.GetSeqVector();
        string sout = "";
        for (size_t i = 0; i < seq_vect.size(); i++) {
            sout += seq_vect[i];
        }
        _ASSERT(NStr::PrintableString(sout) == seq_str);
    }}
    if (seq_core->GetInst().IsSetStrand() &&
        seq_core->GetInst().GetStrand() == CSeq_inst::eStrand_ds) {
        CSeqVector seq_vect_rev = handle.GetSeqVector(false);
        string sout_rev = "";
        for (size_t i = seq_vect_rev.size(); i> 0; i--) {
            sout_rev += seq_vect_rev[i-1];
        }
        _ASSERT(NStr::PrintableString(sout_rev) == seq_str_compl);
    }
    else {
        _ASSERT(seq_str_compl.empty());
    }

    {{
        // Get another seq-map - some lengths may have been resolved
        CSeqMap seq_map = handle.GetSeqMap();
        // Iterate seq-map except the last element
        int len = 0;
        for (size_t i = 0; i < seq_map.size(); i++) {
            switch (seq_map[i].GetType()) {
            case CSeqMap::eSeqData:
                len += seq_map[i].GetLength();
                break;
            case CSeqMap::eSeqRef:
                len += seq_map[i].GetLength();
                break;
            case CSeqMap::eSeqGap:
                len += seq_map[i].GetLength();
                break;
            case CSeqMap::eSeqEnd:
                break;
            default:
                break;
            }
        }
        _ASSERT(seq_map[seq_map.size()-1].GetType() ==
                CSeqMap::eSeqEnd);
        _ASSERT(len == seq_len_resolved);
    }}

    CConstRef<CBioseq> bioseq = &(handle.GetBioseq());

    int count = 0;
    // Test CSeq_descr iterator
    for (CDesc_CI desc_it(handle);
        desc_it;  ++desc_it) {
        count++;
        //### _ASSERT(desc_it->
    }
    _ASSERT(count == seq_desc_cnt);

    // Test CSeq_feat iterator
    CSeq_loc loc;
    loc.SetWhole(id);
    count = 0;
    for (CFeat_CI feat_it(scope, loc, CSeqFeatData::e_not_set);
        feat_it;  ++feat_it) {
        count++;
        //### _ASSERT(feat_it->
    }
    _ASSERT(count == seq_feat_cnt);

    // Test CSeq_feat iterator for the specified range
    // Copy location seq-id
    SerialAssign<CSeq_id>(loc.SetInt().SetId(), id);
    loc.GetInt().SetFrom(0);
    loc.GetInt().SetTo(10);
    count = 0;
    for (CFeat_CI feat_it(scope, loc, CSeqFeatData::e_not_set);
        feat_it;  ++feat_it) {
        count++;
        //### _ASSERT(feat_it->
    }
    _ASSERT(count == seq_featrg_cnt);

    // Test CSeq_align iterator
    loc.SetWhole(id);
    count = 0;
    for (CAlign_CI align_it( scope, loc);
        align_it;  ++align_it) {
        count++;
        //### _ASSERT(align_it->
    }
    _ASSERT(count == seq_align_cnt);

    // Test CSeq_align iterator for the specified range
    // Copy location seq-id
    SerialAssign<CSeq_id>(loc.SetInt().SetId(), id);
    loc.GetInt().SetFrom(10);
    loc.GetInt().SetTo(20);
    count = 0;
    for (CAlign_CI align_it(scope, loc);
        align_it;  ++align_it) {
        count++;
        //### _ASSERT(align_it->
    }
    _ASSERT(count == seq_alignrg_cnt);
}


int CTestApp::Run(void)
{
    NcbiCout << "Testing ObjectManager..." << NcbiEndl;
    CSeq_id id;

    m_ObjMgr = new CObjectManager;
    {
        CScope Scope(*m_ObjMgr);
        CRef<CSeq_entry> entry1 = CreateTestEntry1(0);
        Scope.AddTopLevelSeqEntry(*entry1);
        CRef<CSeq_entry> entry2 = CreateTestEntry2(0);
        Scope.AddTopLevelSeqEntry(*entry2);

        // Test global scope
        id.SetLocal().SetStr("seq11");
        ProcessBioseq(Scope, id,
            40, 40,
            "CAGCAGCGGTACAGGAGGGTGAGACATCCCAGAGCGGTGC",
            "GTCGTCGCCATGTCCTCCCACTCTGTAGGGTCTCGCCACG",
            2, 4, 2, 1, 0);
        id.SetGi(12);
        ProcessBioseq(Scope, id,
            40, 40,
            "CAATAACCTCAGCAGCAACAAGTGGCTTCCAGCGCCCTCC",
            "GTTATTGGAGTCGTCGTTGTTCACCGAAGGTCGCGGGAGG",
            1, 3, 2, 1, 1);
        id.SetGi(21);
        ProcessBioseq(Scope, id,
            22, 62,
            "CAGCACAATAACCTCAGCAGCAACAAGTGGCTTCCAGCGCCCTCCCAGCACAATAAAAAAAA",
            "GTCGTGTTATTGGAGTCGTCGTTGTTCACCGAAGGTCGCGGGAGGGTCGTGTTATTTTTTTT",
            1, 1, 1, 0, 0);
        id.SetGi(22);
        ProcessBioseq(Scope, id, 20, 20, "QGCGEQTMTLLAPTLAASRY", "", 0, 0, 0, 0, 0);
        id.SetGi(23);
        ProcessBioseq(Scope, id,
            13, 13,
            "\\0\\x03\\x02\\x01\\0\\x02\\x01\\x03\\x02\\x03\\0\\x01\\x02",
            "\\x03\\0\\x01\\x02\\x03\\x01\\x02\\0\\x01\\0\\x03\\x02\\x01",
            0, 0, 0, 0, 0);

        // Find seq_id
        {
            set< CRef<const CSeq_id> > setId;
            Scope.FindSeqid(setId, "seq11.3");
        }
    }
    
    for (int idx = 1; idx <= 1; idx++) {
        CRef<CScope> pScope(new CScope(*m_ObjMgr));
        CRef<CSeq_entry> entry1 = CreateTestEntry1(idx);
        CRef<CSeq_entry> entry2 = CreateTestEntry2(idx);
        pScope->AddTopLevelSeqEntry(*entry1);
        pScope->AddTopLevelSeqEntry(*entry2);

        id.SetLocal().SetStr("seq" + NStr::IntToString(11+idx*1000));
        ProcessBioseq(*pScope, id,
            40, 40,
            "CAGCAGCGGTACAGGAGGGTGAGACATCCCAGAGCGGTGC",
            "GTCGTCGCCATGTCCTCCCACTCTGTAGGGTCTCGCCACG",
            2, 4, 2, 1, 0);
        id.SetGi(12+idx*1000);
        ProcessBioseq(*pScope, id,
            40, 40,
            "CAATAACCTCAGCAGCAACAAGTGGCTTCCAGCGCCCTCC",
            "GTTATTGGAGTCGTCGTTGTTCACCGAAGGTCGCGGGAGG",
            1, 3, 2, 1, 1);
        id.SetGi(21+idx*1000);
        ProcessBioseq(*pScope, id,
            22, 62,
            "CAGCACAATAACCTCAGCAGCAACAAGTGGCTTCCAGCGCCCTCCCAGCACAATAAAAAAAA",
            "GTCGTGTTATTGGAGTCGTCGTTGTTCACCGAAGGTCGCGGGAGGGTCGTGTTATTTTTTTT",
            1, 1, 1, 0, 0);
        id.SetGi(22+idx*1000);
        ProcessBioseq(*pScope, id, 20, 20, "QGCGEQTMTLLAPTLAASRY", "", 0, 0, 0, 0, 0);
        id.SetGi(23+idx*1000);
        ProcessBioseq(*pScope, id,
            13, 13,
            "\\0\\x03\\x02\\x01\\0\\x02\\x01\\x03\\x02\\x03\\0\\x01\\x02",
            "\\x03\\0\\x01\\x02\\x03\\x01\\x02\\0\\x01\\0\\x03\\x02\\x01",
            0, 0, 0, 0, 0);

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
            feat->SetLocation().SetWhole().SetGi(11+idx*1000);
            ftable.push_back(feat);
        }}
        pScope->AttachAnnot(*entry1, *annot);

        id.SetLocal().SetStr("seq" + NStr::IntToString(11+idx*1000));
        ProcessBioseq(*pScope, id,
            40, 40,
            "CAGCAGCGGTACAGGAGGGTGAGACATCCCAGAGCGGTGC",
            "GTCGTCGCCATGTCCTCCCACTCTGTAGGGTCTCGCCACG",
            2, 5, 3, 1, 0);
        id.SetGi(12+idx*1000);
        ProcessBioseq(*pScope, id,
            40, 40,
            "CAATAACCTCAGCAGCAACAAGTGGCTTCCAGCGCCCTCC",
            "GTTATTGGAGTCGTCGTTGTTCACCGAAGGTCGCGGGAGG",
            1, 3, 2, 1, 1);
        id.SetGi(21+idx*1000);
        ProcessBioseq(*pScope, id,
            62, 62,
            "CAGCACAATAACCTCAGCAGCAACAAGTGGCTTCCAGCGCCCTCCCAGCACAATAAAAAAAA",
            "GTCGTGTTATTGGAGTCGTCGTTGTTCACCGAAGGTCGCGGGAGGGTCGTGTTATTTTTTTT",
            1, 1, 1, 0, 0);
        id.SetGi(22+idx*1000);
        ProcessBioseq(*pScope, id, 20, 20, "QGCGEQTMTLLAPTLAASRY", "", 0, 0, 0, 0, 0);
        id.SetGi(23+idx*1000);
        ProcessBioseq(*pScope, id,
            13, 13,
            "\\0\\x03\\x02\\x01\\0\\x02\\x01\\x03\\x02\\x03\\0\\x01\\x02",
            "\\x03\\0\\x01\\x02\\x03\\x01\\x02\\0\\x01\\0\\x03\\x02\\x01",
            0, 0, 0, 0, 0);

        // Constructed bioseqs
        {{
            CSeq_loc constr_loc;
            list< CRef<CSeq_interval> >& int_list = constr_loc.SetPacked_int();

            CRef<CSeq_interval> int_ref(new CSeq_interval);
            int_ref->SetId().SetGi(11+idx*1000);
            int_ref->SetFrom(5);
            int_ref->SetTo(10);
            int_list.push_back(int_ref);

            int_ref.Reset(new CSeq_interval);
            int_ref->SetId().SetGi(12+idx*1000);
            int_ref->SetFrom(0);
            int_ref->SetTo(20);
            int_list.push_back(int_ref);

            CRef<CBioseq> constr_seq(new CBioseq(constr_loc, "constructed1"));
            CRef<CSeq_entry> constr_entry(new CSeq_entry);
            constr_entry->SetSeq(*constr_seq);
            pScope->AddTopLevelSeqEntry(*constr_entry);
            id.SetLocal().SetStr("constructed1");
            ProcessBioseq(*pScope, id,
                27, 27, "GCGGTACAATAACCTCAGCAGCAACAA", "", 0, 0, 0, 0, 0);
        }}
        {{
            CSeq_loc constr_loc;
            list< CRef<CSeq_interval> >& int_list = constr_loc.SetPacked_int();

            CRef<CSeq_interval> int_ref(new CSeq_interval);
            int_ref->SetId().SetGi(11+idx*1000);
            int_ref->SetFrom(5);
            int_ref->SetTo(10);
            int_ref->SetStrand(eNa_strand_minus);
            int_list.push_back(int_ref);

            int_ref.Reset(new CSeq_interval);
            int_ref->SetId().SetGi(12+idx*1000);
            int_ref->SetFrom(0);
            int_ref->SetTo(20);
            int_list.push_back(int_ref);

            CRef<CBioseq> constr_seq(new CBioseq(constr_loc, "constructed2"));
            CRef<CSeq_entry> constr_entry(new CSeq_entry);
            constr_entry->SetSeq(*constr_seq);
            pScope->AddTopLevelSeqEntry(*constr_entry);
            id.SetLocal().SetStr("constructed2");
            ProcessBioseq(*pScope, id,
                27, 27, "TACCGCCAATAACCTCAGCAGCAACAA", "", 0, 0, 0, 0, 0);
        }}

        CRef<CScope> pScope2 = new CScope(*m_ObjMgr);
        pScope2->AddTopLevelSeqEntry(*entry2);
        id.SetGi(21+idx*1000);
        // Test with unresolved references
        ProcessBioseq(*pScope2, id,
            62, 62,
            "NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN",
            "NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN",
            1, 1, 1, 0, 0);

        CRef<CSeq_entry> entry1a = CreateTestEntry1a(idx);
        pScope2->AddTopLevelSeqEntry(*entry1a);
        id.SetGi(21+idx*1000);
        // Test with resolved references
        ProcessBioseq(*pScope2, id,
            62, 62,
            "AAAAATTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTAAAAATTTTTTTTTTTT",
            "TTTTTAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAATTTTTAAAAAAAAAAAA",
            1, 1, 1, 0, 0);
        // Test scope history
        CRef<CSeq_entry> entry1b = CreateTestEntry1(idx);
        pScope2->AddTopLevelSeqEntry(*entry1b);
        id.SetLocal().SetStr("seq"+NStr::IntToString(11+idx*1000));
        // gi|11 from entry1a must be selected
        ProcessBioseq(*pScope2, id,
            40, 40,
            "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
            "TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT",
            0, 4, 2, 1, 0);
    }

    NcbiCout << " Passed" << NcbiEndl << NcbiEndl;

    return 0;
}


END_NCBI_SCOPE



/////////////////////////////////////////////////////////////////////////////
//  MAIN


USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    return CTestApp().AppMain(argc, argv, 0, eDS_Default, 0);
}
