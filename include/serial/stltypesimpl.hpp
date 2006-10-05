#ifndef STLTYPESIMPL__HPP
#define STLTYPESIMPL__HPP

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
#include <serial/serialdef.hpp>
#include <serial/typeref.hpp>
#include <serial/continfo.hpp>
#include <serial/memberid.hpp>


/** @addtogroup TypeInfoCPP
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class NCBI_XSERIAL_EXPORT CStlClassInfoUtil
{
public:
    static TTypeInfo Get_auto_ptr(TTypeInfo arg, TTypeInfoCreator1 f);
    static TTypeInfo Get_CRef(TTypeInfo arg, TTypeInfoCreator1 f);
    static TTypeInfo Get_CConstRef(TTypeInfo arg, TTypeInfoCreator1 f);
    static TTypeInfo Get_AutoPtr(TTypeInfo arg, TTypeInfoCreator1 f);
    static TTypeInfo Get_list(TTypeInfo arg, TTypeInfoCreator1 f);
    static TTypeInfo GetSet_list(TTypeInfo arg, TTypeInfoCreator1 f);
    static TTypeInfo Get_vector(TTypeInfo arg, TTypeInfoCreator1 f);
    static TTypeInfo GetSet_vector(TTypeInfo arg, TTypeInfoCreator1 f);
    static TTypeInfo Get_set(TTypeInfo arg, TTypeInfoCreator1 f);
    static TTypeInfo Get_multiset(TTypeInfo arg, TTypeInfoCreator1 f);
    static TTypeInfo Get_map(TTypeInfo arg1, TTypeInfo arg2,
                             TTypeInfoCreator2 f);
    static TTypeInfo Get_multimap(TTypeInfo arg1, TTypeInfo arg2,
                                  TTypeInfoCreator2 f);
    static TTypeInfo GetInfo(TTypeInfo& storage,
                             TTypeInfo arg, TTypeInfoCreator1 f);
    static TTypeInfo GetInfo(TTypeInfo& storage,
                             TTypeInfo arg1, TTypeInfo arg2,
                             TTypeInfoCreator2 f);

    // throw exceptions
    static void ThrowDuplicateElementError(void);
    static void CannotGetElementOfSet(void);
};

class NCBI_XSERIAL_EXPORT CStlOneArgTemplate : public CContainerTypeInfo
{
    typedef CContainerTypeInfo CParent;
public:
    CStlOneArgTemplate(size_t size, TTypeInfo dataType,
                       bool randomOrder, const string& name);
    CStlOneArgTemplate(size_t size, TTypeInfo dataType,
                       bool randomOrder);
    CStlOneArgTemplate(size_t size, const CTypeRef& dataType,
                       bool randomOrder);

    const CMemberId& GetDataId(void) const
        {
            return m_DataId;
        }
    void SetDataId(const CMemberId& id);

    bool IsDefault(TConstObjectPtr objectPtr) const;
    void SetDefault(TObjectPtr objectPtr) const;

    // private use
    typedef bool (*TIsDefaultFunction)(TConstObjectPtr objectPtr);
    typedef void (*TSetDefaultFunction)(TObjectPtr objectPtr);

    void SetMemFunctions(TTypeCreate create,
                         TIsDefaultFunction isDefault,
                         TSetDefaultFunction setDefault);

private:
    TIsDefaultFunction m_IsDefault;
    TSetDefaultFunction m_SetDefault;

    CMemberId m_DataId;
};

class NCBI_XSERIAL_EXPORT CStlTwoArgsTemplate : public CStlOneArgTemplate
{
    typedef CStlOneArgTemplate CParent;
public:
    CStlTwoArgsTemplate(size_t size,
                        TTypeInfo keyType, TPointerOffsetType keyOffset,
                        TTypeInfo valueType, TPointerOffsetType valueOffset,
                        bool randomOrder);
    CStlTwoArgsTemplate(size_t size,
                        const CTypeRef& keyType, TPointerOffsetType keyOffset,
                        const CTypeRef& valueType, TPointerOffsetType valueOffset,
                        bool randomOrder);

    const CMemberId& GetKeyId(void) const
        {
            return m_KeyId;
        }
    const CMemberId& GetValueId(void) const
        {
            return m_ValueId;
        }
    void SetKeyId(const CMemberId& id);
    void SetValueId(const CMemberId& id);

private:
    CMemberId m_KeyId;
    CTypeRef m_KeyType;
    TPointerOffsetType m_KeyOffset;

    CMemberId m_ValueId;
    CTypeRef m_ValueType;
    TPointerOffsetType m_ValueOffset;

    static TTypeInfo CreateElementTypeInfo(TTypeInfo info);
};

//#include <serial/impl/stltypesimpl.inl>

END_NCBI_SCOPE

#endif  /* STLTYPESIMPL__HPP */


/* @} */


/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.10  2006/10/05 19:23:04  gouriano
* Some headers moved into impl
*
* Revision 1.9  2005/11/21 16:18:06  vasilche
* Implemented serialization of set and map with custom comparator.
*
* Revision 1.8  2004/04/02 16:57:35  gouriano
* made it possible to create named CTypeInfo for containers
*
* Revision 1.7  2003/07/22 21:47:04  vasilche
* Added SET OF implemented as vector<>.
*
* Revision 1.6  2003/04/15 16:19:04  siyan
* Added doxygen support
*
* Revision 1.5  2002/12/23 18:38:51  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.4  2001/09/04 14:08:27  ucko
* Handle CConstRef analogously to CRef in type macros
*
* Revision 1.3  2000/11/07 17:25:13  vasilche
* Fixed encoding of XML:
*     removed unnecessary apostrophes in OCTET STRING
*     removed unnecessary content in NULL
* Added module names to CTypeInfo and CEnumeratedTypeValues
*
* Revision 1.2  2000/10/13 20:22:47  vasilche
* Fixed warnings on 64 bit compilers.
* Fixed missing typename in templates.
*
* Revision 1.1  2000/10/13 16:28:33  vasilche
* Reduced header dependency.
* Avoid use of templates with virtual methods.
* Reduced amount of different maps used.
* All this lead to smaller compiled code size (libraries and programs).
*
* ===========================================================================
*/
