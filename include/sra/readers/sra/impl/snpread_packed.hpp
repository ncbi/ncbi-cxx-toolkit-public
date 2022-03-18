#ifndef SRA__READER__SRA__IMPL__SNPREAD_PACKED__HPP
#define SRA__READER__SRA__IMPL__SNPREAD_PACKED__HPP
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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   Access to SNP files - packed in-memory OM representation
 *
 */

#include <corelib/ncbistd.hpp>
#include <sra/readers/sra/snpread.hpp>

BEGIN_NCBI_NAMESPACE;
BEGIN_NAMESPACE(objects);

class CSeq_annot_SNP_Info;

BEGIN_NAMESPACE(SNPDbPacked);


typedef pair< CRef<CSeq_annot>, CRef<CSeq_annot_SNP_Info> > TPackedAnnot;
TPackedAnnot GetPackedFeatAnnot(const CSNPDbSeqIterator& seq,
                                CRange<TSeqPos> range,
                                const CSNPDbSeqIterator::SFilter& filter,
                                CSNPDbSeqIterator::TFlags flags = CSNPDbSeqIterator::fDefaultFlags);
TPackedAnnot GetPackedFeatAnnot(const CSNPDbSeqIterator& seq,
                                CRange<TSeqPos> range,
                                CSNPDbSeqIterator::TFlags flags = CSNPDbSeqIterator::fDefaultFlags);


END_NAMESPACE(SNPDbPacked);

END_NAMESPACE(objects);
END_NCBI_NAMESPACE;

#endif // SRA__READER__SRA__IMPL__SNPREAD_PACKED__HPP
