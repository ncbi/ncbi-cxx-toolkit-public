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
 * File Description:
 *   Sample library
 *
 */

#include <ncbi_pch.hpp>

#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_loc_mapper.hpp>
#include <objmgr/util/seq_loc_util.hpp>
#include <objmgr/util/sequence.hpp>
#include <misc/hgvs/hgvs_parser2.hpp>
#include <misc/hgvs/seq_id_resolver.hpp>

#include <util/ncbi_cache.hpp>
#include <util/xregexp/regexp.hpp>
#include <objects/entrez2/entrez2_client.hpp>

#include <objects/genomecoll/GC_Sequence.hpp>
#include <objects/genomecoll/GC_TypedSeqId.hpp>


BEGIN_NCBI_SCOPE

bool CSeq_id_Resolver::CanCreate(const string& s)
{
    if(m_regexp) {
        return m_regexp->IsMatch(s);
    } else {
        try {
            // Note: we will not resolve bare numeric ids
            // to gis unless they are prefixed "gi|", as otherwise
            // it is likely that the input was intended to refer to
            // a prefix-less chromosome name or a local id, so we
            // prefer to fail rather than process a wrong answer. VAR-575
            CSeq_id id(s);
            return !id.IsGi() || NStr::StartsWith(s, "gi|");
        } catch(CException&) {
            return false;
        }
    }
}

CSeq_id_Handle CSeq_id_Resolver::x_Create(const string& s)
{    
    // resolve to accver whenether possible (e.g. for gis or versionless accs)
    // with fall-back on the original (eg. for lcl)
    CSeq_id_Handle orig_idh = CSeq_id_Handle::GetHandle(s);
    CSeq_id_Handle acc_idh  = sequence::GetId(orig_idh, *m_scope, sequence::eGetId_ForceAcc);
    return acc_idh ? acc_idh : orig_idh;
}

CSeq_id_Resolver::~CSeq_id_Resolver()
{
    if (m_regexp) {
        delete m_regexp;
    }
}

CSeq_id_Resolver__LRG::CSeq_id_Resolver__LRG(CScope& scope)
  : CSeq_id_Resolver(scope)
{
    m_regexp = new CRegexp("^(LRG_\\d+)[-_\\.]?([pt]\\d+)?$");
}

CSeq_id_Handle CSeq_id_Resolver__LRG::x_Create(const string& s)
{
    string lrg_id_str = m_regexp->GetSub(s, 1);         //e.g. LRG_123
    string lrg_product_id_str = m_regexp->GetSub(s, 2); //e.g. p1 or t1


    CRef<CSeq_id> lrg_seq_id(new CSeq_id("gnl|LRG|" + lrg_id_str));
    CSeq_id_Handle ng_idh = sequence::GetId(*lrg_seq_id, *m_scope, sequence::eGetId_ForceAcc);
    if (NStr::IsBlank(lrg_product_id_str)) {
        //bare LRG_# with no product
        return ng_idh;
    }

    CSeq_id_Handle product_idh;
    SAnnotSelector sel;
    sel.SetResolveTSE();
    sel.IncludeFeatType(CSeqFeatData::e_Gene);
    sel.IncludeFeatType(CSeqFeatData::e_Rna);
    sel.IncludeFeatType(CSeqFeatData::e_Cdregion);

    for (CFeat_CI ci(m_scope->GetBioseqHandle(ng_idh), sel); ci; ++ci) {
        const CMappedFeat& mf = *ci;
        if (!mf.IsSetDbxref()) {
            continue;
        }
        ITERATE (CSeq_feat::TDbxref, it, mf.GetDbxref()) {
            const CDbtag& dbtag = **it;
            if (NStr::Equal(dbtag.GetDb(), "LRG") &&
                dbtag.GetTag().IsStr() &&
                NStr::Equal(dbtag.GetTag().GetStr(), lrg_product_id_str) &&
                mf.IsSetProduct() &&
                mf.GetProduct().GetId()
               ) {
                product_idh = sequence::GetId(*mf.GetProduct().GetId(), *m_scope, sequence::eGetId_ForceAcc);
                break;
            }
        }
    }

    if (!product_idh) {
        NCBI_THROW(CException, eUnknown, "Can't find LRG product " + lrg_product_id_str + " on " + ng_idh.AsString());
    }
    return product_idh;
}




CSeq_id_Resolver__CCDS::CSeq_id_Resolver__CCDS(CScope& scope)
   : CSeq_id_Resolver(scope)
   , m_entrez(new objects::CEntrez2Client())
{
    m_regexp = new CRegexp("^CCDS\\d+\\.\\d+$");
}

CSeq_id_Handle CSeq_id_Resolver__CCDS::x_Create(const string& s)
{
    //todo: include several versions forward in the query.
    //Get gis back; sort desc; walk bioseqs and look for the CCDS of interest.

    string query_str = "srcdb_refseq[prop] AND biomol_mRNA[prop] AND dbxref_ccds[prop] AND \"CCDS:" + s + "\"";
    vector<int> gis;
    m_entrez->Query(query_str, "nuccore", gis, 0, 5);

    if (gis.size() != 1) {
        NCBI_THROW(CException, eUnknown, "Could not resolve " + s + " to a unique gi");
    }
    CSeq_id_Handle gi_handle = CSeq_id_Handle::GetHandle(GI_FROM(int, gis.front()));
    CSeq_id_Handle idh = sequence::GetId(gi_handle, *m_scope, sequence::eGetId_ForceAcc);
    return idh;
}

CSeq_id_Resolver__CCDS::~CSeq_id_Resolver__CCDS()
{
    delete m_entrez;
}


CSeq_id_Resolver__ChrNamesFromGC::CSeq_id_Resolver__ChrNamesFromGC(const CGC_Assembly& assembly, CScope& scope)
  : CSeq_id_Resolver(scope),
    m_SLMapper(new CSeq_loc_Mapper(assembly,
                                   CSeq_loc_Mapper::eGCA_Refseq
                                   //,&scope,
                                   //CSeq_loc_Mapper::eCopyScope
                                  )
              )
{
}

CSeq_id_Handle CSeq_id_Resolver__ChrNamesFromGC::x_Create(const string& s)
{
    typedef CCache<string, CSeq_id_Handle> TLocCache;
    static auto_ptr<TLocCache> loccache(new TLocCache(15));
    static const int kRetrFlags = TLocCache::fGet_NoInsert;

    TLocCache::EGetResult result;
    const CSeq_id_Handle exist_idh = loccache->Get(s, kRetrFlags, &result);
    if (result == TLocCache::eGet_Found) {
        LOG_POST(Info << "cached id for: " << s);
        return exist_idh;
    }

    CSeq_id_Handle idh;
    CConstRef<CSeq_id> origid;
    const bool is_numeric = (NStr::StringToNumeric(s) != -1);
    if (is_numeric) {
        CRef<CSeq_id> sid(new CSeq_id());
        // ensure numeric chromosome names are stored as "local str" not "local id"
        sid->SetLocal().SetStr(s);
        origid = sid;
    }
    else {
        try {
            idh = sequence::GetId(CSeq_id_Handle::GetHandle(s), *m_scope, sequence::eGetId_ForceAcc);
            if (idh && !idh.IsGi()) {
                origid = idh.GetSeqIdOrNull();
            }
        }
        catch (const CException& e) {
            // ignore errors, mostly "Malformatted ID"
        }
    }
    LOG_POST(Info << "input: " << s);
    if (origid.IsNull()) {
        origid.Reset(new CSeq_id(s, CSeq_id::fParse_AnyLocal));
        LOG_POST(Info << "created seq-id: " << origid->AsFastaString());
    }
    LOG_POST(Info << "created seq-id-handle: " << idh.AsString());
    CConstRef<CSeq_loc> origloc(new CSeq_loc(const_cast<CSeq_id&>(*origid), 0, 0));
    CConstRef<CSeq_loc> newloc = x_MapLoc(*origloc);
    const CSeq_id& id = *(newloc.NotNull() ? newloc : origloc)->GetId();
    idh = sequence::GetId(id, *m_scope, sequence::eGetId_Best);
    loccache->Add(s, idh);
    return idh;
}

CConstRef<CSeq_loc> CSeq_id_Resolver__ChrNamesFromGC::x_MapLoc(const CSeq_loc& loc) const
{
    try {
        CConstRef<CSeq_loc> new_loc = m_SLMapper->Map(loc);
        return (new_loc.IsNull() || new_loc->IsNull()) ? CConstRef<CSeq_loc>() : new_loc;
    }
    catch (const CException& e) {
        ERR_POST("Exception in CSeq_loc_Mapper: " << e.GetMsg());
        return CConstRef<CSeq_loc>();
    }
}

bool CSeq_id_Resolver__ChrNamesFromGC::CanCreate(const string& s)
{
    return bool(x_Create(s));
}


END_NCBI_SCOPE
