#ifndef OBJECTS_OBJMGR_IMPL___TSE_LOADLOCK__HPP
#define OBJECTS_OBJMGR_IMPL___TSE_LOADLOCK__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   CTSE_Lock -- class to lock TSEs from garbage collector
*
*/


#include <corelib/ncbiobj.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/data_source.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CTSE_Info;

class NCBI_XOBJMGR_EXPORT CTSE_LoadLock
{
public:
    CTSE_LoadLock(void)
        {
        }
    ~CTSE_LoadLock(void)
        {
            Reset();
        }

    CTSE_LoadLock(const CTSE_LoadLock& lock)
        {
            *this = lock;
        }
    CTSE_LoadLock& operator=(const CTSE_LoadLock& lock);

    DECLARE_OPERATOR_BOOL_REF(m_Info);

    CTSE_Info& operator*(void)
        {
            return *m_Info;
        }
    const CTSE_Info& operator*(void) const
        {
            return *m_Info;
        }
    CTSE_Info* operator->(void)
        {
            return &*m_Info;
        }
    const CTSE_Info* operator->(void) const
        {
            return &*m_Info;
        }
    
    bool IsLoaded(void) const;
    void SetLoaded(void);

    void Reset(void);
    void ReleaseLoadLock(void);

protected:
    friend class CDataSource;

    //void x_SetLoadLock(CDataSource* ds, CTSE_Info* info);

private:
    CRef<CTSE_Info>     m_Info;
    mutable CRef<CDataSource>   m_DataSource;
    CRef<CObject>       m_LoadLock;
};

/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////

END_SCOPE(objects)
END_NCBI_SCOPE

#endif//OBJECTS_OBJMGR_IMPL___TSE_LOADLOCK__HPP
