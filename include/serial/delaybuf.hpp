#ifndef DELAYBUF__HPP
#define DELAYBUF__HPP

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
#include <corelib/ncbiobj.hpp>
#include <serial/serialdef.hpp>
#include <memory>


/** @addtogroup UserCodeSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

class CByteSource;
class CItemInfo;

class NCBI_XSERIAL_EXPORT CDelayBuffer
{
public:
    CDelayBuffer(void)
        {
        }
    ~CDelayBuffer(void);

    bool Delayed(void) const
        {
            return m_Info.get() != 0;
        }

    DECLARE_OPERATOR_BOOL_PTR(m_Info.get());

    void Forget(void);
    
    void Update(void)
        {
            if ( Delayed() )
                DoUpdate();
        }

    bool HaveFormat(ESerialDataFormat format) const
        {
            const SInfo* info = m_Info.get();
            return info && info->m_DataFormat == format;
        }
    CByteSource& GetSource(void) const
        {
            return *m_Info->m_Source;
        }

    TMemberIndex GetIndex(void) const;

    void SetData(const CItemInfo* itemInfo, TObjectPtr object,
                 ESerialDataFormat dataFormat, CByteSource& data);

private:
    struct SInfo
    {
    public:
        SInfo(const CItemInfo* itemInfo, TObjectPtr object,
              ESerialDataFormat dataFormat, CByteSource& source);
        ~SInfo(void);

        // member info
        const CItemInfo* m_ItemInfo;
        // main object
        TObjectPtr m_Object;
        // data format
        ESerialDataFormat m_DataFormat;
        // data source
        mutable CRef<CByteSource> m_Source;
    };

    // private method declarations to prevent implicit generation by compiler
    CDelayBuffer(const CDelayBuffer&);
    CDelayBuffer& operator==(const CDelayBuffer&);
    static void* operator new(size_t);

    void DoUpdate(void);

    auto_ptr<SInfo> m_Info;
};


/* @} */


END_NCBI_SCOPE

#endif  /* DELAYBUF__HPP */



/* ---------------------------------------------------------------------------
* $Log$
* Revision 1.11  2006/10/12 15:08:23  gouriano
* Some header files moved into impl
*
* Revision 1.10  2006/10/05 19:23:04  gouriano
* Some headers moved into impl
*
* Revision 1.9  2005/01/24 17:05:48  vasilche
* Safe boolean operators.
*
* Revision 1.8  2003/04/15 14:15:07  siyan
* Added doxygen support
*
* Revision 1.7  2002/12/23 18:38:50  dicuccio
* Added WIn32 export specifier: NCBI_XSERIAL_EXPORT.
* Moved all CVS logs to the end.
*
* Revision 1.6  2002/11/04 21:28:59  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.5  2000/09/18 20:00:01  vasilche
* Separated CVariantInfo and CMemberInfo.
* Implemented copy hooks.
* All hooks now are stored in CTypeInfo/CMemberInfo/CVariantInfo.
* Most type specific functions now are implemented via function pointers instead of virtual functions.
*
* Revision 1.4  2000/08/15 19:44:38  vasilche
* Added Read/Write hooks:
* CReadObjectHook/CWriteObjectHook for objects of specified type.
* CReadClassMemberHook/CWriteClassMemberHook for specified members.
* CReadChoiceVariantHook/CWriteChoiceVariant for specified choice variants.
* CReadContainerElementHook/CWriteContainerElementsHook for containers.
*
* Revision 1.3  2000/06/16 16:31:04  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.2  2000/06/01 19:06:55  vasilche
* Added parsing of XML data.
*
* Revision 1.1  2000/04/28 16:58:01  vasilche
* Added classes CByteSource and CByteSourceReader for generic reading.
* Added delayed reading of choice variants.
*
* ===========================================================================
*/
