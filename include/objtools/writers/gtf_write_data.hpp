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
 *   GTF transient data structures
 *
 */

#ifndef OBJTOOLS_WRITERS___GTF_WRITE_DATA__HPP
#define OBJTOOLS_WRITERS___GTF_WRITE_DATA__HPP

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/util/feature.hpp>

#include <objtools/writers/gff2_write_data.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ============================================================================
class CGtfRecord
//  ============================================================================
    : public CGffWriteRecordFeature
{
public:
    CGtfRecord(
        CGffFeatureContext& fc,
        bool bNoExonNumbers = false ) :
    CGffWriteRecordFeature(fc),
    m_bNoExonNumbers(bNoExonNumbers) {};

    ~CGtfRecord() {};

public:
    void SetCdsPhase(
        const list< CRef< CSeq_interval > >&,
        ENa_strand );

    void SetCdsPhase_Force(
        int phase) {
            mPhase = NStr::NumericToString(phase); };

    unsigned int GetExtent() const {
        return (mSeqStop - mSeqStart + 1);
    };

    bool MakeChildRecord(
        const CGtfRecord&,
        const CSeq_interval&,
        unsigned int = 0 );

    void SetGeneId(
        const std::string& geneId) { m_strGeneId = geneId; };
    void SetTranscriptId(
        const std::string& transcriptId) { m_strTranscriptId = transcriptId; };
    void SetExonNumber(
        unsigned int exonNumber)
    {
        SetAttribute("exon_number", NStr::NumericToString(exonNumber));
    };
    void SetPartNumber(
        unsigned int partNumber)
    {
        SetAttribute("part", NStr::NumericToString(partNumber));
    };

    string StrAttributes() const;
    string StrStructibutes() const;

    string GeneId() const { return m_strGeneId; };
    string TranscriptId() const { return m_strTranscriptId; };

    feature::CFeatTree& FeatTree() { return m_fc.FeatTree(); };

protected:
    static string x_AttributeToString(
        const string&,
        const string& );

    string m_strGeneId;
    string m_strTranscriptId;
    bool m_bNoExonNumbers;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_WRITERS___GTF_WRITE_DATA__HPP
