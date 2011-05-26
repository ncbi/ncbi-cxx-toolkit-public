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
 * File Description:  Write vcf file
 *
 */

#ifndef OBJTOOLS_WRITERS___VCF_WRITER__HPP
#define OBJTOOLS_READERS___VCF_WRITER__HPP

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE


//  ============================================================================
class NCBI_XOBJWRITE_EXPORT CVcfWriter   
//  ============================================================================
{
public:
    typedef enum {
        fNormal =       0,
    } TFlags;
    
public:
    CVcfWriter(
        CScope&,
        CNcbiOstream&,
        TFlags = fNormal );

    virtual ~CVcfWriter();

    bool WriteAnnot( const CSeq_annot& );

protected:
    CScope& m_Scope;
    CNcbiOstream& m_Os;
    TFlags m_uFlags;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJTOOLS_WRITERS___VCF_WRITER__HPP
