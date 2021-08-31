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
 * Authors: Frank Ludwig
 *
 * File Description:  Write alignment file
 *
 */

#ifndef OBJTOOLS_WRITERS_PSL_WRITER_HPP
#define OBJTOOLS_WRITERS_PSL_WRITER_HPP

#include <objtools/writers/writer.hpp>
#include <util/sequtil/sequtil.hpp>
#include <objects/seqalign/Spliced_seg.hpp>

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE

class CDense_seg;
class CScope;
class CSparse_align;
class CSparse_seg;
class CPslFormatter;

//  ----------------------------------------------------------------------------
class NCBI_XOBJWRITE_EXPORT CPslWriter:
    public CWriterBase
//  ----------------------------------------------------------------------------
{
public:
    CPslWriter(
        CScope& scope,
        CNcbiOstream& ostr,
        unsigned int uFlags);

    CPslWriter(
        CNcbiOstream& ostr,
        unsigned int uFlags);

    virtual ~CPslWriter() = default;

    bool WriteAnnot(
        const CSeq_annot& annot,
        const string& name="",
        const string& descr="") override;

    bool WriteAlign(
        const CSeq_align& align,
        const string& name="",
        const string& descr="") override;

protected:
    CRef<CScope> m_pScope;
    int mRecordCounter;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_WRITERS_PSL_WRITER_HPP
