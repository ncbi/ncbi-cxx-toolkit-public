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
 *`
 * Author:  Colleen Bollin
 *
 * File Description:
 *   For validating splice sites
 *   .......
 *
 */

#ifndef VALIDATOR___SPLICE_PROBLEMS__HPP
#define VALIDATOR___SPLICE_PROBLEMS__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_autoinit.hpp>

#include <objmgr/scope.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>

#include <objmgr/util/feature.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CCit_sub;
class CCit_art;
class CCit_gen;
class CSeq_feat;
class CBioseq;
class CSeqdesc;
class CSeq_annot;
class CTrna_ext;
class CProt_ref;
class CSeq_loc;
class CFeat_CI;
class CPub_set;
class CAuth_list;
class CTitle;
class CMolInfo;
class CUser_object;
class CSeqdesc_CI;
class CDense_diag;
class CDense_seg;
class CSeq_align_set;
class CPubdesc;
class CBioSource;
class COrg_ref;
class CDelta_seq;
class CGene_ref;
class CCdregion;
class CRNA_ref;
class CImp_feat;
class CSeq_literal;
class CBioseq_Handle;
class CSeq_feat_Handle;
class CCountries;
class CInferencePrefixList;
class CComment_set;
class CTaxon3_reply;
class ITaxon3;
class CT3Error;

BEGIN_SCOPE(validator)

class CValidError_imp;
class CTaxValidationAndCleanup;
class CGeneCache;
class CValidError_base;

typedef Char(&TSpliceSite)[2];

// =============================  Validate SeqFeat  ============================



class NCBI_VALIDATOR_EXPORT CSpliceProblems {
public:
    CSpliceProblems() : m_ExceptionUnnecessary(false), m_ErrorsNotExpected(false) {};
    ~CSpliceProblems() {};

    void CalculateSpliceProblems(const CSeq_feat& feat, bool check_all, bool pseudo, CBioseq_Handle loc_handle);

    // first is problem flags, second is position
    typedef pair<size_t, TSeqPos> TSpliceProblem;
    typedef vector<TSpliceProblem> TSpliceProblemList;

    typedef enum {
        eSpliceSiteRead_OK = 0,
        eSpliceSiteRead_BadSeq,
        eSpliceSiteRead_Gap,
        eSpliceSiteRead_OutOfRange,
        eSpliceSiteRead_WrongNT
    } ESpliceSiteRead;

    bool SpliceSitesHaveErrors();
    bool IsExceptionUnnecessary() const { return m_ExceptionUnnecessary; }
    bool AreErrorsUnexpected() const { return m_ErrorsNotExpected; }
    const TSpliceProblemList& GetDonorProblems() const { return m_DonorProblems; }
    const TSpliceProblemList& GetAcceptorProblems() const { return m_AcceptorProblems; }

private:
    TSpliceProblemList m_DonorProblems;
    TSpliceProblemList m_AcceptorProblems;
    bool m_ExceptionUnnecessary;
    bool m_ErrorsNotExpected;

    void ValidateSpliceCdregion(const CSeq_feat& feat, const CBioseq_Handle& bsh, ENa_strand strand);
    void ValidateSpliceMrna(const CSeq_feat& feat, const CBioseq_Handle& bsh, ENa_strand strand);
    void ValidateSpliceExon(const CSeq_feat& feat, const CBioseq_Handle& bsh, ENa_strand strand);
    void ValidateDonorAcceptorPair(ENa_strand strand, TSeqPos stop, const CSeqVector& vec_donor, TSeqPos seq_len_donor,
        TSeqPos start, const CSeqVector& vec_acceptor, TSeqPos seq_len_acceptor);


    ESpliceSiteRead ReadDonorSpliceSite(ENa_strand strand, TSeqPos stop, const CSeqVector& vec, TSeqPos seq_len, TSpliceSite& site);
    ESpliceSiteRead ReadDonorSpliceSite(ENa_strand strand, TSeqPos stop, const CSeqVector& vec, TSeqPos seq_len);
    ESpliceSiteRead ReadAcceptorSpliceSite(ENa_strand strand, TSeqPos start, const CSeqVector& vec, TSeqPos seq_len, TSpliceSite& site);
    ESpliceSiteRead ReadAcceptorSpliceSite(ENa_strand strand, TSeqPos start, const CSeqVector& vec, TSeqPos seq_len);
};


const string kSpliceSiteGTAG = "GT-AG";
const string kSpliceSiteGCAG = "GC-AG";
const string kSpliceSiteATAC = "AT-AC";
const string kSpliceSiteGT = "GT";
const string kSpliceSiteGC = "GC";
const string kSpliceSiteAG = "AG";

typedef Char const (&TConstSpliceSite)[2];

bool CheckAdjacentSpliceSites(const string& signature, ENa_strand strand, TConstSpliceSite donor, TConstSpliceSite acceptor);
bool CheckSpliceSite(const string& signature, ENa_strand strand, TConstSpliceSite site);
bool CheckIntronSpliceSites(ENa_strand strand, TConstSpliceSite donor, TConstSpliceSite acceptor);
bool CheckIntronDonor(ENa_strand strand, TConstSpliceSite donor);
bool CheckIntronAcceptor(ENa_strand strand, TConstSpliceSite acceptor);




END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* VALIDATOR___SPLICE_PROBLEMS__HPP */
