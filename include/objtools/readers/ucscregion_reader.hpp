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
 * Author: Sergiy Gotvyanskyy
 *
 * File Description:
 *   Distance matrix readers (UCSC-style Region format)
 *
 */

#ifndef OBJTOOLS_READERS___UCSCREGION_READER__HPP
#define OBJTOOLS_READERS___UCSCREGION_READER__HPP

#include <corelib/ncbistd.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objtools/readers/reader_base.hpp>
#include <objtools/readers/message_listener.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects) // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
class NCBI_XOBJREAD_EXPORT CUCSCRegionReader: public CReaderBase
{
public:
    CUCSCRegionReader(unsigned int = fNormal);
    virtual ~CUCSCRegionReader();  

    virtual CRef<CSeq_annot> ReadSeqAnnot(ILineReader& lr, ILineErrorListener* pEC);
    virtual CRef< CSerialObject > 
    ReadObject(
        ILineReader& lr,
        ILineErrorListener* pErrors=0);

protected:

    CRef<CSeq_feat> xParseFeatureUCSCFormat(const string& Line, int Number);
    void x_SetFeatureLocation
            (CRef<CSeq_feat>& feature, const vector<string>& fields);

    void xSmartFieldSplit(vector<string>& fields,
        CTempString line);

    bool xParseFeature(
        const vector<string>&,
        CRef<CSeq_annot>&,
        ILineErrorListener*);
};
//  ----------------------------------------------------------------------------

END_SCOPE(objects)
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___BEDREADER__HPP
