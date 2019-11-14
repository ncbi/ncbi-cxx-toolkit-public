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
 *   BED file reader
 *
 */

#ifndef OBJTOOLS_READERS___PSL_READER__HPP
#define OBJTOOLS_READERS___PSL_READER__HPP

#include <corelib/ncbistd.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Score.hpp>

#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/reader_base.hpp>
#include <objtools/readers/gff2_data.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects) // namespace ncbi::objects::

class CGFFReader;
class CGff2Record;
class SRecord;

class CPslData;

//  ----------------------------------------------------------------------------
class NCBI_XOBJREAD_EXPORT CPslReader
//  ----------------------------------------------------------------------------
    : public CReaderBase
{
public:
    typedef int TFlags;

public:
    CPslReader(
        int iFlags);

    virtual ~CPslReader();
    
    virtual CRef< CSerialObject >
    ReadObject(
        ILineReader& lr,
        ILineErrorListener* pErrors=0 );
                
    virtual CRef< CSeq_annot >
    ReadSeqAnnot(
        ILineReader& lr,
        ILineErrorListener* pErrors=0 );

    virtual CRef< CSeq_annot >
    ReadSeqAnnot(
        CNcbiIstream& istr,
        ILineErrorListener* pErrors=0 );

protected:
    CRef<CSeq_align> xCreateSeqAlign(
        const CPslData&);

    ILineErrorListener* m_pErrors;
    unsigned int mCurrentRecordCount;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___PSL_READER__HPP
