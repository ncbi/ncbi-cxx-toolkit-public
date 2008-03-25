#ifndef UNITYPE_HPP
#define UNITYPE_HPP

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
*   TYpe definition of 'SET OF' and 'SEQUENCE OF'
*
*/

#include "type.hpp"

BEGIN_NCBI_SCOPE

class CUniSequenceDataType : public CDataType {
    typedef CDataType CParent;
public:
    CUniSequenceDataType(const AutoPtr<CDataType>& elementType);

    void PrintASN(CNcbiOstream& out, int indent) const;
    void PrintSpecDumpExtra(CNcbiOstream& out, int indent) const;
    void PrintXMLSchema(CNcbiOstream& out, int indent, bool contents_only=false) const;
    void PrintDTDElement(CNcbiOstream& out, bool contents_only=false) const;
    void PrintDTDExtra(CNcbiOstream& out) const;

    void FixTypeTree(void) const;
    bool CheckType(void) const;
    bool CheckValue(const CDataValue& value) const;
    TObjectPtr CreateDefault(const CDataValue& value) const;

    CDataType* GetElementType(void)
        {
            return m_ElementType.get();
        }
    const CDataType* GetElementType(void) const
        {
            return m_ElementType.get();
        }
    void SetElementType(const AutoPtr<CDataType>& type);

    CTypeInfo* CreateTypeInfo(void);
    bool NeedAutoPointer(const CTypeInfo* typeInfo) const;
    
    AutoPtr<CTypeStrings> GetFullCType(void) const;
    virtual const char* GetASNKeyword(void) const;
    virtual string      GetSpecKeyword(void) const;
    virtual const char* GetDEFKeyword(void) const;

    bool IsNonEmpty(void) const
        {
            return m_NonEmpty;
        }
    void SetNonEmpty(bool nonEmpty)
        {
            m_NonEmpty = nonEmpty;
        }
    bool IsNoPrefix(void) const
        {
            return m_NoPrefix;
        }
    void SetNoPrefix(bool noprefix)
        {
            m_NoPrefix = noprefix;
        }

private:
    AutoPtr<CDataType> m_ElementType;
    bool m_NonEmpty;
    bool m_NoPrefix;
};

class CUniSetDataType : public CUniSequenceDataType {
    typedef CUniSequenceDataType CParent;
public:
    CUniSetDataType(const AutoPtr<CDataType>& elementType);

    CTypeInfo* CreateTypeInfo(void);
    
    AutoPtr<CTypeStrings> GetFullCType(void) const;
    virtual const char* GetASNKeyword(void) const;
    virtual const char* GetDEFKeyword(void) const;
};

END_NCBI_SCOPE

#endif
