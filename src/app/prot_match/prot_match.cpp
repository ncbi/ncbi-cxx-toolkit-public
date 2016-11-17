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
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/edit/protein_match/setup_match.hpp>
#include <objtools/edit/protein_match/generate_match_table.hpp>
#include <objtools/edit/protein_match/prot_match_exception.hpp>

/*
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/seq_feat_handle.hpp>
#include <objects/general/Object_id.hpp>
*/
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

class CProteinMatchApp : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);

private:
    CObjectIStream* x_InitInputStream(const CArgs& args);
    CObjectOStream* x_InitOutputStream(const string& filename, 
    const bool binary) const;
    void x_ReadUpdateFile(CObjectIStream& istr, CSeq_entry& seq_entry) const;
    bool x_TryReadSeqEntry(CObjectIStream& istr, CSeq_entry& seq_entry) const;
    bool x_TryReadBioseqSet(CObjectIStream& istr, CSeq_entry& seq_entry) const;
    void x_WriteEntry(CSeq_entry_Handle seh, 
        const string filename, 
        const bool as_binary);
    void x_WriteEntry(const CSeq_entry& entry,
        const string filename,
        const bool as_binary);
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

    return;
}


int CProteinMatchApp::Run(void)
{
    const bool as_binary = true;
    const bool as_text = false;

    const CArgs& args = GetArgs();
    
    // Set up scope 
    CRef<CObjectManager> obj_mgr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*obj_mgr);
    CRef<CScope> scope(new CScope(*obj_mgr));
    scope->AddDefaults();

    unique_ptr<CObjectIStream> pInStream(x_InitInputStream(args));

    // Could use skip hooks here instead
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

    const bool multiple_sets = (nuc_prot_sets.size() > 1);

    CMatchSetup match_setup;
    int count=0;
    for (CSeq_entry_Handle nuc_prot_seh : nuc_prot_sets) {
        CSeq_entry_Handle nucleotide_seh = match_setup.GetNucleotideSEH(nuc_prot_seh);

        CSeq_entry_Handle gb_seh = match_setup.GetGenBankTopLevelEntry(nucleotide_seh);
        if (!gb_seh) {
            NCBI_THROW(CProteinMatchException,
                eBadInput,
                "Failed to fetch GenBank entry");
        }

        // Write the genbank nuc-prot-set to a file
        string filename = args["o"].AsString() + ".genbank";
        if (multiple_sets) {
            filename += NStr::NumericToString(count);
        }
        filename += ".asn";
        x_WriteEntry(gb_seh, filename, as_binary);

        // Write the genbank nucleotide sequence to a file
        CSeq_entry_Handle gb_nuc_seh = match_setup.GetNucleotideSEH(gb_seh);
        filename = args["o"].AsString() + ".genbank_nuc";
        if (multiple_sets) {
            filename += NStr::NumericToString(count);
        }
        filename += ".asn";
        x_WriteEntry(gb_nuc_seh, filename, as_text);

        CRef<CSeq_id> local_id;
        if (match_setup.GetNucSeqIdFromCDSs(nuc_prot_seh, local_id)) {
            match_setup.UpdateNucSeqIds(local_id, nucleotide_seh, nuc_prot_seh);
        }

        // Write processed update
        filename = args["o"].AsString() + ".local";
        if (multiple_sets) {
            filename += NStr::NumericToString(count);
        }
        filename += ".asn";
        x_WriteEntry(nuc_prot_seh, filename, as_binary);


        // Write update nucleotide sequence
        filename = args["o"].AsString() + ".local_nuc";
        if (multiple_sets) {
            filename += NStr::NumericToString(count);
        }
        filename += ".asn";
        x_WriteEntry(nucleotide_seh, filename, as_text);

        ++count;
    }

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


CObjectIStream* CProteinMatchApp::x_InitInputStream(
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


CObjectOStream* CProteinMatchApp::x_InitOutputStream(const string& filename, 
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


void CProteinMatchApp::x_WriteEntry(CSeq_entry_Handle seh, 
        const string filename, 
        const bool as_binary) {
    x_WriteEntry(*(seh.GetCompleteSeq_entry()),
            filename,
            as_binary);
}


void CProteinMatchApp::x_WriteEntry(const CSeq_entry& seq_entry,
        const string filename, 
        const bool as_binary) {
    try { 
        unique_ptr<CObjectOStream> pOstr(x_InitOutputStream(filename, as_binary));
        *pOstr << seq_entry;
    } catch (CException& e) {
        NCBI_THROW(CProteinMatchException,
            eOutputError,
            "Failed to write " + filename);
    }
}

END_NCBI_SCOPE
USING_NCBI_SCOPE;

int main(int argc, const char* argv[]) 
{
    return CProteinMatchApp().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}

