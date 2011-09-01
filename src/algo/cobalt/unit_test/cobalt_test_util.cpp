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
* Author:  Greg Boratyn
*
* File Description:
*   Utilities for Cobalt unit tests
*
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/seq_vector.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objtools/data_loaders/blastdb/bdbloader.hpp>
#include <objtools/readers/fasta.hpp>
#include <serial/iterator.hpp>
#include <algo/cobalt/cobalt.hpp>

#ifndef SKIP_DOXYGEN_PROCESSING

USING_NCBI_SCOPE;
USING_SCOPE(cobalt);
USING_SCOPE(objects);


int ReadFastaQueries(const string& filename,
                      vector< CRef<objects::CSeq_loc> >& seqs,
                      CRef<objects::CScope>& scope,
                      objects::CSeqIdGenerator* id_generator /* = NULL*/)
{
    seqs.clear();
    CNcbiIfstream instream(filename.c_str());
    if (!instream) {
        return -1;
    }

    CStreamLineReader line_reader(instream);
    CFastaReader fasta_reader(line_reader, 
                              CFastaReader::fAssumeProt |
                              CFastaReader::fForceType |
                              CFastaReader::fNoParseID);

    if (id_generator) {
        fasta_reader.SetIDGenerator(*id_generator);
    }

    while (!line_reader.AtEOF()) {

        scope->AddDefaults();
        CRef<CSeq_entry> entry = fasta_reader.ReadOneSeq();

        if (entry == 0) {
            return -1;
        }
        scope->AddTopLevelSeqEntry(*entry);
        CTypeConstIterator<CBioseq> itr(ConstBegin(*entry));
        CRef<CSeq_loc> seqloc(new CSeq_loc());
        seqloc->SetWhole().Assign(*itr->GetId().front());
        seqs.push_back(seqloc);
    }

    return 0;
}



int ReadMsa(const string& filename, CRef<CSeq_align>& align,
            CRef<CScope> scope, objects::CSeqIdGenerator* id_generator
            /* = NULL*/)
{
    if (scope.Empty()) {
        return -1;
    }

    CNcbiIfstream instream(filename.c_str());
    if (!instream) {
        return -1;
    }
    CStreamLineReader line_reader(instream);
    CFastaReader fasta_reader(line_reader, CFastaReader::fAssumeProt | 
                              CFastaReader::fForceType |
                              CFastaReader::fNoParseID |
                              CFastaReader::fValidate);

    if (id_generator) {
        fasta_reader.SetIDGenerator(*id_generator);
    }

    CRef<CSeq_entry> entry = fasta_reader.ReadAlignedSet(-1);
    if (entry.Empty()) {
        return -1;
    }
    scope->AddTopLevelSeqEntry(*entry);

    // notify of a problem if the whole file was not read
    if (!line_reader.AtEOF()) {
        return -1;
    }

    align = entry->GetAnnot().front()->GetData().GetAlign().front();

    return 0;
}


#endif /* SKIP_DOXYGEN_PROCESSING */
