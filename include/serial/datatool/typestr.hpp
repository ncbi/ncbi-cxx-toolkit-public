#ifndef TYPESTR_HPP
#define TYPESTR_HPP

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
*   C++ class info: includes, used classes, C++ code etc.
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.13  2005/04/26 14:18:50  vasilche
* Allow allocation of objects in CObjectMemoryPool.
*
* Revision 1.12  2004/09/07 14:09:45  grichenk
* Fixed assignment of default value to aliased types
*
* Revision 1.11  2003/10/21 13:48:48  grichenk
* Redesigned type aliases in serialization library.
* Fixed the code (removed CRef-s, added explicit
* initializers etc.)
*
* Revision 1.10  2003/04/29 18:29:34  gouriano
* object data member initialization verification
*
* Revision 1.9  2000/11/07 17:25:31  vasilche
* Added module names to CTypeInfo and CEnumeratedTypeValues
*
* Revision 1.8  2000/07/11 20:36:02  vasilche
* Removed unnecessary generation of namespace references for enum members.
* Removed obsolete methods.
*
* Revision 1.7  2000/06/16 16:31:13  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.6  2000/04/17 19:11:05  vasilche
* Fixed failed assertion.
* Removed redundant namespace specifications.
*
* Revision 1.5  2000/04/12 15:36:41  vasilche
* Added -on <namespace> argument to datatool.
* Removed unnecessary namespace specifications in generated files.
*
* Revision 1.4  2000/04/07 19:26:14  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.3  2000/03/07 20:04:57  vasilche
* Added NewInstance method to generated classes.
*
* Revision 1.2  2000/03/07 14:06:06  vasilche
* Added generation of reference counted objects.
*
* Revision 1.1  2000/02/01 21:46:24  vasilche
* Added CGeneratedChoiceTypeInfo for generated choice classes.
* Removed CMemberInfo subclasses.
* Added support for DEFAULT/OPTIONAL members.
* Changed class generation.
* Moved datatool headers to include/internal/serial/tool.
*
* Revision 1.5  2000/01/10 19:46:47  vasilche
* Fixed encoding/decoding of REAL type.
* Fixed encoding/decoding of StringStore.
* Fixed encoding/decoding of NULL type.
* Fixed error reporting.
* Reduced object map (only classes).
*
* Revision 1.4  1999/12/01 17:36:28  vasilche
* Fixed CHOICE processing.
*
* Revision 1.3  1999/11/16 15:41:17  vasilche
* Added plain pointer choice.
* By default we use C pointer instead of auto_ptr.
* Start adding initializers.
*
* Revision 1.2  1999/11/15 19:36:21  vasilche
* Fixed warnings on GCC
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>

BEGIN_NCBI_SCOPE

class CClassContext;
class CNamespace;

class CTypeStrings {
public:
    CTypeStrings(void);
    virtual ~CTypeStrings(void);

    const string& GetModuleName(void) const
        {
            return m_ModuleName;
        }
    void SetModuleName(const string& name);

    // kind of C++ representation
    enum EKind {
        eKindStd, // standard type
        eKindEnum, // enum
        eKindString, // std::string
        eKindPointer, // plain pointer
        eKindRef, // CRef<>
        eKindObject, // class (CHOICE, SET, SEQUENCE) inherited from CObject
        eKindClass, // any other class (CHOICE, SET, SEQUENCE)
        eKindContainer, // stl container
        eKindOther
    };
    virtual EKind GetKind(void) const = 0;

    virtual string GetCType(const CNamespace& ns) const = 0;
    virtual string GetPrefixedCType(const CNamespace& ns,
                                    const string& methodPrefix) const = 0;
    virtual bool HaveSpecialRef(void) const;
    virtual string GetRef(const CNamespace& ns) const = 0;

    // for external types
    virtual const CNamespace& GetNamespace(void) const;

    // for enum types
    virtual const string& GetEnumName(void) const;

    virtual bool CanBeKey(void) const;
    virtual bool CanBeCopied(void) const;
    virtual bool NeedSetFlag(void) const;

    static void AdaptForSTL(AutoPtr<CTypeStrings>& type);

    virtual string NewInstance(const string& init,
                               const string& place = kEmptyStr) const;

    virtual string GetInitializer(void) const;
    virtual string GetDestructionCode(const string& expr) const;
    virtual string GetIsSetCode(const string& var) const;
    virtual string GetResetCode(const string& var) const;
    virtual string GetDefaultCode(const string& var) const;

    virtual void GenerateCode(CClassContext& ctx) const;
    virtual void GenerateUserHPPCode(CNcbiOstream& out) const;
    virtual void GenerateUserCPPCode(CNcbiOstream& out) const;

    virtual void GenerateTypeCode(CClassContext& ctx) const;
    virtual void GeneratePointerTypeCode(CClassContext& ctx) const;

private:
    string m_ModuleName;
};

END_NCBI_SCOPE

#endif
