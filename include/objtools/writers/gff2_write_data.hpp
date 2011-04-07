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
 * Author: Frank Ludwig
 *
 * File Description:
 *   GFF3 transient data structures
 *
 */

#ifndef OBJTOOLS_WRITERS___GFF2DATA__HPP
#define OBJTOOLS_WRITERS___GFF2DATA__HPP

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/feature.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
class CGff2WriteRecord
//  ----------------------------------------------------------------------------
    : public CObject
{
public:
    typedef CCdregion::EFrame TFrame;
    typedef map<string, string> TAttributes;
    typedef TAttributes::iterator TAttrIt;
    typedef TAttributes::const_iterator TAttrCit;

public:
    CGff2WriteRecord(
        feature::CFeatTree&
    );

    CGff2WriteRecord(
        const CGff2WriteRecord&
    );

    virtual ~CGff2WriteRecord();

    //
    //  Input/output:
    //
    virtual bool AssignFromAsn(
        CMappedFeat );

    virtual bool AssignLocation(
        const CSeq_interval& );

    bool MergeRecord(
        const CGff2WriteRecord& );

    bool AssignType(
        const string& strType ) {
        m_strType = strType;
        return true;
    };

    bool AssignSequenceNumber(
        unsigned int,
        const string& = "" );

    virtual string StrType() const;
    virtual string StrAttributes() const;
    virtual string StrId() const;
    virtual string StrSource() const;
    virtual string StrSeqStart() const;
    virtual string StrSeqStop() const;
    virtual string StrScore() const;
    virtual string StrStrand() const;
    virtual string StrPhase() const;
    virtual string StrStructibutes() const { return ""; };

    //
    // Accessors:
    //        
    string Id() const { 
        return m_strId; 
    };
    size_t SeqStart() const { 
        return m_uSeqStart; 
    };
    size_t SeqStop() const { 
        return m_uSeqStop; 
    };
    string Source() const { 
        return m_strSource; 
    };
    string Type() const { 
        return m_strType; 
    };
    double Score() const { 
        return IsSetScore() ? *m_pdScore : 0.0; 
    };
    ENa_strand Strand() const { 
        return IsSetStrand() ? *m_peStrand : eNa_strand_unknown; 
    };
    unsigned int Phase() const {
        return IsSetPhase() ? *m_puPhase : 0; 
    };
    virtual string SortTieBreaker() const { 
        return ""; 
    };

    bool IsSetScore() const { 
        return m_pdScore != 0; 
    };
    bool IsSetStrand() const { 
        return m_peStrand != 0; 
    };
    bool IsSetPhase() const { 
        return m_puPhase != 0; 
    };

    const TAttributes& Attributes() const { 
        return m_Attributes; 
    };

    bool GetAttribute(
        const string&,
        string& ) const;

protected:
    virtual bool x_AssignType(
        CMappedFeat );

    virtual bool x_AssignAttributes(
        CMappedFeat );

    //
    //  Feature level:
    //
    virtual bool x_AssignAttributesGene(
        CMappedFeat );

    virtual bool x_AssignAttributesMrna(
        CMappedFeat );

    virtual bool x_AssignAttributesCds(
        CMappedFeat );

    virtual bool x_AssignAttributesMiscFeature(
        CMappedFeat );

    //
    //  Qualifier level:
    //
    virtual bool x_AssignAttributeNote(
        CMappedFeat );

    virtual bool x_AssignAttributePartial(
        CMappedFeat );

    virtual bool x_AssignAttributePseudo(
        CMappedFeat );

    virtual bool x_AssignAttributeDbXref(
        CMappedFeat );

    virtual bool x_AssignAttributeGene(
        CMappedFeat );

    virtual bool x_AssignAttributeGeneSynonym(
        CMappedFeat );

    virtual bool x_AssignAttributeLocusTag(
        CMappedFeat );

    virtual bool x_AssignAttributeProduct(
        CMappedFeat );

    virtual bool x_AssignAttributeAllele(
        CMappedFeat );

    virtual bool x_AssignAttributeMap(
        CMappedFeat );

    virtual bool x_AssignAttributeCodonStart(
        CMappedFeat );

    //
    //  Helper functions:
    //
    static string x_GeneRefToGene(
        const CGene_ref& );

    static string x_GeneRefToLocusTag(
        const CGene_ref& );

    static string x_GeneRefToGeneSyn(
        const CGene_ref& );

    CMappedFeat x_GetGeneParent(
        CMappedFeat );

    //
    //
    //
    bool x_AssignTypeFromAsn(
        CMappedFeat );
    bool x_AssignSeqIdFromAsn(
        CMappedFeat );
    bool x_AssignStartFromAsn(
        CMappedFeat );
    bool x_AssignStopFromAsn(
        CMappedFeat );
    bool x_AssignSourceFromAsn(
        CMappedFeat );
    bool x_AssignScoreFromAsn(
        CMappedFeat );
    bool x_AssignStrandFromAsn(
        CMappedFeat );
    bool x_AssignPhaseFromAsn(
        CMappedFeat );

    static string x_FeatIdString(
        const CFeat_id& id );

    CSeq_feat::TData::ESubtype x_GetSubtypeOf(
//        const CSeq_annot&,
        const CFeat_id& );
        
    static bool x_IsParentOf(
        CSeq_feat::TData::ESubtype,
        CSeq_feat::TData::ESubtype );

    static bool x_NeedsQuoting(
        const string& );

    static string x_MakeGffDbtag( 
        const CDbtag& dbtag );

    virtual void x_PriorityProcess(
        const string&,
        map<string, string >&,
        string& ) const;

    //
    // Data:
    //
    feature::CFeatTree& m_feat_tree;
    string m_strId;
    size_t m_uSeqStart;
    size_t m_uSeqStop;
    string m_strSource;
    string m_strType;
    double* m_pdScore;
    ENa_strand* m_peStrand;
    unsigned int* m_puPhase;
    string m_strAttributes;    
    TAttributes m_Attributes;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_WRITERS___GFF2DATA__HPP
