
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
* Author: Aleksey Grichenko
*
* File Description:
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2002/01/28 19:44:49  gouriano
* changed the interface of BioseqHandle: two functions moved from Scope
*
* Revision 1.3  2002/01/23 21:59:31  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.2  2002/01/16 16:25:56  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:17  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/


#include <objects/objmgr1/bioseq_handle.hpp>
#include "data_source.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CBioseq_Handle::~CBioseq_Handle(void)
{
}


const CSeq_id* CBioseq_Handle::GetSeqId(void) const
{
    if (!m_Value) return 0;
    return m_Value.m_SeqId.GetPointer();
}


const CBioseq& CBioseq_Handle::GetBioseq(void) const
{
    return x_GetDataSource().GetBioseq(*this);
}


const CSeq_entry& CBioseq_Handle::GetTopLevelSeqEntry(void) const
{
    return x_GetDataSource().GetTSE(*this);
}

CBioseq_Handle::TBioseqCore CBioseq_Handle::GetBioseqCore(void) const
{
    return x_GetDataSource().GetBioseqCore(*this);
}


const CSeqMap& CBioseq_Handle::GetSeqMap(void) const
{
    return x_GetDataSource().GetSeqMap(*this);
}


CSeqVector CBioseq_Handle::GetSeqVector(bool plus_strand)
{
    return CSeqVector(*this, plus_strand, *m_Scope);
}


END_SCOPE(objects)
END_NCBI_SCOPE
