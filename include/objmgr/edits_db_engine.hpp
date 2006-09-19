#ifndef __OBJMGR__EDITS_DB_ENGINE__HPP
#define __OBJMGR__EDITS_DB_ENGINE__HPP

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
* Author: Maxim Didenko
*
* File Description:
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/plugin_manager.hpp>

#include <list>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeqEdit_Cmd;
class CSeq_id_Handle;

class NCBI_XOBJMGR_EXPORT IEditsDBEngine : public CObject
{
public:
    typedef list<CRef<CSeqEdit_Cmd> >    TCommands;
    typedef map<CSeq_id_Handle, string > TChangedIds;
    
    virtual ~IEditsDBEngine();

    virtual bool HasBlob(const string& blobid) const = 0;
    virtual bool FindSeqId(const CSeq_id_Handle& id, string& blobid) const = 0;
    virtual void NotifyIdChanged(const CSeq_id_Handle& id, 
                                 const string& newblobid) = 0;

    virtual void SaveCommand(const CSeqEdit_Cmd& cmd) = 0;

    virtual void BeginTransaction() = 0;
    virtual void CommitTransaction() = 0;
    virtual void RollbackTransaction() = 0;

    virtual void GetCommands(const string& blobid, TCommands& cmds) const = 0;

};

END_SCOPE(objects)

NCBI_DECLARE_INTERFACE_VERSION(objects::IEditsDBEngine,  "xeditsdbengine", 1, 0, 0);
 
template<>
class CDllResolver_Getter<objects::IEditsDBEngine>
{
public:
    CPluginManager_DllResolver* operator()(void)
    {
        CPluginManager_DllResolver* resolver =
            new CPluginManager_DllResolver
            (CInterfaceVersion<objects::IEditsDBEngine>::GetName(),
             kEmptyStr,
             CVersionInfo::kAny,
             CDll::eAutoUnload);
        resolver->SetDllNamePrefix("ncbi");
        return resolver;
    }
};

END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.3  2006/09/19 19:18:04  vasilche
* Fixed last name.
*
* Revision 1.2  2006/05/01 16:56:44  didenko
* Attach SeqEntry edit command revamp
*
* Revision 1.1  2006/01/25 18:59:03  didenko
* Redisgned bio objects edit facility
*
* ===========================================================================
*/

#endif  // __OBJMGR__EDITS_DB_ENGINE__HPP
