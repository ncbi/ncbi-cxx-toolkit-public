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
 * Author:  Kevin Bealer
 *
 */

/// @file writedb_impl.cpp
/// Implementation for the CWriteDB_Impl class.
/// class for WriteDB.

#include <ncbi_pch.hpp>
#include <objtools/writers/writedb/writedb_error.hpp>
#include <objects/general/general__.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <util/sequtil/sequtil_convert.hpp>
#include <serial/typeinfo.hpp>

#include "writedb_impl.hpp"
#include "writedb_convert.hpp"

#include <iostream>
#include <sstream>

BEGIN_NCBI_SCOPE

/// Import C++ std namespace.
USING_SCOPE(std);

CWriteDB_Impl::CWriteDB_Impl(const string & dbname,
                             bool           protein,
                             const string & title,
                             EIndexType     indices)
    : m_Dbname      (dbname),
      m_Protein     (protein),
      m_Title       (title),
      m_MaxFileSize (0),
      m_Indices     (indices),
      m_Closed      (false),
      m_Pig         (0),
      m_HaveSequence(false)
{
    CTime now(CTime::eCurrent);
    
    m_Date = now.AsString("b d, Y  ");
    string t = now.AsString("H:m P");
    
    if (t[0] == '0') {
        t.assign(t, 1, t.size() - 1);
    }
    
    m_Date += t;
}

CWriteDB_Impl::~CWriteDB_Impl()
{
    Close();
}

void CWriteDB_Impl::x_ResetSequenceData()
{
    m_Bioseq.Reset();
    m_SeqVector = CSeqVector();
    m_Deflines.Reset();
    m_Ids.clear();
    m_Linkouts.clear();
    m_Memberships.clear();
    m_Pig = 0;
    
    m_Sequence.erase();
    m_Ambig.erase();
    m_BinHdr.erase();
}

void CWriteDB_Impl::AddSequence(const CTempString & seq,
                                const CTempString & ambig)
{
    // Publish previous sequence (if any)
    x_Publish();
    
    // Blank slate for new sequence.
    x_ResetSequenceData();
    
    m_Sequence.assign(seq.data(), seq.length());
    m_Ambig.assign(ambig.data(), ambig.length());
    
    x_SetHaveSequence();
}

void CWriteDB_Impl::AddSequence(const CBioseq & bs)
{
    // Publish previous sequence
    x_Publish();
    
    // Blank slate for new sequence.
    x_ResetSequenceData();
    
    m_Bioseq.Reset(& bs);
    
    x_SetHaveSequence();
}

void CWriteDB_Impl::AddSequence(const CBioseq & bs, CSeqVector & sv)
{
    AddSequence(bs);
    m_SeqVector = sv;
}

void CWriteDB_Impl::AddSequence(const CBioseq_Handle & bsh)
{
    CSeqVector sv(bsh);
    AddSequence(*bsh.GetCompleteBioseq(), sv);
}

void CWriteDB_Impl::Close()
{
    if (m_Closed)
        return;
    
    m_Closed = true;
    
    x_Publish();
    m_Sequence.erase();
    m_Ambig.erase();
    
    if (! m_Volume.Empty()) {
        m_Volume->Close();
        
        if (m_VolumeList.size() == 1) {
            m_Volume->RenameSingle();
        } else {
            x_MakeAlias();
        }
        
        m_Volume.Reset();
    }
}

string CWriteDB_Impl::x_MakeAliasName()
{
    return m_Dbname + (m_Protein ? ".pal" : ".nal");
}

void CWriteDB_Impl::x_MakeAlias()
{
    string dblist;
    for(unsigned i = 0; i < m_VolumeList.size(); i++) {
        if (dblist.size())
            dblist += " ";
        
        dblist += CWriteDB_File::MakeShortName(m_Dbname, i);
    }
    
    string nm = x_MakeAliasName();
    
    ofstream alias(nm.c_str());
    
    alias << "#\n# Alias file created: " << m_Date  << "\n#\n"
          << "TITLE "        << m_Title << "\n"
          << "DBLIST "       << dblist  << "\n\n";
}

void CWriteDB_Impl::x_GetBioseqBinaryHeader(const CBioseq & bioseq,
                                            string        & bin_hdr)
{
    if (! bin_hdr.empty()) {
        return;
    }
    
    if (! bioseq.CanGetDescr()) {
        return;
    }
    
    // Getting the binary headers, when they exist, is probably faster
    // than building new deflines from the 'visible' CBioseq parts.
    
    string binkey = "ASN1_BlastDefLine";
    vector< vector< char >* > bindata;
    
    ITERATE(list< CRef< CSeqdesc > >, iter, bioseq.GetDescr().Get()) {
        if ((**iter).IsUser()) {
            const CUser_object & uo = (**iter).GetUser();
            const CObject_id & oi = uo.GetType();
            
            if (oi.IsStr() && oi.GetStr() == binkey) {
                if (uo.CanGetData()) {
                    vector< CRef< CUser_field > > D = uo.GetData();
                    
                    if (D.size() &&
                        D[0].NotEmpty() &&
                        D[0]->CanGetLabel() &&
                        D[0]->GetLabel().IsStr() &&
                        D[0]->GetLabel().GetStr() == binkey &&
                        D[0]->CanGetData() &&
                        D[0]->GetData().IsOss()) {
                        
                        bindata = D[0]->GetData().GetOss();
                        break;
                    }
                }
            }
        }
    }
    
    if (! bindata.empty()) {
        if (bindata[0] && (! bindata[0]->empty())) {
            vector<char> & b = *bindata[0];
            
            bin_hdr.assign(& b[0], b.size());
        }
    }
}

void
CWriteDB_Impl::x_BuildDeflinesFromBioseq(const CBioseq                  & bioseq,
                                         CConstRef<CBlast_def_line_set> & deflines,
                                         const vector< vector<int> >    & membbits,
                                         const vector< vector<int> >    & linkouts,
                                         int                              pig)
{
    if (! (bioseq.CanGetDescr() && bioseq.CanGetId())) {
        return;
    }
    
    vector<int> taxids;
    string titles;
    
    // Scan the CBioseq for taxids and the title string.
    
    ITERATE(list< CRef< CSeqdesc > >, iter, bioseq.GetDescr().Get()) {
        const CSeqdesc & desc = **iter;
        
        if (desc.IsTitle()) {
            //defline->SetTitle((**iter)->GetTitle());
            titles = (**iter).GetTitle();
        } else if (desc.IsSource()) {
            if (desc.IsOrg() && desc.GetOrg().CanGetDb()) {
                ITERATE(vector< CRef< CDbtag > >,
                        dbiter,
                        desc.GetOrg().GetDb()) {
                    
                    if ((**dbiter).CanGetDb() &&
                        (**dbiter).GetDb() == "taxon") {
                        
                        const CObject_id & oi = (**dbiter).GetTag();
                        
                        if (oi.IsId()) {
                            //defline->SetTaxid(oi.GetId());
                            taxids.push_back(oi.GetId());
                        }
                    }
                }
            }
        }
    }
    
    // The bioseq has a field contianing the ids for the first
    // defline.  The title string contains the title for the first
    // defline, plus all the other defline titles and ids.  This code
    // unpacks them and builds a normal blast defline set.
    
    list< CRef<CSeq_id> > ids = bioseq.GetId();
    
    unsigned taxid_i(0), mship_i(0), links_i(0);
    bool used_pig(false);
    
    // Build the deflines.
    
    CRef<CBlast_def_line_set> bdls(new CBlast_def_line_set);
    CRef<CBlast_def_line> defline;
    
    while(! ids.empty()) {
        defline.Reset(new CBlast_def_line);
        
        defline->SetSeqid() = ids;
        ids.clear();
        
        size_t pos = titles.find(" >");
        string T;
        
        if (pos != titles.npos) {
            T.assign(titles, 0, pos);
            titles.erase(0, pos + 2);
            
            pos = titles.find(" ");
            string nextid;
            
            if (pos != titles.npos) {
                nextid.assign(titles, 0, pos);
                titles.erase(0, pos + 1);
            } else {
                nextid.swap(titles);
            }
            
            // Parse '|' seperated ids.
            CSeq_id::ParseFastaIds(ids, nextid);
        } else {
            T = titles;
        }
        
        defline->SetTitle(T);
        
        if (taxid_i < taxids.size()) {
            defline->SetTaxid(taxids[taxid_i++]);
        }
        
        if (mship_i < membbits.size()) {
            const vector<int> & V = membbits[mship_i++];
            defline->SetMemberships().assign(V.begin(), V.end());
        }
        
        if (links_i < linkouts.size()) {
            const vector<int> & V = linkouts[mship_i++];
            defline->SetLinks().assign(V.begin(), V.end());
        }
        
        if ((! used_pig) && pig) {
            defline->SetOther_info().push_back(pig);
            used_pig = true;
        }
        
        bdls->Set().push_back(defline);
    }
    
    deflines = bdls;
}

void CWriteDB_Impl::
x_SetDeflinesFromBinary(const string                   & bin_hdr,
                        CConstRef<CBlast_def_line_set> & deflines)
{
    CRef<CBlast_def_line_set> bdls(new CBlast_def_line_set);
    
    istringstream iss(bin_hdr);
    iss >> MSerial_AsnBinary >> *bdls;
    
    deflines.Reset(&* bdls);
}

static CRef<CBlast_def_line_set>
s_EditDeflineSet(CConstRef<CBlast_def_line_set> & deflines)
{
    CRef<CBlast_def_line_set> bdls(new CBlast_def_line_set);
    SerialAssign(*bdls, *deflines);
    return bdls;
}

void
CWriteDB_Impl::x_ExtractDeflines(CConstRef<CBioseq>             & bioseq,
                                 CConstRef<CBlast_def_line_set> & deflines,
                                 string                         & bin_hdr,
                                 const vector< vector<int> >    & membbits,
                                 const vector< vector<int> >    & linkouts,
                                 int                              pig)
{
    bool use_bin = (deflines.Empty() && pig == 0);
    
    if (! bin_hdr.empty()) {
        return;
    }
    
    if (deflines.Empty()) {
        // Use bioseq if deflines are not provided.
        
        if (bioseq.Empty()) {
            NCBI_THROW(CWriteDBException,
                       eArgErr,
                       "Error: Cannot find CBioseq or deflines.");
        }
        
        // CBioseq objects from SeqDB have binary headers embedded in
        // them.  If these are found, we try to use them.  However,
        // using binary headers may not help us much if we also want
        // lists of sequence identifiers (for building ISAM files).
        
        if (use_bin) {
            x_GetBioseqBinaryHeader(*bioseq, bin_hdr);
        }
        
        if (bin_hdr.empty()) {
            x_BuildDeflinesFromBioseq(*bioseq,
                                      deflines,
                                      membbits,
                                      linkouts,
                                      pig);
        }
    }
    
    if (bin_hdr.empty() &&
        (deflines.Empty() || deflines->Get().empty())) {
        
        NCBI_THROW(CWriteDBException,
                   eArgErr,
                   "Error: No deflines provided.");
    }
    
    if (pig != 0) {
        const list<int> * L = 0;
        
        if (deflines->Get().front()->CanGetOther_info()) {
            L = & deflines->Get().front()->GetOther_info();
        }
        
        // If the pig does not agree with the current value, set the
        // new value and force a rebuild of the binary headers.  If
        // there is more than one value in the list, leave the others
        // in place.
        
        if ((L == 0) || L->empty()) {
            CRef<CBlast_def_line_set> bdls = s_EditDeflineSet(deflines);
            bdls->Set().front()->SetOther_info().push_back(pig);
            
            deflines.Reset(&* bdls);
            bin_hdr.erase();
        } else if (L->front() != pig) {
            CRef<CBlast_def_line_set> bdls = s_EditDeflineSet(deflines);
            bdls->Set().front()->SetOther_info().front() = pig;
            
            deflines.Reset(&* bdls);
            bin_hdr.erase();
        }
    }
    
    if (bin_hdr.empty()) {
        // Compress the deflines to binary.
        
        ostringstream oss;
        oss << MSerial_AsnBinary << *deflines;
        bin_hdr = oss.str();
    }
    
    if (deflines.Empty() && (! bin_hdr.empty())) {
        // Uncompress the deflines from binary.
        
        x_SetDeflinesFromBinary(bin_hdr, deflines);
    }
}

void CWriteDB_Impl::x_CookHeader()
{
    x_ExtractDeflines(m_Bioseq,
                      m_Deflines,
                      m_BinHdr,
                      m_Memberships,
                      m_Linkouts,
                      m_Pig);
}

void CWriteDB_Impl::x_CookIds()
{
    if (! m_Ids.empty()) {
        return;
    }
    
    if (m_Deflines.Empty()) {
        if (m_BinHdr.empty()) {
            NCBI_THROW(CWriteDBException,
                       eArgErr,
                       "Error: Cannot find IDs or deflines.");
        }
        
        x_SetDeflinesFromBinary(m_BinHdr, m_Deflines);
    }
    
    ITERATE(list< CRef<CBlast_def_line> >, iter, m_Deflines->Get()) {
        const list< CRef<CSeq_id> > & ids = (**iter).GetSeqid();
        // m_Ids.insert(m_Ids.end(), ids.begin(), ids.end());
        // Spelled out for WorkShop. :-/
        m_Ids.reserve(m_Ids.size() + ids.size());
        ITERATE (list<CRef<CSeq_id> >, it, ids) {
            m_Ids.push_back(*it);
        }
    }
}

void CWriteDB_Impl::x_MaskSequence()
{
    // Scan and mask the sequence itself.
    for(unsigned i = 0; i < m_Sequence.size(); i++) {
        if (m_MaskLookup[m_Sequence[i] & 0xFF] != 0) {
            m_Sequence[i] = m_MaskByte[0];
        }
    }
}

void CWriteDB_Impl::x_CookSequence()
{
    if (! m_Sequence.empty())
        return;
    
    if (! (m_Bioseq.NotEmpty() && m_Bioseq->CanGetInst())) {
        NCBI_THROW(CWriteDBException,
                   eArgErr,
                   "Need sequence data.");
    }
    
    const CSeq_inst & si = m_Bioseq->GetInst();
    
    if (m_Bioseq->GetInst().CanGetSeq_data()) {
        const CSeq_data & sd = si.GetSeq_data();
        
        string msg;
        
        switch(sd.Which()) {
        case CSeq_data::e_Ncbistdaa:
            WriteDB_StdaaToBinary(si, m_Sequence);
            break;
            
        case CSeq_data::e_Ncbieaa:
            WriteDB_EaaToBinary(si, m_Sequence);
            break;
            
        case CSeq_data::e_Iupacaa:
            WriteDB_IupacaaToBinary(si, m_Sequence);
            break;
            
        case CSeq_data::e_Ncbi2na:
            WriteDB_Ncbi2naToBinary(si, m_Sequence);
            break;
            
        case CSeq_data::e_Ncbi4na:
            WriteDB_Ncbi4naToBinary(si, m_Sequence, m_Ambig);
            break;
            
        default:
            msg = "Need to write conversion for data type [";
            msg += NStr::IntToString((int) sd.Which());
            msg += "].";
        }
        
        if (! msg.empty()) {
            NCBI_THROW(CWriteDBException, eArgErr, msg);
        }
    } else {
        int sz = m_SeqVector.size();
        
        if (sz == 0) {
            NCBI_THROW(CWriteDBException,
                       eArgErr,
                       "No sequence data in Bioseq, "
                       "and no Bioseq_Handle available.");
        }
        
        if (m_Protein) {
            // I add one to the string length to allow the "i+1" in
            // the loop to be done safely.
            
            m_Sequence.reserve(sz);
            m_SeqVector.GetSeqData(0, sz, m_Sequence);
        } else {
            // I add one to the string length to allow the "i+1" in the
            // loop to be done safely.
        
            string na8;
            na8.reserve(sz + 1);
            m_SeqVector.GetSeqData(0, sz, na8);
            na8.resize(sz + 1);
        
            string na4;
            na4.resize((sz + 1) / 2);
        
            for(int i = 0; i < sz; i += 2) {
                na4[i/2] = (na8[i] << 4) + na8[i+1];
            }
        
            WriteDB_Ncbi4naToBinary(na4.data(),
                                    na4.size(),
                                    si.GetLength(),
                                    m_Sequence,
                                    m_Ambig);
        }
    }
}

// The CPU should be kept at 190 degrees for 10 minutes.
void CWriteDB_Impl::x_CookData()
{
    // We need sequence, ambiguity, and binary deflines.  If any of
    // these is missing, it is created from other data if possible.
    
    // For now I am disabling binary headers, because in normal usage
    // I would expect to see sequences from ID1 or similar, and the
    // non-binary case is slightly more complex.
    
    x_CookHeader();
    x_CookIds();
    x_CookSequence();
    
    if (m_Protein && m_MaskedLetters.size()) {
        x_MaskSequence();
    }
}

bool CWriteDB_Impl::x_HaveSequence() const
{
    return m_HaveSequence;
}

void CWriteDB_Impl::x_SetHaveSequence()
{
    _ASSERT(! m_HaveSequence);
    m_HaveSequence = true;
}

void CWriteDB_Impl::x_ClearHaveSequence()
{
    _ASSERT(m_HaveSequence);
    m_HaveSequence = false;
}

void CWriteDB_Impl::x_Publish()
{
    // This test should fail only on the first call, or if an
    // exception was thrown.
    
    if (x_HaveSequence()) {
        _ASSERT(! (m_Bioseq.Empty() && m_Sequence.empty()));
        
        x_ClearHaveSequence();
    } else {
        return;
    }
    
    x_CookData();
    
    bool done = false;
    
    if (! m_Volume.Empty()) {
        done = m_Volume->WriteSequence(m_Sequence,
                                       m_Ambig,
                                       m_BinHdr,
                                       m_Ids,
                                       m_Pig);
    }
    
    if (! done) {
        int index = m_VolumeList.size();
        
        if (m_Volume.NotEmpty()) {
            m_Volume->Close();
        }
        
        {
            m_Volume.Reset(new CWriteDB_Volume(m_Dbname,
                                               m_Protein,
                                               m_Title,
                                               m_Date,
                                               index,
                                               m_MaxFileSize,
                                               m_MaxVolumeLetters,
                                               m_Indices));
            
            m_VolumeList.push_back(m_Volume);
        }
        
        done = m_Volume->WriteSequence(m_Sequence,
                                       m_Ambig,
                                       m_BinHdr,
                                       m_Ids,
                                       m_Pig);
        
        if (! done) {
            NCBI_THROW(CWriteDBException,
                       eArgErr,
                       "Cannot write sequence to volume.");
        }
    }
}

void CWriteDB_Impl::SetDeflines(const CBlast_def_line_set & deflines)
{
    m_Deflines.Reset(& deflines);
}

void CWriteDB_Impl::SetPig(int pig)
{
    m_Pig = pig;
}

void CWriteDB_Impl::SetMaxFileSize(Uint8 sz)
{
    m_MaxFileSize = sz;
}

void CWriteDB_Impl::SetMaxVolumeLetters(Uint8 sz)
{
    m_MaxVolumeLetters = sz;
}

CRef<CBlast_def_line_set>
CWriteDB_Impl::ExtractBioseqDeflines(const CBioseq & bs)
{
    // Get information
    
    CConstRef<CBlast_def_line_set> deflines;
    string binary_header;
    vector< vector<int> > v1, v2;
    
    CConstRef<CBioseq> bsref(& bs);
    x_ExtractDeflines(bsref, deflines, binary_header, v2, v2, 0);
    
    // Convert to return type
    
    CRef<CBlast_def_line_set> bdls;
    bdls.Reset(const_cast<CBlast_def_line_set*>(&*deflines));
    
    return bdls;
}

void CWriteDB_Impl::SetMaskedLetters(const string & masked)
{
    // Only supported for protein.
    if (! m_Protein) {
        NCBI_THROW(CWriteDBException,
                   eArgErr,
                   "Error: Nucleotide masking not supported.");
    }
    
    m_MaskedLetters = masked;
    
    if (masked.empty()) {
        vector<char> none;
        m_MaskLookup.swap(none);
        return;
    }
    
    // Convert set of masked letters to stdaa, use the result to build
    // a lookup table.
    
    string mask_bytes;
    CSeqConvert::Convert(m_MaskedLetters,
                         CSeqUtil::e_Iupacaa,
                         0,
                         m_MaskedLetters.size(),
                         mask_bytes,
                         CSeqUtil::e_Ncbistdaa);
    
    _ASSERT(mask_bytes.size() == m_MaskedLetters.size());
    
    // Build a table of character-to-bool.
    // (Bool is represented by char 0 and 1.)
    
    m_MaskLookup.resize(256, (char)0);
    
    for (unsigned i = 0; i < mask_bytes.size(); i++) {
        int ch = ((int) mask_bytes[i]) & 0xFF;
        m_MaskLookup[ch] = (char)1;
    }
    
    // Convert the masking character - always 'X' - to stdaa.
    
    if (m_MaskByte.empty()) {
        string mask_byte = "X";
        
        CSeqConvert::Convert(mask_byte,
                             CSeqUtil::e_Iupacaa,
                             0,
                             1,
                             m_MaskByte,
                             CSeqUtil::e_Ncbistdaa);
        
        _ASSERT(m_MaskByte.size() == 1);
    }
}

void CWriteDB_Impl::ListVolumes(vector<string> & vols)
{
    vols.clear();
    
    ITERATE(vector< CRef<CWriteDB_Volume> >, iter, m_VolumeList) {
        vols.push_back((**iter).GetVolumeName());
    }
}

void CWriteDB_Impl::ListFiles(vector<string> & files)
{
    files.clear();
    
    ITERATE(vector< CRef<CWriteDB_Volume> >, iter, m_VolumeList) {
        (**iter).ListFiles(files);
    }
    
    if (m_VolumeList.size() > 1) {
        files.push_back(x_MakeAliasName());
    }
}

END_NCBI_SCOPE
