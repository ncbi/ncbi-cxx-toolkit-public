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
#include <set>
#include <map>

USING_NCBI_SCOPE;

class CClassCode;

class CTypeStrings {
public:
    typedef set<string> TIncludes;
    typedef map<string, string> TForwards;

    enum ETypeType {
        eStdType,
        eStringType,
        eEnumType,
        eClassType,
        ePointerType,
        eTemplateType,
        ePointerTemplateType
    };
    void SetStd(const string& c, bool stringType = false);
    void SetStd(const string& c, const string& m, bool stringType = false);
    void SetEnum(const string& c, const string& e);
    void SetClass(const string& c);
    void SetTemplate(const string& c, const string& m);
    void SetTemplate(const string& c, const string& m,
                     const CTypeStrings& arg);
    void SetTemplate(const string& c, const string& m,
                     const CTypeStrings& arg1, const CTypeStrings& arg2);
    void SetChoice(void);

    bool CanBeKey(void) const;
    bool CanBeInSTL(void) const;
    void ToPointer(void);
    void ToPointer(const string& c, const string& m, bool simplePointer);
        
    ETypeType GetType(void) const
        {
            return m_Type;
        }
    const string& GetCType(void) const
        {
            return m_CType;
        }
    const string& GetMacro(void) const
        {
            return m_Macro;
        }
    bool IsChoice(void) const
        {
            return m_Choice;
        }

    string GetRef(void) const;
    string GetInitializer(void) const;
    string GetDestructorCode(void) const;

    void AddHPPInclude(const string& s)
        {
            m_HPPIncludes.insert(s);
        }
    void AddCPPInclude(const string& s)
        {
            m_CPPIncludes.insert(s);
        }
    void AddForwardDeclaration(const string& s, const string& ns)
        {
            m_ForwardDeclarations[s] = ns;
        }
    void AddIncludes(const CTypeStrings& arg);

    void AddMember(CClassCode& code,
                   const string& member) const;
    void AddMember(CClassCode& code,
                   const string& name, const string& member) const;
private:
    void x_AddMember(CClassCode& code,
                     const string& name, const string& member) const;

    ETypeType m_Type;
    string m_CType;
    string m_Macro;
    bool m_Choice;

    TIncludes m_HPPIncludes;
    TIncludes m_CPPIncludes;
    TForwards m_ForwardDeclarations;
};

#endif
