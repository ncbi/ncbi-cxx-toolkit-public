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
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/util/feature.hpp>

#include <objtools/writers/write_util.hpp>
#include <objtools/writers/feature_context.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ============================================================================
class NCBI_XOBJWRITE_EXPORT CGffWriteRecord
//  ============================================================================
    : public CObject
{
public:
    typedef map<string, vector<string> > TAttributes;
    typedef TAttributes::iterator TAttrIt;
    typedef TAttributes::const_iterator TAttrCit;

public:
    CGffWriteRecord( 
        CGffFeatureContext& fc,
        const string& id="" );
    CGffWriteRecord(
        const CGffWriteRecord& );
    virtual ~CGffWriteRecord();

    //
    //  Input/output:
    //
    virtual bool CorrectLocation(
        const CGffWriteRecord&,
        const CSeq_interval&,
        unsigned int );

    bool CorrectType(
        const string& strType ) {
        m_strType = strType;
        return true;
    };

    bool CorrectPhase(
        int );

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

    const TAttributes& Attributes() const { 
        return m_Attributes; 
    };

    bool SetAttribute(
        const string&,
        const string& );

    bool GetAttribute(
        const string&,
        vector<string>& ) const;

    bool DropAttribute(
        const string& );

    virtual bool NeedsQuoting(
        const string& str) const {return CWriteUtil::NeedsQuoting(str);};

protected:
    virtual void x_StrAttributesAppendValue(
        const string&,
        const string&,
        const string&,
        map<string, vector<string> >&,
        string& ) const;

    CGffFeatureContext& m_fc;

    string m_strId;
    unsigned int m_uSeqStart;
    unsigned int m_uSeqStop;
    string m_strSource;
    string m_strType;
    string* m_pScore;
    ENa_strand* m_peStrand;
    unsigned int* m_puPhase;
    string m_strAttributes;    
    TAttributes m_Attributes;

    static const char* ATTR_SEPARATOR;
};

//  ============================================================================
class CGffWriteRecordFeature
//  ============================================================================
    : public CGffWriteRecord
{
public:
    CGffWriteRecordFeature(
        CGffFeatureContext& fc,
        const string& id="" ): CGffWriteRecord(fc, id){};

    virtual bool AssignFromAsn(
        CMappedFeat,
        unsigned int =0);

protected:
    virtual bool x_AssignType(
        CMappedFeat,
        unsigned int =0 );
    virtual bool x_AssignSeqId(
        CMappedFeat );
    virtual bool x_AssignStart(
        CMappedFeat );
    virtual bool x_AssignStop(
        CMappedFeat );
    virtual bool x_AssignSource(
        CMappedFeat );
    virtual bool x_AssignScore(
        CMappedFeat );
    virtual bool x_AssignStrand(
        CMappedFeat );
    virtual bool x_AssignPhase(
        CMappedFeat );
    virtual bool x_AssignAttributes(
        CMappedFeat,
        unsigned int =0);
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_WRITERS___GFF2DATA__HPP
