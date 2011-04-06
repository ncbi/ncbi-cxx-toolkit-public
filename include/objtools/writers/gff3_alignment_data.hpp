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

#ifndef OBJTOOLS_WRITERS___GFF3ALIGNMENTDATA__HPP
#define OBJTOOLS_WRITERS___GFF3ALIGNMENTDATA__HPP

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
//#include <objmgr/util/feature.hpp>
#include <objtools/alnmgr/alnmap.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
class CGffAlignmentRecord
//  ----------------------------------------------------------------------------
{
public:
    CGffAlignmentRecord(
            unsigned int uFlags =0,
            unsigned int uRecordId =0 ):
        m_uFlags( uFlags ),
        m_strId( "" ),
        m_pdScore( 0 ),
        m_peStrand( 0 ),
        m_puPhase( 0 )
    {
        if ( uRecordId ) {
            m_strAttributes = string( "ID=" ) + NStr::UIntToString( uRecordId );
        }
    };

    virtual ~CGffAlignmentRecord() {
        delete m_pdScore;
        delete m_peStrand;
        delete m_puPhase;
    };

    void SetSourceLocation( 
        const CSeq_id&,
        ENa_strand );

    void SetTargetLocation( 
        const CSeq_id&,
        ENa_strand );

    void SetScore(
        const CScore& );

    void SetPhase(
        unsigned int );

    void AddInsertion(
        const CAlnMap::TSignedRange& targetPiece ); 

    void AddDeletion(
        const CAlnMap::TSignedRange& sourcePiece ); 

    void AddMatch(
        const CAlnMap::TSignedRange& sourcePiece,
        const CAlnMap::TSignedRange& targetPiece ); 

    string StrId() const { 
        return m_strId; };
    string StrSource() const { 
        return ( m_strSource.empty() ? "." : m_strSource ); };
    string StrType() const { 
        return "match";
        return ( m_strType.empty() ? "." : m_strType ); };
    string StrSeqStart() const {
        return NStr::UIntToString( m_sourceRange.GetFrom() + 1 ); };
    string StrSeqStop() const {
        return NStr::UIntToString( m_sourceRange.GetTo() + 1 ); };
    string StrScore() const;
    string StrStrand() const;
    string StrPhase() const {
        return ( m_puPhase ? NStr::UIntToString( *m_puPhase ) : "." ); };
    string StrAttributes() const;

protected:
    unsigned int m_uFlags;
    string m_strId;
    string m_strSource;
    string m_strType;
    double* m_pdScore;
    ENa_strand* m_peStrand;
    unsigned int* m_puPhase;
    string m_strAttributes;    
    string m_strAlignment;
    string m_strOtherScores;

    CAlnMap::TSignedRange m_targetRange;
    CAlnMap::TSignedRange m_sourceRange;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_WRITERS___GFF3ALIGNMENTDATA__HPP
