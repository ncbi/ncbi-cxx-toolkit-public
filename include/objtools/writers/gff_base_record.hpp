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
 *   GFF record structure
 *
 */

#ifndef OBJTOOLS_WRITERS___GFF_BASE_RECORD__HPP
#define OBJTOOLS_WRITERS___GFF_BASE_RECORD__HPP

#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqalign/Score.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ============================================================================
class NCBI_XOBJWRITE_EXPORT CGffBaseRecord
//  ============================================================================
    : public CObject
{
public:
    typedef map<string, vector<string> > TAttributes;
    typedef TAttributes::iterator TAttrIt;
    typedef TAttributes::const_iterator TAttrCit;

    typedef map<string, string> TScores;
    typedef TScores::iterator TScoreIt;
    typedef TScores::const_iterator TScoreCit;

    CGffBaseRecord( 
        const string& id="");
    CGffBaseRecord(
        const CGffBaseRecord&);
    virtual ~CGffBaseRecord();

public: //attribute management
    bool AddAttribute(
        const string&,
        const string&);
    bool AddAttributes(
        const string&,
        const vector<string>&);
    bool SetAttribute(
        const string&,
        const string&);
    bool SetAttributes(
        const string&,
        const vector<string>&);
    bool GetAttributes(
        const string&,
        vector<string>&) const;
    bool DropAttributes(
        const string& );

public:
    void SetRecordId(
        const string&);
    void SetSeqId(
        const string&);
    void SetMethod(
        const string&);
    void SetType(
        const string&);
    void SetLocation(
        unsigned int, //0-based seqstart
        unsigned int, //0-based seqstop
        ENa_strand = objects::eNa_strand_unknown);
    void SetStrand(
        ENa_strand);
    void SetScore(
        const CScore&);
    void SetPhase(
        unsigned int);

    virtual string StrSeqId() const;
    virtual string StrMethod() const;
    virtual string StrType() const;
    virtual string StrSeqStart() const;
    virtual string StrSeqStop() const;
    virtual string StrStrand() const;
    virtual string StrScore() const;
    virtual string StrPhase() const;
    virtual string StrAttributes() const;

protected:
    static const char* ATTR_SEPARATOR;
    CRef<CSeq_loc> m_pLoc;

    string mSeqId;
    string mType;
    string mMethod;
    unsigned int mSeqStart;
    unsigned int mSeqStop;
    string mScore;
    string mStrand;
    string mPhase;
    TAttributes mAttributes;
    TScores mExtraScores;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_WRITERS___GFF_BASE_RECORD__HPP
