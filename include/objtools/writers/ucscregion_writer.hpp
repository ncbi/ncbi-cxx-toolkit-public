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
 * Authors:  Sergiy Gotvyanskyy
 *
 * File Description:  Formatter, Genbank to UCSC Region.
 *
 */

#ifndef OBJTOOLS_WRITERS___UCSCREGION_WRITER__HPP
#define OBJTOOLS_WRITERS___UCSCREGION_WRITER__HPP

#include <objtools/writers/writer.hpp>
#include <objtools/writers/bed_track_record.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

//  ============================================================================
class NCBI_XOBJWRITE_EXPORT CUCSCRegionWriter: 
    public CWriterBase 
//  ============================================================================
{
public:
    enum eFlags
    {
      fNormal,
      fSkipStrand
    };
    /// Constructor.
    /// @param scope
    ///   scope to be used for ID reference resolution (it's OK to create one
    ///   on the fly).
    /// @param ostr
    ///   stream objects should be written to.
    /// @param colCount
    ///   number of columns per output record. Each record in a BED file must
    ///   have the same number of columns. Hence the writer will truncate or
    ///   extend all records to colCount output columns.
    /// @param flags
    ///   any output customization flags.
    ///
    CUCSCRegionWriter(
        CScope& scope,
        CNcbiOstream& ostr,
        unsigned int flags=fNormal );

    virtual ~CUCSCRegionWriter();

    bool WriteAnnot(const CSeq_annot&, const CTempString& separators="\t");

protected:
    CScope& m_Scope;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif  // OBJTOOLS_WRITERS___BED_WRITER__HPP
