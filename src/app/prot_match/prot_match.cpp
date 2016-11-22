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

#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/util/sequence.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/edit/protein_match/setup_match.hpp>
#include <objtools/edit/protein_match/generate_match_table.hpp>
#include <objtools/edit/protein_match/prot_match_exception.hpp>

#include "run_binary.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

class CProteinMatchApp : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);

private:
    CObjectIStream* x_InitObjectIStream(const CArgs& args);
    CObjectOStream* x_InitObjectOStream(const string& filename, 
        const bool binary) const;
    CObjectIStream* x_InitObjectIStream(const string& filename,
        const bool binary) const;

    void x_ReadUpdateFile(CObjectIStream& istr, CSeq_entry& seq_entry) const;
    bool x_TryReadSeqEntry(CObjectIStream& istr, CSeq_entry& seq_entry) const;
    bool x_TryReadBioseqSet(CObjectIStream& istr, CSeq_entry& seq_entry) const;
    void x_WriteEntry(const CSeq_entry& entry,
        const string filename,
        const bool as_binary);

    void x_GetBlastArgs(const string& update_file, 
        const string& genbank_file,
        const string& alignment_file,
        vector<string>& blast_args) const;

    void x_GetCompareAnnotsArgs(const string& update_file,
        const string& genbank_file,
        const string& alignment_manifest_file,
        const string& annot_file,
        vector<string>& compare_annots_args) const;

    void x_ReadAnnotFile(const string& filename, list<CRef<CSeq_annot>>& seq_annots) const;

    void x_LogTempFile(const string& string);
    void x_DeleteTempFiles(void);

    struct SSeqEntryFilenames {
        string gb_nuc_prot_set;
        string gb_nuc_seq;
        string local_nuc_prot_set;
        string local_nuc_seq;
    };

    SSeqEntryFilenames x_GenerateSeqEntryTempFiles(CSeq_entry_Handle nuc_prot_set,
        CScope& scope,
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
        "Output stub",
        CArgDescriptions::eOutputFile);

    SetupArgDescriptions(arg_desc.release());

    m_pMatchSetup.reset(new CMatchSetup());

    return;
}


int CProteinMatchApp::Run(void)
{
    const bool as_binary = true;
    const bool as_text = false;

    CBinRunner assm_assm_blastn("./assm_assm_blastn");
    CBinRunner compare_annots("./compare_annots");
    const CArgs& args = GetArgs();
    
    // Set up scope 
    CRef<CObjectManager> obj_mgr = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*obj_mgr));

    unique_ptr<CObjectIStream> pInStream(x_InitObjectIStream(args));

    // Could use skip hooks here instead
    // ans2fasta, asnvalidate
    // Preexisting nucleotide entries may not exist.
    CSeq_entry input_entry;
    x_ReadUpdateFile(*pInStream, input_entry); 

    CSeq_entry_Handle input_seh;
    try {
        input_seh = scope->AddTopLevelSeqEntry(input_entry);
    }
    catch (CException&) {
        NCBI_THROW(CProteinMatchException,
            eBadInput,
            "Could not obtain valid seq-entry handle");
    }    

    list<CSeq_entry_Handle> nuc_prot_sets;
    CMatchSetup::GetNucProtSets(input_seh, nuc_prot_sets);

    if (nuc_prot_sets.empty()) { // Should also probably post a warning
        return 0;
    }

    const string out_stub = args["o"].AsString();
     // Handle abuse!! 
    int count=0;
    for (CSeq_entry_Handle nuc_prot_seh : nuc_prot_sets) {

        SSeqEntryFilenames seq_entry_files = 
            x_GenerateSeqEntryTempFiles(nuc_prot_seh,
            *scope, 
            out_stub, 
            count);
        const string count_string = NStr::NumericToString(count);
        
        const string alignment_file = out_stub + ".merged" + count_string + ".asn";
        vector<string> blast_args;
        x_GetBlastArgs(
            seq_entry_files.local_nuc_seq, 
            seq_entry_files.gb_nuc_seq, 
            alignment_file,
            blast_args);

        assm_assm_blastn.Exec(blast_args);
        x_LogTempFile(alignment_file);
       
        // Create alignment manifest tempfile 
        const string manifest_file = out_stub + ".aln" + count_string + ".mft";
        try {
            CNcbiOfstream ostr(manifest_file);
            ostr << alignment_file << endl;
        }
        catch(...) {
            NCBI_THROW(CProteinMatchException,
                eOutputError,
                "Could not write alignment manifest");
        }
        x_LogTempFile(manifest_file);

        const string annot_file = out_stub + ".compare" + count_string + ".asn";

        vector<string> compare_annots_args;
        x_GetCompareAnnotsArgs(
            seq_entry_files.local_nuc_prot_set,
            seq_entry_files.gb_nuc_prot_set,
            manifest_file, 
            annot_file,
            compare_annots_args);

         compare_annots.Exec(compare_annots_args);
         x_LogTempFile(annot_file);


        list<CRef<CSeq_annot>> seq_annots;
        x_ReadAnnotFile(annot_file, seq_annots);

        // Append to match table here
//        x_DeleteTempFiles();
        ++count;
    }
    // Write match table here

    scope->RemoveEntry(input_entry);
    return 0;
}


void CProteinMatchApp::x_ReadUpdateFile(CObjectIStream& istr, CSeq_entry& seq_entry) const
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
    }

    return;
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


CProteinMatchApp::SSeqEntryFilenames 
CProteinMatchApp::x_GenerateSeqEntryTempFiles(CSeq_entry_Handle nuc_prot_seh,
    CScope& scope,
    const string& out_stub,
    const unsigned int count) 
{
    static const bool as_binary = true;
    static const bool as_text = false;

    CSeq_entry_Handle nucleotide_seh = m_pMatchSetup->GetNucleotideSEH(nuc_prot_seh);

    CSeq_entry_Handle gb_seh = m_pMatchSetup->GetGenBankTopLevelEntry(nucleotide_seh);
    if (!gb_seh) {
        NCBI_THROW(CProteinMatchException,
            eBadInput,
            "Failed to fetch GenBank entry");
    }

    const string count_string = NStr::NumericToString(count);

    CConstRef<CBioseq_set> gb_nuc_prot_set = m_pMatchSetup->GetGenBankNucProtSet(nucleotide_seh.GetCompleteSeq_entry()->GetSeq());
    CConstRef<CSeq_entry> gb_se = gb_seh.GetCompleteSeq_entry();

    // Write the genbank nuc-prot-set to a file
    string gb_nuc_prot_file = out_stub
        + ".genbank"
        + count_string
        + ".asn";
    x_WriteEntry(*(gb_nuc_prot_set->GetParentEntry()), gb_nuc_prot_file, as_binary);
    x_LogTempFile(gb_nuc_prot_file);

    // Write the genbank nucleotide sequence to a file
    //const CBioseq& gb_nuc_seq = gb_se->GetSet().GetNucFromNucProtSet();
    const CBioseq& gb_nuc_seq = gb_nuc_prot_set->GetNucFromNucProtSet();
    const string gb_nuc_file = out_stub
        + ".genbank_nuc"
        + count_string
        + ".asn";
    x_WriteEntry(*(gb_nuc_seq.GetParentEntry()), gb_nuc_file, as_text);
    x_LogTempFile(gb_nuc_file);

    CRef<CSeq_id> local_id;

    //if (m_pMatchSetup->GetNucSeqIdFromCDSs(*(nuc_prot_seh.GetCompleteSeq_entry()), scope, local_id)) {
    if (m_pMatchSetup->GetNucSeqIdFromCDSs(nuc_prot_seh, local_id)) {
        m_pMatchSetup->UpdateNucSeqIds(local_id, nucleotide_seh, nuc_prot_seh);
    }

    // Write processed update
    const string local_nuc_prot_file = out_stub
        + ".local"
        + count_string
        + ".asn";
    x_WriteEntry(*(nuc_prot_seh.GetCompleteSeq_entry()), local_nuc_prot_file, as_binary);
    x_LogTempFile(local_nuc_prot_file);


    // Write update nucleotide sequence
    const string local_nuc_file = out_stub
        + ".local_nuc"
        + count_string
        + ".asn";
    x_WriteEntry(*(nucleotide_seh.GetCompleteSeq_entry()), local_nuc_file, as_text);
    x_LogTempFile(local_nuc_file);

    SSeqEntryFilenames filenames;
    filenames.gb_nuc_prot_set = gb_nuc_prot_file;
    filenames.gb_nuc_seq = gb_nuc_file;
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
    } catch (CException& e) {
        NCBI_THROW(CProteinMatchException,
            eOutputError,
            "Failed to write " + filename);
    }
}


void CProteinMatchApp::x_GetBlastArgs(
    const string& update_file, 
    const string& genbank_file,
    const string& alignment_file,
    vector<string>& blast_args) const
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

    vector<string> args {
        "-query", update_file,
        "-target", genbank_file,
        "-task", "megablast",
        "-word_size", "16",
        "-evalue", "0.01",
        "-gapopen", "2",
        "-gapextend", "1",
        "-best_hit_overhang", "0.1",
        "-best_hit_score_edge", "0.1",
        "-align-output", alignment_file,
        "-ofmt", "asn-binary",
        "-nogenbank"
    };
    
    blast_args = args;
}


void CProteinMatchApp::x_GetCompareAnnotsArgs(
        const string& update_file,
        const string& genbank_file,
        const string& alignment_manifest_file,
        const string& annot_file,
        vector<string>& compare_annots_args)  const
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

    vector<string> args {
        "-q_scope_type", "annots",
        "-q_scope_args", update_file,
        "-s_scope_type", "annots",
        "-s_scope_args", genbank_file,
        "-alns", alignment_manifest_file,
        "-o_asn", annot_file,
        "-nogenbank" };
    
    compare_annots_args = args;
}


void CProteinMatchApp::x_LogTempFile(const string& filename)
{   
    m_TempFiles.push_back(filename);
}


void CProteinMatchApp::x_DeleteTempFiles(void) 
{
    // Delete temporary files here
}


END_NCBI_SCOPE
USING_NCBI_SCOPE;

int main(int argc, const char* argv[]) 
{
    return CProteinMatchApp().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}

