#ifndef OBJMGR_UTIL_AUTODEF_SOURCE_DESC__HPP
#define OBJMGR_UTIL_AUTODEF_SOURCE_DESC__HPP

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
* Author:  Colleen Bollin
*
* File Description:
*   Creates unique definition lines for sequences in a set using organism
*   descriptions and feature clauses.
*/

#include <corelib/ncbistd.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seq/MolInfo.hpp>

#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seq_entry_handle.hpp>

#include <objmgr/util/autodef_available_modifier.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class NCBI_XOBJUTIL_EXPORT CAutoDefSourceModifierInfo
{
public:
    CAutoDefSourceModifierInfo(bool isOrgMod, int subtype, string value);
    CAutoDefSourceModifierInfo(const CAutoDefSourceModifierInfo &other);
    ~CAutoDefSourceModifierInfo();

    bool AddQual (bool isOrgMod, int subtype);
    bool RemoveQual (bool isOrgMod, int subtype);

    unsigned int GetRank() const;
    int Compare(const CAutoDefSourceModifierInfo& mod) const;
    bool IsOrgMod () const;
    int  GetSubtype () const; 
    string GetValue () const;

    bool operator>(const CAutoDefSourceModifierInfo& mod) const
    {
        return Compare (mod) > 0;
    }

    bool operator<(const CAutoDefSourceModifierInfo& mod) const
    {
        return Compare (mod) < 0;
    }


private:
    bool m_IsOrgMod;
    int  m_Subtype;
    string m_Value;
};


inline bool CAutoDefSourceModifierInfo::IsOrgMod() const
{
    return m_IsOrgMod;
}


inline int CAutoDefSourceModifierInfo::GetSubtype() const
{
    return m_Subtype;
}


inline string CAutoDefSourceModifierInfo::GetValue() const
{
    return m_Value;
}


class IAutoDefCombo
{
public:
    virtual ~IAutoDefCombo() { }
    virtual string GetSourceDescriptionString(const CBioSource& biosrc) = 0;
};


class NCBI_XOBJUTIL_EXPORT CAutoDefSourceDescription 
{
public:
    CAutoDefSourceDescription(const CBioSource& bs);
    CAutoDefSourceDescription(CAutoDefSourceDescription *other);
    ~CAutoDefSourceDescription();
    
    typedef vector<string> TStringVector;
    typedef vector<CAutoDefAvailableModifier> TAvailableModifierVector;
        
    const CBioSource& GetBioSource() const;
    
    void GetAvailableModifiers (TAvailableModifierVector &modifier_list);
    
    bool IsTrickyHIV();
    
    string GetComboDescription(IAutoDefCombo *mod_combo);

    bool AddQual (bool isOrgMod, int subtype);
    bool RemoveQual (bool isOrgMod, int subtype);
    int Compare(const CAutoDefSourceDescription& s) const;
    bool operator>(const CAutoDefSourceDescription& src) const
    {
        return Compare (src) > 0;
    }

    bool operator<(const CAutoDefSourceDescription& src) const
    {
        return Compare (src) < 0;
    }

    typedef vector<CAutoDefSourceModifierInfo> TModifierVector;
    const TModifierVector& GetModifiers() const { return m_Modifiers; }
    typedef list<string> TDescString;
    const TDescString& GetStrings() const { return m_DescStrings; }

private:
    const CBioSource& m_BS;
    TModifierVector m_Modifiers;
    TDescString m_DescStrings;

};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif //OBJMGR_UTIL_AUTODEF_SOURCE_DESC__HPP
