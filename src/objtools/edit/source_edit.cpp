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
* Author: Colleen Bollin, NCBI
*
* File Description:
*   functions for editing and working with biosources
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>
#include <objtools/edit/source_edit.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/general/Dbtag.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)


bool CleanupForTaxnameChange( objects::CBioSource& src )
{
    bool rval = RemoveOldName(src);
    rval |= RemoveTaxId(src);
    if (src.IsSetOrg() && src.GetOrg().IsSetCommon()) {
        src.SetOrg().ResetCommon();
        rval = true;
    }
    if (src.IsSetOrg() && src.GetOrg().IsSetSyn()) {
        src.SetOrg().ResetSyn();
        rval = true;
    }
    return rval;
}

bool RemoveOldName( objects::CBioSource& src )
{
    bool erased = false;
    if (src.IsSetOrg() && src.GetOrg().IsSetOrgname()
        && src.GetOrg().GetOrgname().IsSetMod()) {
        COrgName::TMod::iterator it = src.SetOrg().SetOrgname().SetMod().begin();
        while (it != src.SetOrg().SetOrgname().SetMod().end()) {
            if ((*it)->GetSubtype() && (*it)->GetSubtype() == COrgMod::eSubtype_old_name) {
                it = src.SetOrg().SetOrgname().SetMod().erase(it);
                erased = true;
            } else {
                it++;
            }
        }
        if (src.GetOrg().GetOrgname().GetMod().empty()) {
            src.SetOrg().SetOrgname().ResetMod();
        }
    }
    return erased;
}

bool RemoveTaxId( objects::CBioSource& src )
{
    bool erased = false;
    if (src.IsSetOrg() && src.GetOrg().IsSetDb()) {
        COrg_ref::TDb::iterator it = src.SetOrg().SetDb().begin();
        while (it != src.SetOrg().SetDb().end()) {
            if ((*it)->IsSetDb() && NStr::EqualNocase((*it)->GetDb(), "taxon")) {
                it = src.SetOrg().SetDb().erase(it);
                erased = true;
            } else {
                it++;
            }
        }
        if (src.GetOrg().GetDb().empty()) {
            src.SetOrg().ResetDb();
        }
    }
    return erased;
}


END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

