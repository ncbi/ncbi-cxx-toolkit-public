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
*   TSE info -- entry for data source seq-id to TSE map
*
*/


#include "tse_info.hpp"

#include "annot_object.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CTSE_Info::
//
//    General information and indexes for top level seq-entries
//


const size_t kTSEMutexPoolSize = 16;
static CMutex s_TSEMutexPool[kTSEMutexPoolSize];


CTSE_Info::CTSE_Info(void)
    : m_Dead(false)
{
    m_LockCount.Set(0);
}


CTSE_Info::~CTSE_Info(void)
{
}


CFastMutex CTSE_Info::sm_TSEPool_Mutex;


void CTSE_Info::x_AssignMutex(void) const
{
    CFastMutexGuard guard(sm_TSEPool_Mutex);
    if ( m_TSE_Mutex )
        return;
    m_TSE_Mutex = &s_TSEMutexPool[((size_t)const_cast<CTSE_Info*>(this) >> 4) % kTSEMutexPoolSize];
}


void CTSE_Info::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
    ddc.SetFrame("CTSE_Info");
    CObject::DebugDump( ddc, depth);

    ddc.Log("m_TSE", m_TSE.GetPointer(),0);
    ddc.Log("m_Dead", m_Dead);
    if (depth == 0) {
        DebugDumpValue(ddc, "m_BioseqMap.size()", m_BioseqMap.size());
        DebugDumpValue(ddc, "m_AnnotMap.size()",  m_AnnotMap.size());
    } else {
        unsigned int depth2 = depth-1;
        { //--- m_BioseqMap
            DebugDumpValue(ddc, "m_BioseqMap.type",
                "map<CSeq_id_Handle, CRef<CBioseq_Info>>");
            CDebugDumpContext ddc2(ddc,"m_BioseqMap");
            TBioseqMap::const_iterator it;
            for (it=m_BioseqMap.begin(); it!=m_BioseqMap.end(); ++it) {
                string member_name = "m_BioseqMap[ " +
                    (it->first).AsString() +" ]";
                ddc2.Log(member_name, (it->second).GetPointer(),depth2);
            }
        }
        { //--- m_AnnotMap
            DebugDumpValue(ddc, "m_AnnotMap.type",
                "map<CSeq_id_Handle, CRangeMultimap<CRef<CAnnotObject>,"
                "CRange<TSeqPos>::position_type>>");

            CDebugDumpContext ddc2(ddc,"m_AnnotMap");
            TAnnotMap::const_iterator it;
            for (it=m_AnnotMap.begin(); it!=m_AnnotMap.end(); ++it) {
                string member_name = "m_AnnotMap[ " +
                    (it->first).AsString() +" ]";
                if (depth2 == 0) {
                    member_name += "size()";
                    DebugDumpValue(ddc2, member_name, (it->second).size());
                } else {
                    // CRangeMultimap
                    CDebugDumpContext ddc3(ddc2, member_name);
                    TRangeMap::const_iterator itrm;
                    for (itrm=(it->second).begin();
                        itrm!=(it->second).end(); ++itrm) {
                        // CRange as string
                        string rg;
                        if ((itrm->first).Regular()) {
                            rg =    NStr::UIntToString( (itrm->first).GetFrom()) +
                            "..." + NStr::UIntToString( (itrm->first).GetTo());
                        } else if ((itrm->first).Empty()) {
                            rg = "null";
                        } else if ((itrm->first).HaveInfiniteBound()) {
                            rg = "whole";
                        } else if ((itrm->first).HaveEmptyBound()) {
                            rg = "empty";
                        } else {
                            rg = "unknown";
                        }
                        string rm_name = member_name + "[ " + rg + " ]";
                        // CAnnotObject
                        ddc3.Log(rm_name, itrm->second, depth2-1);
                    }
                }
            }
        }
    }
    DebugDumpValue(ddc, "m_LockCount", m_LockCount.Get());
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.7  2002/07/08 20:51:02  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.6  2002/07/01 15:32:30  grichenk
* Fixed 'unused variable depth3' warning
*
* Revision 1.5  2002/05/31 17:53:00  grichenk
* Optimized for better performance (CTSE_Info uses atomic counter,
* delayed annotations indexing, no location convertions in
* CAnnot_Types_CI if no references resolution is required etc.)
*
* Revision 1.4  2002/05/29 21:21:13  gouriano
* added debug dump
*
* Revision 1.3  2002/03/14 18:39:13  gouriano
* added mutex for MT protection
*
* Revision 1.2  2002/02/21 19:27:06  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.1  2002/02/07 21:25:05  grichenk
* Initial revision
*
*
* ===========================================================================
*/
