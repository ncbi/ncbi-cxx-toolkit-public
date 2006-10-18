#ifndef PTRSTR_HPP
#define PTRSTR_HPP

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
*/

#include <serial/datatool/typestr.hpp>
#include <corelib/ncbiutil.hpp>

BEGIN_NCBI_SCOPE

class CPointerTypeStrings : public CTypeStrings
{
    typedef CTypeStrings CParent;
public:
    CPointerTypeStrings(CTypeStrings* type);
    CPointerTypeStrings(AutoPtr<CTypeStrings> type);
    ~CPointerTypeStrings(void);

    const CTypeStrings* GetDataType(void) const
        {
            return m_DataType.get();
        }

    void GenerateTypeCode(CClassContext& ctx) const;

    EKind GetKind(void) const;

    string GetCType(const CNamespace& ns) const;
    string GetPrefixedCType(const CNamespace& ns,
                            const string& methodPrefix) const;
    string GetRef(const CNamespace& ns) const;

    string GetInitializer(void) const;
    string GetDestructionCode(const string& expr) const;
    string GetIsSetCode(const string& var) const;
    string GetResetCode(const string& var) const;

private:
    AutoPtr<CTypeStrings> m_DataType;
};

class CRefTypeStrings : public CPointerTypeStrings
{
    typedef CPointerTypeStrings CParent;
public:
    CRefTypeStrings(CTypeStrings* type);
    CRefTypeStrings(AutoPtr<CTypeStrings> type);
    ~CRefTypeStrings(void);

    EKind GetKind(void) const;

    string GetCType(const CNamespace& ns) const;
    string GetPrefixedCType(const CNamespace& ns,
                            const string& methodPrefix) const;
    string GetRef(const CNamespace& ns) const;

    string GetInitializer(void) const;
    string GetDestructionCode(const string& expr) const;
    string GetIsSetCode(const string& var) const;
    string GetResetCode(const string& var) const;
};

END_NCBI_SCOPE

#endif
/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.8  2006/10/18 13:04:26  gouriano
* Moved Log to bottom
*
* Revision 1.7  2003/04/29 18:29:34  gouriano
* object data member initialization verification
*
* Revision 1.6  2000/08/25 15:58:47  vasilche
* Renamed directory tool -> datatool.
*
* Revision 1.5  2000/07/11 20:36:01  vasilche
* Removed unnecessary generation of namespace references for enum members.
* Removed obsolete methods.
*
* Revision 1.4  2000/06/16 16:31:13  vasilche
* Changed implementation of choices and classes info to allow use of the same classes in generated and user written classes.
*
* Revision 1.3  2000/04/17 19:11:05  vasilche
* Fixed failed assertion.
* Removed redundant namespace specifications.
*
* Revision 1.2  2000/04/07 19:26:11  vasilche
* Added namespace support to datatool.
* By default with argument -oR datatool will generate objects in namespace
* NCBI_NS_NCBI::objects (aka ncbi::objects).
* Datatool's classes also moved to NCBI namespace.
*
* Revision 1.1  2000/03/07 14:06:05  vasilche
* Added generation of reference counted objects.
*
* ===========================================================================
*/
