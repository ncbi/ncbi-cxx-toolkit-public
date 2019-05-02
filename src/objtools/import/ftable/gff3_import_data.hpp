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

#ifndef GFF3_IMPORT_DATA__HPP
#define GFF3_IMPORT_DATA__HPP

#include <corelib/ncbifile.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Cdregion.hpp>

#include "feat_import_data.hpp"

#include <map>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

//  ============================================================================
class CGff3ImportData:
    public CFeatImportData
//  ============================================================================
{
public:
    using ATTRIBUTES = std::map<std::string, std::vector<std::string> >;

    CGff3ImportData(
        const CIdResolver&,
        CImportMessageHandler&);

    CGff3ImportData(
        const CGff3ImportData& rhs);

    virtual ~CGff3ImportData() {};

    void
    Initialize(
        const std::string&,
        const std::string&,
        const std::string&,
        TSeqPos,
        TSeqPos,
        bool, double,
        ENa_strand,
        const std::string&,
        const std::vector<std::pair<std::string, std::string>>&);

    virtual void Serialize(
        CNcbiOstream&) override;

    CRef<CSeq_feat>
    GetData() const;

    std::string
    Id() const { return mId; };

    std::string
    Parent() const { return mParent; };

    std::string
    Source() const { return mSource; };

    bool
    HasScore() const { return static_cast<bool>(mpScore); };

    double
    Score() const { return HasScore() ? *mpScore : 0; };
private:
    void xInitializeAttributes(
        const std::vector<std::pair<std::string, std::string>>&);

    bool xInitializeDbxref(
        const std::string&,
        const std::string&);

    bool xInitializeComment(
        const std::string&,
        const std::string&);

    bool xInitializeDataGene(
        const std::string&,
        const std::string&);

    bool xInitializeDataRna(
        const std::string&,
        const std::string&);

    bool xInitializeDataCds(
        const std::string&,
        const std::string&);

    bool xInitializeMultiValue(
        const std::string&,
        const std::string&);

    CRef<CSeq_feat> mpFeat;
    string mId;
    string mParent;
    string mSource;
    unique_ptr<double> mpScore;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif
