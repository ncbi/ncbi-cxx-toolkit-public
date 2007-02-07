#ifndef PATHHOOK__HPP
#define PATHHOOK__HPP

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
* Author: Andrei Gourianov
*
* File Description:
*   Helper classes to set serialization hooks by stack path
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>


/** @addtogroup ObjStreamSupport
 *
 * @{
 */

BEGIN_NCBI_SCOPE

class CItemInfo;
class CObjectStack;
class CTypeInfo;

class NCBI_XSERIAL_EXPORT CPathHook
    : protected multimap<CObjectStack*, pair<string, CRef<CObject> > >
{
public:
    CPathHook(void);
    ~CPathHook(void);

    bool     IsEmpty(void) const {return m_Empty;}
    bool     SetHook(CObjectStack* stk, const string& path, CObject* hook);
    CObject* GetHook(CObjectStack& stk) const;

    static bool Match(const string& mask, const string& path);

private:
    CObject* x_Get(CObjectStack& stk, const string& path) const;
    bool m_Empty;
    bool m_Regular;
    bool m_All;
    bool m_Wildcard;
};


class NCBI_XSERIAL_EXPORT CStreamPathHookBase
    : protected map<string,CRef<CObject> >
{
public:
    CStreamPathHookBase(void);
    ~CStreamPathHookBase(void);

    bool     IsEmpty(void) const {return m_Empty;}
    bool     SetHook(const string& path, CObject* hook);
    CObject* GetHook(CObjectStack& stk) const;

    static CTypeInfo* FindType(const CObjectStack& stk);
    static CItemInfo* FindItem(const CObjectStack& stk);
private:
    CObject* x_Get(const string& path) const;
    bool m_Empty;
    bool m_Regular;
    bool m_All;
    bool m_Member;
    bool m_Wildcard;
};


template <typename TInfo, typename THook>
class CStreamPathHook : public CStreamPathHookBase
{
public:
    void SetHook(const string& path, THook hook)
    {
        CStreamPathHookBase::SetHook(path,hook);
    }
    THook GetHook(CObjectStack& stk) const
    {
        return static_cast<THook>(CStreamPathHookBase::GetHook(stk));
    }
    static TInfo FindItem(const CObjectStack& stk)
    {
        return dynamic_cast<TInfo>(CStreamPathHookBase::FindItem(stk));
    } 
};


template <typename THook>
class CStreamObjectPathHook : public CStreamPathHookBase
{
public:
    void SetHook(const string& path, THook hook)
    {
        CStreamPathHookBase::SetHook(path,hook);
    }
    THook GetHook(CObjectStack& stk) const
    {
        return static_cast<THook>(CStreamPathHookBase::GetHook(stk));
    }
};

END_NCBI_SCOPE

#endif  /* PATHHOOK__HPP */


/* @} */


/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2004/08/17 14:38:47  dicuccio
* Added export specifiers
*
* Revision 1.1  2004/01/05 14:24:09  gouriano
* Added possibility to set serialization hooks by stack path
*
*
* ===========================================================================
*/
