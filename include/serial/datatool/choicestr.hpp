#ifndef CHOICESTR_HPP
#define CHOICESTR_HPP

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
* Revision 1.16  2005/04/26 14:18:50  vasilche
* Allow allocation of objects in CObjectMemoryPool.
*
* Revision 1.15  2003/11/20 14:32:11  gouriano
* changed generated C++ code so NULL data types have no value
*
* Revision 1.14  2002/11/19 19:47:50  gouriano
* added support of XML attributes of choice variants
*
* Revision 1.13  2002/11/14 21:07:11  gouriano
* added support of XML attribute lists
*
* Revision 1.12  2002/10/15 13:53:08  gouriano
* use "noprefix" flag
*
* Revision 1.11  2001/06/11 14:34:58  grichenk
* Added support for numeric tags in ASN.1 specifications and data streams.
*
* Revision 1.10  2000/08/25 15:58:45  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.9  2000/06/16 16:31:12  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.8  2000/05/03 14:38:10  vasilche
* SERIAL: added support for delayed reading to generated classes.
* DATATOOL: added code generation for delayed reading.
*
* Revision 1.7  2000/04/17 19:11:04  vasilche
* Fixed failed assertion.
* Removed redundant namespace specifications.
*
* Revision 1.6  2000/04/12 15:36:39  vasilche
* Added -on <namespace> argument to datatool.
* Removed unnecessary namespace specifications in generated files.
*
* Revision 1.5  2000/04/07 19:26:07  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.4  2000/03/17 16:47:38  vasilche
* Added copyright message to generated files.
* All objects pointers in choices now share the only CObject pointer.
*
* Revision 1.3  2000/03/07 14:06:04  vasilche
* Added generation of reference counted objects.
*
* Revision 1.2  2000/02/11 17:09:26  vasilche
* Removed unneeded flags.
*
* Revision 1.1  2000/02/01 21:46:14  vasilche
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

#include <serial/datatool/typestr.hpp>
#include <serial/datatool/classstr.hpp>
#include <corelib/ncbiutil.hpp>

BEGIN_NCBI_SCOPE

class CChoiceTypeStrings : public CClassTypeStrings
{
    typedef CClassTypeStrings CParent;
public:
    enum EMemberType {
        eSimpleMember,
        eStringMember,
        ePointerMember,
        eObjectPointerMember,
        eBufferMember
    };
    struct SVariantInfo {
        string externalName;
        string cName;
        EMemberType memberType;
        AutoPtr<CTypeStrings> type;
        bool delayed;
        bool in_union;
        int memberTag;
        bool noPrefix;
        bool attlist;
        bool noTag;
        bool simple;
        const CDataType* dataType;

        SVariantInfo(const string& name, const AutoPtr<CTypeStrings>& type,
                     bool delayed, bool in_union,
                     int tag, bool noPrefx, bool attlst, bool noTg,
                     bool simpl, const CDataType* dataTp);
    };
    typedef list<SVariantInfo> TVariants;

    CChoiceTypeStrings(const string& externalName, const string& className);
    ~CChoiceTypeStrings(void);

    bool HaveAssignment(void) const
        {
            return m_HaveAssignment;
        }

    void AddVariant(const string& name, const AutoPtr<CTypeStrings>& type,
                    bool delayed, bool in_union, int tag,
                    bool noPrefix, bool attlist,
                    bool noTag, bool simple, const CDataType* dataType);

protected:
    void GenerateClassCode(CClassCode& code,
                           CNcbiOstream& getters,
                           const string& methodPrefix,
                           bool haveUserClass,
                           const string& classPrefix) const;
    bool x_IsNullType(TVariants::const_iterator i) const;
    bool x_IsNullWithAttlist(TVariants::const_iterator i) const;

private:
    TVariants m_Variants;
    bool m_HaveAssignment;
};

class CChoiceRefTypeStrings : public CClassRefTypeStrings
{
    typedef CClassRefTypeStrings CParent;
public:
    CChoiceRefTypeStrings(const string& className, const CNamespace& ns,
                          const string& fileName);

};

END_NCBI_SCOPE

#endif
