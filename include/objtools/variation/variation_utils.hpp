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
#include <util/ncbi_cache.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
using namespace std;


class CVariationUtilities
{
public:
    static void   CorrectRefAllele(CRef<CVariation>& v, CScope& scope);      
    static void   CorrectRefAllele(CVariation_ref& vr, const CSeq_loc& loc, CScope& scope);
    static void   CorrectRefAllele(CRef<CSeq_annot>& v, CScope& scope);
    static void   CorrectRefAllele(CVariation_ref& var, const string& new_ref); 

    static bool   IsReferenceCorrect(const CSeq_feat& feat, string& wrong_ref, string& correct_ref, CScope& scope);

    static int    GetVariationType(const CVariation_ref& vr);
    static void   GetVariationRefAlt(const CVariation_ref& vr, string &ref,  vector<string> &alt);
    static void   GetVariationRefAlt(const CVariation& vr, string &ref,  vector<string> &alt);
	static string GetAlleleFromLoc(const CSeq_loc& loc, CScope& scope);

    //Do all alleles in variation ref have a common repeat unit
    // and thus possible to shift them.
    static bool CommonRepeatUnit(const CVariation_ref& vr);
    static bool CommonRepeatUnit(const CVariation& vr);
    static bool CommonRepeatUnit(string alt1, string alt2);

private:
    static string GetRefAlleleFromVP(CRef<CVariantPlacement> vp, CScope& scope, TSeqPos length);
    static void FixAlleles(CRef<CVariation> v, string old_ref, string new_ref);
    static void FixAlleles(CVariation_ref& vr, string old_ref, string new_ref) ;
    static const unsigned int MAX_LEN=1000;

    static bool x_isBaseRepeatingUnit(const string& candidate, const string& target);
    static string x_RepeatedSubstring(const string &str) ;
};

class CVariationNormalization_base_cache
{
public:
    static int x_GetSeqSize(CSeqVector &seqvec);
    static CRef<CSeqVector> x_PrefetchSequence(CScope &scope, const CSeq_id &seq_id,ENa_strand strand);
    static string x_GetSeq(int pos, int length, CSeqVector &seqvec);

protected:
    static void x_rotate_left(string &v);
    static void x_rotate_right(string &v);
    static string x_CompactifySeq(string a);

    static CCache<string,CRef<CSeqVector> > m_cache;
    static const unsigned int CCACHE_SIZE = 16;
};

template<class T>
class CVariationNormalization_base : public CVariationNormalization_base_cache
{
public:
    static void x_Shift(CVariation& var, CScope &scope);
	static void x_Shift(CSeq_annot& annot, CScope &scope);
	static void x_Shift(CSeq_feat& feat, CScope &scope);

    static bool x_ProcessShift(string &a, int &pos_left, int &pos_right, CSeqVector &seqvec, int type) {return T::x_ProcessShift(a,pos_left,pos_right,seqvec,type);}
    static void x_ModifyLocation(CSeq_loc &loc, CSeq_literal &literal, string a, int pos_left, int pos_right, int type) {T::x_ModifyLocation(loc,literal,a,pos_left,pos_right,type);}
    static bool x_IsShiftable(CSeq_loc &loc, string &ref, CScope &scope, int type);
    static void x_ExpandLocation(CSeq_loc &loc, int pos_left, int pos_right);

    struct SEndPosition {
        SEndPosition(int l, int r, int nl, int nr) :
            left(l), right(r), new_left(nl), new_right(nr) {}

        SEndPosition(const SEndPosition& sep) :
            left(sep.left), right(sep.right), 
            new_left(sep.new_left), new_right(sep.new_right) {}

        SEndPosition& operator=(SEndPosition sep)
        {
            swap(this->left,  sep.left);
            swap(this->right, sep.right);
            swap(this->new_left,  sep.new_left);
            swap(this->new_right, sep.new_right);

            return *this;
        }


        int left;
        int right;
        int new_left;
        int new_right;

        void CorrectNewPositions()
        {
            if (new_left == -1)
            {
                new_left = left;
            }
            if (new_right == -1)
            {
                new_right = right;
            }
        }
        bool HasBadPositions()
        {
            if (new_left != left)
            {
                ERR_POST(Error <<  "Left position is ambiguous due to different leaf alleles" << Endm);
                return true;
            }
            
            if (new_right != right)
            {
                ERR_POST(Error <<  "Right position is ambiguous due to different leaf alleles" << Endm);
                return true;
            }

            return false;
        }

        friend std::ostream& operator<< (ostream& stream, const SEndPosition& sep)
        {
            stream << sep.left << " " << sep.right 
                << " " << sep.new_left << " " << sep.new_right;

            return stream;
        }

        bool operator==(const SEndPosition& sep) const {
            if( left == sep.left &&
                right == sep.right &&
                new_left == sep.new_left &&
                new_right == sep.new_right)
                return true;
            return false;
        }

        //comp operator

    };

    static void x_ModifyDeleltionLocation(
        CSeq_loc& loc, SEndPosition& sep, string& ref, 
        CSeq_literal& ref_literal, 
        CSeqVector& seqvec, int type);

    static void x_ProcessInstance(CVariation_inst &inst, CSeq_loc &loc, bool &is_deletion,  
        CRef<CSeq_literal>& ref_seqliteral, string &ref, 
        SEndPosition& end_position, 
        CSeqVector& seqvec, int &rtype);    

};

class CVariationNormalizationLeft : public CVariationNormalization_base<CVariationNormalizationLeft>
{
public:
    static bool x_ProcessShift(string &a, int &pos_left, int &pos_right, CSeqVector &seqvec, int type);
    static void x_ModifyLocation(CSeq_loc &loc, CSeq_literal& literal, string a, int pos_left, int pos_right, int type);
};

class CVariationNormalizationInt : public CVariationNormalization_base<CVariationNormalizationInt>
{
public:
    static bool x_ProcessShift(string &a, int &pos_left, int &pos_right, CSeqVector &seqvec, int type);
    static void x_ModifyLocation(CSeq_loc &loc, CSeq_literal &literal, string a, int pos_left, int pos_right, int type);
};

class CVariationNormalizationRight : public CVariationNormalization_base<CVariationNormalizationRight>
{
public:
    static bool x_ProcessShift(string &a, int &pos_left, int &pos_right, CSeqVector &seqvec, int type);
    static void x_ModifyLocation(CSeq_loc &loc, CSeq_literal &literal, string a, int pos_left, int pos_right, int type);
};

class CVariationNormalizationLeftInt :  public CVariationNormalization_base<CVariationNormalizationLeftInt>
{
public:
    static bool x_ProcessShift(string &a, int &pos_left, int &pos_right, CSeqVector &seqvec, int type);
    static void x_ModifyLocation(CSeq_loc &loc, CSeq_literal &literal, string a, int pos_left, int pos_right, int type);
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
    
/////////////////////////////////////////////////////////
 //For later creation, as thin wrappers on the logic below
    //ASSUME scope has everything you need, but that lookups *can* fail so handle gracefully.
    static void NormalizeVariation(CVariation& var, ETargetContext target_ctxt, CScope& scope);     
    static void  NormalizeVariation(CSeq_annot& var, ETargetContext target_ctxt, CScope& scope);

/////////////////////////////////////////////////////////
 
 
//Initially, the following will be the public methods, each encapsulating a *step* in the normalization process.
//We can add the above wrappers and enum as a final touch. 
 
    //These methods search out ambiguous ins/del, and alter SeqLoc
    //to a Seq-equiv: the first is the original SeqLoc
    //the second is an interval representing the IOA
    static void NormalizeAmbiguousVars(CVariation& var, CScope &scope);
    static void NormalizeAmbiguousVars(CSeq_annot& var, CScope &scope);
 
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

    //Future thoughts:
    // Combine/collapse objects with same SeqLoc/Type
    // Simplify objects with common prefix in ref and all alts.
    // Identify mixed type objects, and split.

    static void AlterToVarLoc(CVariation& var, CScope& scope);
    static void AlterToVarLoc(CSeq_annot& var, CScope& scope);

    static bool IsShiftable(CSeq_loc &loc, string &ref, CScope &scope, int type); 
};
