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

#include <objmgr/util/sequence.hpp>
#include <misc/hgvs/hgvs_parser2.hpp>
#include <misc/hgvs/seq_id_resolver.hpp>

#include <util/xregexp/regexp.hpp>
#include <objects/entrez2/entrez2_client.hpp>

#include <objects/genomecoll/GC_Sequence.hpp>
#include <objects/genomecoll/GC_TypedSeqId.hpp>


BEGIN_NCBI_SCOPE

bool CSeq_id_Resolver::CanCreate(const string& s)
{
    return m_regexp ? m_regexp->IsMatch(s) : true;
}

CSeq_id_Handle CSeq_id_Resolver::x_Create(const string& s)
{
    return sequence::GetId(CSeq_id_Handle::GetHandle(s), *m_scope, sequence::eGetId_ForceAcc);
}

CSeq_id_Resolver::~CSeq_id_Resolver()
{
    if(m_regexp) {
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
    if(lrg_product_id_str == "") {
        //bare LRG_# with no product
        return ng_idh;
    }

    CSeq_id_Handle product_idh;
    SAnnotSelector sel;
    sel.SetResolveTSE();
    for(CFeat_CI ci(m_scope->GetBioseqHandle(ng_idh)); ci; ++ci) {
        const CMappedFeat& mf = *ci;
        if(!mf.IsSetDbxref()) {
            continue;
        }
        ITERATE(CSeq_feat::TDbxref, it, mf.GetDbxref()) {
            const CDbtag& dbtag = **it;
            if(dbtag.GetDb() == "LRG"
               && dbtag.GetTag().IsStr()
               && dbtag.GetTag().GetStr() == lrg_product_id_str
               && mf.IsSetProduct()
               && mf.GetProduct().GetId())
            {
                product_idh = sequence::GetId(*mf.GetProduct().GetId(), *m_scope, sequence::eGetId_ForceAcc);
                break;
            }
        }
    }

    if(!product_idh) {
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

    if(gis.size() != 1) {
        NCBI_THROW(CException, eUnknown, "Could not resolve " + s + " to a unique gi");
    }
    CSeq_id_Handle gi_handle = CSeq_id_Handle::GetHandle(gis.front());
    CSeq_id_Handle idh = sequence::GetId(gi_handle, *m_scope, sequence::eGetId_ForceAcc);
    return idh;
}

CSeq_id_Resolver__CCDS::~CSeq_id_Resolver__CCDS()
{
    delete m_entrez;
}


CSeq_id_Resolver__ChrNamesFromGC::CSeq_id_Resolver__ChrNamesFromGC(const CGC_Assembly& assembly, CScope& scope)
  : CSeq_id_Resolver(scope)
{
    m_regexp = new CRegexp("^(Chr|CHR|chr)\\w+$");

    CGC_Assembly::TSequenceList tls;
    assembly.GetMolecules(tls, CGC_Assembly::eTopLevel);
    ITERATE(CGC_Assembly::TSequenceList, it, tls) {
        const CGC_Sequence& seq= **it;
        if(!seq.IsSetSeq_id_synonyms()) {
            continue;
        }
        CConstRef<CSeq_id> syn_seq_id = seq.GetSynonymSeq_id(CGC_TypedSeqId::e_Private, CGC_SeqIdAlias::e_None);
        if(!syn_seq_id) {
            continue;
        }
        string syn = syn_seq_id->GetSeqIdString(false);
        if(!NStr::StartsWith(syn, "chr")) {
            syn = "chr" + syn;
        }
        if(m_data.find(syn) != m_data.end()) {
            NCBI_THROW(CException, eUnknown, "Non-unique chromosome names: " + syn);
        } else {
            m_data[syn] = sequence::GetId(seq.GetSeq_id(), *m_scope, sequence::eGetId_ForceAcc);
        }
    }
}

CSeq_id_Handle CSeq_id_Resolver__ChrNamesFromGC::x_Create(const string& s)
{
    CSeq_id_Handle idh;
    string s2 = NStr::Replace(s, "Chr", "chr", 0, 1);
    NStr::ReplaceInPlace(s2, "CHR", "chr", 0, 1);
    if(m_data.find(s2) != m_data.end()) {
        idh = m_data.find(s2)->second;
    }
    return idh;
}
END_NCBI_SCOPE
