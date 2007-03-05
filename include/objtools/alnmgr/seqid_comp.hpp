#ifndef OBJTOOLS_ALNMGR___SEQID_COMP__HPP
#define OBJTOOLS_ALNMGR___SEQID_COMP__HPP
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
* Author:  Kamen Todorov, NCBI
*
* File Description:
*   Seq-id comparison functors
*
* ===========================================================================
*/


#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <objects/seqalign/seqalign_exception.hpp>

#include <objmgr/scope.hpp>

/// Implementation includes


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


template <typename TSeqIdPtr>
class SCompareOrdered :
    public binary_function<TSeqIdPtr, TSeqIdPtr, bool>
{
public:
    bool operator()(TSeqIdPtr left_seq_id,
                    TSeqIdPtr right_seq_id) const
    {
        return left_seq_id->CompareOrdered(*right_seq_id) < 0;
    }
};


template <typename TSeqIdPtr>
class CSeqIdBioseqHandleComp :
    public binary_function<TSeqIdPtr, TSeqIdPtr, bool>
{
public:
    CSeqIdBioseqHandleComp(CScope& scope) : m_Scope(scope) {}

    bool operator()(TSeqIdPtr left_seq_id,
                    TSeqIdPtr right_seq_id) const
    {
        CBioseq_Handle l_bioseq_handle = m_Scope.GetBioseqHandle(*left_seq_id);
        CBioseq_Handle r_bioseq_handle = m_Scope.GetBioseqHandle(*right_seq_id);
        if ( !l_bioseq_handle ) {
            string err_str =
                string("Seq-id cannot be resolved: ")
                + left_seq_id->AsFastaString();
            NCBI_THROW(CSeqalignException, eInvalidSeqId, err_str);
        }
        if ( !r_bioseq_handle ) {
            string err_str =
                string("Seq-id cannot be resolved: ")
                + right_seq_id->AsFastaString();
            NCBI_THROW(CSeqalignException, eInvalidSeqId, err_str);
        }
        return l_bioseq_handle < r_bioseq_handle;
    }
private:
    CScope& m_Scope;
};



END_NCBI_SCOPE

#endif  // OBJTOOLS_ALNMGR___SEQID_COMP__HPP
