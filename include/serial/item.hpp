#ifndef ITEM__HPP
#define ITEM__HPP

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
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2000/10/03 17:22:32  vasilche
* Reduced header dependency.
* Reduced size of debug libraries on WorkShop by 3 times.
* Fixed tag allocation for parent classes.
* Fixed CObject allocation/deallocation in streams.
* Moved instantiation of several templates in separate source file.
*
* Revision 1.1  2000/09/18 20:00:01  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/serialdef.hpp>
#include <serial/serialutil.hpp>
#include <serial/typeref.hpp>
#include <serial/memberid.hpp>

BEGIN_NCBI_SCOPE

class CItemsInfo;

class CItemInfo
{
public:
	typedef size_t TOffset;
    enum {
		eNoOffset = -1
	};

    CItemInfo(const CMemberId& id, TOffset offset, TTypeInfo type);
    CItemInfo(const CMemberId& id, TOffset offset, const CTypeRef& type);
    CItemInfo(const char* id, TOffset offset, TTypeInfo type);
    CItemInfo(const char* id, TOffset offset, const CTypeRef& type);
    virtual ~CItemInfo(void);

    const CMemberId& GetId(void) const;
    CMemberId& GetId(void);

    TMemberIndex GetIndex(void) const;

    TOffset GetOffset(void) const;

    TTypeInfo GetTypeInfo(void) const;

    virtual void UpdateDelayedBuffer(CObjectIStream& in,
                                     TObjectPtr classPtr) const = 0;

    TObjectPtr GetItemPtr(TObjectPtr object) const;
    TConstObjectPtr GetItemPtr(TConstObjectPtr object) const;

private:
    friend class CItemsInfo;

    // member ID
    CMemberId m_Id;
    // member index
    TMemberIndex m_Index;
    // offset of member inside object
    TOffset m_Offset;
    // type of member
    CTypeRef m_Type;
};

#include <serial/item.inl>

END_NCBI_SCOPE

#endif  /* ITEM__HPP */
