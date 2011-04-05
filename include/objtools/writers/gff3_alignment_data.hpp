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
#include <objmgr/util/feature.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
class CGffAlignmentRecord
//  ----------------------------------------------------------------------------
{
public:
    CGffAlignmentRecord(
            unsigned int uRecordId =0 ):
        m_strId( "" ),
        m_uSeqStart( 0 ),
        m_uSeqStop( 0 ),
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
        TSeqPos,
        TSeqPos,
        ENa_strand );

    void SetTargetLocation( 
        const CSeq_id&,
        TSeqPos,
        TSeqPos,
        ENa_strand );

    void SetScore(
        const CScore& );

    void SetPhase(
        unsigned int );

    void AddAlignmentPiece( 
        char,
        unsigned int );

    string StrId() const { 
        return m_strId; };
    string StrSource() const { 
        return ( m_strSource.empty() ? "." : m_strSource ); };
    string StrType() const { 
        return "match";
        return ( m_strType.empty() ? "." : m_strType ); };
    string StrSeqStart() const {
        return NStr::UIntToString( m_uSeqStart + 1 ); };
    string StrSeqStop() const {
        return NStr::UIntToString( m_uSeqStop + 1 ); };
    string StrScore() const {
        return ( m_pdScore ? NStr::DoubleToString( *m_pdScore, 3 ) : "." ); };
    string StrStrand() const {
        if ( ! m_peStrand ) {
            return ".";
        }
        switch ( *m_peStrand ) {
            default: return ".";
            case eNa_strand_plus: return "+";
            case eNa_strand_minus: return "-";
        }
    }
    string StrPhase() const {
        return ( m_puPhase ? NStr::UIntToString( *m_puPhase ) : "." ); };
    string StrAttributes() const {
        return m_strAttributes + ";Gap=" + m_strAlignment; };

protected:
    string m_strId;
    size_t m_uSeqStart;
    size_t m_uSeqStop;
    string m_strSource;
    string m_strType;
    double* m_pdScore;
    ENa_strand* m_peStrand;
    unsigned int* m_puPhase;
    string m_strAttributes;    
    string m_strAlignment;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_WRITERS___GFF3ALIGNMENTDATA__HPP
