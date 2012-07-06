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
#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>

#include <util/format_guess.hpp>
#include <db/bdb/bdb_cursor.hpp>

#include <serial/objistr.hpp>

#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>

#include <objmgr/util/obj_sniff.hpp>

#include <objtools/readers/fasta.hpp>
#include <objtools/lds/lds_reader.hpp>
#include <objtools/lds/lds_db.hpp>
#include <objtools/lds/lds_expt.hpp>

#include <objtools/lds/lds_query.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;

CRef<CSeq_entry> LDS_LoadTSE(const CLDS_Query::SObjectDescr& obj_descr,
                             CNcbiIstream& in, CFastaReader::TFlags fasta_flags)
{
    switch (obj_descr.format) {
    case CFormatGuess::eFasta:
    {{
        if (fasta_flags == -1) { // invalid; stand-in for the default value
            fasta_flags = (CFastaReader::fAssumeNuc | CFastaReader::fOneSeq
                           | CFastaReader::fParseRawID
                           | CFastaReader::fParseGaps
                           | CFastaReader::fStrictGuess);
        }
        CStreamLineReader lr(in);
        CFastaReader      fr(lr, fasta_flags);
        return fr.ReadOneSeq();
    }}
    case CFormatGuess::eTextASN:
    case CFormatGuess::eXml:
    case CFormatGuess::eBinaryASN:
    {
        auto_ptr<CObjectIStream>
            is(CObjectIStream::Open(FormatGuess2Serial(obj_descr.format), in));
        if (obj_descr.type_str == "Bioseq") {
            //
            // If object is a bare Bioseq: read it and
            // construct a Seq_entry on it
            //
            CRef<CBioseq> bioseq(new CBioseq());
            is->Read(ObjectInfo(*bioseq));
            CRef<CSeq_entry> seq_entry(new CSeq_entry());
            seq_entry->SetSeq(*bioseq);
            return seq_entry;
        }
        if (obj_descr.type_str == "Seq-entry") {
            CRef<CSeq_entry> seq_entry(new CSeq_entry());
            is->Read(ObjectInfo(*seq_entry));
            return seq_entry;
        }
        if (obj_descr.type_str == "Bioseq-set") {
            CRef<CBioseq_set> bioseq_set(new CBioseq_set());
            is->Read(ObjectInfo(*bioseq_set));

            CRef<CSeq_entry> seq_entry(new CSeq_entry());
            CSeq_entry::TSet& s = *bioseq_set;
            seq_entry->SetSet(s);
            return seq_entry;
        }
        if (obj_descr.type_str == "Seq-annot") {
            CRef<CSeq_annot> annot(new CSeq_annot());
            is->Read(ObjectInfo(*annot));

            CRef<CSeq_entry> tse(new CSeq_entry());
            tse->SetSet().SetSeq_set();
            tse->SetSet().SetAnnot().push_back(annot);
            return tse;
        }
        if (obj_descr.type_str == "Seq-align") {
            CRef<CSeq_align> obj(new CSeq_align());
            is->Read(ObjectInfo(*obj));
            CRef<CSeq_annot> annot(new CSeq_annot());
            annot->SetData().SetAlign().push_back(obj);

            CRef<CSeq_entry> tse(new CSeq_entry());
            tse->SetSet().SetSeq_set();
            tse->SetSet().SetAnnot().push_back(annot);
            return tse;
        }
        if (obj_descr.type_str == "Seq-align-set") {
            CRef<CSeq_align_set> obj(new CSeq_align_set());
            is->Read(ObjectInfo(*obj));
            CRef<CSeq_annot> annot(new CSeq_annot());
            NON_CONST_ITERATE ( CSeq_align_set::Tdata, it, obj->Set() ) {
                annot->SetData().SetAlign().push_back(*it);
            }

            CRef<CSeq_entry> tse(new CSeq_entry());
            tse->SetSet().SetSeq_set();
            tse->SetSet().SetAnnot().push_back(annot);
            return tse;
        }
        LDS_THROW(eInvalidDataType, "Non Seq-entry object type");
    }
    default:
        LDS_THROW(eNotImplemented, "Not implemeneted yet.");
    }
}


CRef<CSeq_entry> LDS_LoadTSE(const CLDS_Query::SObjectDescr& obj_descr,
                             CFastaReader::TFlags fasta_flags)
{
    if (!obj_descr.is_object || obj_descr.id <= 0) {
        return CRef<CSeq_entry>();
    }

    CNcbiIfstream in(obj_descr.file_name.c_str(), IOS_BASE::binary);
    if ( !in.is_open() ) {
        string msg = "Cannot open file:";
        msg.append(obj_descr.file_name);
        LDS_THROW(eFileNotFound, msg);
    }
    in.seekg(obj_descr.pos);
    return LDS_LoadTSE(obj_descr, in, fasta_flags);
}


CRef<CSeq_entry> LDS_LoadTSE(CLDS_Database&       lds_db,
                             int                  object_id,
                             bool                 trace_to_top,
                             CFastaReader::TFlags fasta_flags)
{
    const map<string, int>& type_map = lds_db.GetObjTypeMap();

    CLDS_Query query(lds_db);
    CLDS_Query::SObjectDescr obj_descr =
        query.GetObjectDescr(type_map, object_id, trace_to_top);
    return LDS_LoadTSE(obj_descr, fasta_flags);
}


CRef<CSeq_annot> LDS_LoadAnnot(SLDS_TablesCollection& lds_db,
                               const CLDS_Query::SObjectDescr& obj_descr)
{
    CRef<CSeq_annot> annot;
    CNcbiIfstream in(obj_descr.file_name.c_str(), IOS_BASE::binary);
    if (!in.is_open()) {
        string msg = "Cannot open file:";
        msg.append(obj_descr.file_name);
        LDS_THROW(eFileNotFound, msg);
    }

    switch (obj_descr.format) {
    case CFormatGuess::eTextASN:
    case CFormatGuess::eXml:
    case CFormatGuess::eBinaryASN:
    {
        in.seekg(obj_descr.pos);
        auto_ptr<CObjectIStream>
            is(CObjectIStream::Open(FormatGuess2Serial(obj_descr.format), in));
        if (obj_descr.type_str == "Seq-annot") {
            //
            // If object is a bare Bioseq: read it and
            // construct a Seq_entry on it
            //
            annot.Reset(new CSeq_annot());
            is->Read(ObjectInfo(*annot));
        }
        else if (obj_descr.type_str == "Seq-align") {
            CRef<CSeq_align> align(new CSeq_align());
            is->Read(ObjectInfo(*align));
            annot.Reset(new CSeq_annot());
            annot->SetData().SetAlign().push_back(align);
        }
        else if (obj_descr.type_str == "Seq-align-set") {
            CRef<CSeq_align_set> align_set(new CSeq_align_set());
            is->Read(ObjectInfo(*align_set));
            annot.Reset(new CSeq_annot());
            NON_CONST_ITERATE ( CSeq_align_set::Tdata, it, align_set->Set() ) {
                annot->SetData().SetAlign().push_back(*it);
            }
        }
        else {
            LDS_THROW(eInvalidDataType,
                      "Non Seq-aanot compatible object type");
        }
        break;
    }
    default:
        LDS_THROW(eNotImplemented, "Invalid file format");
    }
    return annot;
}

END_SCOPE(objects)
END_NCBI_SCOPE
