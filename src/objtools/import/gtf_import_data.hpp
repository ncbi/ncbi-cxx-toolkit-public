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

#ifndef GTF_IMPORT_DATA__HPP
#define GTF_IMPORT_DATA__HPP

#include <corelib/ncbifile.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include "feat_import_data.hpp"

#include <map>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

//  ============================================================================
class CGtfImportData:
    public CFeatImportData
//  ============================================================================
{
public:
    using ATTRIBUTES = std::map<std::string, std::vector<std::string> >;

    CGtfImportData(
        const CIdResolver&);

    CGtfImportData(
        const CGtfImportData& rhs);

    virtual ~CGtfImportData() {
        delete mpScore; };

    virtual void InitializeFrom(
        const std::vector<std::string>&) override;

    virtual void Serialize(
        CNcbiOstream&) override;

    const std::string& Source() const { return mSource; };
    const std::string& Type() const { return mType; };
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

    const CSeq_loc& Location() const { return mLocation; };

    void
    AdjustFeatureType(
        const std::string& type) { mType = type; };

protected:
    void xSetSource(
        const std::string&);
    void xSetType(
        const std::string&);
    void xSetScore(
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

    void xSetLocation(
        const std::string&,
        const std::string&,
        const std::string&,
        const std::string&);

protected:
    CSeq_loc mLocation;
    //std::string mSeqId;
    std::string mSource;
    std::string mType;
    double* mpScore;
    int* mpFrame;

    ATTRIBUTES mAttributes;
    std::string mGeneId;
    std::string mTranscriptId;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif
