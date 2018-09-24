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
* Author:  Frank Ludwig, NCBI
*
* File Description:
*   Test application for the CFormatGuess component
*
* ===========================================================================
*/

#ifndef GTF_RECORD__HPP
#define GTF_RECORD__HPP

#include <corelib/ncbifile.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <util/line_reader.hpp>

#include <objtools/import/id_resolver.hpp>

#include <map>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

//  ============================================================================
class CGtfRecord
//  ============================================================================
{
public:
    using ATTRIBUTES = std::map<std::string, std::vector<std::string> >;

    CGtfRecord(
        const CIdResolver&);

    CGtfRecord(
        const CGtfRecord& rhs);

    virtual ~CGtfRecord() {
        delete mpScore; };

    virtual void InitializeFrom(
        const std::vector<std::string>&);

    virtual void Serialize(
        CNcbiOstream&);

    std::string SeqId() const { return mSeqId; };
    std::string Source() const { return mSource; };
    std::string Type() const { return mType; };
    TSeqPos SeqStart() const { return mSeqStart; };
    TSeqPos SeqStop() const { return mSeqStop; };
    ENa_strand SeqStrand() const { return mSeqStrand; };
    bool IsSetScore() const { return mpScore; };
    double Score() const { return *mpScore; };
    bool IsSetFrame() const { return mpFrame; };
    int Frame() const { return *mpFrame; };
    std::string GeneId() const { return mGeneId; };
    std::string TranscriptId() const { return mTranscriptId; };
    const ATTRIBUTES& Attributes() const { return mAttributes; };

    string 
    AttributeValueOf(
        const std::string&) const;

    CRef<CSeq_id> 
    SeqIdRef() const;

    CRef<CSeq_loc> 
    LocationRef() const;

    void
    AdjustFeatureType(
        const std::string& type) { mType = type; };

protected:
    void xSetSeqId(
        const std::string&);
    void xSetSource(
        const std::string&);
    void xSetType(
        const std::string&);
    void xSetSeqStart(
        const std::string&);
    void xSetSeqStop(
        const std::string&);
    void xSetScore(
        const std::string&);
    void xSetSeqStrand(
        const std::string&);
    void xSetFrame(
        const std::string&);
    void xSetAttributes(
        const std::string&);

    void xSplitAttributeString(
        const std::string&,
        std::vector<std::string>&);
    void xImportSplitAttribute(
        const std::string&);

protected:
    const CIdResolver& mIdResolver;
    string mSeqId;
    string mSource;
    string mType;
    TSeqPos mSeqStart;
    TSeqPos mSeqStop;
    double* mpScore;
    ENa_strand mSeqStrand;
    int* mpFrame;

    ATTRIBUTES mAttributes;
    std::string mGeneId;
    std::string mTranscriptId;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif
