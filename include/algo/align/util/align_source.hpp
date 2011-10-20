#ifndef GPIPE_COMMON___ALIGN_SOURCE__HPP
#define GPIPE_COMMON___ALIGN_SOURCE__HPP

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
 * Authors:  Eyal Mozes
 *
 * File Description:
 *
 */

#include <objects/seqalign/Seq_align.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


class IAlignSource
{
public:
    virtual ~IAlignSource() {}
    virtual bool EndOfData() const = 0;
    virtual CRef<CSeq_align> GetNext() = 0;
    virtual size_t DataSizeSoFar() const = 0;
};

END_NCBI_SCOPE

#endif  // GPIPE_COMMON___ALIGN_SOURCE__HPP
