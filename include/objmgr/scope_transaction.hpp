#ifndef __OBJMGR__SCOPE_TRANSACTION__HPP
#define __OBJMGR__SCOPE_TRANSACTION__HPP

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
*   Scope transation
*
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CScope;
class IScopeTransaction_Impl;

/// Scope Transaction
/// 
/// An instance of this class can be used to combine editing operations into
/// a logical block. It only can be created by an instance of CSope class and 
/// it can only be created on the stack. This class is not thread save.
///
/// @sa IEditSaver
///
class NCBI_XOBJMGR_EXPORT CScopeTransaction
{
public:

    /// Destractor. If method Commit has not been called explicitly
    /// all modifications made during this transaction will be rollbacked.
    ///
    ~CScopeTransaction();

    /// Finish the editing operation. 
    /// if an instance of IEditSaver interface was attached to the TSE 
    /// of modified objects the method CommitTransaction of that interface
    /// will be called.
    ///
    void Commit();

    /// Undo all made modificatins.
    /// if an instance of IEditSaver interface was attached to the TSE 
    /// of modified objects the method RollBackTransaction of that interface
    /// will be called.
    ///
    void RollBack();

    /// If the editing operation affects objects from another scope
    /// this scope should be added to the current transaction.
    void AddScope(CScope& scope);

private:
    friend class CScope;
    CScopeTransaction(CScope&);

    IScopeTransaction_Impl& x_GetImpl();
    void x_Set(IScopeTransaction_Impl&);

    CRef<CObject> m_Impl;

private:
    // only stack allocation is allowed
    void* operator new(size_t);
    void* operator new[](size_t); 

};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // __OBJMGR__SCOPE_TRANSACTION__HPP
