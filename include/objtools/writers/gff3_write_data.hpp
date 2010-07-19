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

#ifndef OBJTOOLS_WRITERS___GFF3DATA__HPP
#define OBJTOOLS_WRITERS___GFF3DATA__HPP

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
class CGff3WriteRecord
//  ----------------------------------------------------------------------------
{
public:
    typedef CCdregion::EFrame TFrame;
    typedef map<string, string> TAttributes;
    typedef TAttributes::iterator TAttrIt;
    typedef TAttributes::const_iterator TAttrCit;

public:
    CGff3WriteRecord(
        CSeq_annot_Handle sah
    );
    virtual ~CGff3WriteRecord();

    //
    //  Input/output:
    //
    virtual bool AssignFromAsn(
        const CSeq_feat& );

    bool MakeExon(
        const CGff3WriteRecord&,
        const CSeq_interval& );

    bool MergeRecord(
        const CGff3WriteRecord& );

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
    bool x_AssignTypeFromAsn(
        const CSeq_feat& );
    bool x_AssignSeqIdFromAsn(
        const CSeq_feat& );
    bool x_AssignStartFromAsn(
        const CSeq_feat& );
    bool x_AssignStopFromAsn(
        const CSeq_feat& );
    bool x_AssignSourceFromAsn(
        const CSeq_feat& );
    bool x_AssignScoreFromAsn(
        const CSeq_feat& );
    bool x_AssignStrandFromAsn(
        const CSeq_feat& );
    bool x_AssignPhaseFromAsn(
        const CSeq_feat& );
    virtual bool x_AssignAttributesFromAsnCore(
        const CSeq_feat& );
    virtual bool x_AssignAttributesFromAsnExtended(
        const CSeq_feat& );

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

    virtual void x_PriorityProcess(
        const string&,
        map<string, string >&,
        string& ) const;

    //
    // Data:
    //
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

    CSeq_annot_Handle m_Sah;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_WRITERS___GFF3DATA__HPP
