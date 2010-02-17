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
 * File Description:  Write gff file
 *
 */

#ifndef OBJTOOLS_WRITERS___GFF_RECORD__HPP
#define OBJTOOLS_READERS___GFF_RECORD__HPP

#include <corelib/ncbistd.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE
//  ============================================================================
class NCBI_XOBJWRITE_EXPORT CGffRecord
//  ============================================================================
{
public:
    CGffRecord();
    ~CGffRecord() {};
    
    bool SetRecord(
        const CRef<CSeq_annot>& annot,
        const CRef< CSeq_feat > );
        
    void DumpRecord(
        CNcbiOstream& );
        
protected:
    static string FeatIdString(
        const CFeat_id& id );

    bool AssignType(
        const CRef< CSeq_feat > pFeature );
    bool AssignSeqId(
        const CRef< CSeq_feat > pFeature );
    bool AssignStart(
        const CRef< CSeq_feat > pFeature );
    bool AssignStop(
        const CRef< CSeq_feat > pFeature );
    bool AssignSource(
        const CRef< CSeq_feat > pFeature );
    bool AssignScore(
        const CRef< CSeq_feat > pFeature );
    bool AssignStrand(
        const CRef< CSeq_feat > pFeature );
    bool AssignPhase(
        const CRef< CSeq_feat > pFeature );
    bool AssignAttributesCore(
        const CRef<CSeq_annot>& annot,
        const CRef< CSeq_feat > pFeature );
    bool AssignAttributesExtended(
        const CRef< CSeq_feat > pFeature );

    void AddAttribute( 
        const string& key,
        const string& value );

    static CSeq_feat::TData::ESubtype GetSubtypeOf(
        const CRef<CSeq_annot>& annot,
        const CFeat_id& );
        
    static bool IsParentOf(
        CSeq_feat::TData::ESubtype,
        CSeq_feat::TData::ESubtype );
        
    string m_strSeqId;
    string m_strSource;
    string m_strType;
    string m_strStart;
    string m_strEnd;
    string m_strScore;
    string m_strStrand;
    string m_strPhase;
    string m_strAttributes;

    map< string, CSeq_feat::TData::ESubtype > m_IdToTypeMap;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJTOOLS_WRITERS___GFF_RECORD__HPP
