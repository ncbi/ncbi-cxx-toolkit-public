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
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/taxon3/T3Data.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/biblio/Id_pat.hpp> 
#include <objects/biblio/Auth_list.hpp>
#include <objmgr/seq_vector.hpp>

#include <serial/iterator.hpp>

#include <vector>
#include <list>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CGb_qual;
class CScope;
class CSeq_entry;
class CT3Data;

BEGIN_SCOPE(validator)


// =============================================================================
//                                 Functions
// =============================================================================

bool IsClassInEntry(const CSeq_entry& se, CBioseq_set::EClass clss);
bool IsDeltaOrFarSeg(const CSeq_loc& loc, CScope* scope);
bool IsBlankStringList(const list< string >& str_list);
TGi GetGIForSeqId(const CSeq_id& id);
CScope::TIds GetSeqIdsForGI(TGi gi);

CSeqVector GetSequenceFromLoc(const CSeq_loc& loc, CScope& scope,
    CBioseq_Handle::EVectorCoding coding = CBioseq_Handle::eCoding_Iupac);

CSeqVector GetSequenceFromFeature(const CSeq_feat& feat, CScope& scope,
    CBioseq_Handle::EVectorCoding coding = CBioseq_Handle::eCoding_Iupac,
    bool product = false);

string GetSequenceStringFromLoc(const CSeq_loc& loc,  CScope& scope);


inline
bool IsResidue(unsigned char residue) { return residue <= 250; }
CConstRef<CSeq_id> GetReportableSeqIdForAlignment(const CSeq_align& align, CScope& scope);
string GetAccessionFromObjects(const CSerialObject* obj, const CSeq_entry* ctx, CScope& scope, int* version);

CBioseq_set_Handle GetSetParent (CBioseq_set_Handle set, CBioseq_set::TClass set_class);
CBioseq_set_Handle GetSetParent (CBioseq_Handle set, CBioseq_set::TClass set_class);
CBioseq_set_Handle GetGenProdSetParent (CBioseq_set_Handle set);
CBioseq_set_Handle GetGenProdSetParent (CBioseq_Handle set);
CBioseq_set_Handle GetNucProtSetParent (CBioseq_Handle bioseq);

CBioseq_Handle GetNucBioseq (CBioseq_set_Handle bioseq_set);
CBioseq_Handle GetNucBioseq (CBioseq_Handle bioseq);

typedef enum {
  eAccessionFormat_valid = 0,
  eAccessionFormat_no_start_letters,
  eAccessionFormat_wrong_number_of_digits,
  eAccessionFormat_null,
  eAccessionFormat_too_long,
  eAccessionFormat_missing_version,
  eAccessionFormat_bad_version } EAccessionFormatError;

EAccessionFormatError ValidateAccessionString (string accession, bool require_version);

bool s_FeatureIdsMatch (const CFeat_id& f1, const CFeat_id& f2);
bool s_StringHasPMID (string str);
bool HasBadCharacter (string str);
bool EndsWithBadCharacter (string str);

bool IsBioseqWithIdInSet (const CSeq_id& id, CBioseq_set_Handle set);

typedef enum {
  eDateValid_valid = 0x0,
  eDateValid_bad_str = 0x01,
  eDateValid_bad_year = 0x02,
  eDateValid_bad_month = 0x04,
  eDateValid_bad_day = 0x08,
  eDateValid_bad_season = 0x10,
  eDateValid_bad_other = 0x20 ,
  eDateValid_empty_date = 0x40 } EDateValid;

int CheckDate (const CDate& date, bool require_full_date = false);
string GetDateErrorDescription (int flags);

bool IsBioseqTSA (const CBioseq& seq, CScope* scope);

void GetPubdescLabels 
(const CPubdesc& pd, 
 vector<int>& pmids, vector<int>& muids, vector<int>& serials,
 vector<string>& published_labels, vector<string>& unpublished_labels);

bool IsNCBIFILESeqId (const CSeq_id& id);
bool IsRefGeneTrackingObject (const CUser_object& user);

string GetValidatorLocationLabel (const CSeq_loc& loc);
void AppendBioseqLabel(string& str, const CBioseq& sq, bool supress_context);
string GetBioseqIdLabel(const CBioseq& sq, bool limited = false);

bool HasECnumberPattern (const string& str);

string GetAuthorsString (const CAuth_list& auth_list);

bool SeqIsPatent (const CBioseq& seq);
bool SeqIsPatent (CBioseq_Handle seq);

bool s_PartialAtGapOrNs (CScope* scope, const CSeq_loc& loc, unsigned int tag);

CBioseq_Handle BioseqHandleFromLocation (CScope* m_Scope, const CSeq_loc& loc);

typedef enum {
    eBioseqEndIsType_None = 0,
    eBioseqEndIsType_Last,
    eBioseqEndIsType_All
} EBioseqEndIsType;


void NCBI_VALIDATOR_EXPORT CheckBioseqEndsForNAndGap 
(CBioseq_Handle bsh,
 EBioseqEndIsType& begin_n,
 EBioseqEndIsType& begin_gap,
 EBioseqEndIsType& end_n,
 EBioseqEndIsType& end_gap);

typedef enum {
    eDuplicate_Not = 0,
    eDuplicate_Duplicate,
    eDuplicate_SameIntervalDifferentLabel,
    eDuplicate_DuplicateDifferentTable,
    eDuplicate_SameIntervalDifferentLabelDifferentTable
} EDuplicateFeatureType;

typedef const CSeq_feat::TDbxref TDbtags;

EDuplicateFeatureType NCBI_VALIDATOR_EXPORT IsDuplicate 
    (CSeq_feat_Handle f1, const CSeq_feat& feat1, 
     CSeq_feat_Handle f2, const CSeq_feat& feat2,CScope& scope);

bool IsLocFullLength (const CSeq_loc& loc, const CBioseq_Handle& bsh);
CConstRef <CSeq_feat> GetGeneForFeature (const CSeq_feat& f1, CScope *scope);
bool PartialsSame (const CSeq_loc& loc1, const CSeq_loc& loc2);

// specific-host functions
/// returns true and error_msg will be empty, if specific host is valid
/// returns true and error_msg will be "Host is empty", if specific host is empty
/// returns false if specific host is invalid
bool NCBI_VALIDATOR_EXPORT IsSpecificHostValid(const string& host, string& error_msg);
/// returns the corrected specific host, if the specific host is invalid and can be corrected
/// returns an empty string, if the specific host is invalid and cannot be corrected
/// returns the original value except the preceding/trailing spaces, if the specific host is valid
string NCBI_VALIDATOR_EXPORT FixSpecificHost(const string& host);

bool NCBI_VALIDATOR_EXPORT IsCommonName (const CT3Data& data);
bool NCBI_VALIDATOR_EXPORT HasMisSpellFlag (const CT3Data& data);
bool NCBI_VALIDATOR_EXPORT FindMatchInOrgRef (string str, const COrg_ref& org);
void NCBI_VALIDATOR_EXPORT AdjustSpecificHostForTaxServer (string& spec_host);

END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* VALIDATOR___UTILITIES__HPP */
