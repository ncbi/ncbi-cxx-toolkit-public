#include <ncbi_pch.hpp>
#include <objmgr/util/sequence.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objtools/readers/hgvs/id_resolver.hpp>
#include <objtools/readers/hgvs/irep_to_seqfeat_errors.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CIdResolver::CIdResolver(CScope& scope)  : m_Scope(scope),
    m_LRGregex(new CRegexp("^(LRG_\\d+)([pt]\\d+)?$")),
    m_CCDSregex(new CRegexp("^CCDS\\d+\\.\\d+$")),
    m_E2Client(new CEntrez2Client())
{
}

CSeq_id_Handle CIdResolver::GetAccessionVersion(const string& identifier) const
{

    if (identifier.empty()) {
        NCBI_THROW(CVariationValidateException,
                   eIDResolveError,
                   "Empty sequence identifier string");
    }

    CSeq_id_Handle idh;

    if (x_TryProcessLRG(identifier, idh)) {
        return idh;
    }
/*
    // Need to add code to handle CCDS identifier
    // Waiting for the appropriate service to be put in place
*/

    try {
        auto temp_idh = CSeq_id_Handle::GetHandle(identifier);
        idh = sequence::GetId(temp_idh, 
                              m_Scope, 
                              sequence::eGetId_ForceAcc | sequence::eGetId_ThrowOnError);
    }
    catch (...) {}
   

    if (!idh) { // Failed to resolve id
        NCBI_THROW(CVariationValidateException,
                   eIDResolveError,
                   "Could not resolve sequence identifier: " + identifier);
    }

    return idh;
}


bool CIdResolver::x_TryProcessGenomicLRG(const string& identifier, CSeq_id_Handle& idh) const
{ 
    if (identifier.empty()) {
        // Throw an exception
    }

    if (!x_LooksLikeLRG(identifier)) {
        return false;
    }

    CRef<CSeq_id> lrg_seqid;

    try {
        lrg_seqid = Ref(new CSeq_id("gnl|LRG|" + identifier));
    }
    catch(...) {
        // Throw another exception here 
        // Could not create a valid SeqId from the identifier string
        return false;
    }

    try {
        idh = sequence::GetId(*lrg_seqid, 
                              m_Scope, 
                              sequence::eGetId_ForceAcc | sequence::eGetId_ThrowOnError);
    } 
    catch(...) { 
        // Throw another exception here
        // Could not return a TextSeq-id
        return false;
    }

    return true;
}  



bool CIdResolver::x_TryProcessLRG(const string& identifier, CSeq_id_Handle& idh) const
{
    m_LRGregex->IsMatch(identifier);

    string genome_id = m_LRGregex->GetSub(identifier,1);
    string lrg_product_id = m_LRGregex->GetSub(identifier,2);

    CSeq_id_Handle genome_idh;

    // The following routine calls CRegexp::IsMatch, which 
    // resets all results from previous GetMatch() calls. 
    // This is why we fetch the lrg_product_id above.
    if (!x_TryProcessGenomicLRG(genome_id, genome_idh)) {
        return false;
    }

    if (NStr::IsBlank(lrg_product_id)) { // Nothing more to do here.
      idh = genome_idh;                  // No need to search for products.
      return true;
    }

    
    SAnnotSelector selector;
    selector.SetResolveTSE();
    selector.IncludeFeatType(CSeqFeatData::e_Gene);
    selector.IncludeFeatType(CSeqFeatData::e_Rna);
    selector.IncludeFeatType(CSeqFeatData::e_Cdregion);

    auto bsh = m_Scope.GetBioseqHandle(genome_idh);

    if (!bsh) { // Throw an exception
        NCBI_THROW(CVariationValidateException,
                   eIDResolveError,
                   "Could not find Bioseq for identifier : " + genome_id);
    }


    for (CFeat_CI ci(bsh,selector); ci; ++ci) { 
        const auto& mapped_feat = *ci;
        if (!mapped_feat.IsSetDbxref()) {
            continue;
        }    
        ITERATE(CSeq_feat::TDbxref, it, mapped_feat.GetDbxref()) {
            const auto& dbtag = **it;
            if (NStr::Equal(dbtag.GetDb(), "LRG") &&
                dbtag.GetTag().IsStr() &&
                NStr::Equal(dbtag.GetTag().GetStr(), lrg_product_id) &&
                mapped_feat.IsSetProduct() && 
                mapped_feat.GetProduct().GetId()) 
            {
                try {
                    idh = sequence::GetId(*mapped_feat.GetProduct().GetId(),
                                           m_Scope,
                                           sequence::eGetId_ForceAcc);
                   return true;          
                } 
                catch (...) 
                {
                    break;
                }
            }
        }
    }

    NCBI_THROW(CVariationValidateException,
               eIDResolveError,
               "Could not find SeqId for : " + identifier);

    return false;
}


/*
CSeq_id_Handle CIdResolver::x_ProcessCCDS(const string& identifier)
{
    //string query_string = "srcdb_refseq[prop] AND biomol_mRNA[prop] AND dbxref_ccds[prop] AND \"CCDS:" + identifier + "\"";
    string query_string = "srcdb_refseq[prop] AND dbxref_ccds[prop] AND \"CCDS:" + identifier + "\"";
    //string query_string = "srcdb_refseq[prop] AND \"CCDS:" + identifier + "\"";

    query_string = "srcdb_refseq[prop] AND dbxref_ccds[prop] AND CCDS4702";
   // query_string = "dbxref_ccds[prop] AND CCDS4702";

    vector<TGi> gi;
    const size_t start_offs = 0;
    const size_t count = 5;
    m_E2Client->Query(query_string, "protein", gi, start_offs, count);
  

    if (gi.size() != 1) {
        // Throw an exception
    } 


    for(int i=0; i<gi.size(); ++i) {   
        auto gih = CSeq_id_Handle::GetHandle(gi[i]);

        auto idh = sequence::GetId(gih,
                               m_Scope,
                               sequence::eGetId_ForceAcc);

    }
    CSeq_id_Handle idh;
    return idh;
}
*/


bool CIdResolver::x_LooksLikeLRG(const string& identifier) const
{
    return m_LRGregex->IsMatch(identifier);
}

bool CIdResolver::x_LooksLikeCCDS(const string& identifier) const
{
    return  m_CCDSregex->IsMatch(identifier);
}


END_SCOPE(objects)
END_NCBI_SCOPE

