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

#include <serial/tool/typestr.hpp>
#include <serial/tool/classstr.hpp>
#include <corelib/ncbiutil.hpp>

class CChoiceTypeStrings : public CClassTypeStrings
{
    typedef CClassTypeStrings CParent;
public:
    enum EMemberType {
        eSimpleMember,
        eStringMember,
        ePointerMember
    };
    struct SVariantInfo {
        string externalName;
        string cName;
        EMemberType memberType;
        string memberRef;
        AutoPtr<CTypeStrings> type;
        string cType;

        SVariantInfo(const string& name, AutoPtr<CTypeStrings> type);
    };
    typedef list<SVariantInfo> TVariants;

    CChoiceTypeStrings(const string& externalName, const string& className);
    ~CChoiceTypeStrings(void);

    void AddVariant(const string& name, AutoPtr<CTypeStrings> type);

protected:
    void GenerateClassCode(CClassCode& code,
                           CNcbiOstream& getters,
                           const string& methodPrefix,
                           const string& codeClassName) const;

private:
    TVariants m_Variants;
};

class CChoiceRefTypeStrings : public CClassRefTypeStrings
{
    typedef CClassRefTypeStrings CParent;
public:
    CChoiceRefTypeStrings(const string& className,
                          const string& namespaceName,
                          const string& fileName);
};

#endif
