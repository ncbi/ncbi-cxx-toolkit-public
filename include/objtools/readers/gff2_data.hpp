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

#ifndef OBJTOOLS_READERS___GFF2DATA__HPP
#define OBJTOOLS_READERS___GFF2DATA__HPP

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
class CGff2Record
//  ----------------------------------------------------------------------------
{
public:
    typedef CCdregion::EFrame TFrame;
    typedef map<string, string> TAttributes;
    typedef TAttributes::iterator TAttrIt;
    typedef TAttributes::const_iterator TAttrCit;

public:
    CGff2Record();
    virtual ~CGff2Record();

    //
    //  Input/output:
    //
    virtual bool AssignFromGff(
        const string& );

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
    TFrame Phase() const {
        return IsSetPhase() ? *m_pePhase : CCdregion::eFrame_not_set; 
    };
    string AttributesLiteral() const { 
        return m_strAttributes; 
    };

    bool IsSetScore() const { 
        return m_pdScore != 0; 
    };
    bool IsSetStrand() const { 
        return m_peStrand != 0; 
    };
    bool IsSetPhase() const { 
        return m_pePhase != 0; 
    };
    bool IsAlignmentRecord() const {
        if (NStr::StartsWith(Type(), "match")) {
            return true;
        }
        return false;
    };
    CRef<CSeq_id> GetSeqId(
        int ) const;
    CRef<CSeq_loc> GetSeqLoc(
        int ) const;

    const TAttributes& Attributes() const { 
        return m_Attributes; 
    };

    bool GetAttribute(
        const string&,
        string& ) const;

    bool GetAttribute(
        const string&,
        list<string>& ) const;

    virtual bool InitializeFeature(
        int,
        CRef<CSeq_feat> ) const; 

    virtual bool UpdateFeature(
        int,
        CRef<CSeq_feat> ) const;

    static void TokenizeGFF(vector<CTempString>& columns, const CTempString& line);
protected:
    virtual bool x_AssignAttributesFromGff(
        const string& );

    static string x_NormalizedAttributeKey(
        const string& );

    static string x_NormalizedAttributeValue(
        const string& );

	bool x_SplitGffAttributes(
		const string&,
		vector< string >& ) const;

    virtual bool x_MigrateId(
        CRef<CSeq_feat> ) const;

    virtual bool x_MigrateStartStopStrand(
        CRef<CSeq_feat> ) const;

    virtual bool x_MigrateType(
        CRef<CSeq_feat> ) const;

    virtual bool x_MigrateScore(
        CRef<CSeq_feat> ) const;

    virtual bool x_MigratePhase(
        CRef<CSeq_feat> ) const;

    virtual bool x_MigrateAttributes(
        int,
        CRef<CSeq_feat> ) const;

    virtual bool x_InitFeatureLocation(
        int,
        CRef<CSeq_feat> ) const;

    virtual bool x_InitFeatureData(
        int,
        CRef<CSeq_feat> ) const;

    virtual bool x_MigrateAttributesSubSource(
        int,
        CRef<CSeq_feat>,
        TAttributes& ) const;

    virtual bool x_MigrateAttributesOrgName(
        int,
        CRef<CSeq_feat>,
        TAttributes& ) const;

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
    TFrame* m_pePhase;
    string m_strAttributes;    
    TAttributes m_Attributes;

};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___GFF2DATA__HPP
