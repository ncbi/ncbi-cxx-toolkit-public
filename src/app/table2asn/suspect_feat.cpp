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
* Author:  Sergiy Gotvyanskyy, NCBI
*
* File Description:
*   Reader and handler for suspect product rules files
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbifile.hpp>
#include <objects/macro/String_constraint_set.hpp>
#include <objects/macro/Suspect_rule_set.hpp>
#include <objects/macro/Suspect_rule.hpp>
#include <serial/objistr.hpp>


BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

namespace
{
    CRef<CSuspect_rule_set> g_rules;
};

bool FixSuspectProductName(string& name)
{
    static const char PRODUCT_RULES_LIST[] =
#ifdef NCBI_OS_MSWIN
        "\\\\panfs\\pan1\\wgs_gb_sub\\processing\\rules_usethis.txt";
#else
        "/panfs/pan1.be-md.ncbi.nlm.nih.gov/wgs_gb_sub/processing/rules_usethis.txt";
#endif
    if (g_rules.Empty())
    {
        g_rules.Reset(new CSuspect_rule_set);
        if (CFile(PRODUCT_RULES_LIST).Exists())
        {
            CNcbiIfstream instream(PRODUCT_RULES_LIST);
            auto_ptr<CObjectIStream> pObjIstrm(CObjectIStream::Open(eSerial_AsnText, instream, eNoOwnership));
            pObjIstrm->Read(g_rules, g_rules->GetThisTypeInfo());
        }
    }


    if (g_rules.Empty() || g_rules->Get().empty())
        return false;

    ITERATE(CSuspect_rule_set::Tdata, it, g_rules->Get())
    {
        if ((**it).ApplyToString(name))
            return true;
    }

    return false;
}

END_objects_SCOPE
END_NCBI_SCOPE


