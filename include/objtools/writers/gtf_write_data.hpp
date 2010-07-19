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

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ============================================================================
class CGtfRecord
//  ============================================================================
    : public CGff3WriteRecord
{
public: 
    CGtfRecord(
        CSeq_annot_Handle sah
    ): CGff3WriteRecord( sah ) {};

    ~CGtfRecord() {};

public:
    bool AssignFromAsn(
        const CSeq_feat& );

    void ForceType(
        const string& strType ) {
        m_strType = strType;
    };

    void SetCdsPhase(
        const list< CRef< CSeq_interval > >&,
        ENa_strand );
 
    CSeq_annot_Handle AnnotHandle() const {
        return m_Sah;
    };

    bool MakeChildRecord(
        const CGtfRecord&,
        const CSeq_interval&,
        unsigned int = 0 );

    string StrAttributes() const;
    string StrGeneId() const;
    string StrTranscriptId() const;
    string StrStructibutes() const;

    string GeneId() const { return m_strGeneId; };
    string TranscriptId() const { return m_strTranscriptId; };

    string SortTieBreaker() const { 
        return StrGeneId() + "|" + StrTranscriptId(); 
    };

protected:
    bool x_AssignAttributesFromAsnCore(
        const CSeq_feat& );

    bool x_AssignAttributesFromAsnExtended(
        const CSeq_feat& );

    static bool x_NeedsQuoting(
        const string& ) { return true; };

    string m_strGeneId;
    string m_strTranscriptId;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_WRITERS___GTF_WRITE_DATA__HPP
