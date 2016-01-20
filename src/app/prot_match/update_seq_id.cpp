#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbistd.hpp>


#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatSupport.hpp>
#include <objects/seqfeat/InferenceSupport.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/general/Object_id.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/seq/seq_macros.hpp>
#include <objects/seqalign/seqalign_macros.hpp>


BEGIN_NCBI_SCOPE

USING_SCOPE(objects);


class CSeqIdUpdateApp : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);

private:
    void x_UpdateSeqId(const CObject_id& old_id, 
                       const CTextseq_id& new_id,
                       CSeq_id& id);

    void x_UpdateSeqLoc(const CObject_id& old_id,
                       const CTextseq_id& new_id,
                       CSeq_loc& seq_loc);

    void x_UpdateSeqFeat(const CObject_id& old_id,
                         const CTextseq_id& new_id, 
                         CSeq_feat& seq_feat);

    void x_UpdateSeqFeatSupport(const CObject_id& old_id,
                                const CTextseq_id& new_id,
                                CSeqFeatSupport& sf_support);

    void x_UpdateInferenceSupport(const CObject_id& old_id,
                                  const CTextseq_id& new_id,
                                  CInferenceSupport& inf_support);

    void x_UpdateSeqAnnot(const CObject_id& old_id,
                          const CTextseq_id& new_id,
                          CSeq_annot& seq_annot);

    void x_UpdateBioseq(const CObject_id& old_id,
                        const CTextseq_id& new_id,
                        CBioseq& bioseq);

    void x_UpdateSeqHist(const CObject_id& old_id,
                         const CTextseq_id& new_id,
                         CSeq_hist& hist);

    void x_UpdateSeqExt(const CObject_id& old_id,
                        const CTextseq_id& new_id,
                        CSeq_ext& ext);

    void x_UpdateSeqAlign(const CObject_id& old_id,
                          const CTextseq_id& new_id,
                          CSeq_align& align);

    void x_UpdateSeqEntry(const CObject_id& old_id, 
                          const CTextseq_id& new_id,
                          CSeq_entry& entry); 
    // Need to get the Seq_entry handle from this Seq_entry

};


void CSeqIdUpdateApp::Init(void) 
{
}


int CSeqIdUpdateApp::Run(void) 
{
    return 0;
}

void CSeqIdUpdateApp::x_UpdateSeqId(const CObject_id& old_id,
                                    const CTextseq_id& new_id,
                                    CSeq_id& id) 
{
    // Need to figure out what to do here!

    if (id.IsLocal() &&
        id.GetLocal().Compare(old_id) == 0 ) {
        std::cout << "Replace old id with new id\n";
    }

    return;
}

void CSeqIdUpdateApp::x_UpdateSeqLoc(const CObject_id& old_id,
                                     const CTextseq_id& new_id,
                                     CSeq_loc& seq_loc) 
{

    CRef<CSeq_id> seq_id = Ref(new CSeq_id());
    x_UpdateSeqId(old_id,
                  new_id,
                  *seq_id);

    seq_loc.SetId(*seq_id);

    return;
}


void CSeqIdUpdateApp::x_UpdateBioseq(const CObject_id& old_id,
                                     const CTextseq_id& new_id,
                                     CBioseq& bioseq)
{
    NON_CONST_ITERATE(CBioseq::TId, it, bioseq.SetId()) 
    {
        x_UpdateSeqId(old_id, new_id, **it); 
    }

    EDIT_EACH_SEQANNOT_ON_BIOSEQ(it, bioseq) 
    {
        x_UpdateSeqAnnot(old_id, new_id, **it);
    }

    if (bioseq.IsSetInst() &&
        bioseq.GetInst().IsSetHist()) {
        x_UpdateSeqHist(old_id, new_id, bioseq.SetInst().SetHist());
    }

    if (bioseq.IsSetInst() &&
        bioseq.GetInst().IsSetExt()) {
        x_UpdateSeqExt(old_id, new_id, bioseq.SetInst().SetExt());
    }

    return;
}


void CSeqIdUpdateApp::x_UpdateSeqHist(const CObject_id& old_id, 
                                      const CTextseq_id& new_id,
                                      CSeq_hist& hist) 
{

    if (hist.IsSetAssembly()) {
        NON_CONST_ITERATE(CSeq_hist::TAssembly, it, hist.SetAssembly()) {
            x_UpdateSeqAlign(old_id, new_id, **it);
        }
    }

    if (hist.IsSetReplaces() &&
        hist.GetReplaces().IsSetIds()) {
        NON_CONST_ITERATE(CSeq_hist_rec::TIds, it, hist.SetReplaces().SetIds()) {
            x_UpdateSeqId(old_id, new_id, **it);
        }
    }

    if (hist.IsSetReplaced_by() && 
        hist.GetReplaced_by().IsSetIds()) {
        NON_CONST_ITERATE(CSeq_hist_rec::TIds, it, hist.SetReplaced_by().SetIds()) {
            x_UpdateSeqId(old_id, new_id, **it);
        }
    }

    return;
}



void CSeqIdUpdateApp::x_UpdateSeqAlign(const CObject_id& old_id, 
                                       const CTextseq_id& new_id,
                                       CSeq_align& align)
{
    EDIT_EACH_RECURSIVE_SEQALIGN_ON_SEQALIGN(ait, align) 
    {
        x_UpdateSeqAlign(old_id, new_id, align);
    }

    if (!align.IsSetSegs()) {
        return;
    }

    if (align.GetSegs().IsDendiag()) {
        EDIT_EACH_DENDIAG_ON_SEQALIGN(dit, align) 
        {
            EDIT_EACH_SEQID_ON_DENDIAG(it, **dit) 
            {
                x_UpdateSeqId(old_id, new_id, **it);       
            }
        }
        return;
    }


    if (align.GetSegs().IsDenseg()) {
        EDIT_EACH_SEQID_ON_DENSEG(it, align.SetSegs().SetDenseg()) 
        {
            x_UpdateSeqId(old_id, new_id, **it);
        }
        return;
    }


    if (align.GetSegs().IsStd()) {
        EDIT_EACH_STDSEG_ON_SEQALIGN(sit, align) {

            if ((*sit)->IsSetIds()) {
                NON_CONST_ITERATE(CStd_seg::TIds, it, (*sit)->SetIds()) {
                    x_UpdateSeqId(old_id, new_id, **it);
                }
            }

            if ((*sit)->IsSetLoc()) {
                NON_CONST_ITERATE(CStd_seg::TLoc, it, (*sit)->SetLoc()) {
                    x_UpdateSeqLoc(old_id, new_id, **it);
                }
            }
        }
    }
    return;
}



void CSeqIdUpdateApp::x_UpdateSeqExt(const CObject_id& old_id, 
                                     const CTextseq_id& new_id,
                                     CSeq_ext& ext) 
{
    if (ext.IsSeg() &&
        ext.GetSeg().IsSet()) {
        NON_CONST_ITERATE(CSeg_ext::Tdata, it, ext.SetSeg().Set()) {
            x_UpdateSeqLoc(old_id, new_id, **it);
        }
        return;
    }

    if (ext.IsRef()) {
        x_UpdateSeqLoc(old_id, new_id, ext.SetRef().Set());
        return;
    }

    if (ext.IsMap() &&
        ext.GetMap().IsSet()) {
        NON_CONST_ITERATE(CMap_ext::Tdata, it, ext.SetMap().Set()) {
            x_UpdateSeqFeat(old_id, new_id, **it);
        }
        return;
    }

    if (ext.IsDelta() &&
        ext.GetDelta().IsSet()) {
        NON_CONST_ITERATE(CDelta_ext::Tdata, it, ext.SetDelta().Set()) {
            if ((*it)->IsLoc()) {
                x_UpdateSeqLoc(old_id, new_id, (*it)->SetLoc());
            }
        }
        return;
    }

    return;
}


void CSeqIdUpdateApp::x_UpdateSeqAnnot(const CObject_id& old_id, 
                                       const CTextseq_id& new_id,
                                       CSeq_annot& annot) 
{
    EDIT_EACH_SEQFEAT_ON_SEQANNOT(it, annot)
    {
        x_UpdateSeqFeat(old_id, new_id, **it);
    }


    EDIT_EACH_SEQALIGN_ON_SEQANNOT(it, annot)
    {
        x_UpdateSeqAlign(old_id, new_id, **it);
    }


    EDIT_EACH_ANNOTDESC_ON_SEQANNOT(it, annot) 
    {
        if ((*it)->IsSrc()) {
            x_UpdateSeqId(old_id, new_id, (*it)->SetSrc());
        } 
        else 
        if ((*it)->IsRegion()) {
            x_UpdateSeqLoc(old_id, new_id, (*it)->SetRegion());
        }
    }

    if (!annot.IsSetData()) {
        return;
    }

    CSeq_annot::TData& data = annot.SetData();


    if (data.IsIds()) {
        NON_CONST_ITERATE(CSeq_annot::C_Data::TIds, it, data.SetIds()) 
        {
            x_UpdateSeqId(old_id, new_id, **it);
        }
        return;
    }


    if (data.IsLocs()) {
        NON_CONST_ITERATE(CSeq_annot::C_Data::TLocs, it, data.SetLocs()) {
            x_UpdateSeqLoc(old_id, new_id, **it);
        }
        return;
    }

/*
    if (data.IsSeq_table() &&
        data.GetSeq_table().IsSetColumns()) {

        NON_CONST_ITERATE(CSeq_table::TColumns, cit, data.SetSeq_table().SetColumns()) 
        {
            if ((*cit)->IsSetData() &&
                (*cit)->GetData().IsLoc()) {

                NON_CONST_ITERATE(CSeqTable_multi_data::TLoc, it, (*cit)->SetData().SetLoc()) 
                {
                    x_UpdateSeqLoc(old_id, new_id, **it);
                }
            } 
        }
    }
    */

    return;
}


void CSeqIdUpdateApp::x_UpdateSeqFeat(const CObject_id& old_id,
                                      const CTextseq_id& new_id,
                                      CSeq_feat& seq_feat) 
{
    if (seq_feat.IsSetLocation()) {
        x_UpdateSeqLoc(old_id, new_id, seq_feat.SetLocation());
    }

    if (seq_feat.IsSetProduct()) {
        x_UpdateSeqLoc(old_id, new_id, seq_feat.SetProduct());
    }

    // ...
}



void CSeqIdUpdateApp::x_UpdateSeqFeatSupport(const CObject_id& old_id,
                                             const CTextseq_id& new_id,
                                             CSeqFeatSupport& sf_support)
{
    if (sf_support.IsSetInference())
    {
        NON_CONST_ITERATE(CSeqFeatSupport::TInference, it, sf_support.SetInference()) 
        {
            CInferenceSupport& inf_support = **it; 
            x_UpdateInferenceSupport(old_id,
                                     new_id,
                                     inf_support);
        }
    }
}


void CSeqIdUpdateApp::x_UpdateInferenceSupport(const CObject_id& old_id,
                                               const CTextseq_id& new_id,
                                               CInferenceSupport& inf_sup)
{
    // Do we need this for protein matching?

    return;
}




void CSeqIdUpdateApp::x_UpdateSeqEntry(const CObject_id& old_id,
                                       const CTextseq_id& new_id,
                                       CSeq_entry& entry) 
{
    return;
}



END_NCBI_SCOPE

USING_NCBI_SCOPE;


int main(int argc, const char* argv[])
{
    return CSeqIdUpdateApp().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}
