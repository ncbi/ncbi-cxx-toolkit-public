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
* Authors:  Aaron Ucko, NCBI;  Anatoliy Kuznetsov, NCBI.
*
* File Description:
*   Reader for FASTA-format sequences.  (The writer is CFastaOStream, in
*   src/objmgr/util/sequence.cpp.)
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objtools/readers/fasta.hpp>
#include <objtools/readers/reader_exception.hpp>

#include <corelib/ncbiutil.hpp>
#include <util/format_guess.hpp>

#include <objects/general/Object_id.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/NCBIeaa.hpp>
#include <objects/seq/IUPACna.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/seqport_util.hpp>

#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_loc_mix.hpp>
#include <objects/seqloc/Seq_point.hpp>

#include <objects/seqset/Bioseq_set.hpp>

#include <ctype.h>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

static SIZE_TYPE s_EndOfFastaID(const string& str, SIZE_TYPE pos)
{
    SIZE_TYPE vbar = str.find('|', pos);
    if (vbar == NPOS) {
        return NPOS; // bad
    }

    CSeq_id::E_Choice choice =
        CSeq_id::WhichInverseSeqId(str.substr(pos, vbar - pos).c_str());

#if 1
    if (choice != CSeq_id::e_not_set) {
        SIZE_TYPE vbar_prev = vbar;
        int count;
        for (count=0; ; ++count, vbar_prev = vbar) {
            vbar = str.find('|', vbar_prev + 1);
            if (vbar == NPOS) {
                break;
            }
            choice = CSeq_id::WhichInverseSeqId(
                str.substr(vbar_prev + 1, vbar - vbar_prev - 1).c_str());
            if (choice != CSeq_id::e_not_set) {
                vbar = vbar_prev;
                break;
            }
        }
    } else {
        return NPOS; // bad
    }
#else
    switch (choice) {
    case CSeq_id::e_Patent: case CSeq_id::e_Other: // 3 args
        vbar = str.find('|', vbar + 1);
        // intentional fall-through - this allows us to correctly
        // calculate the number of '|' separations for FastA IDs

    case CSeq_id::e_Genbank:   case CSeq_id::e_Embl:    case CSeq_id::e_Pir:
    case CSeq_id::e_Swissprot: case CSeq_id::e_General: case CSeq_id::e_Ddbj:
    case CSeq_id::e_Prf:       case CSeq_id::e_Pdb:     case CSeq_id::e_Tpg:
    case CSeq_id::e_Tpe:       case CSeq_id::e_Tpd:
        // 2 args
        if (vbar == NPOS) {
            return NPOS; // bad
        }
        vbar = str.find('|', vbar + 1);
        // intentional fall-through - this allows us to correctly
        // calculate the number of '|' separations for FastA IDs

    case CSeq_id::e_Local: case CSeq_id::e_Gibbsq: case CSeq_id::e_Gibbmt:
    case CSeq_id::e_Giim:  case CSeq_id::e_Gi:
        // 1 arg
        if (vbar == NPOS) {
            if (choice == CSeq_id::e_Other) {
                // this is acceptable - member is optional
                break;
            }
            return NPOS; // bad
        }
        vbar = str.find('|', vbar + 1);
        break;

    default: // unrecognized or not set
        return NPOS; // bad
    }
#endif

    return (vbar == NPOS) ? str.size() : vbar;
}


static void s_FixSeqData(CBioseq* seq)
{
    _ASSERT(seq);
    CSeq_inst& inst = seq->SetInst();
    switch (inst.GetRepr()) {
    case CSeq_inst::eRepr_delta:
    {
        TSeqPos length = 0;
        NON_CONST_ITERATE (CDelta_ext::Tdata, it,
                           inst.SetExt().SetDelta().Set()) {
            if ((*it)->IsLiteral()) {
                CSeq_literal& lit  = (*it)->SetLiteral();
                CSeq_data&    data = lit.SetSeq_data();
                if (data.IsIupacna()) {
                    lit.SetLength(data.GetIupacna().Get().size());
                    CSeqportUtil::Pack(&data);
                } else {
                    string& s = data.SetNcbieaa().Set();
                    lit.SetLength(s.size());
                    s.reserve(s.size()); // free extra allocation
                }
                length += lit.GetLength();
            }
        }
        break;
    }
    case CSeq_inst::eRepr_raw:
    {
        CSeq_data& data = inst.SetSeq_data();
        if (data.IsIupacna()) {
            inst.SetLength(data.GetIupacna().Get().size());
            CSeqportUtil::Pack(&data);
        } else {
            string& s = data.SetNcbieaa().Set();
            inst.SetLength(s.size());
            s.reserve(s.size()); // free extra allocation
        }        
        break;
    }
    default: // especially not_set!
        break;
    }
}


void s_AddData(CSeq_inst& inst, const string& residues)
{
    CRef<CSeq_data> data;
    if (inst.IsSetExt()  &&  inst.GetExt().IsDelta()) {
        CDelta_ext::Tdata& delta_data = inst.SetExt().SetDelta().Set();
        if (delta_data.empty()  ||  !delta_data.back()->IsLiteral()) {
            CRef<CDelta_seq> delta_seq(new CDelta_seq);
            delta_data.push_back(delta_seq);
            data = &delta_seq->SetLiteral().SetSeq_data();
        } else {
            data = &delta_data.back()->SetLiteral().SetSeq_data();
        }
    } else {
        data = &inst.SetSeq_data();
    }

    string* s = 0;
    if (inst.GetMol() == CSeq_inst::eMol_aa) {
        if (data->IsNcbieaa()) {
            s = &data->SetNcbieaa().Set();
        } else {
            data->SetNcbieaa().Set(residues);
        }
    } else {
        if (data->IsIupacna()) {
            s = &data->SetIupacna().Set();
        } else {
            data->SetIupacna().Set(residues);
        }
    }

    if (s) {
        // grow exponentially to avoid O(n^2) behavior
        if (s->capacity() < s->size() + residues.size()) {
            s->reserve(s->capacity()
                       + max(residues.size(), s->capacity() / 2));
        }
        *s += residues;
    }
}


static CSeq_inst::EMol s_ParseFastaDefline(CBioseq::TId& ids, string& title,
                                           const string& line,
                                           TReadFastaFlags flags, int* counter)
{
    SIZE_TYPE       start = 0;
    CSeq_inst::EMol mol   = CSeq_inst::eMol_not_set;
    do {
        ++start;
        SIZE_TYPE space = line.find_first_of(" \t", start);
        string    name  = line.substr(start, space - start), local;

        if (flags & fReadFasta_NoParseID) {
            local = name;
        } else {
            // try to parse out IDs
            SIZE_TYPE pos = 0;
            while (pos < name.size()) {
                SIZE_TYPE end = s_EndOfFastaID(name, pos);
                if (end == NPOS) {
                    if (pos > 0) {
                        NCBI_THROW2(CObjReaderParseException, eFormat,
                                    "s_ParseFastaDefline: Bad ID "
                                    + name.substr(pos),
                                    pos);
                    } else {
                        local = name;
                        break;
                    }
                }

                CRef<CSeq_id> id(new CSeq_id(name.substr(pos, end - pos)));
                ids.push_back(id);
                if (mol == CSeq_inst::eMol_not_set
                    &&  !(flags & fReadFasta_ForceType)) {
                    CSeq_id::EAccessionInfo ai = id->IdentifyAccession();
                    if (ai & CSeq_id::fAcc_nuc) {
                        mol = CSeq_inst::eMol_na;
                    } else if (ai & CSeq_id::fAcc_prot) {
                        mol = CSeq_inst::eMol_aa;
                    }
                }
                pos = end + 1;
            }
        }
            
        if ( !local.empty() ) {
            ids.push_back(CRef<CSeq_id>
                          (new CSeq_id(CSeq_id::e_Local, local, kEmptyStr)));
        }

        start = line.find('\1', start);
        if (space != NPOS  &&  title.empty()) {
            title.assign(line, space + 1,
                         (start == NPOS) ? NPOS : (start - space - 1));
        }
    } while (start != NPOS  &&  (flags & fReadFasta_AllSeqIds));

    if (ids.empty()) {
        if (flags & fReadFasta_RequireID) {
            NCBI_THROW2(CObjReaderParseException, eFormat,
                        "s_ParseFastaDefline: no ID present", 0);
        }
        CRef<CSeq_id> id(new CSeq_id);
        id->SetLocal().SetId(++*counter);
        ids.push_back(id);
    }

    return mol;
}


static void s_GuessMol(CSeq_inst::EMol& mol, const string& data,
                       TReadFastaFlags flags, istream& in)
{
    if (mol != CSeq_inst::eMol_not_set) {
        return; // already known; no need to guess
    }

    if (mol == CSeq_inst::eMol_not_set  &&  !(flags & fReadFasta_ForceType)) {
        switch (CFormatGuess::SequenceType(data.data(), data.size())) {
        case CFormatGuess::eNucleotide:  mol = CSeq_inst::eMol_na;  return;
        case CFormatGuess::eProtein:     mol = CSeq_inst::eMol_aa;  return;
        default:                         break;
        }
    }

    // ForceType was set, or CFormatGuess failed, so we have to rely on
    // explicit assumptions
    if (flags & fReadFasta_AssumeNuc) {
        _ASSERT(!(flags & fReadFasta_AssumeProt));
        mol = CSeq_inst::eMol_na;
    } else if (flags & fReadFasta_AssumeProt) {
        mol = CSeq_inst::eMol_aa;
    } else { 
        NCBI_THROW2(CObjReaderParseException, eFormat,
                    "ReadFasta: unable to deduce molecule type"
                    " from IDs, flags, or sequence",
                    in.tellg() - CT_POS_TYPE(0));
    }
}


CRef<CSeq_entry> ReadFasta(CNcbiIstream& in, TReadFastaFlags flags,
                           int* counter, vector<CConstRef<CSeq_loc> >* lcv)
{
    if ( !in ) {
        NCBI_THROW2(CObjReaderParseException, eFormat,
                    "ReadFasta: Input stream no longer valid",
                    in.tellg() - CT_POS_TYPE(0));
    } else {
        CT_INT_TYPE c = in.peek();
        if ( !strchr(">#!\n\r", CT_TO_CHAR_TYPE(c)) ) {
            NCBI_THROW2
                (CObjReaderParseException, eFormat,
                 "ReadFasta: Input doesn't start with a defline or comment",
                 in.tellg() - CT_POS_TYPE(0));
        }
    }

    CRef<CSeq_entry>       entry(new CSeq_entry);
    CBioseq_set::TSeq_set& sset  = entry->SetSet().SetSeq_set();
    CRef<CBioseq>          seq(0); // current Bioseq
    string                 line;
    TSeqPos                pos = 0, lc_start = 0;
    bool                   was_lc = false;
    CRef<CSeq_id>          best_id;
    CRef<CSeq_loc>         lowercase(0);
    int                    defcounter = 1;

    if ( !counter ) {
        counter = &defcounter;
    }

    while ( !in.eof() ) {
        if ((flags & fReadFasta_OneSeq)  &&  seq.NotEmpty()
            &&  (in.peek() == '>')) {
            break;
        }
        NcbiGetlineEOL(in, line);
        if (in.eof()  &&  line.empty()) {
            break;
        } else if (line.empty()) {
            continue;
        }
        if (line[0] == '>') {
            // new sequence
            if (seq) {
                s_FixSeqData(seq);
                if (was_lc) {
                    lowercase->SetPacked_int().AddInterval
                        (*best_id, lc_start, pos);
                }
            }
            seq = new CBioseq;
            if (flags & fReadFasta_NoSeqData) {
                seq->SetInst().SetRepr(CSeq_inst::eRepr_not_set);
            } else {
                seq->SetInst().SetRepr(CSeq_inst::eRepr_raw);
            }
            {{
                CRef<CSeq_entry> entry2(new CSeq_entry);
                entry2->SetSeq(*seq);
                sset.push_back(entry2);
            }}
            string          title;
            CSeq_inst::EMol mol = s_ParseFastaDefline(seq->SetId(), title,
                                                      line, flags, counter);
            if (mol == CSeq_inst::eMol_not_set
                &&  (flags & fReadFasta_NoSeqData)) {
                if (flags & fReadFasta_AssumeNuc) {
                    _ASSERT(!(flags & fReadFasta_AssumeProt));
                    mol = CSeq_inst::eMol_na;
                } else if (flags & fReadFasta_AssumeProt) {
                    mol = CSeq_inst::eMol_aa;
                }
            }
            seq->SetInst().SetMol(mol);

            if ( !title.empty() ) {
                CRef<CSeqdesc> desc(new CSeqdesc);
                desc->SetTitle(title);
                seq->SetDescr().Set().push_back(desc);
            }

            if (lcv) {
                pos       = 0;
                was_lc    = false;
                best_id   = FindBestChoice(seq->GetId(), CSeq_id::Score);
                lowercase = new CSeq_loc;
                lowercase->SetNull();
                lcv->push_back(lowercase);
            }
        } else if (line[0] == '#'  ||  line[0] == '!') {
            continue; // comment
        } else if ( !seq ) {
            NCBI_THROW2
                (CObjReaderParseException, eFormat,
                 "ReadFasta: No defline preceding data",
                 in.tellg() - CT_POS_TYPE(0));
        } else if ( !(flags & fReadFasta_NoSeqData) ) {
            // These don't change, but the calls may be relatively expensive,
            // esp. with ref-counted implementations.
            SIZE_TYPE   line_size = line.size();
            const char* line_data = line.data();
            // actual data; may contain embedded junk
            CSeq_inst&  inst      = seq->SetInst();
            string      residues(line_size + 1, '\0');
            char*       res_data  = const_cast<char*>(residues.data());
            SIZE_TYPE   res_count = 0;
            for (SIZE_TYPE i = 0;  i < line_size;  ++i) {
                char c = line_data[i];
                if (isalpha(c)) {
                    if (lowercase) {
                        bool is_lc = islower(c) ? true : false;
                        if (is_lc && !was_lc) {
                            lc_start = pos;
                        } else if (was_lc && !is_lc) {
                            lowercase->SetPacked_int().AddInterval
                                (*best_id, lc_start, pos);
                        }
                        was_lc = is_lc;
                        ++pos;
                    }
                    res_data[res_count++] = toupper(c);
                } else if (c == '-'  &&  (flags & fReadFasta_ParseGaps)) {
                    CDelta_ext::Tdata& d = inst.SetExt().SetDelta().Set();
                    if (inst.GetRepr() == CSeq_inst::eRepr_raw) {
                        CRef<CDelta_seq> ds(new CDelta_seq);
                        inst.SetRepr(CSeq_inst::eRepr_delta);
                        if (inst.IsSetSeq_data()) {
                            ds->SetLiteral().SetSeq_data(inst.SetSeq_data());
                            d.push_back(ds);
                            inst.ResetSeq_data();
                        }
                    }
                    if ( res_count ) {
                        residues.resize(res_count);
                        if (inst.GetMol() == CSeq_inst::eMol_not_set) {
                            s_GuessMol(inst.SetMol(), residues, flags, in);
                        }
                        s_AddData(inst, residues);
                    }
                    {{
                        CRef<CDelta_seq> gap(new CDelta_seq);
                        gap->SetLoc().SetNull();
                        d.push_back(gap);
                    }}
                    res_count = 0;
                } else if (c == '-'  ||  c == '*') {
                    // valid, at least for proteins
                    res_data[res_count++] = c;
                } else if (c == ';') {
                    continue; // skip rest of line
                } else if ( !isspace(c) ) {
                    ERR_POST(Warning << "ReadFasta: Ignoring invalid residue "
                             << c << " at position "
                             << (in.tellg() - CT_POS_TYPE(0)));
                }
            }

            if (inst.GetMol() == CSeq_inst::eMol_not_set) {
                s_GuessMol(inst.SetMol(), residues, flags, in);
            }
            
            // Add the accumulated data...
            residues.resize(res_count);
            s_AddData(inst, residues);
        }
    }

    if (seq) {
        s_FixSeqData(seq);
        if (was_lc) {
            lowercase->SetPacked_int().AddInterval(*best_id, lc_start, pos);
        }
    }
    // simplify if possible
    if (sset.size() == 1) {
        entry->SetSeq(*seq);
    }
    return entry;
}


void ReadFastaFileMap(SFastaFileMap* fasta_map, CNcbiIfstream& input)
{
    _ASSERT(fasta_map);

    fasta_map->file_map.resize(0);

    if (!input.is_open()) 
        return;

    while (!input.eof()) {
        SFastaFileMap::SFastaEntry  fasta_entry;
        fasta_entry.stream_offset = input.tellg() - CT_POS_TYPE(0);

        CRef<CSeq_entry> se;
        se = ReadFasta(input, fReadFasta_AssumeNuc | fReadFasta_OneSeq);

        if (!se->IsSeq()) 
            continue;

        const CSeq_entry::TSeq& bioseq = se->GetSeq();
        const CSeq_id* sid = bioseq.GetFirstId();
        fasta_entry.seq_id = sid->AsFastaString();
        if (bioseq.CanGetDescr()) {
            const CSeq_descr& d = bioseq.GetDescr();
            if (d.CanGet()) {
                const CSeq_descr_Base::Tdata& data = d.Get();
                if (!data.empty()) {
                    CSeq_descr_Base::Tdata::const_iterator it = 
                                                      data.begin();
                    if (it != data.end()) {
                        CRef<CSeqdesc> ref_desc = *it;
                        ref_desc->GetLabel(&fasta_entry.description, 
                                           CSeqdesc::eContent);
                    }                                
                }
            }
        }
        fasta_map->file_map.push_back(fasta_entry);
        

    } // while

}

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.14  2005/02/07 19:03:05  ucko
* Improve handling of non-IUPAC residues.
*
* Revision 1.13  2004/11/24 19:28:05  ucko
* +fReadFasta_RequireID
*
* Revision 1.12  2004/05/21 21:42:55  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.11  2004/02/19 22:57:52  ucko
* Accommodate stricter implementations of CT_POS_TYPE.
*
* Revision 1.10  2003/12/05 03:00:36  ucko
* Validate input better.
*
* Revision 1.9  2003/10/03 15:09:18  ucko
* Tweak sequence string allocation to avoid O(n^2) performance from
* linear resizing.
*
* Revision 1.8  2003/08/25 21:30:09  ucko
* ReadFasta: tweak a bit to improve performance, and fix some bugs in
* parsing gaps.
*
* Revision 1.7  2003/08/11 14:39:54  ucko
* Populate "lowercase" with Packed_seqints rather than general Seq_loc_mixes.
*
* Revision 1.6  2003/08/08 21:29:12  dondosha
* Changed type of lcase_mask in ReadFasta to vector of CConstRefs
*
* Revision 1.5  2003/08/07 21:13:21  ucko
* Support a counter for assigning local IDs to sequences with no ID given.
* Fix some minor bugs in lowercase-character support.
*
* Revision 1.4  2003/08/06 19:08:33  ucko
* Slight interface tweak to ReadFasta: report lowercase locations in a
* vector with one entry per Bioseq rather than a consolidated Seq_loc_mix.
*
* Revision 1.3  2003/06/23 20:49:11  kuznets
* Changed to use Seq_id::AsFastaString() when reading fasta file map.
*
* Revision 1.2  2003/06/08 16:17:00  lavr
* Heed MSVC int->bool performance warning
*
* Revision 1.1  2003/06/04 17:26:22  ucko
* Split out from Seq_entry.cpp.
*
*
* ===========================================================================
*/
