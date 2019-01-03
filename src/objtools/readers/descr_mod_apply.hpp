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
 * Authors:  Justin Foley
 *
 */
    
#ifndef _DESCR_MOD_APPLY_HPP_
#define _DESCR_MOD_APPLY_HPP_

#include <corelib/ncbistd.hpp>
#include <objtools/readers/mod_reader.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDescrCache;
    
class CDescrModApply 
{
public:
    CDescrModApply(CBioseq& bioseq);
    virtual ~CDescrModApply();

    using TModEntry = CModHandler::TMods::value_type;
    bool Apply(const TModEntry& mod_entry);
private:
    static bool x_TryBioSourceMod(const TModEntry& mod_entry, bool& preserve_taxid, CDescrCache& descr_cache);
    static bool x_TryPCRPrimerMod(const TModEntry& mod_entry, CDescrCache& descr_cache);
    static void x_SetSubtype(const TModEntry& mod_entry, CDescrCache& descr_cache);

    static bool x_TryOrgRefMod(const TModEntry& mod_entry, bool& preserve_taxid, CDescrCache& descr_cache);
    static void x_SetDBxref(const TModEntry& mod_entry, CDescrCache& descr_cache);
    static bool x_TryOrgNameMod(const TModEntry& mod_entry, CDescrCache& descr_cache);
    static void x_SetOrgMod(const TModEntry& mod_entry, CDescrCache& descr_cache);

    static void x_SetDBLink(const TModEntry& mod_entry, CDescrCache& descr_cache);
    static void x_SetDBLinkField(const string& label, const TModEntry& mod_entry, CDescrCache& descr_cache);
    static void x_SetDBLinkFieldVals(const string& label, const list<CTempString>& vals, CUser_object& db_link);

    static void x_SetMolInfoType(const TModEntry& mod_entry, CDescrCache& descr_cache);
    static void x_SetMolInfoCompleteness(const TModEntry& mod_entry, CDescrCache& descr_cache);
    static void x_SetMolInfoTech(const TModEntry& mod_entry, CDescrCache& descr_cache);
    static void x_SetTpaAssembly(const TModEntry& mod_entry, CDescrCache& descr_cache);
    static void x_SetGBblockIds(const TModEntry& mod_entry, CDescrCache& descr_cache);
    static void x_SetGBblockKeywords(const TModEntry& mod_entry, CDescrCache& descr_cache);
    static void x_SetGenomeProjects(const TModEntry& mod_entry, CDescrCache& descr_cache);
    static void x_SetComment(const TModEntry& mod_entry, CDescrCache& descr_cache);
    static void x_SetPMID(const TModEntry& mod_entry, CDescrCache& descr_cache);

    static const string& x_GetModName(const TModEntry& mod_entry);
    static const string& x_GetModValue(const TModEntry& mod_entry);
    static void x_ThrowInvalidValue(const CModData& mod_data, const string& add_msg="");

    bool m_PreserveTaxId = false;
    unique_ptr<CDescrCache> m_pDescrCache;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _DESCR_MOD_APPLY_HPP_
