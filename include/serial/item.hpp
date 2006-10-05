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
*/

#include <corelib/ncbistd.hpp>
//#include <serial/serialdef.hpp>
#include <serial/serialutil.hpp>
#include <serial/typeref.hpp>
#include <serial/memberid.hpp>


/** @addtogroup FieldsComplex
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CItemsInfo;

class NCBI_XSERIAL_EXPORT CItemInfo
{
public:
    enum {
        eNoOffset = -1
    };

    CItemInfo(const CMemberId& id, TPointerOffsetType offset,
              TTypeInfo type);
    CItemInfo(const CMemberId& id, TPointerOffsetType offset,
              const CTypeRef& type);
    CItemInfo(const char* id, TPointerOffsetType offset,
              TTypeInfo type);
    CItemInfo(const char* id, TPointerOffsetType offset,
              const CTypeRef& type);
    virtual ~CItemInfo(void);

    const CMemberId& GetId(void) const;
    CMemberId& GetId(void);

    TMemberIndex GetIndex(void) const;

    TPointerOffsetType GetOffset(void) const;

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
    TPointerOffsetType m_Offset;
    // type of member
    CTypeRef m_Type;
};


/* @} */


#include <serial/impl/item.inl>

END_NCBI_SCOPE

#endif  /* ITEM__HPP */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.9  2006/10/05 19:23:04  gouriano
* Some headers moved into impl
*
* Revision 1.8  2003/04/15 14:15:19  siyan
* Added doxygen support
*
* Revision 1.7  2003/04/10 20:13:37  vakatov
* Rollback the "uninitialized member" verification -- it still needs to
* be worked upon...
*
* Revision 1.5  2003/03/26 16:13:32  vasilche
* Removed TAB symbols. Some formatting.
*
* Revision 1.4  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.3  2000/10/13 20:22:45  vasilche
* Fixed warnings on 64 bit compilers.
* Fixed missing typename in templates.
*
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
