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
 * File Description:  Write bed file
 *
 */

#ifndef OBJTOOLS_WRITERS___BED_WRITER__HPP
#define OBJTOOLS_READERS___BED_WRITER__HPP

#include <objtools/writers/writer.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE


//  ============================================================================
class NCBI_XOBJWRITE_EXPORT CBedWriter:
    public CWriterBase 
//  ============================================================================
{
public:
    typedef enum {
        fNormal = 0,
    } TFlags;
    
public:
    CBedWriter(
        CScope&,
        CNcbiOstream&,
        TFlags = fNormal );

    virtual ~CBedWriter();

    bool WriteAnnot( 
        const CSeq_annot&,
        const string& = "",
        const string& = "" );

protected:
    bool x_WriteAnnotFTable( 
        const CSeq_annot& );

    virtual bool x_WriteRecord( 
        const CBedTrackRecord& );

    virtual bool x_WriteRecord( 
        const CBedFeatureRecord&,
        size_t = 12 );

    virtual SAnnotSelector x_GetAnnotSelector();

    CScope& m_Scope;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJTOOLS_WRITERS___BED_WRITER__HPP
