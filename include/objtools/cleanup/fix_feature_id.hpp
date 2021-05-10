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
 *  and reliability of the software and data,  the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties,  express or implied,  including
 *  warranties of performance,  merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 *
 * Authors:  Igor Filippov
 */
#ifndef _FIX_FEATURE_ID_H_
#define _FIX_FEATURE_ID_H_

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objmgr/util/feature.hpp>
#include <objmgr/seq_entry_handle.hpp>

#include <unordered_set>
#include <unordered_map>

BEGIN_NCBI_SCOPE

struct NCBI_CLEANUP_EXPORT SFeatIdManager
{
    using TId = objects::CFeat_id::TLocal::TId;
    TId offset;
    unordered_map<TId,TId> id_map;    
    unordered_set<TId> existing_ids;
    unordered_set<TId> unchanged_ids;
    unordered_set<TId> new_ids;
};


class NCBI_CLEANUP_EXPORT CFixFeatureId
{
public:
    using TId = SFeatIdManager::TId;
    using TIdSet = unordered_set<TId>;
    using TFeatMap = map<objects::CSeq_feat_Handle, CRef<objects::CSeq_feat>>;

    static TId s_FindHighestFeatureId(const objects::CSeq_entry_Handle& entry);
    static void s_ApplyToSeqInSet(objects::CSeq_entry_Handle tse, TFeatMap& changed_feats);
    static void s_ApplyToSeqInSet(objects::CSeq_entry_Handle tse);
    static void s_ReassignFeatureIds(const objects::CSeq_entry_Handle& entry, TFeatMap& changed_feats);
    static bool UpdateFeatureIds(objects::CSeq_feat& feature, SFeatIdManager& id_manager);
private:
    static void s_MakeIDPairs(const objects::CSeq_entry_Handle& entry, map<TId,TId> &id_pairs);
    static void s_UpdateFeatureIds(const objects::CSeq_entry_Handle& entry, TFeatMap &changed_feats, TIdSet &existing_ids, TId &offset);
    static bool UpdateFeatureIds(objects::CSeq_entry_Handle entry_handle, TIdSet& existing_ids, TId& offset);
};


END_NCBI_SCOPE

#endif
    // _FIX_FEATURE_ID_H_
