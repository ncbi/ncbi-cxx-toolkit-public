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


class CProtIdUpdateApp : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);

private:

    typedef map<CRef<CSeq_id>, CRef<CSeq_id>> TIdMap;

    void x_UpdateSeqEntry(const TIdMap& id_map,
                          CSeq_entry_Handle& seh);

    void x_UpdateBioseq(const TIdMap& id_map,
                        CBioseq& bioseq);

    void x_UpdateSeqAnnot(const TIdMap& id_map,
                          CSeq_annot& seq_annot);

    void x_UpdateAnnotDesc(const TIdMap& id_map,
                            CAnnotdesc& annot_desc);
 
    void x_UpdateSeqFeat(const TIdMap& id_map,
                         CSeq_feat& seq_feat);

    void x_UpdateSeqAlign(const TIdMap& id_map,
                          CSeq_align& align);

    void x_UpdateSeqGraph(const TIdMap& id_map,
                          CSeq_graph& graph);

    void x_UpdateSeqLoc(const TIdMap& id_map,
                        CSeq_loc& seq_loc);

    void x_UpdateSeqId(const TIdMap& id_map,
                       CSeq_id& id);
    
    void x_UpdateSeqId(const CSeq_id& old_id, 
                       const CSeq_id& new_id,
                       CSeq_id& id);

    CObjectIStream* x_InitInputEntryStream(
            const CArgs& args);

    bool x_ProcessInputTable(CNcbiIstream& istr, 
                             TIdMap& id_map);
};


void CProtIdUpdateApp::Init(void) 
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


int CProtIdUpdateApp::Run(void) 
{
    const CArgs& args = GetArgs();

    // Set up scope
    CRef<CObjectManager> obj_mgr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*obj_mgr);
    CRef<CScope> scope(new CScope(*obj_mgr));
    scope->AddDefaults();

    // Set up object input stream
    unique_ptr<CObjectIStream> istr(x_InitInputEntryStream(args));

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
    map<CRef<CSeq_id>, CRef<CSeq_id>> id_map;
    x_ProcessInputTable(table_str, id_map);

    CNcbiOstream& ostr = (args["o"]) ? 
        args["o"].AsOutputFile() :
        cout;

    unique_ptr<MSerial_Format> pOutFormat(new MSerial_Format_AsnText());
   
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


CObjectIStream* CProtIdUpdateApp::x_InitInputEntryStream(
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


bool CProtIdUpdateApp::x_ProcessInputTable(CNcbiIstream& istr, TIdMap& id_map) 
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
                entries[1] == "AccVer") { // Skip over the column titles
                continue;
            } 
            else 
            if (entries.size() < 2) { // Each row must contain at least a local id 
                continue;             // and an accession+version.
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




void CProtIdUpdateApp::x_UpdateSeqId(const CSeq_id& old_id,
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



void CProtIdUpdateApp::x_UpdateSeqId(const TIdMap& id_map,
                                    CSeq_id& id)
{
    for (const auto& id_pair : id_map) {
        const CSeq_id& old_id = *(id_pair.first);
        const CSeq_id& new_id = *(id_pair.second);
        x_UpdateSeqId(old_id, new_id, id);
    }
    return;
}


void CProtIdUpdateApp::x_UpdateSeqLoc(const TIdMap& id_map,
                                     CSeq_loc& seq_loc) 
{
    CRef<CSeq_id> seq_id = Ref(new CSeq_id());
    seq_id->Assign(*(seq_loc.GetId()));
    x_UpdateSeqId(id_map, *seq_id);
    seq_loc.SetId(*seq_id);
    return;
}


// Update the ids and annotations on the bioseq
void CProtIdUpdateApp::x_UpdateBioseq(const TIdMap& id_map,
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

    return;
}

void CProtIdUpdateApp::x_UpdateSeqAnnot(const TIdMap& id_map,
                                       CSeq_annot& annot) 
{
    // First update Annotdesc on the annotation
    EDIT_EACH_ANNOTDESC_ON_SEQANNOT(it, annot) 
    {
        x_UpdateAnnotDesc(id_map, **it);
    }

    if (!annot.IsSetData()) {
        return;
    }

    // Supported annotation types are 
    // 1) Feature tables
    // 2) Alignments
    // 3) Graphs
    // 4) Seq-id sets
    // 5) Seq-loc sets
    // Seq-table is not supported
    switch (annot.GetData().Which()) {
   
    default : {
        break;
    } 

    case CSeq_annot::TData::e_Ftable: { // Feature table
        EDIT_EACH_SEQFEAT_ON_SEQANNOT(it, annot)
        {
            x_UpdateSeqFeat(id_map, **it);
        }
        break;
    }

    case CSeq_annot::TData::e_Align: { // Set of alignments
        EDIT_EACH_SEQALIGN_ON_SEQANNOT(it, annot)
        {
            x_UpdateSeqAlign(id_map, **it);
        }
        break;
    }

    case CSeq_annot::TData::e_Graph: { // Set of graphs
        // EACH_SEQGRAPH_ON_SEQANNOT gives a compilation error
        // Loop explicitly
        auto&& graphs = annot.SetData().SetGraph();
        for (CRef<CSeq_graph> seq_graph : graphs) {
            x_UpdateSeqGraph(id_map, *seq_graph);
        }
        break;
    }

    case CSeq_annot::TData::e_Ids: {
        NON_CONST_ITERATE(CSeq_annot::C_Data::TIds, it, annot.SetData().SetIds()) 
        {
            x_UpdateSeqId(id_map, **it);
        }
        break;
    }

    case CSeq_annot::TData::e_Locs: {
        NON_CONST_ITERATE(CSeq_annot::C_Data::TLocs, it, annot.SetData().SetLocs()) {
            x_UpdateSeqLoc(id_map, **it);
        }
        break;
    }


    }; // switch

    // Doesn't cover Seq_table
    return;
}



void CProtIdUpdateApp::x_UpdateAnnotDesc(const TIdMap& id_map,
                                          CAnnotdesc& annot_desc)
{
   if (annot_desc.IsSrc()) {
       x_UpdateSeqId(id_map, annot_desc.SetSrc());
   }
   else 
   if (annot_desc.IsRegion()) {
       x_UpdateSeqLoc(id_map, annot_desc.SetRegion());
   }
}


void CProtIdUpdateApp::x_UpdateSeqAlign(const TIdMap& id_map,
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


void CProtIdUpdateApp::x_UpdateSeqGraph(const TIdMap& id_map, 
                                        CSeq_graph& graph)
{
    if (!graph.IsSetLoc()) {
        return;
    }
    x_UpdateSeqLoc(id_map, graph.SetLoc());
}



void CProtIdUpdateApp::x_UpdateSeqFeat(const TIdMap& id_map,
                                      CSeq_feat& seq_feat) 
{
    // If protein feature, the location needs to be updated
    if (seq_feat.IsSetLocation()) {
        x_UpdateSeqLoc(id_map, seq_feat.SetLocation());
    }

    // If this is a CDS, the product needs to be updated
    if (seq_feat.IsSetData() &&
        seq_feat.GetData().IsCdregion() &&
        seq_feat.IsSetProduct()) {
        x_UpdateSeqLoc(id_map, seq_feat.SetProduct());
    }

    return;
}



void CProtIdUpdateApp::x_UpdateSeqEntry(const TIdMap& id_map,
                                       CSeq_entry_Handle& tlseh) 
{
    for (CSeq_entry_CI it(tlseh, CSeq_entry_CI::fRecursive); it; ++it) {
       CSeq_entry_Handle seh = *it;

       if (seh.IsSeq()) { // Bioseq
           CRef<CBioseq> new_seq(new CBioseq());
           new_seq->Assign(*seh.GetSeq().GetBioseqCore());
           
           // update ids on the sequence and in the sequence annotations
           x_UpdateBioseq(id_map, *new_seq);

           CSeq_entry_EditHandle edit_handle(seh); // Is there a better way to 
           edit_handle.SelectNone(); // generate a blank CSeq_entry_edit_Handle??
           edit_handle.SelectSeq(*new_seq);
       }
       else // Must be Bioseq-set
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
    return CProtIdUpdateApp().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}
