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
#ifndef _MOD_READER_HPP_
#define _MOD_READER_HPP_
#include <corelib/ncbistd.hpp>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <objtools/readers/reader_error_codes.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class NCBI_XOBJREAD_EXPORT CModData
{
public:
    CModData(const string& name);
    CModData(const string& name, const string& value);

    void SetName(const string& name);
    void SetValue(const string& value);
    void SetAttrib(const string& attrib);
    bool IsSetAttrib(void) const;

    const string& GetName(void) const;
    const string& GetValue(void) const;
    const string& GetAttrib(void) const;

private:
    string mName;
    string mValue;
    string mAttrib;    
};



class NCBI_XOBJREAD_EXPORT CModHandler 
{
public:

    using TModList = list<CModData>;

    enum EHandleExisting {
        eReplace        = 0,
        ePreserve       = 1,
        eAppendReplace  = 2,
        eAppendPreserve = 3
    };

    using TMods = map<string, list<CModData>>;
    using TModEntry = TMods::value_type;
    using FReportError = function<void(const string& message, EDiagSev severity, EModSubcode subcode)>;

    CModHandler(FReportError fReportError=nullptr);

    void AddMods(const TModList& mods, 
                 EHandleExisting handle_existing, 
                 TModList& rejected_mods);

    const TMods& GetMods(void) const;

    void Clear(void);

    static const string& GetCanonicalName(const TModEntry& mod_entry);
    static const string& AssertReturnSingleValue(const TModEntry& mod_entry);

private:
    string x_GetCanonicalName(const string& name) const;
    string x_GetNormalizedString(const string& name) const;
    static bool x_MultipleValuesAllowed(const string& canonical_name);
    static bool x_IsDeprecated(const string& canonical_name);
    void x_SaveMods(TMods&& mods, EHandleExisting handle_existing, TMods& dest);

    TMods m_Mods;

    using TNameMap = unordered_map<string, string>;
    using TNameSet = unordered_set<string>;
    static const TNameMap sm_NameMap;
    static const TNameSet sm_MultipleValuesForbidden;
    static const TNameSet sm_DeprecatedModifiers;

    FReportError m_fReportError;
};


class CBioseq;
class CSeq_inst;
class CSeq_loc;
class CModReaderException;


class NCBI_XOBJREAD_EXPORT CModAdder
{
public:
    using TMods = CModHandler::TMods;
    using TModEntry = CModHandler::TModEntry;
    using TSkippedMods = list<CModData>;
    using FReportError = CModHandler::FReportError;


    static void Apply(const CModHandler& mod_handler,
            CBioseq& bioseq,
            TSkippedMods& skipped_mods,
            FReportError fReportError=nullptr);

private:

    static const string& x_GetModName(const TModEntry& mod_entry);
    static const string& x_GetModValue(const TModEntry& mod_entry);

    static bool x_TrySeqInstMod(const TModEntry& mod_entry, 
            CSeq_inst& seq_inst,
            TSkippedMods& skipped_mods,
            FReportError fReportError);

    static void x_SetStrand(const TModEntry& mod_entry, 
            CSeq_inst& seq_inst,
            TSkippedMods& skipped_mods,
            FReportError fReportError);

    static void x_SetMolecule(const TModEntry& mod_entry, 
            CSeq_inst& seq_inst,
            TSkippedMods& skipped_mods,
            FReportError fReportError);

    static void x_SetMoleculeFromMolType(const TModEntry& mod_entry, 
            CSeq_inst& seq_inst);

    static void x_SetTopology(const TModEntry& mod_entry, 
            CSeq_inst& seq_inst,
            TSkippedMods& skipped_mods,
            FReportError fReportError);

    static void x_SetHist(const TModEntry& mod_entry, 
            CSeq_inst& seq_inst);

    static void x_ReportInvalidValue(const CModData& mod_data,
                                     TSkippedMods& skipped_mods,
                                     FReportError fReportError);
};


class NCBI_XOBJREAD_EXPORT CTitleParser 
{
public:
    using TModList = CModHandler::TModList;
    static void Apply(const CTempString& title, TModList& mods, string& remainder);
    static bool HasMods(const CTempString& title);
private:
    static bool x_FindBrackets(const CTempString& line, size_t& start, size_t& stop, size_t& eq_pos);
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _MOD_READER_HPP_
