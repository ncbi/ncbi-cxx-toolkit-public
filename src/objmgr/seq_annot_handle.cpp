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
* Author: Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*
*/

#include <objmgr/seq_annot_handle.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/impl/seq_annot_info.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CSeq_annot_Handle::CSeq_annot_Handle(void)
    : m_Scope(0),
      m_Seq_annot(0)
{
}


CSeq_annot_Handle::CSeq_annot_Handle(CScope& scope,
                                     const CSeq_annot_Info& annot)
    : m_Scope(&scope),
      m_Seq_annot(&annot)
{
}


CSeq_annot_Handle::CSeq_annot_Handle(const CSeq_annot_Handle& sah)
    : m_Scope(sah.m_Scope),
      m_Seq_annot(sah.m_Seq_annot)
{
}


CSeq_annot_Handle::~CSeq_annot_Handle(void)
{
}


CSeq_annot_Handle& CSeq_annot_Handle::operator=(const CSeq_annot_Handle& sah)
{
    m_Scope = sah.m_Scope;
    m_Seq_annot = sah.m_Seq_annot;
    return *this;
}


void CSeq_annot_Handle::x_Set(CScope& scope, const CSeq_annot_Info& annot)
{
    m_Scope.Reset(&scope);
    m_Seq_annot.Reset(&annot);
}


void CSeq_annot_Handle::x_Reset(void)
{
    m_Scope.Reset();
    m_Seq_annot.Reset();
}


CScope& CSeq_annot_Handle::GetScope(void) const
{
    return *m_Scope;
}


const CSeq_annot& CSeq_annot_Handle::GetSeq_annot(void) const
{
    return m_Seq_annot->GetSeq_annot();
}


bool CSeq_annot_Handle::IsNamed(void) const
{
    return m_Seq_annot->GetName().IsNamed();
}


const string& CSeq_annot_Handle::GetName(void) const
{
    return m_Seq_annot->GetName().GetName();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2003/10/07 13:43:23  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
* Revision 1.1  2003/09/30 16:22:03  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* ===========================================================================
*/
