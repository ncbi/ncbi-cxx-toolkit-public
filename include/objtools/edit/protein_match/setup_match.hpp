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
 * Authors:  Justin Foley
 */

#ifndef _SETUP_MATCH_H_
#define _SETUP_MATCH_H_

#include <corelib/ncbistd.hpp>
#include <objmgr/seq_entry_handle.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objmgr/scope.hpp>
#include <objects/seqset/Seq_entry.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CMatchSetup {

public:
    CMatchSetup(CRef<CScope> db_scope);
    virtual ~CMatchSetup(void) = default;

    static void GatherNucProtSets(
        CSeq_entry& input_entry,
        list<CRef<CSeq_entry>>& nuc_prot_sets);


    CConstRef<CSeq_entry> GetDBEntry(const CSeq_id& nuc_id);

    CConstRef<CSeq_entry> GetDBEntry(const CBioseq& nuc_seq);

    CSeq_entry_Handle GetTopLevelEntry(const CSeq_id& seq_id);

    bool UpdateNucSeqIds(CRef<CSeq_id> new_id,
        CSeq_entry& nuc_prot_set) const;

    bool GetNucSeqIdFromCDSs(const CSeq_entry& nuc_prot_set,
        CRef<CSeq_id>& id) const;

    bool GetAccession(const CBioseq& bioseq, CRef<CSeq_id>& id) const;

    bool GetNucSeqId(const CBioseq_set& nuc_prot_set, CRef<CSeq_id>& id) const;

    bool GetReplacedIdsFromHist(const CBioseq& nuc_seq, list<CRef<CSeq_id>>& ids) const;

    void GatherCdregionFeatures(const CSeq_entry& nuc_prot_set,
            list<CRef<CSeq_feat>>& cds_feats) const;

    CRef<CSeq_entry> GetCoreNucProtSet(const CSeq_entry& nuc_prot_set, bool exclude_local_ids=false) const;
private:

    struct SIdCompare
    {
        bool operator()(const CRef<CSeq_id>& id1,
            const CRef<CSeq_id>& id2) const 
        {
            return id1->CompareOrdered(*id2) < 0;
        }
    };

    bool x_GetNucSeqIdsFromCDSs(const CSeq_annot& annot,
        set<CRef<CSeq_id>, SIdCompare>& ids) const;

    CBioseq& x_FetchNucSeqRef(CSeq_entry& nuc_prot_set) const;
    CBioseq& x_FetchNucSeqRef(CBioseq_set& nuc_prot_set) const;
    const CBioseq& x_FetchNucSeqRef(const CBioseq_set& nuc_prot_set) const;

    CRef<CScope> m_DBScope;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif
