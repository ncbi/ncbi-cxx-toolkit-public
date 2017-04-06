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
* Author:  Justin Foley
*
* File Description:
*   implicit protein matching driver code
*
* ===========================================================================
*/


#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbifile.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/util/sequence.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/streamiter.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/edit/protein_match/setup_match.hpp>
#include <objtools/edit/protein_match/generate_match_table.hpp>
#include <objtools/edit/protein_match/prot_match_exception.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <misc/data_loaders_util/data_loaders_util.hpp>

#include "run_binary.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

class CProteinMatchApp : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);

private:
    using TSeqEntryLabel = string;
    using TEntryFilenameMap = map<TSeqEntryLabel, string>;
    using TEntryOStreamMap  = map<TSeqEntryLabel, unique_ptr<CObjectOStream>>;

    template<typename TRoot>
    void x_GenerateMatchTable(CObjectIStream& istr, 
        const string& out_stub, 
        bool keep_temps, 
        CBinRunner& assm_assm_blastn,
        CBinRunner& compare_annots, 
        CMatchTabulate& match_tab);

    void x_WriteMatchTable(
        const string& table_file,
        const CMatchTabulate& match_tab,
        bool suppress_exception=false) const;

    void x_ProcessSeqEntry(CRef<CSeq_entry> nuc_prot_set,
        const CProteinMatchApp::TEntryFilenameMap& filename_map,
        CProteinMatchApp::TEntryOStreamMap& ostream_map,
        map<string, string>& nuc_id_replacement_map,
        map<string, list<string>>& local_prot_ids,
        map<string, list<string>>& prot_accessions,
        CMatchTabulate& match_tab);

    CObjectIStream* x_InitObjectIStream(const CArgs& args);
    CObjectOStream* x_InitObjectOStream(const string& filename, 
        const bool binary) const;
    CObjectIStream* x_InitObjectIStream(const string& filename,
        const bool binary) const;

    TTypeInfo x_GetRootTypeInfo(CObjectIStream& istr) const;
    bool x_TryReadSeqEntry(CObjectIStream& istr, CSeq_entry& seq_entry) const;
    bool x_TryReadBioseqSet(CObjectIStream& istr, CSeq_entry& seq_entry) const;
    void x_WriteEntry(const CSeq_entry& entry,
        const string filename,
        const bool as_binary);

    void x_WriteEntry(const CSeq_entry& seq_entry,
        const string& key,
        const CProteinMatchApp::TEntryFilenameMap& filename_map,
        CProteinMatchApp::TEntryOStreamMap& ostream_map) const;

    void x_GetBlastArgs(
        const bool binary_output,
        const string& update_file, 
        const string& genbank_file,
        const string& alignment_file,
        string& blast_args) const;

    void x_GetCompareAnnotsArgs(const string& update_file,
        const string& genbank_file,
        const string& alignment_manifest_file,
        const string& annot_file,
        string& compare_annots_args) const;

    void x_ReadAnnotFile(const string& filename, list<CRef<CSeq_annot>>& seq_annots) const;

    void x_ReadAlignmentFile(const string& filename, CRef<CSeq_align>& alignment) const;

    void x_LogTempFile(const string& string);
    void x_DeleteTempFiles(void);

    struct SSeqEntryFilenames {
        string db_nuc_prot_set;
        string db_nuc_seq;
        string local_nuc_prot_set;
        string local_nuc_seq;
    };

    void x_GetSeqEntryFileNames(const string& file_stub, 
    SSeqEntryFilenames& filenames) const;

    void x_GetSeqEntryFileNames(const string& file_stub,
        TEntryFilenameMap&  filename_map) const;

    void x_CreateSeqEntryOStreams(const TEntryFilenameMap& filename_map,
        TEntryOStreamMap& ostream_map) const;

    void x_WriteToSeqEntryTempFiles(
        const CBioseq_set& nuc_prot_set,
        const CBioseq_set& db_nuc_prot_set, 
        const CProteinMatchApp::TEntryFilenameMap& filename_map,
        CProteinMatchApp::TEntryOStreamMap& ostream_map) const; 

    void x_GatherLocalProteinIds(const CBioseq_set& nuc_prot_set, list<string>& id_list) const;

    void x_GatherProteinAccessions(const CBioseq_set& nuc_prot_set, list<string>& id_list) const;

    void x_RelabelNucSeq(CRef<CSeq_entry> nuc_prot_set);

    SSeqEntryFilenames x_GenerateSeqEntryTempFiles(
        const CBioseq_set& local_nuc_prot_set, // temporary
        const CBioseq_set& db_nuc_prot_set,
        const string& out_stub,
        const unsigned int count);

    unique_ptr<CMatchSetup> m_pMatchSetup;
    list<string> m_TempFiles;
};


void CProteinMatchApp::Init(void)
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions());

    arg_desc->AddKey("i",
        "InputFile",
        "Update input file",
        CArgDescriptions::eInputFile);

    arg_desc->AddKey("o",
        "OutputFile",
        "Match-table file",
        CArgDescriptions::eOutputFile);

    arg_desc->AddOptionalKey("bindir", 
        "Binary_Dir", 
        "Directory containing C++ binaries, if not CWD",
        CArgDescriptions::eString);

    arg_desc->AddOptionalKey("outdir",
        "Output_Dir",
        "Output directory, if not specified write to CWD",
        CArgDescriptions::eString);

    arg_desc->AddFlag("keep-temps", "Retain temporary files");

    CDataLoadersUtil::AddArgumentDescriptions(*arg_desc,
        CDataLoadersUtil::fDefault);

    SetupArgDescriptions(arg_desc.release());

    return;
}


int CProteinMatchApp::Run(void)
{
    const CArgs& args = GetArgs();

    const string bin_dir = args["bindir"] ? 
        args["bindir"].AsString() :
        CDir::GetCwd();

    string out_dir = CDir::GetCwd();
    if (args["outdir"]) {
        CDir outputdir(args["outdir"].AsString());
        if (!outputdir.Exists()) {
            outputdir.Create();
        }
        out_dir = args["outdir"].AsString();
    }

    CBinRunner assm_assm_blastn(bin_dir, "assm_assm_blastn");
    CBinRunner compare_annots(bin_dir, "compare_annots");

    CRef<CObjectManager> obj_mgr = CObjectManager::GetInstance();
    CDataLoadersUtil::SetupObjectManager(args, *obj_mgr);
    CRef<CScope> db_scope = Ref(new CScope(*obj_mgr));
    db_scope->AddDefaults();
    m_pMatchSetup.reset(new CMatchSetup(db_scope));

    bool keep_temps = args["keep-temps"];
    
    unique_ptr<CObjectIStream> pInStream(x_InitObjectIStream(args));

    const string table_file = CDirEntry::MakePath(
        out_dir,
        args["o"].AsString());

    CMatchTabulate match_tab(db_scope);

    const TTypeInfo root_info =  x_GetRootTypeInfo(*pInStream);

    try {
        if (root_info == CSeq_entry::GetTypeInfo()) {
            x_GenerateMatchTable<CSeq_entry>(*pInStream,
                table_file,
                keep_temps,
                assm_assm_blastn,
                compare_annots,
                match_tab);
        } else { // Must be CBioseq_set
            x_GenerateMatchTable<CBioseq_set>(*pInStream,
                table_file,
                keep_temps,
                assm_assm_blastn,
                compare_annots,
                match_tab);
        }

    } 
    catch (...) 
    {
        const bool suppress_write_exceptions = true;
        x_WriteMatchTable(table_file, match_tab, suppress_write_exceptions);
        throw;
    }

    x_WriteMatchTable(table_file, match_tab);
    return 0;
}


void CProteinMatchApp::x_WriteMatchTable(
        const string& table_file,
        const CMatchTabulate& match_tab,
        const bool suppress_exception) const
{
    if (match_tab.GetNum_rows() == 0) {
        ERR_POST(Warning << "Match table is empty");
        return;
    }

    try {
        CNcbiOfstream ostr(table_file.c_str());
        match_tab.WriteTable(ostr);
    }
    catch (...) {
        if (suppress_exception) {
            return;
        }
        NCBI_THROW(CProteinMatchException,
            eOutputError,
            "Could not write match table");
    }
}


template<typename TRoot>
void CProteinMatchApp::x_GenerateMatchTable(CObjectIStream& istr, 
        const string& out_stub,
        bool keep_temps, 
        CBinRunner& assm_assm_blastn,
        CBinRunner& compare_annots, 
        CMatchTabulate& match_tab)

{
    TEntryFilenameMap filename_map;
    x_GetSeqEntryFileNames(out_stub, filename_map);
    
    TEntryOStreamMap ostream_map;
    x_CreateSeqEntryOStreams(filename_map, ostream_map);

    map<string, string> nuc_id_replacement_map; // Replacement app
    map<string, list<string>> local_prot_ids;
    map<string, list<string>> prot_accessions;

    try {

        for (const CBioseq_set& obj : 
            CObjectIStreamIterator<TRoot, CBioseq_set>(istr))
        {

            if (!obj.IsSetClass() ||
                obj.GetClass() != CBioseq_set::eClass_nuc_prot) {
                continue;
            }

            CRef<CSeq_entry> seq_entry = Ref(new CSeq_entry());
            CRef<CBioseq_set> bio_set = Ref(new CBioseq_set());
            bio_set->Assign(obj);
            seq_entry->SetSet(*bio_set);

            x_ProcessSeqEntry(seq_entry,
                filename_map,
                ostream_map,
                nuc_id_replacement_map,
                local_prot_ids,
                prot_accessions,
                match_tab); 
        }

        for ( auto& kv : ostream_map) {
            CObjectOStream& obj_ostream = *kv.second;
            obj_ostream.Flush();
        }

        const string alignment_file = out_stub + ".merged.asn";
        const bool binary_output = true;

        string blast_args;
        x_GetBlastArgs(
            binary_output,
            filename_map["local_nuc_seq"],
            filename_map["db_nuc_seq"],
            alignment_file,
            blast_args);

        x_LogTempFile(alignment_file);
        assm_assm_blastn.Exec(blast_args);
        
        // Create alignment manifest tempfile 
        const string manifest_file = out_stub + ".aln.mft";
        try {
            CNcbiOfstream ostr(manifest_file.c_str());
            x_LogTempFile(manifest_file);
            ostr << alignment_file << endl;
        }
        catch(...) {
            NCBI_THROW(CProteinMatchException,
                eOutputError,
                "Could not write alignment manifest");
        }
        const string annot_file = out_stub + ".compare.asn";

        string compare_annots_args;
        x_GetCompareAnnotsArgs(
            filename_map["local_nuc_prot_set"],
            filename_map["db_nuc_prot_set"],
            manifest_file, 
            annot_file,
            compare_annots_args);

        x_LogTempFile(annot_file);
        compare_annots.Exec(compare_annots_args);
  
        unique_ptr<CObjectIStream> align_istr_ptr(x_InitObjectIStream(alignment_file, true)); // false => not binary
        unique_ptr<CObjectIStream> annot_istr_ptr(x_InitObjectIStream(annot_file, true));
    
        match_tab.GenerateMatchTable(
            local_prot_ids, 
            prot_accessions, 
            nuc_id_replacement_map,
            *align_istr_ptr,
            *annot_istr_ptr);

        if (!keep_temps) {
            x_DeleteTempFiles();
        }
    } 
    catch(...)
    {
        if (!keep_temps) {
            x_DeleteTempFiles();
        }
        throw;
    }
}


void CProteinMatchApp::x_ProcessSeqEntry(CRef<CSeq_entry> nuc_prot_set,
    const CProteinMatchApp::TEntryFilenameMap& filename_map,
    CProteinMatchApp::TEntryOStreamMap& ostream_map,
    map<string, string>& nuc_id_replacement_map,
    map<string, list<string>>& local_prot_ids,
    map<string, list<string>>& prot_accessions,
    CMatchTabulate& match_tab) 
{

    CRef<CSeq_id> local_nuc_acc;
    const bool success = m_pMatchSetup->GetNucSeqId(
        nuc_prot_set->GetSet().GetNucFromNucProtSet(),
        local_nuc_acc);
    string local_nuc_acc_string = ""; 
    if (success) {
        local_nuc_acc_string = local_nuc_acc->GetSeqIdString();

        // Check to see if the nucleotide id has been changed
        CRef<CSeq_id> replaced_nuc_acc; 
        if (m_pMatchSetup->GetReplacedIdFromHist(
                    nuc_prot_set->GetSet().GetNucFromNucProtSet(),
                    replaced_nuc_acc)) {
            const string replaced_nuc_acc_string = replaced_nuc_acc->GetSeqIdString();
            nuc_id_replacement_map[replaced_nuc_acc_string] = local_nuc_acc_string;
        }
    } else {
        // NCBI_THROW ... Couldn't find a local nucleotide id
    }


    CConstRef<CBioseq_set> db_nuc_prot_set = m_pMatchSetup->GetDBNucProtSet(nuc_prot_set->GetSet().GetNucFromNucProtSet());
    if (db_nuc_prot_set.IsNull()) {
        NCBI_THROW(CProteinMatchException, 
        eInternalError,
        "Failed to fetch database entry");
    }

    x_GatherLocalProteinIds(nuc_prot_set->GetSet(), local_prot_ids[local_nuc_acc_string]);

    x_GatherProteinAccessions(*db_nuc_prot_set, prot_accessions[local_nuc_acc_string]);

    x_RelabelNucSeq(nuc_prot_set); // Temporary - need to fix this

    x_WriteToSeqEntryTempFiles(
        nuc_prot_set->GetSet(),
        *db_nuc_prot_set,
        filename_map,
        ostream_map);

    return;
}


TTypeInfo CProteinMatchApp::x_GetRootTypeInfo(CObjectIStream& istr) const
{
    set<TTypeInfo> knownTypes, matchingTypes;
    knownTypes.insert(CSeq_entry::GetTypeInfo());
    knownTypes.insert(CBioseq_set::GetTypeInfo());
    matchingTypes = istr.GuessDataType(knownTypes);

    if (matchingTypes.empty()) {
        NCBI_THROW(CProteinMatchException,
            eInputError,
            "Unrecognized input");
    }

    if (matchingTypes.size() > 1) {
        NCBI_THROW(CProteinMatchException,
            eInputError,
            "Ambiguous input");
    }

    return *matchingTypes.begin();
}


CObjectIStream* CProteinMatchApp::x_InitObjectIStream(
    const CArgs& args) 
{
    ESerialDataFormat serial = eSerial_AsnText;

    const char* infile = args["i"].AsString().c_str();
    CNcbiIstream* pInputStream = new CNcbiIfstream(infile, ios::binary); 
   

    CObjectIStream* pI = CObjectIStream::Open(
            serial, *pInputStream, eTakeOwnership);

    if (!pI) {
        NCBI_THROW(CProteinMatchException, 
                   eInputError, 
                   "Failed to create input stream");
    }
    return pI;
}


bool CProteinMatchApp::x_TryReadSeqEntry(CObjectIStream& istr, CSeq_entry& seq_entry) const
{
    try {
        istr.Read(ObjectInfo(seq_entry));
    }
    catch (CException&) {
        return false;
    }

    return true;
}


bool CProteinMatchApp::x_TryReadBioseqSet(CObjectIStream& istr, CSeq_entry& seq_entry) const
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


void CProteinMatchApp::x_RelabelNucSeq(CRef<CSeq_entry> nuc_prot_set) 
{
    CRef<CSeq_id> local_id;

    if (!m_pMatchSetup->GetNucSeqIdFromCDSs(*nuc_prot_set, local_id)) {
        NCBI_THROW(CProteinMatchException, 
                    eInputError, 
                    "Could not determine a unique nucleotide id");
    }

    if (!m_pMatchSetup->UpdateNucSeqIds(local_id, nuc_prot_set.GetNCObject())) {
        NCBI_THROW(CProteinMatchException, 
                    eExecutionError, 
                    "Unable to assign local nucleotide id");
    }
}


void CProteinMatchApp::x_GatherLocalProteinIds(const CBioseq_set& nuc_prot_set,
    list<string>& id_list) const 
{
    id_list.clear();
    if (!nuc_prot_set.IsSetClass() ||
        nuc_prot_set.GetClass() != CBioseq_set::eClass_nuc_prot) {
        return; // Throw an exception here
    }

    for (CRef<CSeq_entry> seq_entry : nuc_prot_set.GetSeq_set()) {
        const auto& bioseq = seq_entry->GetSeq();
        if (bioseq.IsAa()) {
            const CSeq_id* local_id = bioseq.GetLocalId();
            if (local_id != nullptr) {
                id_list.push_back(local_id->GetSeqIdString());
            }
        }
    }
}


void CProteinMatchApp::x_GatherProteinAccessions(const CBioseq_set& nuc_prot_set,
    list<string>& id_list) const 
{
    id_list.clear();
    if (!nuc_prot_set.IsSetClass() ||
        nuc_prot_set.GetClass() != CBioseq_set::eClass_nuc_prot) {
        return; // Throw an exception here
    }

    const bool with_version = false;

    for (CRef<CSeq_entry> seq_entry : nuc_prot_set.GetSeq_set()) {
        const auto& bioseq = seq_entry->GetSeq();
        if (bioseq.IsAa()) {
            for (CRef<CSeq_id> id : bioseq.GetId()) {
                if (id->IsGenbank() || id->IsOther()) {
                    id_list.push_back(id->GetSeqIdString(with_version));
                    break;
                }
            }
        }
    }
}


void CProteinMatchApp::x_GetSeqEntryFileNames(const string& file_stub, 
    CProteinMatchApp::SSeqEntryFilenames& filenames) const
{
    if (NStr::IsBlank(file_stub)) {
        NCBI_THROW(CProteinMatchException, 
            eInternalError, 
            "Empty file stub");
    }

    filenames.db_nuc_prot_set = file_stub + ".db.asn";
    filenames.db_nuc_seq      = file_stub + ".db_nuc.asn";
    filenames.local_nuc_prot_set = file_stub + ".local.asn";
    filenames.local_nuc_seq      = file_stub + ".local_nuc.asn";   
}


void CProteinMatchApp::x_GetSeqEntryFileNames(const string& file_stub,
    CProteinMatchApp::TEntryFilenameMap&  filename_map) const
{
    filename_map["db_nuc_prot_set"]     = file_stub + ".db.asn";
    filename_map["db_nuc_seq"]          = file_stub + ".db_nuc.asn";
    filename_map["local_nuc_prot_set"]  = file_stub + ".local.asn";
    filename_map["local_nuc_seq"]       = file_stub + ".local_nuc.asn";   
}


void CProteinMatchApp::x_CreateSeqEntryOStreams(const CProteinMatchApp::TEntryFilenameMap& filename_map,
    CProteinMatchApp::TEntryOStreamMap& ostream_map) const
{
    static const bool as_text = false;
    for (const auto& key_val : filename_map) {
        ostream_map[key_val.first].reset(x_InitObjectOStream(key_val.second, as_text));
    }
}


void CProteinMatchApp::x_WriteEntry(const CSeq_entry& seq_entry,
        const string& key,
        const CProteinMatchApp::TEntryFilenameMap& filename_map,
        CProteinMatchApp::TEntryOStreamMap& ostream_map) const
{
    try { 
        *ostream_map[key] << seq_entry;
    } catch (...) {
        NCBI_THROW(CProteinMatchException,
            eOutputError,
            "Failed to write to " + filename_map.at(key));
    }
}


void CProteinMatchApp::x_WriteToSeqEntryTempFiles(
    const CBioseq_set& nuc_prot_set,
    const CBioseq_set& db_nuc_prot_set, 
    const CProteinMatchApp::TEntryFilenameMap& filename_map,
    CProteinMatchApp::TEntryOStreamMap& ostream_map) const 

{
    // Write the genbank nuc-prot-set to a file
    CRef<CSeq_entry> db_nuc_prot_se = Ref(new CSeq_entry());
    db_nuc_prot_se->SetSet().Assign(db_nuc_prot_set);
    x_WriteEntry(*db_nuc_prot_se, 
        "db_nuc_prot_set",
        filename_map,
        ostream_map);

    const CBioseq& db_nuc_seq = db_nuc_prot_set.GetNucFromNucProtSet();
    CRef<CSeq_entry> db_nuc_se = Ref(new CSeq_entry());
    db_nuc_se->SetSeq().Assign(db_nuc_seq);
    x_WriteEntry(*db_nuc_se, 
        "db_nuc_seq",
        filename_map,
        ostream_map);

    CRef<CSeq_entry> nuc_prot_se = Ref(new CSeq_entry());
    nuc_prot_se->SetSet().Assign(nuc_prot_set);
    x_WriteEntry(*nuc_prot_se, 
        "local_nuc_prot_set",
        filename_map,
        ostream_map);

    // Write update nucleotide sequence
    const CBioseq& nuc_seq = nuc_prot_set.GetNucFromNucProtSet();
    CRef<CSeq_entry> nuc_se = Ref(new CSeq_entry());
    nuc_se->SetSeq().Assign(nuc_seq);
    x_WriteEntry(*nuc_se, 
        "local_nuc_seq",
        filename_map,
        ostream_map);
}


CProteinMatchApp::SSeqEntryFilenames 
CProteinMatchApp::x_GenerateSeqEntryTempFiles(
    const CBioseq_set& nuc_prot_set,
    const CBioseq_set& db_nuc_prot_set, 
    const string& out_stub,
    const unsigned int count) 
{
    static const bool as_binary = true;
    static const bool as_text = false;
    const string count_string = NStr::NumericToString(count);


    // Write the genbank nuc-prot-set to a file
    string db_nuc_prot_file = out_stub
        + ".genbank"
        + ".asn";

    CRef<CSeq_entry> db_nuc_prot_se = Ref(new CSeq_entry());
    db_nuc_prot_se->SetSet().Assign(db_nuc_prot_set);

    x_LogTempFile(db_nuc_prot_file);
    x_WriteEntry(*db_nuc_prot_se, db_nuc_prot_file, as_binary);

    // Write the genbank nucleotide sequence to a file
    const string db_nuc_file = out_stub
        + ".genbank_nuc"
        + ".asn";

    const CBioseq& db_nuc_seq = db_nuc_prot_set.GetNucFromNucProtSet();
    CRef<CSeq_entry> db_nuc_se = Ref(new CSeq_entry());
    db_nuc_se->SetSeq().Assign(db_nuc_seq);
    x_LogTempFile(db_nuc_file);
    x_WriteEntry(*db_nuc_se, db_nuc_file, as_text); 

    // Write processed update
    const string local_nuc_prot_file = out_stub
        + ".local"
        + ".asn";

    CRef<CSeq_entry> nuc_prot_se = Ref(new CSeq_entry());
    nuc_prot_se->SetSet().Assign(nuc_prot_set);
    x_LogTempFile(local_nuc_prot_file);


    x_WriteEntry(*nuc_prot_se, local_nuc_prot_file, as_binary);

    // Write update nucleotide sequence
    const string local_nuc_file = out_stub
        + ".local_nuc"
        + ".asn";

    const CBioseq& nuc_seq = nuc_prot_set.GetNucFromNucProtSet();
    CRef<CSeq_entry> nuc_se = Ref(new CSeq_entry());
    nuc_se->SetSeq().Assign(nuc_seq);
    x_LogTempFile(local_nuc_file);
    x_WriteEntry(*nuc_se, local_nuc_file, as_text);

    SSeqEntryFilenames filenames;
    filenames.db_nuc_prot_set = db_nuc_prot_file;
    filenames.db_nuc_seq = db_nuc_file;
    filenames.local_nuc_prot_set = local_nuc_prot_file;
    filenames.local_nuc_seq = local_nuc_file;

    return filenames;
}


void CProteinMatchApp::x_ReadAnnotFile(const string& filename,
    list<CRef<CSeq_annot>>& seq_annots) const
{
    const bool binary = true;
    unique_ptr<CObjectIStream> pObjIstream(x_InitObjectIStream(filename, binary));

    if (!seq_annots.empty()) {
        seq_annots.clear();
    }

    while(!pObjIstream->EndOfData()) {
        CRef<CSeq_annot> pSeqAnnot(new CSeq_annot());
        try {
            pObjIstream->Read(ObjectInfo(*pSeqAnnot));
        }
        catch (CException&) {
            NCBI_THROW(CProteinMatchException, 
                       eInputError, 
                      "Could not read \"" + filename + "\"");
        }
        seq_annots.push_back(pSeqAnnot);
    }
}


void CProteinMatchApp::x_ReadAlignmentFile(const string& filename, 
    CRef<CSeq_align>& pSeqAlign) const
{
    const bool binary = true;
    unique_ptr<CObjectIStream> pObjIstream(x_InitObjectIStream(filename, binary));

    try {
        pObjIstream->Read(ObjectInfo(*pSeqAlign));
    } 
    catch (CException&) {
        NCBI_THROW(CProteinMatchException, 
            eInputError, 
            "Could not read \"" + filename + "\"");
    }
}


CObjectIStream* CProteinMatchApp::x_InitObjectIStream(const string& filename,
    const bool binary) const
{
    CNcbiIstream* pInStream = new CNcbiIfstream(filename.c_str(), ios::in | ios::binary);

    if (pInStream->fail()) {
        NCBI_THROW(CProteinMatchException, 
                   eInputError, 
                   "Could not create input stream for \"" + filename + "\"");
    }

    ESerialDataFormat serial = binary ? eSerial_AsnBinary : eSerial_AsnText;

    CObjectIStream* pObjIStream = CObjectIStream::Open(serial,
                                                       *pInStream,
                                                       eTakeOwnership);

    if (!pObjIStream) {
        NCBI_THROW(CProteinMatchException, 
                   eInputError, 
                   "Unable to open input file \"" + filename + "\"");
    }

    return pObjIStream;
}


CObjectOStream* CProteinMatchApp::x_InitObjectOStream(const string& filename, 
    const bool binary) const
{
    if (filename.empty()) {
        NCBI_THROW(CProteinMatchException, 
                   eOutputError, 
                   "Output file name not specified");
    }

    ESerialDataFormat serial = binary ? eSerial_AsnBinary : eSerial_AsnText;

    CObjectOStream* pOstr = CObjectOStream::Open(filename, serial);

    if (!pOstr) {
        NCBI_THROW(CProteinMatchException, 
                   eOutputError, 
                   "Unable to open output file: " + filename);
    }

    return pOstr;
}


void CProteinMatchApp::x_WriteEntry(const CSeq_entry& seq_entry,
    const string filename, 
    const bool as_binary) {
    try { 
        unique_ptr<CObjectOStream> pOstr(x_InitObjectOStream(filename, as_binary));
        *pOstr << seq_entry;
    } catch (...) {
        NCBI_THROW(CProteinMatchException,
            eOutputError,
            "Failed to write " + filename);
    }
}


void CProteinMatchApp::x_GetBlastArgs(
    const bool binary_output,
    const string& update_file, 
    const string& genbank_file,
    const string& alignment_file,
    string& blast_args) const
{
    if (NStr::IsBlank(update_file) ||
        NStr::IsBlank(genbank_file)) {
        NCBI_THROW(CProteinMatchException,
            eInputError,
            "assm_assm_blastn input file not specified");
    }


    if (NStr::IsBlank(alignment_file)) {
        NCBI_THROW(CProteinMatchException,
            eOutputError,
            "assm_assm_blastn alignment file not specified");
    }

    blast_args =
        " -query " + update_file +
        " -target " + genbank_file +
        " -task megablast" 
        " -word_size 16" 
        " -evalue 0.01" 
        " -gapopen 2" 
        " -gapextend 1" 
        " -best_hit_overhang 0.1" 
        " -best_hit_score_edge 0.1" 
        " -align-output " + alignment_file  +
        " -nogenbank";

    if (binary_output) {
        blast_args += " -ofmt asn-binary"; 
    }
    else 
    {
        blast_args += " -ofmt asn-text";    
    }
}


void CProteinMatchApp::x_GetCompareAnnotsArgs(
        const string& update_file,
        const string& genbank_file,
        const string& alignment_manifest_file,
        const string& annot_file,
        string& compare_annots_args)  const
{
    if (NStr::IsBlank(update_file) ||
        NStr::IsBlank(genbank_file) ||
        NStr::IsBlank(alignment_manifest_file)) {
        NCBI_THROW(CProteinMatchException,
            eInputError,
            "compare_annots input file not specified");
    }

    if (NStr::IsBlank(annot_file)) {
        NCBI_THROW(CProteinMatchException,
            eOutputError, 
            "compare_annots annot file is not specified");
    }

    compare_annots_args = 
        " -q_scope_type annots" 
        " -q_scope_args " + update_file +
        " -s_scope_type annots" 
        " -s_scope_args " + genbank_file +
        " -alns " + alignment_manifest_file +
        " -o_asn " + annot_file +
        " -nogenbank";

}


void CProteinMatchApp::x_LogTempFile(const string& filename)
{   
    m_TempFiles.push_back(filename);
}


void CProteinMatchApp::x_DeleteTempFiles(void) 
{
    string file_list = " ";
    for (string filename : m_TempFiles) {
        auto tempfile = CFile(filename);
        if (tempfile.Exists()) {
            tempfile.Remove();
        }
    }
}


END_NCBI_SCOPE
USING_NCBI_SCOPE;

int main(int argc, const char* argv[]) 
{
    return CProteinMatchApp().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}

