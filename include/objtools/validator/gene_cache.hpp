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
 *   Gene cache for validating features
 *   .......
 *
 */

#ifndef VALIDATOR___GENE_CACHE__HPP
#define VALIDATOR___GENE_CACHE__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_autoinit.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/feat_ci.hpp>  // for CMappedFeat
#include <objmgr/util/seq_loc_util.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/GIBB_mol.hpp>
#include <util/strsearch.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Std_seg.hpp>
#include <objects/seqalign/Packed_seg.hpp>
#include <objects/valid/Comment_set.hpp>
#include <objects/valid/Comment_rule.hpp>
#include <objects/taxon3/taxon3.hpp>

#include <objtools/validator/validator.hpp>
#include <objtools/validator/tax_validation_and_cleanup.hpp>
#include <objtools/validator/utilities.hpp>
#include <objtools/validator/feature_match.hpp>

#include <objtools/alnmgr/sparse_aln.hpp>

#include <objmgr/util/create_defline.hpp>

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


class CGeneCache {
public:
    CGeneCache() {};
    ~CGeneCache() {};

    CConstRef<CSeq_feat> GetGeneFromCache(const CSeq_feat* feat, CScope& scope);
    CConstRef<CSeq_feat> GetmRNAFromCache(const CSeq_feat* feat, CScope& scope);
    CRef<feature::CFeatTree> GetFeatTreeFromCache(const CSeq_loc& loc, CScope& scope);
    CRef<feature::CFeatTree> GetFeatTreeFromCache(const CSeq_feat& feat, CScope& scope);
    CRef<feature::CFeatTree> GetFeatTreeFromCache(CBioseq_Handle bh);
    void Clear() { m_FeatGeneMap.clear(); m_SeqTreeMap.clear(); }
    bool IsPseudo(const CSeq_feat& feat, CScope& scope);

private:

    typedef map<const CSeq_feat*, CConstRef<CSeq_feat> > TFeatGeneMap;
    TFeatGeneMap            m_FeatGeneMap;

    typedef map<CBioseq_Handle, CRef<feature::CFeatTree> > TSeqTreeMap;
    TSeqTreeMap m_SeqTreeMap;

    static bool x_IsPseudo(const CGene_ref& gref);
    static bool x_HasNamedQual(const CSeq_feat& feat, const string& qual);
};




END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif  /* VALIDATOR___GENE_CACHE__HPP */
