#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
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
#include <objects/seqset/seqset_macros.hpp>
#include <objects/seqfeat/Clone_seq_set.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/object_manager.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/annot_ci.hpp>
#include <util/line_reader.hpp>
#include <objtools/edit/protein_match/prot_match_exception.hpp>

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
        CSeq_entry& nuc_prot_set);

    void x_UpdateBioseqSet(const TIdMap& id_map,
        CBioseq_set& seqset);

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
                        CSeq_loc& seq_loc) const;

    void x_UpdateSeqId(const TIdMap& id_map,
                       CSeq_id& id) const;
    
    void x_UpdateSeqId(const CSeq_id& old_id, 
                       const CSeq_id& new_id,
                       CSeq_id& id) const;

    CObjectIStream* x_InitInputEntryStream(
            const CArgs& args);

    bool x_ProcessInputTable(CNcbiIstream& istr, 
                             TIdMap& id_map);

    void  x_ReadUpdateFile(CObjectIStream& istr,
        bool& isSeqSet,
        CSeq_entry& seq_entry) const;

    bool x_TryReadBioseqSet(CObjectIStream& istr, 
        CSeq_entry& seq_entry) const;

    bool x_TryReadSeqEntry(CObjectIStream& istr,
        CSeq_entry& seq_entry) const;
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

    // Set up object input stream
    unique_ptr<CObjectIStream> istr(x_InitInputEntryStream(args));

    // Attempt to read input file
    bool isSeqSet = false;
    CRef<CSeq_entry> seq_entry = Ref(new CSeq_entry());
    x_ReadUpdateFile(*istr, isSeqSet, *seq_entry);

    CNcbiIstream& table_str = args["t"].AsInputFile();
    map<CRef<CSeq_id>, CRef<CSeq_id>> id_map;
    x_ProcessInputTable(table_str, id_map);

    CNcbiOstream& ostr = (args["o"]) ? 
        args["o"].AsOutputFile() :
        cout;

    unique_ptr<MSerial_Format> pOutFormat(new MSerial_Format_AsnText());

    x_UpdateSeqEntry(id_map, *seq_entry);

    if (isSeqSet) {
        ostr << MSerial_Format_AsnText() << seq_entry->GetSet();
    } else {
        ostr << MSerial_Format_AsnText() << *seq_entry;
    }
    return 0;
}


void CProtIdUpdateApp::x_ReadUpdateFile(CObjectIStream& istr,
    bool& isSeqSet,
    CSeq_entry& seq_entry) const
{

    isSeqSet = false;

    set<TTypeInfo> knownTypes, matchingTypes;
    knownTypes.insert(CSeq_entry::GetTypeInfo());
    knownTypes.insert(CBioseq_set::GetTypeInfo());
    matchingTypes = istr.GuessDataType(knownTypes);

    if (matchingTypes.empty()) {
        NCBI_THROW(CProteinMatchException,
            eInputError,
            "Unrecognized input");
    }

    const TTypeInfo typeInfo = *matchingTypes.begin();

    if (typeInfo == CSeq_entry::GetTypeInfo()) {
        if (!x_TryReadSeqEntry(istr, seq_entry)) {
            NCBI_THROW(CProteinMatchException,
                eInputError,
                "Failed to read Seq-entry");
        }
    } 
    else 
    if (typeInfo == CBioseq_set::GetTypeInfo()) {
        if (!x_TryReadBioseqSet(istr, seq_entry)) {
            NCBI_THROW(CProteinMatchException,
                eInputError,
                "Failed to read Bioseq-set");
        }
        isSeqSet = true;
    }
}


bool CProtIdUpdateApp::x_TryReadSeqEntry(CObjectIStream& istr,
    CSeq_entry& seq_entry) const
{
    try {
        istr.Read(ObjectInfo(seq_entry));
    }
    catch (CException&) {
        return false;
    }

    return true;
}


bool CProtIdUpdateApp::x_TryReadBioseqSet(CObjectIStream& istr,
    CSeq_entry& seq_entry) const
{
    CRef<CBioseq_set> seq_set = Ref(new CBioseq_set());
    try {
        istr.Read(ObjectInfo(seq_set.GetNCObject()));
    } 
    catch (CException&) {
        return false;
    }

    seq_entry.SetSet(seq_set.GetNCObject());
    return true;
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
            string delim = " \t";
            NStr::Split(line, delim, entries, NStr::fSplit_MergeDelimiters);
            if (!entries.empty() &&
                entries[0] == "NA_ACCESSION") {
                continue;
            } 
       

            if (entries.size() < 5) { 
                continue;             
            }


            if (entries[2] == "---" ||  // No local ID
                entries[3] != "PROT" || // Not a protein
                (entries[4] != "Same" && entries[4] != "New")) {
                continue;
            }


            CRef<CSeq_id> gb_id = Ref(new CSeq_id());
            gb_id->SetGenbank().Set(entries[1]);

            CRef<CSeq_id> local = Ref(new CSeq_id());
            local->SetLocal().SetStr(entries[2]);

            id_map[local] = gb_id;
        } 
    }


    return true;
}


void CProtIdUpdateApp::x_UpdateSeqId(const CSeq_id& old_id,
    const CSeq_id& new_id,
    CSeq_id& id) const
{
    if (id.Compare(old_id) != CSeq_id::e_YES) {
        return;
    }
    id.Assign(new_id);
    return;
}


void CProtIdUpdateApp::x_UpdateSeqId(const TIdMap& id_map,
    CSeq_id& id) const
{
    for (const auto& id_pair : id_map) {
        const CSeq_id& old_id = *(id_pair.first);
        const CSeq_id& new_id = *(id_pair.second);

        if (id.Compare(old_id) == CSeq_id::e_YES) {
            id.Assign(new_id);
        }
    }
    return;
}


void CProtIdUpdateApp::x_UpdateSeqLoc(const TIdMap& id_map,
    CSeq_loc& seq_loc) const
{
    CRef<CSeq_id> seq_id = Ref(new CSeq_id());
    seq_id->Assign(*(seq_loc.GetId()));
    x_UpdateSeqId(id_map, *seq_id);
    seq_loc.SetId(*seq_id);
    return;
}


void CProtIdUpdateApp::x_UpdateSeqEntry(const TIdMap& id_map,
    CSeq_entry& seq_entry) 
{
    if (seq_entry.IsSeq()) {
        x_UpdateBioseq(id_map, seq_entry.SetSeq());
        return;
    }

    // else is set
    x_UpdateBioseqSet(id_map, seq_entry.SetSet());
    return;
}


void CProtIdUpdateApp::x_UpdateBioseqSet(const TIdMap& id_map,
    CBioseq_set& seqset) 
{
    EDIT_EACH_SEQENTRY_ON_SEQSET(it, seqset) {
        x_UpdateSeqEntry(id_map, **it);
    }


    EDIT_EACH_SEQANNOT_ON_SEQSET(it, seqset) {
        x_UpdateSeqAnnot(id_map, **it);
    }
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
    default : 
        break;

    case CSeq_annot::TData::e_Ftable: 
        EDIT_EACH_SEQFEAT_ON_SEQANNOT(it, annot)
        {
            x_UpdateSeqFeat(id_map, **it);
        }
        break;

    case CSeq_annot::TData::e_Align: 
        EDIT_EACH_SEQALIGN_ON_SEQANNOT(it, annot)
        {
            x_UpdateSeqAlign(id_map, **it);
        }
        break;

    case CSeq_annot::TData::e_Graph: 
        // EACH_SEQGRAPH_ON_SEQANNOT gives a compilation error
        // Loop explicitly
        for (CRef<CSeq_graph> seq_graph : annot.SetData().SetGraph()) {
            x_UpdateSeqGraph(id_map, *seq_graph);
        }
        break;

    case CSeq_annot::TData::e_Ids: 
        NON_CONST_ITERATE(CSeq_annot::C_Data::TIds, it, annot.SetData().SetIds()) 
        {
            x_UpdateSeqId(id_map, **it);
        }
        break;

    case CSeq_annot::TData::e_Locs: 
        NON_CONST_ITERATE(CSeq_annot::C_Data::TLocs, it, annot.SetData().SetLocs()) {
            x_UpdateSeqLoc(id_map, **it);
        }
        break;
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


END_NCBI_SCOPE

USING_NCBI_SCOPE;


int main(int argc, const char* argv[])
{
    return CProtIdUpdateApp().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}
