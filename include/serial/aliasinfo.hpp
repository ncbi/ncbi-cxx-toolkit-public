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
* Revision 1.2  2003/10/21 21:08:45  grichenk
* Fixed aliases-related bug in XML stream
*
* Revision 1.1  2003/10/21 13:45:22  grichenk
* Initial revision
*
*
* ===========================================================================
*/

#include <serial/typeinfo.hpp>
#include <serial/typeref.hpp>


BEGIN_NCBI_SCOPE

class NCBI_XSERIAL_EXPORT CAliasTypeInfo : public CTypeInfo
{
    typedef CTypeInfo CParent;
public:
    CAliasTypeInfo(const string& name, TTypeInfo type);

    static TTypeInfo GetTypeInfo(TTypeInfo base);

    TTypeInfo GetReferencedType(void) const;
    
    const string& GetModuleName(void) const;
    bool MayContainType(TTypeInfo type) const;

    bool IsDefault(TConstObjectPtr object) const;
    bool Equals(TConstObjectPtr object1,
                        TConstObjectPtr object2) const;
    void SetDefault(TObjectPtr dst) const;
    void Assign(TObjectPtr dst, TConstObjectPtr src) const;

    void Delete(TObjectPtr object) const;
    void DeleteExternalObjects(TObjectPtr object) const;

    const CObject* GetCObjectPtr(TConstObjectPtr objectPtr) const;
    TTypeInfo GetRealTypeInfo(TConstObjectPtr object) const;

    bool IsParentClassOf(const CClassTypeInfo* classInfo) const;
    bool IsType(TTypeInfo type) const;

    void SetDataOffset(TPointerOffsetType offset);
    TObjectPtr GetDataPtr(TObjectPtr objectPtr) const;
    TConstObjectPtr GetDataPtr(TConstObjectPtr objectPtr) const;

    TTypeInfo GetRefTypeInfo(void) const;

protected:
    CTypeRef m_DataTypeRef;
    TPointerOffsetType m_DataOffset;

    friend class CAliasTypeInfoFunctions;
private:
    void InitAliasTypeInfoFunctions(void);
};


END_NCBI_SCOPE

#endif  /* TYPEDEFINFO__HPP */
