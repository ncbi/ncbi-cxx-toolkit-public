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

#include <serial/objcopy.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


class CCopySeqIdHook : public CCopyObjectHook
{
public:

    typedef map<string, CRef<CSeq_id>> TIdMap;
    CCopySeqIdHook(TIdMap& id_map) : m_IdMap(id_map) {}

    virtual void CopyObject(CObjectStreamCopier& copier,
        const CObjectTypeInfo& passed_info);

    void x_UpdateSeqId(const TIdMap& id_map, 
        CSeq_id& id) const;

private:
    TIdMap m_IdMap; 
};


void CCopySeqIdHook::CopyObject(CObjectStreamCopier& copier,
    const CObjectTypeInfo& passed_info) 
{
    CSeq_id seq_id;
    copier.In().ReadObject(&seq_id, CSeq_id::GetTypeInfo());

    if (seq_id.IsLocal()) {
        x_UpdateSeqId(m_IdMap, seq_id);
    }

    copier.Out().WriteObject(&seq_id, CSeq_id::GetTypeInfo());
}


void CCopySeqIdHook::x_UpdateSeqId(const TIdMap& id_map,
    CSeq_id& id) const
{
    if (!id.IsLocal()) {
        return;
    }

    const string id_string = id.GetLocal().IsStr() ?
        id.GetLocal().GetStr() :
        NStr::NumericToString(id.GetLocal().GetId());

    const auto match = id_map.find(id_string);

    if (match != id_map.end()) {
        const CSeq_id& new_id = *(match->second);
        id.Assign(new_id); // Reference - Sergiy G. 
    }
    return;
}

class CProtIdUpdateApp : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);

private:

    typedef map<string, CRef<CSeq_id>> TIdMap;

    bool x_ProcessInputTable(CNcbiIstream& istr, 
                             TIdMap& id_map);
};


void CProtIdUpdateApp::Init(void) 
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions());

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

    CNcbiIstream& table_str = args["t"].AsInputFile();
    map<string, CRef<CSeq_id>> id_map;
    x_ProcessInputTable(table_str, id_map);

    unique_ptr<CObjectIStream> istr(CObjectIStream::Open(eSerial_AsnText, args["i"].AsString()));
    unique_ptr<CObjectOStream> ostr(CObjectOStream::Open(eSerial_AsnText, args["o"].AsString()));

    CObjectStreamCopier copier(*istr, *ostr);
    CObjectTypeInfo(CType<CSeq_id>()) 
        .SetLocalCopyHook(copier, new CCopySeqIdHook(id_map));

    set<TTypeInfo> knownTypes, matchingTypes;
    knownTypes.insert(CSeq_entry::GetTypeInfo());
    knownTypes.insert(CBioseq_set::GetTypeInfo());
    matchingTypes = istr->GuessDataType(knownTypes);

    if (matchingTypes.empty()) {
        NCBI_THROW(CProteinMatchException,
            eInputError,
            "Unrecognized input");
    }

    const TTypeInfo typeInfo = *matchingTypes.begin();

    if (typeInfo == CSeq_entry::GetTypeInfo()) {
        copier.Copy(CType<CSeq_entry>());
    } else {
        copier.Copy(CType<CBioseq_set>());
    }

    return 0;
}


bool CProtIdUpdateApp::x_ProcessInputTable(CNcbiIstream& istr, TIdMap& id_map) 
{
    CStreamLineReader lr(istr);

    while ( !lr.AtEOF() ) {
        string line = *++lr;

        if (line.empty() ||
            NStr::StartsWith(line, "#")) {
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

        if (entries[1] == "___") {
            ERR_POST(Warning << entries[1] << ": Protein accession unspecified");
            continue;
        }

        CRef<CSeq_id> gb_id = Ref(new CSeq_id());
        gb_id->SetGenbank().Set(entries[1]);

        string local = entries[2];
        id_map[local] = gb_id;
    }

    return true;
}


END_NCBI_SCOPE
USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    return CProtIdUpdateApp().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}
