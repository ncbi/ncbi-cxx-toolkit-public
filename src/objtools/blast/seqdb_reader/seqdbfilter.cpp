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
 * Author:  Kevin Bealer
 *
 */

/// @file seqdbfilter.cpp
/// Implementation for some assorted ID list filtering code.

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include "seqdbfilter.hpp"
#include "seqdbbitset.hpp"

BEGIN_NCBI_SCOPE

CRef<CSeqDB_FilterTree> CSeqDB_FilterTree::Specialize(string volname) const
{
    CRef<CSeqDB_FilterTree> clone(new CSeqDB_FilterTree);
    clone->SetName(m_Name);
    clone->AddFilters(m_Filters);
    
    ITERATE(vector< CRef<CSeqDB_FilterTree> >, iter, m_SubNodes) {
        CRef<CSeqDB_FilterTree> sub = (**iter).Specialize(volname);
        
        if (sub.NotEmpty()) {
            if (! sub->GetFilters().empty()) {
                clone->AddNode(sub);
            } else {
                clone->AddNodes(sub->GetNodes());
                clone->AddVolumes(sub->GetVolumes());
            }
        }
    }
    
    ITERATE(vector<CSeqDB_BasePath>, iter, m_Volumes) {
        if (iter->GetBasePathS() == volname) {
            clone->AddVolume(*iter);
        }
    }
    
    // If this node is an placeholder with a single child node,
    // replace this node with the child node.
    
    while(clone->m_Filters.empty() &&
          clone->m_Volumes.empty() &&
          clone->m_SubNodes.size() == 1) {
        
        CRef<CSeqDB_FilterTree> sub = clone->m_SubNodes[0];
        clone = sub;
    }
    
    if (clone->m_SubNodes.empty() && clone->m_Volumes.empty()) {
        clone.Reset();
    }
    
    return clone;
}

bool CSeqDB_FilterTree::HasFilter() const
{
    if (! m_Filters.empty()) {
        return false;
    }
    
    ITERATE(vector< CRef<CSeqDB_FilterTree> >, iter, m_SubNodes) {
        if ((**iter).HasFilter())
            return true;
    }
    
    return true;
}

END_NCBI_SCOPE

