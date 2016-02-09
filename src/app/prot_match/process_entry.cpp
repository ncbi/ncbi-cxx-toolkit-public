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
#include <objmgr/seq_annot_ci.hpp>
#include <objmgr/seq_feat_handle.hpp>
#include <objects/general/Object_id.hpp>

#include "prot_match_exception.hpp"


BEGIN_NCBI_SCOPE

USING_SCOPE(objects);


class CProcessUpdateApp : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);

private:
    CSeq_entry_Handle x_GetGenomeSEH(CSeq_entry_Handle& seh);
    bool x_GetProductLocationId(CSeq_entry_Handle& seh, CRef<CSeq_id>& prod_id);
    bool x_ReplaceSequenceId(CRef<CSeq_id>& new_id, CSeq_entry_Handle& seh);

    CObjectIStream* x_InitInputStream(const CArgs& args);

};


void CProcessUpdateApp::Init(void) 
{
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    arg_desc->AddKey("i",
            "InputFile",
            "Seq-entry input file",
            CArgDescriptions::eInputFile);

    arg_desc->AddKey("o", "OutputFile",
            "Processed seq-entry output file",
            CArgDescriptions::eOutputFile);

    arg_desc->AddOptionalKey("og", "GenomicOutputFile",
                             "Genomic seq-entry output file",
                             CArgDescriptions::eOutputFile);

    SetupArgDescriptions(arg_desc.release());

    return;
}


int CProcessUpdateApp::Run(void) 
{
    const CArgs& args = GetArgs();

    // Setup scope
    CRef<CObjectManager> obj_mgr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*obj_mgr);
    CRef<CScope> scope(new CScope(*obj_mgr));
    scope->AddDefaults();

    // Set up object input stream
    auto_ptr<CObjectIStream> pInStream(x_InitInputStream(args));

    // Attempt to read Seq-entry from input file 
    CSeq_entry seq_entry;
    try {
        pInStream->Read(ObjectInfo(seq_entry));
    }
    catch (CException&) {
        NCBI_THROW(CProteinMatchException, 
                   eInputError, 
                   "Failed to read Seq-entry");
    }

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

    CSeq_entry_Handle genome_seh = x_GetGenomeSEH(seh);
    
    CRef<CSeq_id> prod_id;
    if (x_GetProductLocationId(genome_seh, prod_id)) {
        x_ReplaceSequenceId(prod_id, genome_seh);
    }

    try {
        args["o"].AsOutputFile() << MSerial_Format_AsnText() << *seh.GetCompleteSeq_entry();
    } catch(CException& e) {
        NCBI_THROW(CProteinMatchException,
                   eOutputError,
                   "Failed to write processed Seq-entry");
    }

    if (args["og"]) {
        try {
            args["og"].AsOutputFile() << MSerial_Format_AsnText() << *genome_seh.GetCompleteSeq_entry();
        } catch(CException& e) {
            NCBI_THROW(CProteinMatchException,
                       eOutputError,
                       "Failed to write genomic Seq-entry");
        }
    }

    scope->RemoveEntry(seq_entry);

    return 0;
}


CObjectIStream* CProcessUpdateApp::x_InitInputStream(
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



CSeq_entry_Handle CProcessUpdateApp::x_GetGenomeSEH(CSeq_entry_Handle& seh)
{
    if (seh.IsSeq()) {
        const CMolInfo* molinfo = sequence::GetMolInfo(seh.GetSeq());
        if (molinfo->GetBiomol() != CMolInfo::eBiomol_peptide) {
            return seh;
        }
    }   

    for (CSeq_entry_CI it(seh, CSeq_entry_CI::fRecursive); it; ++it) {
        if (it->IsSeq()) {
            const CMolInfo* molinfo = sequence::GetMolInfo(it->GetSeq());
            if (molinfo->GetBiomol() != CMolInfo::eBiomol_peptide) {
                return *it;
            }
        }
    }

    return CSeq_entry_Handle();
}



struct SEquivalentTo
{
   CRef<CSeq_id> sid;

   SEquivalentTo(CRef<CSeq_id>& id) : sid(Ref(new CSeq_id()))
   {
       sid->Assign(*id);
   }

   bool operator()(const CRef<CSeq_id>& id) const 
   {
       return (id->Compare(*sid) == CSeq_id::e_YES);
   }
};



bool CProcessUpdateApp::x_GetProductLocationId(CSeq_entry_Handle& seh, CRef<CSeq_id>& id)
{

    set<CRef<CSeq_id> > ids;
    for (CSeq_annot_CI ai(seh); ai; ++ai) {
        const CSeq_annot_Handle& sah = *ai;

        if (!sah.IsFtable()) {
            continue;
        }

        for (CSeq_annot_ftable_CI fi(sah); fi; ++fi) {
            const CSeq_feat_Handle& sfh = *fi;
            if (!sfh.IsSetProduct()) { 
                continue;
            }

            CRef<CSeq_id> product_id = Ref(new CSeq_id());
            product_id->Assign(*(sfh.GetLocation().GetId()));

            if (!product_id) {
                continue;
            }

            if (ids.empty() || 
                find_if(ids.begin(), ids.end(), SEquivalentTo(product_id)) == ids.end()) {
                ids.insert(product_id);
            }
        }
    }


    if (ids.size() > 1) {
        NCBI_THROW(CProteinMatchException,
                   eBadInput,
                   "Nucleotide seq-entry has multiple product identifiers");
    }

    if (ids.empty()) {
        return false;
    }

    CRef<CSeq_id> sa_id = *(ids.begin());

    const bool with_version = true;
    string local_id_string = sa_id->GetSeqIdString(with_version);


    if (id.IsNull()) {
        id = Ref(new CSeq_id());
    }
    id->SetLocal().SetStr(local_id_string);

    return true;
}


bool CProcessUpdateApp::x_ReplaceSequenceId(CRef<CSeq_id>& new_id, CSeq_entry_Handle& seh) 
{
    if (!seh.IsSeq()) {
        return false;
    }

    CBioseq_EditHandle bseh = seh.GetSeq().GetEditHandle();

    bseh.ResetId(); // Remove the old sequence identifiers
    CSeq_id_Handle new_idh = CSeq_id_Handle::GetHandle(*new_id);
    bseh.AddId(new_idh); // Add the new sequence id

    for (CSeq_annot_CI ai(seh); ai; ++ai) { // Add new id to product annotations
        const CSeq_annot_Handle& sah = *ai;

        for (CSeq_annot_ftable_CI fi(sah); fi; ++fi) {
            const CSeq_feat_Handle& sfh = *fi;
            if (!sfh.IsSetProduct()) { 
                continue;
            }

            CRef<CSeq_feat> new_sf = Ref(new CSeq_feat());
            new_sf->Assign(*sfh.GetSeq_feat());
            new_sf->SetLocation().SetId(*new_id);

            CSeq_feat_EditHandle sfeh(sfh);
            sfeh.Replace(*new_sf);
        }
    }

    return true;
}


END_NCBI_SCOPE

USING_NCBI_SCOPE;


int main(int argc, const char* argv[])
{
    return CProcessUpdateApp().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}
