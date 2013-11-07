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
 * File Description:  Write wiggle file
 *
 */

#ifndef OBJTOOLS_WRITERS___WIGGLE_WRITER__HPP
#define OBJTOOLS_READERS___WIGGLE_WRITER__HPP

#include <corelib/ncbistd.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objtools/writers/writer.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

//  ============================================================================
class NCBI_XOBJWRITE_EXPORT CWiggleWriter:
    public CWriterBase 
//  ============================================================================
{
public:
    CWiggleWriter(
        CNcbiOstream&,
        size_t = 0 );
    ~CWiggleWriter();

    bool WriteAnnot( 
        const CSeq_annot&,
        const string& = "",
        const string& = "" );

protected:
    bool xWriteAnnotTable( const CSeq_annot& );
    bool xWriteAnnotGraphs( const CSeq_annot& );
    
    bool xWriteTrackLine( const CAnnot_descr& );
    bool xWriteSingleGraph( const CSeq_graph& );
    bool xWriteSingleGraphFixedStep( const CSeq_graph&, size_t );
    bool xWriteSingleGraphRecords( const CSeq_graph&, size_t );

    bool xWriteTableFixedStep(const CSeq_table&, const string&, int, int, int);
    bool xWriteTableVariableStep(const CSeq_table&, const string&, int);
    bool xWriteTableBedStyle(const CSeq_table&);

    bool xContainsData( const CSeq_graph&, size_t );
    bool xIsFixedStepData(const CSeq_table&, string&, int&, int&, int&);
    bool xIsVariableStepData(const CSeq_table&, string&, int&);
    
    bool xTableGetChromName(const CSeq_table&, int, string&);
    bool xTableGetPosIn(const CSeq_table&, int, int&);
    bool xTableGetPosOut(const CSeq_table&, int, int, int&);
    bool xTableGetValue(const CSeq_table&, int, double&);
 
    size_t m_uTrackSize;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJTOOLS_WRITERS___WIGGLE_WRITER__HPP
