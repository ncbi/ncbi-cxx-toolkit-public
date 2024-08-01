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
 * Authors:  Frank Ludwig
 *
 * File Description:  Sequence Ontology Type Mapping
 *
 */

#ifndef OBJECTS___SOMAP__HPP
#define OBJECTS___SOMAP__HPP

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

class CSeq_feat;

//  ============================================================================
class NCBI_SEQ_EXPORT CSoMap
//  ============================================================================
{
public:
    static string SoTypeToId(std::string_view);
    static string SoIdToType(std::string_view);
    static bool SoTypeToFeature(std::string_view, CSeq_feat&, bool = false);
    static bool FeatureToSoType(const CSeq_feat&, string&);

    static bool GetSupportedSoTerms(vector<string>&);
    static string ResolveSoAlias(std::string_view);
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJECTS___SOMAP__HPP
