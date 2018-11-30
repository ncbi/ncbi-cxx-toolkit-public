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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class NCBI_XOBJREAD_EXPORT CModValueAndAttrib 
{
public:
    CModValueAndAttrib(const string& value);
    CModValueAndAttrib(const char* value);

    void SetValue(const string& value);
    void SetAttrib(const string& attrib);
    bool IsSetAttrib(void) const;

    const string& GetValue(void) const;
    const string& GetAttrib(void) const;
private:
    string mValue;
    string mAttrib;
};


class IObjtoolsListener;

class NCBI_XOBJREAD_EXPORT CModHandler 
{
public:
    enum EHandleExisting {
        eReplace        = 0,
        ePreserve       = 1,
        eAppendReplace  = 2,
        eAppendPreserve = 3
    };

    using TMods = multimap<string, CModValueAndAttrib>;

    CModHandler(IObjtoolsListener* listener=nullptr);

    void AddMod(const string& name, 
                const string& value,
                EHandleExisting handle_existing);

    void AddMod(const string& name,
                const CModValueAndAttrib& val_attrib,
                EHandleExisting handle_existing);

    void AddMods(const TMods& mods, EHandleExisting handle_existing);

    const TMods& GetMods(void) const;

    void Clear(void);

private:
    string x_GetNormalizedName(const string& name);
    string x_GetCanonicalName(const string& name);

    TMods m_Mods;
    using TNameMap = unordered_map<string, string>;
    using TNameSet = unordered_set<string>;
    static const TNameMap sm_NameMap;
    static const TNameSet sm_MultipleValuesForbidden;
    static const TNameSet sm_DeprecatedModifiers;

    IObjtoolsListener* m_pMessageListener;
};


class CBioseq;
class CSeq_inst;
class CSeq_hist;
class CSeqdesc;
class CUser_object;
class CGB_block;
class CBioSource;
class CMolInfo;
class CPubdesc;
class COrg_ref;
class COrgName;
class COrgMod;
class CSubSource;
class CSeq_annot;
class CGene_ref;
class CProt_ref;


class NCBI_XOBJREAD_EXPORT CModParser 
{
public:
    using TMods = CModHandler::TMods;

    static void Apply(const CBioseq& bioseq, TMods& mods);
private:
    static void x_ImportSeqInst(const CSeq_inst& seq_inst, TMods& mods);
    static void x_ImportHist(const CSeq_hist& seq_hist, TMods& mods);

    static void x_ImportDescriptors(const CBioseq& bioseq, TMods& mods);
    static void x_ImportDesc(const CSeqdesc& desc, TMods& mods);
    static void x_ImportUserObject(const CUser_object& user_object, TMods& mods);
    static void x_ImportDBLink(const CUser_object& user_object, TMods& mods);
    static void x_ImportGBblock(const CGB_block& gb_block, TMods& mods);
    static void x_ImportGenomeProjects(const CUser_object& user_object, TMods& mods);
    static void x_ImportTpaAssembly(const CUser_object& user_object, TMods& mods);
    static void x_ImportBioSource(const CBioSource& biosource, TMods& mods);
    static void x_ImportMolInfo(const CMolInfo& molinfo, TMods& mods);
    static void x_ImportPMID(const CPubdesc& pub_desc, TMods& mods);
    static void x_ImportOrgRef(const COrg_ref& org_ref, TMods& mods);
    static void x_ImportOrgName(const COrgName& org_name, TMods& mods);
    static void x_ImportOrgMod(const COrgMod& org_mod, TMods& mods);
    static void x_ImportSubSource(const CSubSource& subsource, TMods& mods);

    static void x_ImportFeatureModifiers(const CSeq_annot& annot, TMods& mods);
    static void x_ImportGene(const CGene_ref& gene_ref, TMods& mods);
    static void x_ImportProtein(const CProt_ref& prot_ref, TMods& mods);
};


class CPCRPrimerSet;
class CDescrCache;
class CFeatureCache;


class NCBI_XOBJREAD_EXPORT CModAdder
{
public:
    using TMods = CModHandler::TMods;
    using TIterator = typename TMods::const_iterator;
    using TRange = pair<TIterator, TIterator>;

    static void Apply(const CModHandler& mod_handler, CBioseq& bioseq, 
            IObjtoolsListener* pMessageListener);

private:
    static const string& x_GetModName(const TRange& mod_range);
    static const string& x_GetModValue(const TRange& mod_range);

    static bool x_TrySeqInstMod(const TRange& mod_range, CSeq_inst& seq_inst,
            IObjtoolsListener* pMessageListener);
    static void x_SetStrand(const TRange& mod_range, CSeq_inst& seq_inst,
            IObjtoolsListener* pMessageListener);
    static void x_SetMolecule(const TRange& mod_range, CSeq_inst& seq_inst, 
            IObjtoolsListener* pMessageListener);
    static void x_SetMoleculeFromMolType(const TRange& mod_range, CSeq_inst& seq_inst,
            IObjtoolsListener* pMessageListener);
    static void x_SetTopology(const TRange& mod_range, CSeq_inst& seq_inst,
            IObjtoolsListener* pMessageListener);
    static void x_SetHist(const TRange& mod_range, CSeq_inst& seq_inst, 
            IObjtoolsListener* pMessageListener);


    static bool x_TryDescriptorMod(const TRange& mod_range, CDescrCache& descr_cache,
            IObjtoolsListener* pMessageListener);
    static bool x_TryBioSourceMod(const TRange& mod_range, CDescrCache& descr_cache,
            IObjtoolsListener* pMessageListener);
    static bool x_TryPCRPrimerMod(const TRange& mod_range, CDescrCache& descr_cache,
            IObjtoolsListener* pMessageListener);
    static bool x_TryOrgRefMod(const TRange& mod_range, CDescrCache& descr_cache,
            IObjtoolsListener* pMessageListener);
    static bool x_TryOrgNameMod(const TRange& mod_range, CDescrCache& descr_cache, 
            IObjtoolsListener* pMessageListener);
    static void x_SetDBxref(const TRange& mod_range, CDescrCache& descr_cache,
            IObjtoolsListener* pMessageListener);

    static void x_AppendPrimerNames(const string& names, vector<string>& reaction_names);
    static void x_AppendPrimerSeqs(const string& names, vector<string>& reaction_seqs);
    static void x_SetPrimerNames(const string& primer_names, CPCRPrimerSet& primer_set);
    static void x_SetPrimerSeqs(const string& primer_seqs, CPCRPrimerSet& primer_set);

    static void x_SetDBLink(const TRange& mod_range, CDescrCache& descr_cache);
    static void x_SetGBblockIds(const TRange& mod_range, CDescrCache& descr_cache);
    static void x_SetGBblockKeywords(const TRange& mod_range, CDescrCache& descr_cache);
    static void x_SetGenomeProjects(const TRange& mod_range, CDescrCache& descr_cache);
    static void x_SetTpaAssembly(const TRange& mod_range, CDescrCache& descr_cache);
    static void x_SetComment(const TRange& mod_range, CDescrCache& descr_cache);
    static void x_SetPMID(const TRange& mod_range, CDescrCache& descr_cache);
    static void x_SetDBLinkField(const string& label, const TRange& mod_range, CDescrCache& descr_cache);
    static void x_SetDBLinkFieldVals(const string& label, const list<CTempString>& vals, CUser_object& dblink);

    static void x_SetMolInfoType(const TRange& mod_range, CDescrCache& descr_cache);
    static void x_SetMolInfoTech(const TRange& mod_range, CDescrCache& descr_cache);
    static void x_SetMolInfoCompleteness(const TRange& mod_range, CDescrCache& descr_cache);
    static void x_SetSubtype(const TRange& mod_range, CDescrCache& descr_cache);
    static void x_SetOrgMod(const TRange& mod_range, CDescrCache& descr_cache);

    static bool x_TryFeatureMod(const TRange& mod_range, CFeatureCache& feat_cache, 
            IObjtoolsListener* pMessageListener);
    static bool x_TryGeneRefMod(const TRange& mod_range, CFeatureCache& gene_ref_cache,
            IObjtoolsListener* pMessageListener);
    static bool x_TryProteinRefMod(const TRange& mod_range, CFeatureCache& protein_ref_cache,
            IObjtoolsListener* pMessageListener);

    static void x_ReportError(void){} // Need to fill this in
    static void x_ReportInvalidValue(const string& mod_name, const string& mod_value,
            IObjtoolsListener* pMessageListener=nullptr);

    static void x_PutMessage(const string& message, EDiagSev severity, 
            IObjtoolsListener* pMessageListener);
};



END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _MOD_READER_HPP_
