#ifndef CHOICEPTR__HPP
#define CHOICEPTR__HPP

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
* Revision 1.17  2000/07/03 18:42:32  vasilche
* Added interface to typeinfo via CObjectInfo and CConstObjectInfo.
* Reduced header dependency.
*
* Revision 1.16  2000/06/16 16:31:03  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.15  2000/06/01 19:06:55  vasilche
* Added parsing of XML data.
*
* Revision 1.14  2000/05/24 20:08:11  vasilche
* Implemented XML dump.
*
* Revision 1.13  2000/03/29 15:55:18  vasilche
* Added two versions of object info - CObjectInfo and CConstObjectInfo.
* Added generic iterators by class -
* 	CTypeIterator<class>, CTypeConstIterator<class>,
* 	CStdTypeIterator<type>, CStdTypeConstIterator<type>,
* 	CObjectsIterator and CObjectsConstIterator.
*
* Revision 1.12  2000/03/07 14:05:28  vasilche
* Added stream buffering to ASN.1 binary input.
* Optimized class loading/storing.
* Fixed bugs in processing OPTIONAL fields.
*
* Revision 1.11  2000/02/17 20:02:27  vasilche
* Added some standard serialization exceptions.
* Optimized text/binary ASN.1 reading.
* Fixed wrong encoding of StringStore in ASN.1 binary format.
* Optimized logic of object collection.
*
* Revision 1.10  2000/01/05 19:43:43  vasilche
* Fixed error messages when reading from ASN.1 binary file.
* Fixed storing of integers with enumerated values in ASN.1 binary file.
* Added TAG support to key/value of map.
* Added support of NULL variant in CHOICE.
*
* Revision 1.9  1999/12/17 19:04:52  vasilche
* Simplified generation of GetTypeInfo methods.
*
* Revision 1.8  1999/12/01 17:36:20  vasilche
* Fixed CHOICE processing.
*
* Revision 1.7  1999/11/18 20:16:12  vakatov
* Included <serial/typeref.hpp> to fix for the CodeWarrior(MAC) C++ compiler
*
* Revision 1.6  1999/11/16 15:40:13  vasilche
* Added plain pointer choice.
*
* Revision 1.5  1999/10/28 15:37:37  vasilche
* Fixed null choice pointers handling.
* Cleaned enumertion interface.
*
* Revision 1.4  1999/10/25 19:07:12  vasilche
* Fixed coredump on non initialized choices.
* Fixed compilation warning.
*
* Revision 1.3  1999/09/22 20:11:48  vasilche
* Modified for compilation on IRIX native c++ compiler.
*
* Revision 1.2  1999/09/14 18:54:02  vasilche
* Fixed bugs detected by gcc & egcs.
* Removed unneeded includes.
*
* Revision 1.1  1999/09/07 20:57:43  vasilche
* Forgot to add some files.
*
* ===========================================================================
*/

#include <corelib/ncbiutil.hpp>
#include <serial/typeref.hpp>
#include <serial/ptrinfo.hpp>
#include <serial/stdtypes.hpp>
#include <serial/memberlist.hpp>
#include <vector>
#include <map>
#include <memory>

BEGIN_NCBI_SCOPE

// CTypeInfo for pointers which behavelike CHOICE (select one of limited choices)
class CChoicePointerTypeInfo : public CPointerTypeInfo
{
    typedef CPointerTypeInfo CParent;
public:
    typedef CMembers::TMemberIndex TMemberIndex;
    typedef vector<CTypeRef> TVariantTypes;
    typedef map<const type_info*, TMemberIndex, CLessTypeInfo> TVariantsByType;

    CChoicePointerTypeInfo(TTypeInfo typeInfo);
#ifdef TYPENAME
    CChoicePointerTypeInfo(const string& name, TTypeInfo typeInfo);
    CChoicePointerTypeInfo(const char* name, TTypeInfo typeInfo);
    CChoicePointerTypeInfo(const char* name, TTypeInfo arg,
                           TTypeInfo typeInfo);
    CChoicePointerTypeInfo(const char* name, const char* arg,
                           TTypeInfo typeInfo);
#endif
    ~CChoicePointerTypeInfo(void);

    static TTypeInfo GetTypeInfo(TTypeInfo base);

    const CMembersInfo& GetVariants(void) const
        {
            return m_Variants;
        }
    TTypeInfo GetVariantType(TMemberIndex index) const;

protected:
    virtual TTypeInfo GetRealDataTypeInfo(TConstObjectPtr object) const;

    virtual void WriteData(CObjectOStream& out, TConstObjectPtr obejct) const;

    virtual void ReadData(CObjectIStream& in, TObjectPtr object) const;

    void SkipData(CObjectIStream& in) const;

private:
    void Init(void);
    const TVariantsByType& VariantsByType(void) const;
    TMemberIndex FindVariant(TConstObjectPtr object) const;

    CMembersInfo m_Variants;
    mutable auto_ptr<TVariantsByType> m_VariantsByType;
};

template<typename Data>
class CChoiceAutoPtrTypeInfo : public CChoicePointerTypeInfo
{
    typedef CChoicePointerTypeInfo CParent;
public:
    typedef Data TDataType;
    typedef AutoPtr<TDataType> TObjectType;

    CChoiceAutoPtrTypeInfo<Data>(TTypeInfo type)
        : CParent(type)
        { }
    
    TConstObjectPtr x_GetObjectPointer(TConstObjectPtr object) const
        {
            return static_cast<const TObjectType*>(object)->get();
        }
    void SetObjectPointer(TObjectPtr object, TObjectPtr data) const
        {
            static_cast<TObjectType*>(object)->
                reset(static_cast<TDataType*>(data));
        }

    virtual size_t GetSize(void) const
        {
            return sizeof(TObjectType);
        }

    static TTypeInfo GetTypeInfo(TTypeInfo type)
        {
            return new CChoiceAutoPtrTypeInfo<Data>(type);
        }
};

class CNullTypeInfo : public CVoidTypeInfo
{
    typedef CVoidTypeInfo CParent;
public:
    CNullTypeInfo(void);
    ~CNullTypeInfo(void);

    static TTypeInfo GetTypeInfo(void);

    TObjectPtr Create(void) const;

protected:
    void WriteData(CObjectOStream& out, TConstObjectPtr obejct) const;

    void ReadData(CObjectIStream& in, TObjectPtr object) const;

    void SkipData(CObjectIStream& in) const;
};

//#include <serial/choiceptr.inl>

END_NCBI_SCOPE

#endif  /* CHOICEPTR__HPP */
