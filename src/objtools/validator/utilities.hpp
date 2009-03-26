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
 * Author:  Mati Shomrat
 *
 * File Description:
 *      Definition for utility classes and functions.
 */

#ifndef VALIDATOR___UTILITIES__HPP
#define VALIDATOR___UTILITIES__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objmgr/seq_vector.hpp>

#include <serial/iterator.hpp>

#include <vector>
#include <list>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CGb_qual;
class CScope;
class CSeq_entry;

BEGIN_SCOPE(validator)


// =============================================================================
//                           Enumeration of GBQual types
// =============================================================================

class CGbqualType
{
public:
    enum EType {
        e_Bad = 0,
        e_Allele,
        e_Anticodon,
        e_Bio_material,
        e_Bound_moiety,
        e_Cell_line,
        e_Cell_type,
        e_Chromosome,
        e_Chloroplast,
        e_Chromoplast,
        e_Citation,
        e_Clone,
        e_Clone_lib,
        e_Codon,
        e_Codon_start,
        e_Collected_by,
        e_Collection_date,
        e_Compare,
        e_Cons_splice,
        e_Country,
        e_Cultivar,
        e_Culture_collection,
        e_Cyanelle,
        e_Cyt_map,
        e_Direction,
        e_EC_number,
        e_Ecotype,
        e_Environmental_sample,
        e_Db_xref,
        e_Dev_stage,
        e_Estimated_length,
        e_Evidence,
        e_Exception,
        e_Experiment,
        e_Focus,
        e_Frequency,
        e_Function,
        e_Gen_map,
        e_Gene,
        e_Gene_synonym,
        e_Gdb_xref,
        e_Germline,
        e_Haplotype,
        e_Host,
        e_Identified_by,
        e_Inference,
        e_Insertion_seq,
        e_Isolate,
        e_Isolation_source,
        e_Kinetoplast,
        e_Label,
        e_Lab_host,
        e_Lat_lon,
        e_Locus_tag,
        e_Macronuclear,
        e_Map,
        e_Mating_type,
        e_Metagenomic,
        e_Mitochondrion,
        e_Mobile_element,
        e_Mod_base,
        e_Mol_type,
        e_NcRNA_class,
        e_Note,
        e_Number,
        e_Old_locus_tag,
        e_Operon,
        e_Organelle,
        e_Organism,
        e_Partial,
        e_PCR_conditions,
        e_PCR_primers,
        e_Phenotype,
        e_Plasmid,
        e_Pop_variant,
        e_Product,
        e_Protein_id,
        e_Proviral,
        e_Pseudo,
        e_Rad_map,
        e_Rearranged,
        e_Replace,
        e_Ribosomal_slippage,
        e_Rpt_family,
        e_Rpt_type,
        e_Rpt_unit,
        e_Rpt_unit_range,
        e_Rpt_unit_seq,
        e_Satellite,
        e_Segment,
        e_Sequenced_mol,
        e_Serotype,
        e_Serovar,
        e_Sex,
        e_Site,
        e_Site_type,
        e_Specimen_voucher,
        e_Standard_name,
        e_Strain,
        e_Sub_clone,
        e_Sub_species,
        e_Sub_strain,
        e_Tag_peptide,
        e_Tissue_lib,
        e_Tissue_type,
        e_Transcript_id,
        e_Transgenic,
        e_Transl_except,
        e_Transl_table,
        e_Translation,
        e_Transposon,
        e_Trans_splicing,
        e_Usedin,
        e_Variety,
        e_Virion
    };

    // Conversions to enumerated type:
    static EType GetType(const string& qual);
    static EType GetType(const CGb_qual& qual);
    
    // Conversion from enumerated to string:
    static const string& GetString(EType gbqual);

private:
    CGbqualType(void);
    DECLARE_INTERNAL_ENUM_INFO(EType);
};


// =============================================================================
//                        Associating Features and GBQuals
// =============================================================================


class CFeatQualAssoc
{
public:
    typedef vector<CGbqualType::EType> TGBQualTypeVec;
    typedef map<CSeqFeatData::ESubtype, TGBQualTypeVec > TFeatQualMap;

    // Check to see is a certain gbqual is legal within the context of 
    // the specified feature
    static bool IsLegalGbqual(CSeqFeatData::ESubtype ftype,
                              CGbqualType::EType gbqual);

    // Retrieve the mandatory gbquals for a specific feature type.
    static const TGBQualTypeVec& GetMandatoryGbquals(CSeqFeatData::ESubtype ftype);

private:

    CFeatQualAssoc(void);
    void PoplulateLegalGbquals(void);
    void Associate(CSeqFeatData::ESubtype, CGbqualType::EType);
    void PopulateMandatoryGbquals(void);
    bool IsLegal(CSeqFeatData::ESubtype ftype, CGbqualType::EType gbqual);
    const TGBQualTypeVec& GetMandatoryQuals(CSeqFeatData::ESubtype ftype);
    
    static CFeatQualAssoc* Instance(void);

    static auto_ptr<CFeatQualAssoc> sm_Instance;
    // list of feature and their associated gbquals
    TFeatQualMap                    m_LegalGbquals;
    // list of features and their mandatory gbquals
    TFeatQualMap                    m_MandatoryGbquals;
};


// =============================================================================
//                                 Country Names
// =============================================================================


class CCountries
{
public:
    static bool IsValid(const string& country);
    static bool WasValid(const string& country);

private:
    static const string sm_Countries[];
    static const string sm_Former_Countries[];
};



// =============================================================================
//                                 Functions
// =============================================================================

bool IsClassInEntry(const CSeq_entry& se, CBioseq_set::EClass clss);
bool IsDeltaOrFarSeg(const CSeq_loc& loc, CScope* scope);
bool IsBlankString(const string& str);
bool IsBlankStringList(const list< string >& str_list);
int GetGIForSeqId(const CSeq_id& id);
list< CRef< CSeq_id > > GetSeqIdsForGI(int gi);

CSeqVector GetSequenceFromLoc(const CSeq_loc& loc, CScope& scope,
    CBioseq_Handle::EVectorCoding coding = CBioseq_Handle::eCoding_Iupac);

CSeqVector GetSequenceFromFeature(const CSeq_feat& feat, CScope& scope,
    CBioseq_Handle::EVectorCoding coding = CBioseq_Handle::eCoding_Iupac,
    bool product = false);

inline
bool IsResidue(unsigned char residue) { return residue <= 250; }
string GetAccessionFromObjects(const CSerialObject* obj, const CSeq_entry* ctx, CScope& scope);

CBioseq_set_Handle GetGenProdSetParent (CBioseq_set_Handle set);
CBioseq_set_Handle GetGenProdSetParent (CBioseq_Handle set);

typedef enum {
  eAccessionFormat_valid = 0,
  eAccessionFormat_no_start_letters,
  eAccessionFormat_wrong_number_of_digits,
  eAccessionFormat_null,
  eAccessionFormat_too_long,
  eAccessionFormat_missing_version,
  eAccessionFormat_bad_version } EAccessionFormatError;

EAccessionFormatError ValidateAccessionString (string accession, bool require_version);

END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* VALIDATOR___UTILITIES__HPP */
