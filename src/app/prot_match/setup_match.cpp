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
#include <objmgr/util/sequence.hpp>

#include "prot_match_exception.hpp"


BEGIN_NCBI_SCOPE

USING_SCOPE(objects);


class CSetupProtMatchApp : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);

private:
    CSeq_entry_Handle x_GetGenomeSEH(CSeq_entry_Handle seh);
    bool x_GetSequenceIdFromCDSs(CSeq_entry_Handle& seh, CRef<CSeq_id>& prod_id);
    bool x_UpdateSequenceIds(CRef<CSeq_id>& new_id, CSeq_entry_Handle& seh);
    CObjectOStream* x_InitOutputStream(const string& filename, const bool binary) const;
    bool x_TryReadSeqEntry(CObjectIStream& istr, CSeq_entry& seq_entry) const;
    bool x_TryReadBioseqSet(CObjectIStream& istr, CSeq_entry& seq_entry) const;
    CObjectIStream* x_InitInputStream(const CArgs& args);
};


CObjectOStream* CSetupProtMatchApp::x_InitOutputStream(const string& filename, 
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


void CSetupProtMatchApp::Init(void) 
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions());

    arg_desc->AddKey("i",
            "InputFile",
            "Seq-entry input file",
            CArgDescriptions::eInputFile);

    arg_desc->AddKey("o", 
                     "OutputFile",
                     "Output stub",
                     CArgDescriptions::eOutputFile);

    SetupArgDescriptions(arg_desc.release());

    return;
}



bool CSetupProtMatchApp::x_TryReadSeqEntry(CObjectIStream& istr, CSeq_entry& seq_entry) const
{
    try {
        istr.Read(ObjectInfo(seq_entry));
    }
    catch (CException&) {
        return false;
    }

    return true;
}



bool CSetupProtMatchApp::x_TryReadBioseqSet(CObjectIStream& istr, CSeq_entry& seq_entry) const
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



int CSetupProtMatchApp::Run(void) 
{
    const CArgs& args = GetArgs();

    // Set up scope
    CRef<CObjectManager> obj_mgr = CObjectManager::GetInstance();
    CGBDataLoader::RegisterInObjectManager(*obj_mgr);
    CRef<CScope> scope(new CScope(*obj_mgr));
    scope->AddDefaults();

    CRef<CScope> gb_scope(new CScope(*obj_mgr));
    gb_scope->AddDataLoader("GBLOADER");


    // Set up object input stream
    unique_ptr<CObjectIStream> pInStream(x_InitInputStream(args));
    // Attempt to read Seq-entry from input file 
    CSeq_entry seq_entry;


    set<TTypeInfo> knownTypes, matchingTypes;
    knownTypes.insert(CSeq_entry::GetTypeInfo());
    knownTypes.insert(CBioseq_set::GetTypeInfo());
    matchingTypes = pInStream->GuessDataType(knownTypes);


    if (matchingTypes.empty()) {
        NCBI_THROW(CProteinMatchException, 
                   eInputError, 
                   "Unrecognized input");
    }

    if (matchingTypes.size() > 1){
        NCBI_THROW(CProteinMatchException, 
                   eInputError, 
                   "Ambiguous input");
    }

    const TTypeInfo typeInfo = *matchingTypes.begin();

    if (typeInfo == CSeq_entry::GetTypeInfo()) {
        if (!x_TryReadSeqEntry(*pInStream, seq_entry)) {
            NCBI_THROW(CProteinMatchException, 
                       eInputError, 
                       "Failed to read Seq-entry");

        }
    }
    else 
    if (typeInfo == CBioseq_set::GetTypeInfo()) {
        if (!x_TryReadBioseqSet(*pInStream, seq_entry)) {
            NCBI_THROW(CProteinMatchException, 
                       eInputError, 
                       "Failed to read Bioseq-set");
        }
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

    const bool AsBinary = true;
    const bool AsText = false;
    // Seq-entry Handle for the nucleotide sequence
    CSeq_entry_Handle genome_seh = x_GetGenomeSEH(seh);

    CConstRef<CSeq_entry> gb_tse;
    CBioseq_Handle gb_bsh; 
    for (auto pNucId : genome_seh.GetSeq().GetCompleteBioseq()->GetId()) {
        if (pNucId->IsGenbank()) { // May need to use a different scope
            gb_bsh = gb_scope->GetBioseqHandle(*pNucId); // Do need to use a different scope. Want to fetch from genbank, not local.
            auto gbtse_handle = gb_bsh.GetTSE_Handle(); // Need to put a check here
            gb_tse = gbtse_handle.GetCompleteTSE();
            break;
        }
    }

    try {
        string filename = args["o"].AsString();
        filename += ".genbank.asn";
        unique_ptr<CObjectOStream> pOstr(x_InitOutputStream(filename, AsBinary));
        *pOstr << *gb_tse;
      //  args["ogb"].AsOutputFile() << MSerial_Format_AsnBinary() << *gb_tse;
    } catch (CException& e) {
        NCBI_THROW(CProteinMatchException, 
                   eOutputError, 
                   "Failed to write Genbank Seq-entry");
    }


    if (args["o"]) {
        CSeq_entry_Handle gb_nuc_seh = x_GetGenomeSEH(gb_bsh.GetSeq_entry_Handle());
        try {

            string filename = args["o"].AsString();
            filename += ".genbank_nuc.asn";
            unique_ptr<CObjectOStream> pOstr(x_InitOutputStream(filename, AsText));
            *pOstr << *gb_nuc_seh.GetCompleteSeq_entry();
          //  args["o"].AsOutputFile() << MSerial_Format_AsnText() << *gb_nuc_seh.GetCompleteSeq_entry();

        } catch(CException& e) {
            NCBI_THROW(CProteinMatchException,
                       eOutputError,
                       "Failed to write genomic Seq-entry");
        }
    }
   
    CRef<CSeq_id> local_id;
    if (x_GetSequenceIdFromCDSs(seh, local_id)) {
        x_UpdateSequenceIds(local_id, genome_seh); // Remove any preexisting ids and replace by local_id
    }

    try {
        string filename = args["o"].AsString();
        filename += ".local.asn";
        unique_ptr<CObjectOStream> pOstr(x_InitOutputStream(filename, AsBinary));
        *pOstr << *seh.GetCompleteSeq_entry();
    } catch(CException& e) {
        NCBI_THROW(CProteinMatchException,
                   eOutputError,
                   "Failed to write processed Seq-entry");
    }

    if (args["o"]) {
        try {
            string filename = args["o"].AsString();
            filename += ".local_nuc.asn";
            unique_ptr<CObjectOStream> pOstr(x_InitOutputStream(filename, AsText));
            *pOstr << *genome_seh.GetCompleteSeq_entry();
        } catch(CException& e) {
            NCBI_THROW(CProteinMatchException,
                       eOutputError,
                       "Failed to write genomic Seq-entry");
        }
    }

    scope->RemoveEntry(seq_entry);

    return 0;
}


CObjectIStream* CSetupProtMatchApp::x_InitInputStream(
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



CSeq_entry_Handle CSetupProtMatchApp::x_GetGenomeSEH(CSeq_entry_Handle seh)
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


// Search for a unique id in the Seq-locs appearing in CDS features in the sequence annotations. If found,
// convert to a local ID and return true. Else, return false.
bool CSetupProtMatchApp::x_GetSequenceIdFromCDSs(CSeq_entry_Handle& seh, CRef<CSeq_id>& id)
{
    // Set containing distinct product ids
    set<CRef<CSeq_id>> ids;

    // Loop over annotations, looking for product
    for (CSeq_annot_CI ai(seh); ai; ++ai) {
        const CSeq_annot_Handle& sah = *ai;

        if (!sah.IsFtable()) {
            continue;
        }

        for (CSeq_annot_ftable_CI fi(sah); fi; ++fi) {
            const CSeq_feat_Handle& sfh = *fi;

            if (!sfh.IsSetProduct()) { // Skip if product not specified
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
                   "Multiple CDS locations found");
    }

    if (ids.empty()) {
        return false;
    }

    CRef<CSeq_id> sannot_id = *(ids.begin());

    const bool with_version = true;
    string local_id_string = sannot_id->GetSeqIdString(with_version);


    if (id.IsNull()) {
        id = Ref(new CSeq_id());
    }
    id->SetLocal().SetStr(local_id_string);

    return true;
}


// Strip old identifiers on the sequence and annptations and replace with new id
bool CSetupProtMatchApp::x_UpdateSequenceIds(CRef<CSeq_id>& new_id, CSeq_entry_Handle& seh) 
{
    if (!seh.IsSeq()) {
        return false;
    }

    CBioseq_EditHandle bseh = seh.GetSeq().GetEditHandle();

    bseh.ResetId(); // Remove the old sequence identifiers
    CSeq_id_Handle new_idh = CSeq_id_Handle::GetHandle(*new_id);
    bseh.AddId(new_idh); // Add the new sequence id

    for (CSeq_annot_CI ai(seh); ai; ++ai) { // Add new id to sequence annotations
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
    return CSetupProtMatchApp().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}
