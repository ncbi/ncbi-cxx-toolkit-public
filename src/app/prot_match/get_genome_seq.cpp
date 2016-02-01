#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/object_manager.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include "prot_match_exception.hpp"


BEGIN_NCBI_SCOPE
/*

class CProteinMatchException : public CException {
        
public:
    enum EErrCode {
        eInputError,
        eOutputError, 
        eBadInput,
        eNoGenomeSeq
    };

    virtual const char* GetErrCodeString() const 
    {
        switch (GetErrCode()) {
            case eInputError:
                return "eInputError";
            case eOutputError:
                return "eOutputError";
            case eBadInput:
                return "eBadInput";
            case eNoGenomeSeq:
                return "eNoGenomeSeq";
            default:
                return CException::GetErrCodeString();
        }
    };

    NCBI_EXCEPTION_DEFAULT(CProteinMatchException, CException);
};
*/

USING_SCOPE(objects);


class CGenomeSeqApp : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);

private:
    CRef<CSeq_entry> x_GetGenomeSeq(CSeq_entry_Handle& seh);

    CObjectIStream* x_InitInputStream(const CArgs& args);

};


void CGenomeSeqApp::Init(void) 
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddKey("i",
            "InputFile",
            "Path to ASN.1 input file",
            CArgDescriptions::eInputFile);

    arg_desc->AddKey("o", "OutputFile",
            "Path to ASN.1 output file",
            CArgDescriptions::eOutputFile);

    SetupArgDescriptions(arg_desc.release());

    return;
}


int CGenomeSeqApp::Run(void) 
{
    const CArgs& args = GetArgs();

    // Setup scope
    CRef<CObjectManager> obj_mgr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*obj_mgr);
    CRef<CScope> scope(new CScope(*obj_mgr));
    scope->AddDefaults();

    // Set up object input stream
    auto_ptr<CObjectIStream> istr;
    istr.reset(x_InitInputStream(args));

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

    CNcbiOstream& ostr = args["o"].AsOutputFile();

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
                   "Could not obtain valid seq-entry handle");
    }

    CRef<CSeq_entry> genome_seq = x_GetGenomeSeq(seh);

    ostr << MSerial_Format_AsnText() << *genome_seq;

    scope->RemoveEntry(seq_entry);

    return 0;
}


CObjectIStream* CGenomeSeqApp::x_InitInputStream(
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


CRef<CSeq_entry> CGenomeSeqApp::x_GetGenomeSeq(CSeq_entry_Handle& seh)
{ // Refactor this to avoid repetition below
    CRef<CSeq_entry> genome_seq_entry(new CSeq_entry());

    if (seh.IsSeq()) {
        CBioseq_Handle bsh = seh.GetSeq();
        const CMolInfo* molinfo = sequence::GetMolInfo(bsh);

        if (molinfo->GetBiomol() != CMolInfo::eBiomol_peptide) {
            CRef<CBioseq> genome_seq(new CBioseq());
            genome_seq->Assign(*bsh.GetBioseqCore());
            genome_seq_entry->SetSeq(*genome_seq);
            return genome_seq_entry;
        }
    }

    for (CSeq_entry_CI it(seh, CSeq_entry_CI::fRecursive); it; ++it) {

        if (it->IsSeq()) {

            CBioseq_Handle bsh = it->GetSeq();
            const CMolInfo* molinfo = sequence::GetMolInfo(bsh);

            if (molinfo->GetBiomol() != CMolInfo::eBiomol_peptide) {
                CRef<CBioseq> genome_seq(new CBioseq());
                genome_seq->Assign(*bsh.GetBioseqCore());
                genome_seq_entry->SetSeq(*genome_seq);
                break;
            } 
        } 
    }
    return genome_seq_entry;
}

END_NCBI_SCOPE

USING_NCBI_SCOPE;


int main(int argc, const char* argv[])
{
    return CGenomeSeqApp().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}
