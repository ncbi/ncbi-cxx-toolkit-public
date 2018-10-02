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

#ifndef FIVECOL_IMPORT_DATA__HPP
#define FIVECOL_IMPORT_DATA__HPP

#include <corelib/ncbifile.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

//#include <util/line_reader.hpp>

#include "feat_import_data.hpp"

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

//  ============================================================================
class C5ColImportData:
    public CFeatImportData
//  ============================================================================
{
public:
    C5ColImportData(
        const CIdResolver&,
        CFeatMessageHandler&);

    C5ColImportData(
        const C5ColImportData& rhs);

    virtual ~C5ColImportData() {};

    virtual void InitializeFrom(
        const std::vector<std::string>&) override;

    virtual void Serialize(
        CNcbiOstream&) override;

    const CSeq_feat& GetFeature() const { return *mpFeature; };

protected:
    void
    xFeatureInit(
        const std::string&,
        const std::string&);

    void
    xFeatureSetType(
        const std::string&);

    void
    xFeatureSetInterval(
        const std::string&,
        const std::string&);

    void
    xFeatureAddInterval(
        const std::string&,
        const std::string&);

    void
    xFeatureAddAttribute(
        const std::string&,
        const std::string&);

    void
    xParseInterval(
        const std::string&,
        const std::string&,
        CSeq_interval&);

    CRef<CSeq_feat> mpFeature;
    CRef<CSeq_id> mpId;
    TSeqPos mOffset;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif
