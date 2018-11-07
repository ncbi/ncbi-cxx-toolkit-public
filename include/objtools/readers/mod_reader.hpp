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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CModValueAttribs
{
public:
    using TAttribs = map<string, string>;

    CModValueAttribs(const string& value); 
    CModValueAttribs(const char* value);

    void SetValue(const string& value);
    void AddAttrib(const string& attrib_name, const string& attrib_value);
    bool HasAdditionalAttribs(void) const;

    const string& GetValue(void) const;
    const TAttribs& GetAdditionalAttribs(void) const;

private:
    string mValue;
    TAttribs mAdditionalAttribs;
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

class CModParser 
{
public:
    using TMods = multimap<string, CModValueAttribs>;

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
    static bool x_IsUserType(const CUser_object& user_object, const string& type);

    static void x_ImportFeatureModifiers(const CSeq_annot& annot, TMods& mods);
    static void x_ImportGene(const CGene_ref& gene_ref, TMods& mods);
    static void x_ImportProtein(const CProt_ref& prot_ref, TMods& mods);
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _MOD_READER_HPP_
