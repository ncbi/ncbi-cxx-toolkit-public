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

#include <objmgr/scope.hpp>
#include <objmgr/object_manager.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/edit/protein_match/setup_match.hpp>
#include <objtools/edit/protein_match/generate_match_table.hpp>
#include <objtools/edit/protein_match/prot_match_exception.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

class CProteinMatchApp : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);

private:
    CObjectIStream* x_InitInputStream(const CArgs& args);
    void x_ReadUpdateFile(CObjectIStream& istr, CSeq_entry& seq_entry) const;
    bool x_TryReadSeqEntry(CObjectIStream& istr, CSeq_entry& seq_entry) const;
    bool x_TryReadBioseqSet(CObjectIStream& istr, CSeq_entry& seq_entry) const;
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

END_NCBI_SCOPE
USING_NCBI_SCOPE;

int main(int argc, const char* argv[]) 
{
    return CProteinMatchApp().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}

