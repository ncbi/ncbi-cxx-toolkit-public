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
* Author:  Igor Filippov
*
* File Description:
*   Simple library to correct reference allele in Variation object
*
* ===========================================================================
*/

#include <objects/variation/Variation.hpp>
#include <objects/variation/VariantPlacement.hpp>
#include <objects/seqfeat/Variation_inst.hpp>

#include <objmgr/scope.hpp>

#include <util/ncbi_cache.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
using namespace std;

bool x_CheckForFirstNTSeq(const CVariation_inst& inst);

class CVariationUtilities
{
public:
    static void   CorrectRefAllele(CVariation& var, CScope& scope);
    static void   CorrectRefAllele(CSeq_annot& annot, CScope& scope);
    static void   CorrectRefAllele(CSeq_feat& feat, CScope& scope);

    static bool   IsReferenceCorrect(const CSeq_feat& feat, string& asserted_ref, string& ref_at_location, CScope& scope);

    static CVariation_inst::TType  GetVariationType(const CVariation_ref& vr);
    static CVariation_inst::TType  GetVariationType(const CVariation& var);

    //For deletions, this returns asserted deleted sequence
    // not the one extracted from scope (thus, no scope).
    static void   GetVariationRefAlt(const CVariation_ref& vr, string &ref,  vector<string> &alt);
    static void   GetVariationRefAlt(const CVariation& vr, string &ref,  vector<string> &alt);


    //Do all alleles in variation ref have a common repeat unit
    // and thus possible to shift them.

    //Returns kEmptyStr if no common repeat unit
    static string GetCommonRepeatUnit(const CVariation_ref& vr);
    static string GetCommonRepeatUnit(const CVariation& vr);
    static string RepeatedSubstring(const string &str) ;

    //intronic detection
    static bool IsIntronicVariation(const CVariation_inst& inst);
    static bool IsIntronicVariation(const CVariation_ref& var);
    static bool IsIntronicVariation(const CVariation& var);



private:
    static string x_GetAlleleFromLoc(const CSeq_loc& loc, CScope& scope);
    static string x_GetRefAlleleFromVP(CVariantPlacement& vp, CScope& scope, TSeqPos length);

    //These return true if the reference allele was changed
    static bool x_SetReference(CVariation_ref& var, const string& ref_at_location);
    static bool x_FixAlleles(CVariation& variation, string asserted_ref, string ref_at_location);
    static bool x_FixAlleles(CVariation_ref& vr, string asserted_ref, string ref_at_location) ;

    static void x_AddRefAlleleFixFlag(CVariation& var);
    static void x_AddRefAlleleFixFlag(CSeq_feat& var);

    static const unsigned int MAX_LEN=1000;

    static bool x_isBaseRepeatingUnit(const string& candidate, const string& target);
};


///
/// A set of classes to normalize variation.
///
/// The \a CSeqVectorCache class handles storing
/// a cache of \a CSeqVectors, hard coded to 16.
///
/// Not intended to be used directly
class CSeqVectorCache
{
public:
    static TSeqPos GetSeqSize(CSeqVector &seqvec);
    static CRef<CSeqVector> PrefetchSequence(CScope &scope, const CSeq_id &seq_id,ENa_strand strand);
    static string GetSeq(int pos, int length, CSeqVector &seqvec);

private:
    static CCache<string,CRef<CSeqVector> > m_cache;
    static const unsigned int CCACHE_SIZE = 16;
};

/// A struct to keep track of the start and stop of the range.
struct SEndPosition {
    SEndPosition(int l, int r) :
        left(l), right(r) {}

    TSeqPos left;
    TSeqPos right;
};

/// Shifting can be one of four types (left, right, full, left-with-interval)
/// and can be on two related types of objects: Seq-annot (and Seq-feat)
/// and Variation.
///
/// The main method here is x_Shift.  It will process through each variation
/// and shift the SeqLoc and allele string.  The type (or direction) of
/// shifting depends on the derived class used for invocation.  There
/// are three methods that each derived class must implement
template<class T>
class CVariationNormalization_base : public CSeqVectorCache
{
public:

    static void x_Shift(CVariation& var, CScope &scope);
	static void x_Shift(CSeq_annot& annot, CScope &scope);
	static void x_Shift(CSeq_feat& feat, CScope &scope);

    // Four calls to derived classes
    // to customize behavior based on left, right, full or left/int shifting.
    static bool ProcessShift(string &common_repeat_unit, SEndPosition& sep,
        CSeqVector &seqvec, int& rotation_counter, const CVariation_inst::TType type)
    {
        return T::ProcessShift(common_repeat_unit,sep,
            seqvec,rotation_counter,type);
    }

    static void ModifyLocation(CSeq_loc &loc, SEndPosition& sep,
        const CVariation_inst::TType type, const TSeqPos& deletion_size )
    {
        T::ModifyLocation(loc,sep,type, deletion_size);
    }

    static void Rotate(string& v)
    {
        T::Rotate(v);
    }

    template<class U>
    static void SetShiftFlag(U& var)
    {
        T::SetShiftFlag(var);
    }


    /// This method determine if a SeqLoc, and associated allele, are shiftable.
    static bool x_IsShiftable(const CSeq_loc &loc, const string &allele,
        CScope &scope, CVariation_inst::TType type);

    /// Convert Interval SeqLoc into a Point
    static void x_ConvertIntervalToPoint(CSeq_loc &loc, int pos);

    /// Convert a Point SeqLoc into an Interval
    static void x_ConvertPointToInterval(CSeq_loc &loc, int pos_left, int pos_right);

    static void x_SetShiftFlag(CVariation& var,const string& field_text);
    static void x_SetShiftFlag(CSeq_feat& var,const string& field_text);

protected:
   static void RotateLeft(string &v);
   static void RotateRight(string &v);

   static void ResetFullyShifted(CVariation& var, CSeq_loc &loc, SEndPosition& sep,
       const CVariation_inst::TType type, const TSeqPos& deletion_size);
   static void ResetFullyShifted(CSeq_feat& feat, CSeq_loc &loc, SEndPosition& sep,
       const CVariation_inst::TType type, const TSeqPos& deletion_size);



};

class CVariationNormalizationLeft : public CVariationNormalization_base<CVariationNormalizationLeft>
{
public:
    static bool ProcessShift(string &allele, SEndPosition& sep,
        CSeqVector &seqvec, int& rotation_counter, const CVariation_inst::TType type);
    static void ModifyLocation(CSeq_loc &loc,
        SEndPosition& sep, const CVariation_inst::TType type, const TSeqPos& deletion_size );
    static void Rotate(string& v);

    template<class V>
    static void SetShiftFlag(V& v) { x_SetShiftFlag(v,"Left Shifted"); }

};

class CVariationNormalizationDelIns : public CVariationNormalization_base<CVariationNormalizationDelIns>
{
public:
    static bool ProcessShift(string &allele, SEndPosition& sep,
        CSeqVector &seqvec, int& rotation_counter, const CVariation_inst::TType type);
    static void ModifyLocation(CSeq_loc &loc,
        SEndPosition& sep, const CVariation_inst::TType type, const TSeqPos& deletion_size );
    static void Rotate(string& v);

    template<class V>
    static void SetShiftFlag(V& v) { x_SetShiftFlag(v,"Fully Shifted"); }

    static void ConvertExpandedDeletionToDelIns(CSeq_feat& feat, CScope &scope);
    static void ConvertExpandedInsertionToDelIns(CSeq_feat& feat, CScope &scope);

};

class CVariationNormalizationRight : public CVariationNormalization_base<CVariationNormalizationRight>
{
public:
    static bool ProcessShift(string &allele, SEndPosition& sep,
        CSeqVector &seqvec, int& rotation_counter, const CVariation_inst::TType type);
    static void ModifyLocation(CSeq_loc &loc,
        SEndPosition& sep, const CVariation_inst::TType type, const TSeqPos& deletion_size );
    static void Rotate(string& v);

    template<class V>
    static void SetShiftFlag(V& v) { x_SetShiftFlag(v,"Right Shifted"); }


};

class CVariationNormalizationLeftInt :  public CVariationNormalization_base<CVariationNormalizationLeftInt>
{
public:
    static bool ProcessShift(string &allele, SEndPosition& sep,
        CSeqVector &seqvec, int& rotation_counter, const CVariation_inst::TType type);
    static void ModifyLocation(CSeq_loc &loc,
        SEndPosition& sep, const CVariation_inst::TType type, const TSeqPos& deletion_size);
    static void Rotate(string& v);

    template<class V>
    static void SetShiftFlag(V& v) { x_SetShiftFlag(v,"Left Interval Shifted"); }

};

class CVariationNormalization
{
public:
    enum ETargetContext {
       eDbSnp,
       eHGVS,
       eVCF,
       eVarLoc
    };

    static void NormalizeVariation(CVariation& var,
        ETargetContext target_ctxt, CScope& scope);
    static void NormalizeVariation(CSeq_annot& var,
        ETargetContext target_ctxt, CScope& scope);
    static void NormalizeVariation(CSeq_feat& feat,
        ETargetContext target_ctxt, CScope& scope);


    //These methods search out ambiguous ins/del, and alter SeqLoc
    //to a Seq-equiv: the first is the original SeqLoc
    //the second is an interval representing the IOA
    static void AlterToDelIns(CVariation& var, CScope &scope);
    static void AlterToDelIns(CSeq_annot& var, CScope &scope);
    static void AlterToDelIns(CSeq_feat& feat, CScope &scope);

    //HGVS
    //*Assume* Seq-equiv's are present and second one is 'IOA'
    //  (i.e. that SetContextToDbSnp has already been executed on the object)
    //Produce one var/var-ref *per* allele, each with single SeqLoc,
    //representing right-most position
    //For an insert, this is a SeqLoc-Point
    //For a deletion, this is either a point or interval, depending on the size of the deletion.
    // and precisely why we are breaking up alleles into separate HGVS expressions
    static void AlterToHGVSVar(CVariation& var, CScope& scope);
    static void AlterToHGVSVar(CSeq_annot& annot, CScope& scope);
	static void AlterToHGVSVar(CSeq_feat& feat, CScope& scope);

    //VCF business logic
    static void AlterToVCFVar(CVariation& var, CScope& scope);
    static void AlterToVCFVar(CSeq_annot& var, CScope& scope);
    static void AlterToVCFVar(CSeq_feat& feat, CScope& scope);


    static void AlterToVarLoc(CVariation& var, CScope& scope);
    static void AlterToVarLoc(CSeq_annot& var, CScope& scope);
    static void AlterToVarLoc(CSeq_feat& feat, CScope& scope);

    static bool IsShiftable(const CSeq_loc &loc, const string &allele,
        CScope &scope, CVariation_inst::TType type);

    //Is the variant full-shifted, based on inclusion of 'UserObject'
    static bool isFullyShifted(const CVariation& var);
    static bool isFullyShifted(const CSeq_feat& feat);

};
