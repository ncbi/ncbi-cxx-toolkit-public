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

#ifndef OBJTOOLS_WRITERS___GFF_FEATURE_RECORD__HPP
#define OBJTOOLS_WRITERS___GFF_FEATURE_RECORD__HPP

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/util/feature.hpp>

#include <objtools/writers/write_util.hpp>
#include <objtools/writers/feature_context.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ============================================================================
class NCBI_XOBJWRITE_EXPORT CGffFeatureRecord
//  ============================================================================
    : public CObject
{
public:
    typedef map<string, vector<string> > TAttributes;
    typedef TAttributes::iterator TAttrIt;
    typedef TAttributes::const_iterator TAttrCit;

public:
    CGffFeatureRecord( 
        const string& id="");
    CGffFeatureRecord(
        const CGffFeatureRecord&);
    virtual ~CGffFeatureRecord();

    void InitLocation(
        const CSeq_loc&);

    void SetSeqId(
        const string&);
    void SetRecordId(
        const string&);
    void SetSource(
        const string&);
    void SetFeatureType(
        const string&);
    void SetLocation(
        const CSeq_interval&);
    void SetLocation(
        unsigned int, //0-based seqstart
        unsigned int, //0-based seqstop
        ENa_strand = objects::eNa_strand_plus);
    void SetPhase(
        unsigned int);
    void SetStrand(
        ENa_strand);

    string StrType() const;
    string StrAttributes() const;
    string StrSeqId() const;
    string StrSource() const;
    string StrSeqStart() const;
    string StrSeqStop() const;
    string StrScore() const;
    string StrStrand() const;
    string StrPhase() const;
    string StrStructibutes() const { return ""; };

    const TAttributes& Attributes() const { 
        return m_Attributes; 
    };
    const CSeq_loc& Location() const {
        return *m_pLoc;
    };

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
    static const char* ATTR_SEPARATOR;
protected:
    void x_StrAttributesAppendValue(
        const string&,
        const string&,
        const string&,
        map<string, vector<string> >&,
        string& ) const;

    string m_strSeqId;
    string m_strType;
    string m_strSource;
    unsigned int m_uSeqStart;
    unsigned int m_uSeqStop;
    CRef<CSeq_loc> m_pLoc;
    string* m_pScore;
    ENa_strand* m_peStrand;
    unsigned int* m_puPhase;
    string m_strAttributes;    
    TAttributes m_Attributes;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_WRITERS___GFF_FEATURE_RECORD__HPP
