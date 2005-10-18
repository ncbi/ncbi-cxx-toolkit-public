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
 * Author:  Robert Smith
 *
 * File Description:
 *   Basic Cleanup of CSeq_entries.
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <serial/serialbase.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqfeat/Seq_feat.hpp>


#include <objmgr/util/sequence.hpp>
#include <objtools/cleanup/cleanup.hpp>

// #include "cleanupp.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

enum EChangeType {
    eChange_UNKNOWN
};

// *********************** CCleanup implementation **********************


CCleanup::CCleanup() 
{
}


CCleanup::~CCleanup(void)
{
}


CConstRef<CCleanupChange> CCleanup::BasicCleanup
(CSeq_entry& se,
 Uint4 options)
{
    CRef<CCleanupChange> errors(new CCleanupChange(&se));
    se.BasicCleanup();
    return errors;
}


/// Cleanup a Bioseq. 
CConstRef<CCleanupChange> CCleanup::BasicCleanup(CBioseq& bs, Uint4 options)
{
    CRef<CCleanupChange> errors(new CCleanupChange(&bs));
    bs.BasicCleanup();
    return errors;
}


CConstRef<CCleanupChange> CCleanup::BasicCleanup(CBioseq_set& bss, Uint4 options)
{
    CRef<CCleanupChange> errors(new CCleanupChange(&bss));
    bss.BasicCleanup();
    return errors;
}


CConstRef<CCleanupChange> CCleanup::BasicCleanup(CSeq_annot& sa, Uint4 options)
{
    CRef<CCleanupChange> errors(new CCleanupChange(&sa));
    sa.BasicCleanup();
    return errors;
}


CConstRef<CCleanupChange> CCleanup::BasicCleanup(CSeq_feat& sf, Uint4 options)
{
    CRef<CCleanupChange> errors(new CCleanupChange(&sf));
    sf.BasicCleanup();
    return errors;
}


// *********************** CCleanupChange implementation **********************


CCleanupChange::CCleanupChange(const CSerialObject* obj) :
    m_Cleaned(obj)
{
}

void CCleanupChange::AddChangedItem
(unsigned int         cc,
 const string&        msg,
 const string&        desc,
 const CSerialObject& obj,
 CScope&              scope)
{
    CRef<CCleanupChangeItem> item(new CCleanupChangeItem(cc, msg, desc, obj, scope));
    m_ChangeItems.push_back(item);
}


CCleanupChange::~CCleanupChange()
{
}


SIZE_TYPE CCleanupChange::Size(void) const 
{
    return m_ChangeItems.size();
}


const CSerialObject* CCleanupChange::GetCleaned(void) const
{
    return m_Cleaned.GetPointerOrNull();
}


// ************************ CCleanupChange_CI implementation **************

CCleanupChange_CI::CCleanupChange_CI(void) :
    m_Changes(0),
    m_ChangeCodeFilter(kEmptyStr) // eChange_UNKNOWN
{
}


CCleanupChange_CI::CCleanupChange_CI
(const CCleanupChange& ve,
 const string& errcode) :
    m_Changes(&ve),
    m_Current(ve.m_ChangeItems.begin()),
    m_ChangeCodeFilter(errcode)
{
    if ( IsValid()  &&  !Filter(**m_Current) ) {
        Next();
    }
}


CCleanupChange_CI::CCleanupChange_CI(const CCleanupChange_CI& other)
{
    if ( this != &other ) {
        *this = other;
    }
}


CCleanupChange_CI::~CCleanupChange_CI(void)
{
}


CCleanupChange_CI& CCleanupChange_CI::operator=(const CCleanupChange_CI& iter)
{
    if (this == &iter) {
        return *this;
    }

    m_Changes = iter.m_Changes;
    m_Current = iter.m_Current;
    m_ChangeCodeFilter = iter.m_ChangeCodeFilter;
    return *this;
}


CCleanupChange_CI& CCleanupChange_CI::operator++(void)
{
    Next();
    return *this;
}


bool CCleanupChange_CI::IsValid(void) const
{
    return m_Current != m_Changes->m_ChangeItems.end();
}


const CCleanupChangeItem& CCleanupChange_CI::operator*(void) const
{
    return **m_Current;
}


const CCleanupChangeItem* CCleanupChange_CI::operator->(void) const
{
    return &(**m_Current);
}


bool CCleanupChange_CI::Filter(const CCleanupChangeItem& item) const
{
    if ( (m_ChangeCodeFilter.empty()  ||  
          NStr::StartsWith(item.GetChangeCode(), m_ChangeCodeFilter)) ) {
        return true;;
    }
    return false;
}


void CCleanupChange_CI::Next(void)
{
    if ( AtEnd() ) {
        return;
    }

    do {
        ++m_Current;
    } while ( !AtEnd()  &&  !Filter(**m_Current) );
}


bool CCleanupChange_CI::AtEnd(void) const
{
    return m_Current == m_Changes->m_ChangeItems.end();
}


// *********************** CCleanupChangeItem implementation ********************


CCleanupChangeItem::CCleanupChangeItem
(unsigned int         cc,
 const string&        msg,
 const string&        desc,
 const CSerialObject& obj,
 CScope& scope)
  : m_ChangeIndex(cc),
    m_Message(msg),
    m_Desc(desc),
    m_Object(&obj),
    m_Scope(&scope)
{
}


CCleanupChangeItem::~CCleanupChangeItem(void)
{
}


const string& CCleanupChangeItem::ConvertChangeCode(unsigned int err)
{
    if (err <= eChange_UNKNOWN) {
        return sm_Terse [err];
    }
    return sm_Terse [eChange_UNKNOWN];
}


const string& CCleanupChangeItem::GetChangeCode(void) const
{
    return ConvertChangeCode(m_ChangeIndex);
}


unsigned int CCleanupChangeItem::GetChangeIndex(void) const
{
    return m_ChangeIndex;
}


unsigned int CCleanupChangeItem::GetChangeCount(void)
{
    return eChange_UNKNOWN + 1;
}


const string& CCleanupChangeItem::GetMsg(void) const
{
    return m_Message;
}


const string& CCleanupChangeItem::GetObjDesc(void) const
{
    return m_Desc;
}


const string& CCleanupChangeItem::GetVerbose(void) const
{
    if (m_ChangeIndex <= eChange_UNKNOWN) {
        return sm_Verbose [m_ChangeIndex];
    }
    return sm_Verbose [eChange_UNKNOWN];
}


const CSerialObject& CCleanupChangeItem::GetObject(void) const
{
    return *m_Object;
}


END_SCOPE(objects)
END_NCBI_SCOPE

 
/*
* ===========================================================================
*
* $Log$
* Revision 1.3  2005/10/18 14:22:25  rsmith
* BasicCleanup entry points for more classes.
*
* Revision 1.2  2005/10/18 13:34:15  rsmith
* add change reporting classes
*
* Revision 1.1  2005/10/17 12:22:12  rsmith
* initial checkin
*
*
* ===========================================================================
*/

