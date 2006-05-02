#ifndef OBJMGR_UTIL_AUTODEF_MOD_COMBO__HPP
#define OBJMGR_UTIL_AUTODEF_MOD_COMBO__HPP

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
#include <objmgr/util/autodef_source_desc.hpp>
#include <objmgr/util/autodef_source_group.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
    
class NCBI_XOBJUTIL_EXPORT CAutoDefModifierCombo : public CObject
{
public:
    CAutoDefModifierCombo();
    CAutoDefModifierCombo(CAutoDefModifierCombo *orig);
    ~CAutoDefModifierCombo();
    
    unsigned int GetNumGroups();
    CAutoDefSourceGroup *GetSourceGroup(unsigned int index);
    
    unsigned int GetNumSubSources();
    CSubSource::ESubtype GetSubSource(unsigned int index);
    unsigned int GetNumOrgMods();
    COrgMod::ESubtype GetOrgMod(unsigned int index);
    
    bool HasSubSource(CSubSource::ESubtype st);
    bool HasOrgMod(COrgMod::ESubtype st);
    
    void AddSource(const CBioSource& bs);
    
    void AddSubsource(CSubSource::ESubtype st);
    void AddOrgMod(COrgMod::ESubtype st);
    unsigned int GetNumUniqueDescriptions();
    bool AllUnique();
    void GetAvailableModifiers (CAutoDefSourceDescription::TAvailableModifierVector &modifier_list);
    
    typedef vector<CAutoDefSourceGroup *> TSourceGroupVector;
    typedef vector<CSubSource::ESubtype> TSubSourceTypeVector;
    typedef vector<COrgMod::ESubtype> TOrgModTypeVector;
    
private:
    TSourceGroupVector   m_GroupList;
    TSubSourceTypeVector m_SubSources;
    TOrgModTypeVector    m_OrgMods;
    
};


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.3  2006/05/02 13:03:27  bollin
* added labels for modifiers, implemented organism description dialog options
*
* Revision 1.2  2006/04/17 17:39:36  ucko
* Fix capitalization of header filenames.
*
* Revision 1.1  2006/04/17 16:25:01  bollin
* files for automatically generating definition lines, using a combination
* of modifiers to make definition lines unique within a set and listing the
* relevant features on the sequence.
*
*
* ===========================================================================
*/

#endif //OBJMGR_UTIL_AUTODEF_MOD_COMBO__HPP
