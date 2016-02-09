#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
//#include <objects/seqfeat/SeqFeatSupport.hpp>
//#include <objects/seqfeat/InferenceSupport.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/general/Object_id.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objects/seqloc/Textseq_id.hpp>
#include <objects/seq/seq_macros.hpp>
#include <objects/seqalign/seqalign_macros.hpp>
#include <objects/seqtable/SeqTable_multi_data.hpp>
#include <objects/seqtable/Seq_table.hpp>
#include <objects/seqtable/SeqTable_column.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/Trna_ext.hpp>
#include <objects/seqfeat/seqfeat_macros.hpp>
#include <objects/seqfeat/Clone_seq_set.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/object_manager.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/annot_ci.hpp>
#include <util/line_reader.hpp>
#include "prot_match_exception.hpp"

BEGIN_NCBI_SCOPE

USING_SCOPE(objects);


class CSeqIdUpdateApp : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);

private:

    typedef map<CRef<CSeq_id>, CRef<CSeq_id> > TIdMap;


    void x_UpdateSeqId(const TIdMap& id_map,
                       CSeq_id& id);
    
    void x_UpdateSeqId(const CSeq_id& old_id, 
                       const CSeq_id& new_id,
                       CSeq_id& id);

    void x_UpdateSeqLoc(const TIdMap& id_map,
                        CSeq_loc& seq_loc);

    void x_UpdateSeqFeat(const TIdMap& id_map,
                         CSeq_feat& seq_feat);
/*
    void x_UpdateSeqFeatSupport(const CSeq_id& old_id,
                                const CSeq_id& new_id,
                                CSeqFeatSupport& sf_support);

    void x_UpdateInferenceSupport(const CSeq_id& old_id,
                                  const CSeq_id& new_id,
                                  CInferenceSupport& inf_support);
*/
    void x_UpdateSeqAnnot(const TIdMap& id_map,
                          CSeq_annot& seq_annot);

    void x_UpdateBioseq(const TIdMap& id_map,
                        CBioseq& bioseq);

    void x_UpdateSeqHist(const TIdMap& id_map,
                         CSeq_hist& hist);
/*
    void x_UpdateSeqExt(const TIdMap& id_map,
                        CSeq_ext& ext);
*/
    void x_UpdateSeqAlign(const TIdMap& id_map,
                          CSeq_align& align);

    void x_UpdateSeqEntry(const TIdMap& id_map,
                          CSeq_entry_Handle& seh);

    CObjectIStream* x_InitInputEntryStream(
            const CArgs& args);

    bool x_ProcessInputTable(CNcbiIstream& istr, 
                             TIdMap& id_map);
};


void CSeqIdUpdateApp::Init(void) 
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddKey("i",
            "InputFile",
            "Path to Seq-entry input file",
            CArgDescriptions::eInputFile);

    arg_desc->AddKey("t",
            "Table",
            "Path to file containing protein match table",
            CArgDescriptions::eInputFile);

    arg_desc->AddOptionalKey("o", "OutputFile",
            "Path to Seq-entry output file. Defaults to stdout",
            CArgDescriptions::eOutputFile);

    SetupArgDescriptions(arg_desc.release());

    return;
}


int CSeqIdUpdateApp::Run(void) 
{
    const CArgs& args = GetArgs();

    // Setup scope
    CRef<CObjectManager> obj_mgr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*obj_mgr);
    CRef<CScope> scope(new CScope(*obj_mgr));
    scope->AddDefaults();

    // Set up object input stream
    auto_ptr<CObjectIStream> istr;
    istr.reset(x_InitInputEntryStream(args));

    // Attempt to read Seq-entry from input file 
    CSeq_entry seq_entry;
    try {
        istr->Read(ObjectInfo(seq_entry));
    }
    catch (CException&) {
        NCBI_THROW(CProteinMatchException,
                   eInputError,
                   "Failed to read Seq-entry");
    }

    CNcbiIstream& table_str = args["t"].AsInputFile();
    map<CRef<CSeq_id>, CRef<CSeq_id> > id_map;
    x_ProcessInputTable(table_str, id_map);

    CNcbiOstream& ostr = (args["o"]) ? 
        args["o"].AsOutputFile() :
        cout;

    auto_ptr<MSerial_Format> pOutFormat;
    pOutFormat.reset(new MSerial_Format_AsnText());
   
    // Fetch Seq-entry Handle 
    CSeq_entry_Handle seh;
    try {
        seh = scope->AddTopLevelSeqEntry( seq_entry );
    } 
    catch (CException&) {
        NCBI_THROW(CProteinMatchException, 
                   eBadInput, 
                   "Could not obtain Seq-entry handle");
    }

    x_UpdateSeqEntry(id_map, seh);

    scope->RemoveEntry(seq_entry);

    ostr << MSerial_Format_AsnText() << seq_entry;
    return 0;
}


CObjectIStream* CSeqIdUpdateApp::x_InitInputEntryStream(
        const CArgs& args) 
{
    ESerialDataFormat serial = eSerial_AsnText;

    const char* infile = args["i"].AsString().c_str();
    CNcbiIstream* pInputStream = new CNcbiIfstream(infile, ios::binary); // What does this mean? 
   

    CObjectIStream* pI = CObjectIStream::Open(
            serial, *pInputStream, eTakeOwnership);

    if (!pI) {
        NCBI_THROW(CProteinMatchException, 
                   eInputError,
                   "Failed to create input stream");
    }
    return pI;
}


bool CSeqIdUpdateApp::x_ProcessInputTable(CNcbiIstream& istr, TIdMap& id_map) 
{
    CStreamLineReader lr(istr);

    while ( !lr.AtEOF() ) {
        char c = lr.PeekChar();
        if (c == '#') {
            ++lr;
        } else {
            string line = *++lr;
    
            if (line.empty()) {
                continue;
            }

            vector<string> entries;
            string delim = "\t";
            NStr::Split(line, delim, entries);
            if (entries.size() >= 2 &&
                entries[0] == "LocalID" &&
                entries[1] == "AccVer") {
                continue;
            } 
            else 
            if (entries.size() < 2) {
                continue;
            }

            CRef<CSeq_id> gb_id = Ref(new CSeq_id());
            gb_id->SetGenbank().Set(entries[1]);

            CRef<CSeq_id> local = Ref(new CSeq_id());

            local->SetLocal().SetStr(entries[0]);

            id_map[local] = gb_id;
        } 
    }

    return true;
}




void CSeqIdUpdateApp::x_UpdateSeqId(const CSeq_id& old_id,
                                    const CSeq_id& new_id,
                                    CSeq_id& id) 
{
    if (id.Compare(old_id) != CSeq_id::e_YES) {
        return;
    }

    // Generalize this function to handle different IDs

    if (new_id.IsLocal()) {
        if (new_id.GetLocal().IsStr()) {
            id.SetLocal().SetStr(new_id.GetLocal().GetStr());
        }
        else 
        if (new_id.GetLocal().IsId()) {
            id.SetLocal().SetId(new_id.GetLocal().GetId());
        }
        return;
    }

    if (new_id.IsGenbank()) {
        if (new_id.GetGenbank().IsSetName()) {
            id.SetGenbank().SetName(new_id.GetGenbank().GetName());
        }

        if (new_id.GetGenbank().IsSetAccession()) {
            id.SetGenbank().SetAccession(new_id.GetGenbank().GetAccession());
        }

        if (new_id.GetGenbank().IsSetRelease()) {
            id.SetGenbank().SetRelease(new_id.GetGenbank().GetRelease());
        }

        if (new_id.GetGenbank().IsSetVersion()) {
            id.SetGenbank().SetVersion(new_id.GetGenbank().GetVersion());
        }
        return;
    }

    return;
}



void CSeqIdUpdateApp::x_UpdateSeqId(const TIdMap& id_map,
                                    CSeq_id& id)
{
    TIdMap::const_iterator cit;

    for (cit = id_map.begin(); cit != id_map.end(); ++cit) {
        const CSeq_id& old_id = *cit->first;
        const CSeq_id& new_id = *cit->second;

        x_UpdateSeqId(old_id, new_id, id);
    }

    return;
}


void CSeqIdUpdateApp::x_UpdateSeqLoc(const TIdMap& id_map,
                                     CSeq_loc& seq_loc) 
{

    CRef<CSeq_id> seq_id = Ref(new CSeq_id());
    seq_id->Assign(*(seq_loc.GetId()));

    x_UpdateSeqId(id_map, *seq_id);

    seq_loc.SetId(*seq_id);

    return;
}


void CSeqIdUpdateApp::x_UpdateBioseq(const TIdMap& id_map,
                                     CBioseq& bioseq)
{

    NON_CONST_ITERATE(CBioseq::TId, it, bioseq.SetId()) 
    {
        x_UpdateSeqId(id_map, **it); 
    }
     

    EDIT_EACH_SEQANNOT_ON_BIOSEQ(it, bioseq) 
    {
        x_UpdateSeqAnnot(id_map, **it);
    }

    if (bioseq.IsSetInst() &&
        bioseq.GetInst().IsSetHist()) {
        x_UpdateSeqHist(id_map, bioseq.SetInst().SetHist());
    }

/*
    if (bioseq.IsSetInst() &&
        bioseq.GetInst().IsSetExt()) {
        x_UpdateSeqExt(old_id, new_id, bioseq.SetInst().SetExt());
    }
*/
    return;
}


void CSeqIdUpdateApp::x_UpdateSeqHist(const TIdMap& id_map, 
                                      CSeq_hist& hist) 
{
    if (hist.IsSetAssembly()) {
        NON_CONST_ITERATE(CSeq_hist::TAssembly, it, hist.SetAssembly()) {
            x_UpdateSeqAlign(id_map, **it);
        }
    }

    if (hist.IsSetReplaces() &&
        hist.GetReplaces().IsSetIds()) {
        NON_CONST_ITERATE(CSeq_hist_rec::TIds, it, hist.SetReplaces().SetIds()) {
            x_UpdateSeqId(id_map, **it);
        }
    }

    if (hist.IsSetReplaced_by() && 
        hist.GetReplaced_by().IsSetIds()) {
        NON_CONST_ITERATE(CSeq_hist_rec::TIds, it, hist.SetReplaced_by().SetIds()) {
            x_UpdateSeqId(id_map, **it);
        }
    }
    return;
}



void CSeqIdUpdateApp::x_UpdateSeqAlign(const TIdMap& id_map,
                                       CSeq_align& align)
{
    EDIT_EACH_RECURSIVE_SEQALIGN_ON_SEQALIGN(ait, align) 
    {
        x_UpdateSeqAlign(id_map, align);
    }

    if (!align.IsSetSegs()) {
        return;
    }

    if (align.GetSegs().IsDendiag()) {
        EDIT_EACH_DENDIAG_ON_SEQALIGN(dit, align) 
        {
            EDIT_EACH_SEQID_ON_DENDIAG(it, **dit) 
            {
                x_UpdateSeqId(id_map, **it);       
            }
        }
        return;
    }


    if (align.GetSegs().IsDenseg()) {
        EDIT_EACH_SEQID_ON_DENSEG(it, align.SetSegs().SetDenseg()) 
        {
            x_UpdateSeqId(id_map, **it);
        }
        return;
    }


    if (align.GetSegs().IsStd()) {
        EDIT_EACH_STDSEG_ON_SEQALIGN(sit, align) {

            if ((*sit)->IsSetIds()) {
                NON_CONST_ITERATE(CStd_seg::TIds, it, (*sit)->SetIds()) {
                    x_UpdateSeqId(id_map, **it);
                }
            }

            if ((*sit)->IsSetLoc()) {
                NON_CONST_ITERATE(CStd_seg::TLoc, it, (*sit)->SetLoc()) {
                    x_UpdateSeqLoc(id_map, **it);
                }
            }
        }
    }

    return;
}


/*
void CSeqIdUpdateApp::x_UpdateSeqExt(const TIdMap& id_map,
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
*/


void CSeqIdUpdateApp::x_UpdateSeqAnnot(const TIdMap& id_map,
                                       CSeq_annot& annot) 
{
    EDIT_EACH_SEQFEAT_ON_SEQANNOT(it, annot)
    {
        x_UpdateSeqFeat(id_map, **it);
    }

    EDIT_EACH_SEQALIGN_ON_SEQANNOT(it, annot)
    {
        x_UpdateSeqAlign(id_map, **it);
    }
    return;

    EDIT_EACH_ANNOTDESC_ON_SEQANNOT(it, annot) 
    {
        if ((*it)->IsSrc()) {
            x_UpdateSeqId(id_map, (*it)->SetSrc());
        } 
        else 
        if ((*it)->IsRegion()) {
            x_UpdateSeqLoc(id_map, (*it)->SetRegion());
        }
    }

    if (!annot.IsSetData()) {
        return;
    }

    CSeq_annot::TData& data = annot.SetData();


    if (data.IsIds()) {
        NON_CONST_ITERATE(CSeq_annot::C_Data::TIds, it, data.SetIds()) 
        {
            x_UpdateSeqId(id_map, **it);
        }
        return;
    }


    if (data.IsLocs()) {
        NON_CONST_ITERATE(CSeq_annot::C_Data::TLocs, it, data.SetLocs()) {
            x_UpdateSeqLoc(id_map, **it);
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


            if ((*cit)->IsSetData() &&
                (*cit)->GetData().IsId()) {

                NON_CONST_ITERATE(CSeqTable_multi_data::TId, it, (*cit)->SetData().SetId()) 
                {
                    x_UpdateSeqId(old_id, new_id, **it);
                }
            }


            if ((*cit)->IsSetDefault() &&
                (*cit)->GetDefault().IsLoc()) {
                x_UpdateSeqLoc(old_id, new_id, (*cit)->SetDefault().SetLoc());
            }
            else
            if ((*cit)->IsSetDefault() &&
                (*cit)->GetDefault().IsId()) {
                x_UpdateSeqId(old_id, new_id, (*cit)->SetDefault().SetId());
            }


            if ((*cit)->IsSetSparse_other() &&
                (*cit)->GetSparse_other().IsLoc()) {
                x_UpdateSeqLoc(old_id, new_id, (*cit)->SetSparse_other().SetLoc());
            }
            else
            if ((*cit)->IsSetSparse_other() &&
                (*cit)->GetSparse_other().IsId()) {
                x_UpdateSeqId(old_id, new_id, (*cit)->SetSparse_other().SetId());
            } 
        }
    }
*/
    return;
}


void CSeqIdUpdateApp::x_UpdateSeqFeat(const TIdMap& id_map,
                                      CSeq_feat& seq_feat) 
{

    if (seq_feat.IsSetLocation()) {
        x_UpdateSeqLoc(id_map, seq_feat.SetLocation());
    }

    if (seq_feat.IsSetProduct()) {
        x_UpdateSeqLoc(id_map, seq_feat.SetProduct());
    }

/*

    if (seq_feat.IsSetData() &&
        seq_feat.GetData().IsRna() &&
        seq_feat.GetData().GetRna().IsSetExt() &&
        seq_feat.GetData().GetRna().GetExt().IsTRNA() &&
        seq_feat.GetData().GetRna().GetExt().GetTRNA().IsSetAnticodon())
    {
        x_UpdateSeqLoc(old_id, new_id, seq_feat.SetData().SetRna().SetExt().SetTRNA().SetAnticodon());
    }


    if (seq_feat.IsSetData() &&
        seq_feat.GetData().IsCdregion())
    {
        EDIT_EACH_CODEBREAK_ON_CDREGION(cb, seq_feat.SetData().SetCdregion())
        {
            if ((*cb)->IsSetLoc())
            {
                x_UpdateSeqLoc(old_id, new_id, (*cb)->SetLoc());
            }
        }
    }

    if (seq_feat.IsSetData() &&
        seq_feat.GetData().IsClone() &&
        seq_feat.GetData().GetClone().IsSetClone_seq() &&
        seq_feat.GetData().GetClone().GetClone_seq().IsSet()) 
    {
        NON_CONST_ITERATE(CClone_seq_set::Tdata, it, seq_feat.SetData().SetClone().SetClone_seq().Set()) 
        {
            if ((*it)->IsSetLocation()) {
                x_UpdateSeqLoc(old_id, new_id, (*it)->SetLocation());
            }


            if ((*it)->IsSetSeq()) {
                x_UpdateSeqLoc(old_id, new_id, (*it)->SetSeq());
            }

        }
    }

    */

    return;
}



void CSeqIdUpdateApp::x_UpdateSeqEntry(const TIdMap& id_map,
                                       CSeq_entry_Handle& tlseh) 
{

    for (CSeq_entry_CI it(tlseh, CSeq_entry_CI::fRecursive); it; ++it) {
       CSeq_entry_Handle seh = *it;

       CRef<CSeq_entry> new_entry(new CSeq_entry());

       if (seh.IsSeq()) {
           CRef<CBioseq> new_seq(new CBioseq());
           new_seq->Assign(*seh.GetSeq().GetBioseqCore());

           x_UpdateBioseq(id_map, *new_seq);

           new_entry->SetSeq(*new_seq);

           CSeq_entry_EditHandle edit_handle(seh); // Is there a better way to 
           edit_handle.SelectNone(); // generate a blank CSeq_entry_edit_Handle??
           edit_handle.SelectSeq(*new_seq);
       }
       else
       {
           SAnnotSelector sel;
           for (CAnnot_CI annot_ci(seh, sel); annot_ci; ++annot_ci) 
           {
               CRef<CSeq_annot> new_annot(new CSeq_annot());
               new_annot->Assign(*annot_ci->GetSeq_annotCore());
               x_UpdateSeqAnnot(id_map, *new_annot);

               CSeq_annot_Handle sah(*annot_ci);
               CSeq_annot_Handle new_sah = sah.GetScope().AddSeq_annot(*new_annot); // Add the new annot to the scope
               sah.Swap(new_sah);
           }
       }
    }

    return;
}



END_NCBI_SCOPE

USING_NCBI_SCOPE;


int main(int argc, const char* argv[])
{
    return CSeqIdUpdateApp().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}
