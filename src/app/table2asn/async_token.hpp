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
* Authors:
*
* File Description:
*   table2asn thread state
*
*/

#include <mutex>

BEGIN_NCBI_SCOPE

using namespace objects;

class CSerialObject;

namespace objects::feature {
    class CFeatTree;
}
class CValidMessageHandler;

struct TAsyncToken
{
    using TFeatMap = map<string, CRef<objects::CSeq_feat>>;

    CRef<CSeq_entry>  entry;
    CRef<CSeq_submit> submit;
    CRef<CSeq_entry>  top_entry;
    CRef<CScope>      scope;
    CSeq_entry_Handle seh;
    CRef<objects::CBioseq> bioseq;
    CRef<objects::feature::CFeatTree> feat_tree;
    TFeatMap map_transcript_to_mrna;
    TFeatMap map_protein_to_mrna;
    TFeatMap map_locus_to_gene;
    atomic_bool* pPubLookupDone = nullptr;
    std::mutex* cleanup_mutex = nullptr;
    CValidMessageHandler* pMsgHandler = nullptr;

    operator CConstRef<CSeq_entry>() const;
    void Clear();

    void InitFeatures();

    CRef<objects::CSeq_feat> ParentGene(const objects::CSeq_feat& cds);
    CRef<feature::CFeatTree> FeatTree();
    CRef<objects::CSeq_feat> FindFeature(const objects::CFeat_id& id);
    CRef<objects::CSeq_feat> FeatFromMap(const string& key, const TFeatMap&) const;
    CRef<CSeq_feat> FindGeneByLocusTag(const CSeq_feat& cds) const;
    CRef<objects::CSeq_feat> ParentMrna(const objects::CSeq_feat& cds);
    CRef<objects::CSeq_feat> FindMrnaByQual(const objects::CSeq_feat& cds) const;
};

END_NCBI_SCOPE
