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
 * Authors:  Igor Filippov
 */


#include <ncbi_pch.hpp>
#include <objects/seqfeat/Feat_id.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_entry_ci.hpp>
#include <objtools/cleanup/fix_feature_id.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


CObject_id::TId CFixFeatureId::s_FindHighestFeatureId(const CSeq_entry_Handle& entry)
{
    CObject_id::TId feat_id = 0;
    for (CFeat_CI feat_it(entry); feat_it; ++feat_it) {
        if (feat_it->IsSetId()) {
            const CFeat_id &id = feat_it->GetId();
            if (id.IsLocal() && id.GetLocal().IsId() && id.GetLocal().GetId() > feat_id) {
                feat_id = id.GetLocal().GetId();
            }
        }
    }
    return feat_id;
}

void CFixFeatureId::s_UpdateFeatureIds(const CSeq_entry_Handle& entry, map<CSeq_feat_Handle, CRef<CSeq_feat> > &changed_feats, int offset)
{
    for ( CFeat_CI feat_it(entry); feat_it; ++feat_it ) 
    {
        bool modified = false;
        CRef<CSeq_feat> edited;
        CSeq_feat_Handle fh = feat_it->GetSeq_feat_Handle();
        if (changed_feats.find(fh) != changed_feats.end())
        {
            edited = changed_feats[fh];
        }
        else
        {
            edited.Reset(new CSeq_feat);
            edited->Assign(feat_it->GetOriginalFeature());
        }

        if (edited->IsSetId() && edited->GetId().IsLocal() && edited->GetId().GetLocal().IsId())        
        {
            int id = edited->GetId().GetLocal().GetId();
            edited->SetId().SetLocal().SetId(id+offset);  
            modified = true;
        }
        if (edited->IsSetXref())
        {
            CSeq_feat::TXref::iterator xref_it = edited->SetXref().begin();
            while ( xref_it != edited->SetXref().end() )
            {
                if ((*xref_it)-> IsSetId() && (*xref_it)->GetId().IsLocal() && (*xref_it)->GetId().GetLocal().IsId())
                {
                    int id = (*xref_it)->GetId().GetLocal().GetId();
                    (*xref_it)->SetId().SetLocal().SetId(id+offset);
                    modified = true;
                }
                ++xref_it;
            }
        }
        if (modified)
        {
            changed_feats[fh] = edited;
        }
    }
}


void CFixFeatureId::s_ApplyToSeqInSet(CSeq_entry_Handle tse, map<CSeq_feat_Handle, CRef<CSeq_feat> > &changed_feats)
{
    int offset = 0;
    if (tse && tse.IsSet() && tse.GetSet().IsSetClass() && tse.GetSet().GetClass() == CBioseq_set::eClass_genbank) 
    {
        for(CSeq_entry_CI direct_child_ci( tse.GetSet(), CSeq_entry_CI::eNonRecursive ); direct_child_ci; ++direct_child_ci ) 
        {
            const CSeq_entry_Handle& entry = *direct_child_ci;
            CObject_id::TId top_id = s_FindHighestFeatureId(entry);
            s_UpdateFeatureIds(entry, changed_feats, offset);
            offset += top_id;
        }
    }
}

void CFixFeatureId::s_MakeIDPairs(const CSeq_entry_Handle& entry, map<int,int> &id_pairs)
{
    int feat_id = 0;
    for (CFeat_CI feat_it(entry); feat_it; ++feat_it) {
        if (feat_it->IsSetId()) {
            const CFeat_id &id = feat_it->GetId();
            if (id.IsLocal() && id.GetLocal().IsId() && id_pairs.find(id.GetLocal().GetId()) == id_pairs.end()) {
                id_pairs[id.GetLocal().GetId()] = ++feat_id;
            }
        }
    }
}

void CFixFeatureId::s_ReassignFeatureIds(const CSeq_entry_Handle& entry, map<CSeq_feat_Handle, CRef<CSeq_feat> > &changed_feats)
{
    if (!entry)
        return;
    map<int,int> id_pairs;
    CFixFeatureId::s_MakeIDPairs(entry, id_pairs);

    for ( CFeat_CI feat_it(entry); feat_it; ++feat_it ) 
    {
        bool modified = false;
        CRef<CSeq_feat> edited;
        CSeq_feat_Handle fh = feat_it->GetSeq_feat_Handle();
        if (changed_feats.find(fh) != changed_feats.end())
        {
            edited = changed_feats[fh];
        }
        else
        {
            edited.Reset(new CSeq_feat);
            edited->Assign(feat_it->GetOriginalFeature());
        }

        if (edited->IsSetId() && edited->GetId().IsLocal() && edited->GetId().GetLocal().IsId())        
        {
            int id = id_pairs[edited->GetId().GetLocal().GetId()];
            edited->SetId().SetLocal().SetId(id);  
            modified = true;
        }
       if (edited->IsSetXref())
        {
            CSeq_feat::TXref::iterator xref_it = edited->SetXref().begin();
            while ( xref_it != edited->SetXref().end() )
            {
                if ((*xref_it)-> IsSetId() && (*xref_it)->GetId().IsLocal() && (*xref_it)->GetId().GetLocal().IsId())
                {
                    modified = true;
                    if (id_pairs.find((*xref_it)->GetId().GetLocal().GetId()) != id_pairs.end())
                        {
                            int id = id_pairs[(*xref_it)->GetId().GetLocal().GetId()];
                            (*xref_it)->SetId().SetLocal().SetId(id);
                        }
                    else
                        {
                            (*xref_it)->ResetId();
                            xref_it = edited->SetXref().erase(xref_it);
                            continue;
                        }
                }
                ++xref_it;
            }
            if (edited->SetXref().empty())
                edited->ResetXref();
        }
       if (modified)
       {
           changed_feats[fh] = edited;
       }
    }
}

END_NCBI_SCOPE
