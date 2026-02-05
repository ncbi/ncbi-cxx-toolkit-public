/* $Id$
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
 * Author:  Eyal Mozes
 *
 * File Description:
 *
 * ===========================================================================
 */

#ifndef CU_CDGILOOKUP_HPP
#define CU_CDGILOOKUP_HPP

#include <objmgr/scope.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

class NCBI_CDUTILS_EXPORT CDGiLookup
{
public:
    enum EGiNotFoundAction { eLeaveUnchanged, eRemove, eThrow};

    CDGiLookup(EGiNotFoundAction policy = eLeaveUnchanged)
    : m_Policy(policy)
    {}

    /// Set the scope to be used in gi lookups
    void SetScope(CScope &scope)
    { m_Scope.Reset(&scope); }

    /// Insert the gi in the id field in any bioseq that doesn't have one
    /// Internally index the bioseq gis and other seq-ids
    void InsertGis(vector< CRef< CBioseq > >& bioseqs);

    /// Replace subject seq-ids, and non-local query seq-ids, by gis
    void InsertGis(CSeq_align_set& alignments);

    /// provide bioseqs that are already available with gis for indexing
    void IndexGis(const vector< CRef< CBioseq > >& bioseqs);

private:
    EGiNotFoundAction m_Policy;

    CRef<CScope> m_Scope;

    map<CSeq_id_Handle, TGi> m_GiIndex;

    CScope &x_GetScope();

    TGi x_LookupGi(const CSeq_id &id);

    void x_IndexGi(const CBioseq& bioseq);

    void x_ReportGiNotFound(const CSeq_id &id);
};

END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif
