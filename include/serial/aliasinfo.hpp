#ifndef ALIASINFO__HPP
#define ALIASINFO__HPP

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
*   Alias type info
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.7  2004/06/07 13:59:30  gouriano
* Corrected reading of class aliases (set proper Create() function)
*
* Revision 1.6  2004/04/30 13:28:39  gouriano
* Remove obsolete function declarations
*
* Revision 1.5  2004/03/25 15:56:27  gouriano
* Added possibility to copy and compare serial object non-recursively
*
* Revision 1.4  2003/12/08 22:14:40  grichenk
* Fixed CAliasTypeInfo::IsType()
*
* Revision 1.3  2003/11/24 14:10:03  grichenk
* Changed base class for CAliasTypeInfo to CPointerTypeInfo
*
* Revision 1.2  2003/10/21 21:08:45  grichenk
* Fixed aliases-related bug in XML stream
*
* Revision 1.1  2003/10/21 13:45:22  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#include <serial/ptrinfo.hpp>
#include <serial/typeref.hpp>


BEGIN_NCBI_SCOPE

class NCBI_XSERIAL_EXPORT CAliasTypeInfo : public CPointerTypeInfo
{
    typedef CPointerTypeInfo CParent;
public:
    CAliasTypeInfo(const string& name, TTypeInfo type);

    bool IsDefault(TConstObjectPtr object) const;
    bool Equals(TConstObjectPtr object1, TConstObjectPtr object2,
                ESerialRecursionMode how = eRecursive) const;
    void SetDefault(TObjectPtr dst) const;
    void Assign(TObjectPtr dst, TConstObjectPtr src,
                ESerialRecursionMode how = eRecursive) const;

    void Delete(TObjectPtr object) const;
    void DeleteExternalObjects(TObjectPtr object) const;

    const CObject* GetCObjectPtr(TConstObjectPtr objectPtr) const;
    TTypeInfo GetRealTypeInfo(TConstObjectPtr object) const;

    bool IsParentClassOf(const CClassTypeInfo* classInfo) const;

    void SetDataOffset(TPointerOffsetType offset);
    TObjectPtr GetDataPtr(TObjectPtr objectPtr) const;
    TConstObjectPtr GetDataPtr(TConstObjectPtr objectPtr) const;

    void SetCreateFunction(TTypeCreate func)
        {
            CParent::SetCreateFunction(func);
        }

protected:
    static TObjectPtr GetDataPointer(const CPointerTypeInfo* objectType,
                                     TObjectPtr objectPtr);
    static void SetDataPointer(const CPointerTypeInfo* objectType,
                               TObjectPtr objectPtr,
                               TObjectPtr dataPtr);

    TPointerOffsetType m_DataOffset;

    friend class CAliasTypeInfoFunctions;
private:
    void InitAliasTypeInfoFunctions(void);
};


END_NCBI_SCOPE

#endif  /* TYPEDEFINFO__HPP */
