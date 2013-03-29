static char const rcsid[] = "$Id: cobalt_app_util.cpp";

/*
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: cobalt_app_util.cpp

Author: Jason Papadopoulos

Contents: Utility functions for COBALT command line applications

******************************************************************************/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <objmgr/object_manager.hpp>
#include <objtools/data_loaders/blastdb/bdbloader.hpp>
#include <serial/iterator.hpp>
#include <objtools/readers/reader_exception.hpp>

#include "cobalt_app_util.hpp"

BEGIN_NCBI_SCOPE;
BEGIN_SCOPE(cobalt);


/// Modification of the CFastaReader class that allows for reading a single
/// sequence as a degenarate multiple sequence alignment
class CFastaReaderEx : public CFastaReader
{
public:
    /// Constructor
    CFastaReaderEx(ILineReader& reader, CFastaReader::TFlags flags=0)
        : CFastaReader(reader, flags) {}

    /// Read alignment
    /// Modified parent class function that allows reading one sequence as
    /// aligned set
    CRef<CSeq_entry> ReadAlignedSet(void) {
        CFastaReader::TIds ids;
        CRef<CSeq_entry> entry = x_ReadSeqsToAlign(ids);
        CRef<CSeq_annot> annot(new CSeq_annot);

        x_AddMultiwayAlignment(*annot, ids);
        entry->SetSet().SetAnnot().push_back(annot);

        return entry;
    }
};

void GetSeqLocFromStream(CNcbiIstream& instream,
                         vector< CRef<objects::CSeq_loc> >& seqs,
                         CRef<objects::CScope>& scope,
                         objects::CFastaReader::TFlags flags)
{
    seqs.clear();

    // read one query at a time, and use a separate seq_entry,
    // scope, and lowercase mask for each query. This lets different
    // query sequences have the same ID. Later code will distinguish
    // between queries by using different elements of retval[]

    CStreamLineReader line_reader(instream);
    CFastaReader fasta_reader(line_reader, flags);

    while (!line_reader.AtEOF()) {

        CRef<CSeq_entry> entry = fasta_reader.ReadOneSeq();

        if (entry == 0) {
            NCBI_THROW(CObjReaderException, eInvalid, 
                        "Could not retrieve seq entry");
        }
        scope->AddTopLevelSeqEntry(*entry);
        CTypeConstIterator<CBioseq> itr(ConstBegin(*entry));
        CRef<CSeq_loc> seqloc(new CSeq_loc());
        seqloc->SetWhole().Assign(*itr->GetId().front());
        seqs.push_back(seqloc);
    }
}


CRef<objects::CSeq_align> GetAlignmentFromStream(CNcbiIstream& instream,
                                     CRef<objects::CScope>& scope,
                                     objects::CFastaReader::TFlags flags,
                                     objects::CSeqIdGenerator& id_generator)
{
    // read all sequences as a multiple sequence alignment and put
    // the alignment in a single seq_entry

    CStreamLineReader line_reader(instream);
    CFastaReaderEx fasta_reader(line_reader, flags);
    fasta_reader.SetIDGenerator(id_generator);

    CRef<CSeq_entry> entry = fasta_reader.ReadAlignedSet();

    if (entry == 0) {
        NCBI_THROW(CObjReaderException, eInvalid, 
                   "Could not retrieve seq entry");
    }
    scope->AddTopLevelSeqEntry(*entry);

    // notify of a problem if the whole file was not read
    if (!line_reader.AtEOF()) {
        NCBI_THROW(CObjReaderException, eInvalid, "Problem reading MSA");
    }

    CRef<objects::CSeq_align> align
        = entry->GetAnnot().front()->GetData().GetAlign().front();

    return align;
}


END_SCOPE(cobalt);
END_NCBI_SCOPE;
