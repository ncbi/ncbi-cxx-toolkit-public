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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:  LDS reader implementation.
 *
 */
#include <corelib/ncbifile.hpp>

#include <util/format_guess.hpp>
#include <bdb/bdb_cursor.hpp>

#include <serial/objistr.hpp>

#include <objects/seq/Bioseq.hpp>

#include <objmgr/util/obj_sniff.hpp>

#include <objtools/readers/fasta.hpp>
#include <objtools/lds/lds_reader.hpp>
#include <objtools/lds/lds_db.hpp>
#include <objtools/lds/lds_expt.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CRef<CSeq_entry> LDS_LoadTSE(SLDS_TablesCollection& db, 
                             const map<string, int>& type_map,
                             int object_id)
{
    db.object_db.object_id = object_id;
    if (db.object_db.Fetch() != eBDB_Ok)
        return CRef<CSeq_entry>();

    int tse_id = db.object_db.TSE_object_id;
    if (tse_id) {
        LDS_THROW(eWrongEntry, "Non top-level SeqEntry.");
    }

    // Check the object type here
    //
    int object_type = db.object_db.object_type;
    string str_type;
    for (map<string, int>::const_iterator tit = type_map.begin();
         tit != type_map.end();
         ++tit) {
        if (tit->second == object_type) {
            str_type = tit->first;
            break;
        }
    } // for

    int file_id = db.object_db.file_id;

    db.file_db.file_id = file_id;
    if (db.file_db.Fetch() != eBDB_Ok) {
        LDS_THROW(eRecordNotFound, "File record not found.");
    }

    CFormatGuess::EFormat format = 
                (CFormatGuess::EFormat)(int)db.file_db.format;
    const char* fname = db.file_db.file_name;

    CNcbiIfstream in(fname, IOS_BASE::in | IOS_BASE::binary);
    if (!in.is_open()) {
        string msg = "Cannot open file:";
        msg.append(fname);
        LDS_THROW(eFileNotFound, msg);
    }
    size_t offset = db.object_db.file_offset;

    switch (format) {
    case CFormatGuess::eFasta:
        return ReadFasta(in, fReadFasta_AssumeNuc);
    case CFormatGuess::eTextASN:
    case CFormatGuess::eBinaryASN:
        {
        in.seekg(offset);
        auto_ptr<CObjectIStream> 
               is(CObjectIStream::Open(FormatGuess2Serial(format), in));
        if (str_type == "Bioseq") {
            //
            // If object is a bare Bioseq: read it and 
            // construct a Seq_entry on it
            //
            CRef<CBioseq> bioseq(new CBioseq());
            is->Read(ObjectInfo(*bioseq));
            CRef<CSeq_entry> seq_entry(new CSeq_entry());
            seq_entry->SetSeq(*bioseq);
            return seq_entry;
        } else 
        if (str_type == "Seq-entry") {
            CRef<CSeq_entry> seq_entry(new CSeq_entry());
            is->Read(ObjectInfo(*seq_entry));
            return seq_entry;
        } else {
            LDS_THROW(eInvalidDataType, "Non Seq-entry object type");
        }

        }
        break;
    default:
        LDS_THROW(eNotImplemented, "Not implemeneted yet.");
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.2  2003/06/09 18:06:33  kuznets
 * CSeq_entry reader changed to handle top level CBioseqs.
 *
 * Revision 1.1  2003/06/06 20:02:34  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
