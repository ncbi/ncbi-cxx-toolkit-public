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

/// @file seqdbvol.cpp
/// Implementation for the CSeqDBVol class, which provides an
/// interface for all functionality of one database volume.

#include <ncbi_pch.hpp>
#include "seqdbvol.hpp"
#include "seqdboidlist.hpp"

#include <objects/general/general__.hpp>

#include <serial/objistr.hpp>
#include <serial/serial.hpp>
#include <corelib/ncbimtx.hpp>

#include <sstream>

BEGIN_NCBI_SCOPE

CSeqDBVol::CSeqDBVol(CSeqDBAtlas   & atlas,
                     const string  & name,
                     char            prot_nucl)
    : m_Atlas   (atlas),
      m_VolName (name),
      m_Idx     (atlas, name, prot_nucl),
      m_Seq     (atlas, name, prot_nucl),
      m_Hdr     (atlas, name, prot_nucl)
{
    // ISAM files are optional
    
    try {
        m_IsamPig =
            new CSeqDBIsam(m_Atlas,
                           name,
                           prot_nucl,
                           'p',
                           CSeqDBIsam::ePig);
    }
    catch(CSeqDBException &) {
    }
    
    try {
        m_IsamGi =
            new CSeqDBIsam(m_Atlas,
                           name,
                           prot_nucl,
                           'n',
                           CSeqDBIsam::eGi);
    }
    catch(CSeqDBException &) {
    }
    
    try {
        m_IsamStr =
            new CSeqDBIsam(m_Atlas,
                           name,
                           prot_nucl,
                           's',
                           CSeqDBIsam::eStringID);
    }
    catch(CSeqDBException &) {
    }
}

char CSeqDBVol::GetSeqType(void) const
{
    return x_GetSeqType();
}

char CSeqDBVol::x_GetSeqType(void) const
{
    return m_Idx.GetSeqType();
}

Int4 CSeqDBVol::GetSeqLengthProt(Uint4 oid) const
{
    TIndx start_offset = 0;
    TIndx end_offset   = 0;
    
    if (! m_Idx.GetSeqStartEnd(oid, start_offset, end_offset))
        return -1;
    
    _ASSERT(kSeqTypeProt == m_Idx.GetSeqType());

    // Subtract one, for the inter-sequence null.
    return end_offset - start_offset - 1;
}


// Assumes locked.

Int4 CSeqDBVol::GetSeqLengthExact(Uint4 oid) const
{
    TIndx start_offset = 0;
    TIndx end_offset   = 0;
    
    if (! m_Idx.GetSeqStartEnd(oid, start_offset, end_offset))
        return -1;
    
    _ASSERT(m_Idx.GetSeqType() == kSeqTypeNucl);
    
    Int4 whole_bytes = Int4(end_offset - start_offset - 1);
    
    // The last byte is partially full; the last two bits of
    // the last byte store the number of nucleotides in the
    // last byte (0 to 3).
    
    char amb_char = 0;
    
    m_Seq.ReadBytes(& amb_char, end_offset - 1, end_offset);
    
    Int4 remainder = amb_char & 3;
    return (whole_bytes * 4) + remainder;
}


Int4 CSeqDBVol::GetSeqLengthApprox(Uint4 oid) const
{
    TIndx start_offset = 0;
    TIndx end_offset   = 0;
    
    if (! m_Idx.GetSeqStartEnd(oid, start_offset, end_offset))
        return -1;
    
    _ASSERT(m_Idx.GetSeqType() == kSeqTypeNucl);
    
    Int4 whole_bytes = Int4(end_offset - start_offset - 1);
    
    // Same principle as below - but use lower bits of oid
    // instead of fetching the actual last byte.  this should
    // correct for bias, unless sequence length modulo 4 has a
    // significant statistical bias, which seems unlikely to
    // me.
    
    return (whole_bytes * 4) + (oid & 0x03);
}


static CMutex s_MapNaMutex;

static vector<Uint1>
s_SeqDBMapNA2ToNA4Setup(void)
{
    vector<Uint1> translated;
    translated.resize(512);
    
    Uint1 convert[16] = {17,  18,  20,  24,  33,  34,  36,  40,
                         65,  66,  68,  72, 129, 130, 132, 136};
    Int2 pair1 = 0;
    Int2 pair2 = 0;
    
    for(pair1 = 0; pair1 < 16; pair1++) {
        for(pair2 = 0; pair2 < 16; pair2++) {
            Int2 index = (pair1 * 16 + pair2) * 2;
            
            translated[index]   = convert[pair1];
            translated[index+1] = convert[pair2];
        }
    }
    
    return translated;
}

static void
s_SeqDBMapNA2ToNA4(const char   * buf2bit,
                   vector<char> & buf4bit,
                   int            base_length)
{
    static vector<Uint1> expanded = s_SeqDBMapNA2ToNA4Setup();
    
    Uint4 estimated_length = (base_length + 1)/2;
    buf4bit.reserve(estimated_length);
    
    int inp_chars = base_length/4;
    
    for(int i=0; i<inp_chars; i++) {
        Uint4 inp_char = (buf2bit[i] & 0xFF);
        
        buf4bit.push_back(expanded[ (inp_char*2)     ]);
        buf4bit.push_back(expanded[ (inp_char*2) + 1 ]);
    }
    
    int bases_remain = base_length - (inp_chars*4);
    
    if (bases_remain) {
        Uint1 remainder_bits = 2 * bases_remain;
        Uint1 remainder_mask = (0xFF << (8 - remainder_bits)) & 0xFF;
        Uint4 last_masked = buf2bit[inp_chars] & remainder_mask;
        
        buf4bit.push_back(expanded[ (last_masked*2) ]);
        
        if (bases_remain > 2) {
            buf4bit.push_back(expanded[ (last_masked*2)+1 ]);
        }
    }
    
    _ASSERT(estimated_length == buf4bit.size());
}

static vector<Uint1>
s_SeqDBMapNA2ToNA8Setup(void)
{
    // Builds a table; each two bit slice holds 0,1,2 or 3.  These are
    // converted to whole bytes containing 1,2,4, or 8, respectively.
    
    vector<Uint1> translated;
    translated.reserve(1024);
    
    for(Uint4 i = 0; i<256; i++) {
        Uint4 p1 = (i >> 6) & 0x3;
        Uint4 p2 = (i >> 4) & 0x3;
        Uint4 p3 = (i >> 2) & 0x3;
        Uint4 p4 = i & 0x3;
        
        translated.push_back(1 << p1);
        translated.push_back(1 << p2);
        translated.push_back(1 << p3);
        translated.push_back(1 << p4);
    }
    
    return translated;
}

static void
s_SeqDBMapNA2ToNA8(const char   * buf2bit,
                   vector<char> & buf8bit,
                   Uint4          base_length,
                   bool           sentinel_bytes)
{
    static vector<Uint1> expanded = s_SeqDBMapNA2ToNA8Setup();
    
    int sreserve = 0;
    
    if (sentinel_bytes) {
        sreserve = 2;
        buf8bit.push_back((unsigned char)(0));
    }
    
    buf8bit.reserve(base_length + sreserve);
    
    int whole_input_chars = base_length/4;
    
    // In a nucleotide search, this loop is probably a noticeable time
    // consumer, at least relative to the CSeqDB universe.  Each input
    // byte is used to look up a 4 byte output translation.  That four
    // byte section is copied to the output vector.  By pre-processing
    // the arithmetic in the ~Setup() function, we can just pull bytes
    // from a vector.
    
    for(int i = 0; i < whole_input_chars; i++) {
        Uint4 table_offset = (buf2bit[i] & 0xFF) * 4;
        
        buf8bit.push_back(expanded[ table_offset ]);
        buf8bit.push_back(expanded[ table_offset + 1 ]);
        buf8bit.push_back(expanded[ table_offset + 2 ]);
        buf8bit.push_back(expanded[ table_offset + 3 ]);
    }
    
    int bases_remain = base_length & 0x3;
    
    if (bases_remain) {
        // We use the last byte of the input to look up a four byte
        // translation; but we only use the bytes of it that we need.
        
        Uint4 table_offset = (buf2bit[whole_input_chars] & 0xFF) * 4;
        
        while(bases_remain--) {
            buf8bit.push_back(expanded[ table_offset++ ]);
        }
    }
    
    if (sentinel_bytes) {
        buf8bit.push_back((unsigned char)(0));
    }
    
    _ASSERT(base_length == (buf8bit.size() - sreserve));
}

static void
s_SeqDBMapNcbiNA8ToBlastNA8(vector<char> & buf)
{
    Uint1 trans_ncbina8_to_blastna8[] = {
        15, /* Gap, 0 */
        0,  /* A,   1 */
        1,  /* C,   2 */
        6,  /* M,   3 */
        2,  /* G,   4 */
        4,  /* R,   5 */
        9,  /* S,   6 */
        13, /* V,   7 */
        3,  /* T,   8 */
        8,  /* W,   9 */
        5,  /* Y,  10 */
        12, /* H,  11 */
        7,  /* K,  12 */
        11, /* D,  13 */
        10, /* B,  14 */
        14  /* N,  15 */
    };
    
    for(Uint4 i = 0; i < buf.size(); i++) {
        buf[i] = trans_ncbina8_to_blastna8[ (Uint4) buf[i] ];
    }
}

//--------------------
// NEW (long) version
//--------------------

inline Uint4 s_ResLenNew(const vector<Int4> & ambchars, Uint4 i)
{
    return (ambchars[i] >> 16) & 0xFFF;
}

inline Uint4 s_ResPosNew(const vector<Int4> & ambchars, Uint4 i)
{
    return ambchars[i+1];
}

//-----------------------
// OLD (compact) version
//-----------------------

inline Uint4 s_ResVal(const vector<Int4> & ambchars, Uint4 i)
{
    return (ambchars[i] >> 28) & 0xF;
}

inline Uint4 s_ResLenOld(const vector<Int4> & ambchars, Uint4 i)
{
    return (ambchars[i] >> 24) & 0xF;
}

inline Uint4 s_ResPosOld(const vector<Int4> & ambchars, Uint4 i)
{
    return ambchars[i] & 0xFFFFFF; // RES_OFFSET
}


static bool
s_SeqDBRebuildDNA_NA4(vector<char>       & buf4bit,
                      const vector<Int4> & amb_chars)
{
    if (buf4bit.empty())
        return false;
    
    if (amb_chars.empty()) 
        return true;
    
    Uint4 amb_num = amb_chars[0];
    
    /* Check if highest order bit set. */
    bool new_format = (amb_num & 0x80000000) != 0;
    
    if (new_format) {
	amb_num &= 0x7FFFFFFF;
    }
    
    for(Uint4 i=1; i < amb_num+1; i++) {
        Int4  row_len  = 0;
        Int4  position = 0;
        Uint1 char_r   = 0;
        
	if (new_format) {
            char_r    = s_ResVal   (amb_chars, i);
            row_len   = s_ResLenNew(amb_chars, i); 
            position  = s_ResPosNew(amb_chars, i);
	} else {
            char_r    = s_ResVal   (amb_chars, i);
            row_len   = s_ResLenOld(amb_chars, i); 
            position  = s_ResPosOld(amb_chars, i);
	}
        
        Int4  pos = position / 2;
        Int4  rem = position & 1;  /* 0 or 1 */
        Uint1 char_l = char_r << 4;
        
        Int4 j;
        Int4 index = pos;
        
        // This could be made slightly faster for long runs.
        
        for(j = 0; j <= row_len; j++) {
            if (!rem) {
           	buf4bit[index] = (buf4bit[index] & 0x0F) + char_l;
            	rem = 1;
            } else {
           	buf4bit[index] = (buf4bit[index] & 0xF0) + char_r;
            	rem = 0;
                index++;
            }
    	}
        
	if (new_format) /* for new format we have 8 bytes for each element. */
            i++;
    }
    
    return true;
}

static bool
s_SeqDBRebuildDNA_NA8(vector<char>       & buf4bit,
                      const vector<Int4> & amb_chars)
{
    if (buf4bit.empty())
        return false;
    
    if (amb_chars.empty()) 
        return true;
    
    Uint4 amb_num = amb_chars[0];
    
    /* Check if highest order bit set. */
    bool new_format = (amb_num & 0x80000000) != 0;
    
    if (new_format) {
	amb_num &= 0x7FFFFFFF;
    }
    
    for(Uint4 i = 1; i < amb_num+1; i++) {
        Int4  row_len  = 0;
        Int4  position = 0;
        Uint1 trans_ch = 0;
        
	if (new_format) {
            trans_ch  = s_ResVal   (amb_chars, i);
            row_len   = s_ResLenNew(amb_chars, i); 
            position  = s_ResPosNew(amb_chars, i);
	} else {
            trans_ch  = s_ResVal   (amb_chars, i);
            row_len   = s_ResLenOld(amb_chars, i); 
            position  = s_ResPosOld(amb_chars, i);
	}
        
        Int4 index = position;
        
        // This could be made slightly faster for long runs.
        
        for(Int4 j = 0; j <= row_len; j++) {
            buf4bit[index++] = trans_ch;
    	}
        
	if (new_format) /* for new format we have 8 bytes for each element. */
            i++;
    }
    
    return true;
}

static void
s_SeqDBWriteSeqDataProt(CSeq_inst  & seqinst,
                        const char * seq_buffer,
                        Int4         length)
{
    // stuff - ncbistdaa
    // mol = aa
        
    // This possibly/probably copies several times.
    // 1. One copy into stdaa_data.
    // 2. Second copy into NCBIstdaa.
    // 3. Third copy into seqdata.
    
    vector<char> aa_data;
    aa_data.resize(length);
    
    for(int i = 0; i < length; i++) {
        aa_data[i] = seq_buffer[i];
    }
    
    seqinst.SetSeq_data().SetNcbistdaa().Set().swap(aa_data);
    seqinst.SetMol(CSeq_inst::eMol_aa);
}

static void
s_SeqDBWriteSeqDataNucl(CSeq_inst    & seqinst,
                        const char   * seq_buffer,
                        Int4           length)
{
    Int4 whole_bytes  = length / 4;
    Int4 partial_byte = ((length & 0x3) != 0) ? 1 : 0;
    
    vector<char> na_data;
    na_data.resize(whole_bytes + partial_byte);
    
    for(int i = 0; i<whole_bytes; i++) {
        na_data[i] = seq_buffer[i];
    }
    
    if (partial_byte) {
        na_data[whole_bytes] = seq_buffer[whole_bytes] & (0xFF - 0x03);
    }
    
    seqinst.SetSeq_data().SetNcbi2na().Set().swap(na_data);
    seqinst.SetMol(CSeq_inst::eMol_na);
}

static void
s_SeqDBWriteSeqDataNucl(CSeq_inst    & seqinst,
                        const char   * seq_buffer,
                        Int4           length,
                        vector<Int4> & amb_chars)
{
    vector<char> buffer_4na;
    s_SeqDBMapNA2ToNA4(seq_buffer, buffer_4na, length); // length is not /4 here
    s_SeqDBRebuildDNA_NA4(buffer_4na, amb_chars);
    
    seqinst.SetSeq_data().SetNcbi4na().Set().swap(buffer_4na);
    seqinst.SetMol(CSeq_inst::eMol_na);
}

void s_GetDescrFromDefline(CRef<CBlast_def_line_set> deflines, string & descr)
{
    descr.erase();
    
    string seqid_str;
    
    typedef list< CRef<CBlast_def_line> >::const_iterator TDefIt; 
    typedef list< CRef<CSeq_id        > >::const_iterator TSeqIt;
   
    const list< CRef<CBlast_def_line> > & dl = deflines->Get();
    
    for(TDefIt iter = dl.begin(); iter != dl.end(); iter++) {
        ostringstream oss;
        
        const CBlast_def_line & defline = **iter;
        
        if (! descr.empty()) {
            //oss << "\1";
            oss << " ";
        }
        
        if (defline.CanGetSeqid()) {
            const list< CRef<CSeq_id > > & sl = defline.GetSeqid();
            
            for (TSeqIt seqit = sl.begin(); seqit != sl.end(); seqit++) {
                (*seqit)->WriteAsFasta(oss);
            }
        }
        
        if (defline.CanGetTitle()) {
            oss << defline.GetTitle();
        }
        
        descr += oss.str();
    }
}

// This is meant to mimic the readdb behavior.  It produces a string
// with the title of the first defline, then the seqids plus title for
// each subsequent defline.

void s_GetBioseqTitle(CRef<CBlast_def_line_set> deflines, string & title)
{
    title.erase();
    
    string seqid_str;
    
    typedef list< CRef<CBlast_def_line> >::const_iterator TDefIt; 
    typedef list< CRef<CSeq_id        > >::const_iterator TSeqIt;
    
    const list< CRef<CBlast_def_line> > & dl = deflines->Get();
    
    bool first_defline(true);
    
    for(TDefIt iter = dl.begin(); iter != dl.end(); iter++) {
        ostringstream oss;
        
        const CBlast_def_line & defline = **iter;
        
        if (! title.empty()) {
            //oss << "\1";
            oss << " ";
        }
        
        bool wrote_seqids(false);
        
        if ((!first_defline) && defline.CanGetSeqid()) {
            const list< CRef<CSeq_id > > & sl = defline.GetSeqid();
            
            bool first_seqid(true);
            
            // First should look like: "<title>"
            // Others should look like: " ><seqid>|<seqid>|<seqid><title>"
            
            // Should this be two sections not one loop?

            for (TSeqIt seqit = sl.begin(); seqit != sl.end(); seqit++) {
                if (first_seqid) {
                    oss << ">";
                } else {
                    oss << "|";
                }
                
                (*seqit)->WriteAsFasta(oss);
                
                first_seqid = false;
                wrote_seqids = true;
            }
        }
        
        // Omit seqids from first defline
        first_defline = false;
        
        if (defline.CanGetTitle()) {
            if (wrote_seqids) {
                oss << " ";
            }
            oss << defline.GetTitle();
        }
        
        title += oss.str();
    }
}

static bool s_SeqDB_SeqIdIn(const list< CRef< CSeq_id > > & a, const CSeq_id & b)
{
    typedef list< CRef<CSeq_id> > TSeqidList;
    
    for(TSeqidList::const_iterator now = a.begin(); now != a.end(); now++) {
        CSeq_id::E_SIC rv = (*now)->Compare(b);
        
        switch(rv) {
        case CSeq_id::e_YES:
            return true;
            
        case CSeq_id::e_NO:
            return false;
            
        default:
            break;
        }
    }
    
    return false;
}

CRef<CBlast_def_line_set>
CSeqDBVol::x_GetTaxDefline(Uint4            oid,
                           bool             have_oidlist,
                           Uint4            membership_bit,
                           Uint4            preferred_gi,
                           CSeqDBLockHold & locked) const
{
    typedef list< CRef<CBlast_def_line> > TBDLL;
    typedef TBDLL::iterator               TBDLLIter;
    typedef TBDLL::const_iterator         TBDLLConstIter;
    
    // 1. read a defline set w/ gethdr.
    
    CRef<CBlast_def_line_set> BDLS = x_GetHdrText(oid, locked);
    
    // 2. filter based on "rdfp->membership_bit" or similar.
    
    if (have_oidlist && (membership_bit != 0)) {
        // Create the memberships mask (should this be fixed to allow
        // membership bits greater than 32?)
        
        Uint4 memb_mask = 0x1 << (membership_bit-1);
        
        TBDLL & dl = BDLS->Set();
        
        for(TBDLLIter iter = dl.begin(); iter != dl.end(); ) {
            const CBlast_def_line & defline = **iter;
            
            if ((defline.CanGetMemberships() == false) ||
                ((defline.GetMemberships().front() & memb_mask)) == 0) {
                
                TBDLLIter eraseme = iter++;
                
                dl.erase(eraseme);
            }
        }
    }
    
    // 3. if there is a preferred gi, bump it to the top.
    
    if (preferred_gi != 0) {
        CSeq_id seqid(CSeq_id::e_Gi, preferred_gi);
        
        TBDLL & dl = BDLS->Set();
        
        for(TBDLLIter iter = dl.begin(); iter != dl.end(); ) {
            if (s_SeqDB_SeqIdIn((**iter).GetSeqid(), seqid)) {
                dl.push_front(*iter);
                dl.erase(iter);
                break;
            }
        }
    }
    
    return BDLS;
}

CRef<CSeqdesc>
CSeqDBVol::x_GetTaxonomy(Uint4                 oid,
                         bool                  have_oidlist,
                         Uint4                 membership_bit,
                         Uint4                 preferred_gi,
                         CRef<CSeqDBTaxInfo>   tax_info,
                         CSeqDBLockHold      & locked) const
{
    const char * TAX_DATA_OBJ_LABEL = "TaxNamesData";
    
    CRef<CSeqdesc> taxonomy;
    
    CRef<CBlast_def_line_set> bdls =
        x_GetTaxDefline(oid,
                        have_oidlist,
                        membership_bit,
                        preferred_gi,
                        locked);
    
    if (bdls.Empty()) {
        return taxonomy;
    }
    
    typedef list< CRef<CBlast_def_line> > TBDLL;
    typedef TBDLL::iterator               TBDLLIter;
    typedef TBDLL::const_iterator         TBDLLConstIter;
    
    const TBDLL & dl = bdls->Get();
    
    CRef<CUser_object> uobj(new CUser_object);
    
    CRef<CObject_id> uo_oi(new CObject_id);
    uo_oi->SetStr(TAX_DATA_OBJ_LABEL);
    uobj->SetType(*uo_oi);
    
    bool found = false;
    
    for(TBDLLConstIter iter = dl.begin(); iter != dl.end(); iter ++) {
        Uint4 taxid = (*iter)->GetTaxid();
        
        CSeqDBTaxNames tnames;
        
        if (tax_info.Empty()) {
            continue;
        }
        
        bool worked = tax_info->GetTaxNames(taxid, tnames, locked);
        
        if (! worked) {
            continue;
        }
        
        found = true;
        
        CRef<CUser_field> uf(new CUser_field);
        
        CRef<CObject_id> uf_oi(new CObject_id);
        uf_oi->SetId(taxid);
        uf->SetLabel(*uf_oi);
        
        vector<string> & strs = uf->SetData().SetStrs();
        
        uf->SetNum(4);
        strs.resize(4);
        
        strs[0] = tnames.GetSciName();
        strs[1] = tnames.GetCommonName();
        strs[2] = tnames.GetBlastName();
        strs[3] = tnames.GetSKing();
        
        uobj->SetData().push_back(uf);
    }
    
    if (found) {
        taxonomy = new CSeqdesc;
        taxonomy->SetUser(*uobj); // or something.
    }
    
    return taxonomy;
}

CRef<CSeqdesc> CSeqDBVol::x_GetAsnDefline(Uint4 oid, CSeqDBLockHold & locked) const
{
    const char * ASN1_DEFLINE_LABEL = "ASN1_BlastDefLine";
    
    CRef<CSeqdesc> asndef;
    
    vector<char> hdr_data;
    x_GetHdrBinary(oid, hdr_data, locked);
    
    if (! hdr_data.empty()) {
        CRef<CUser_object> uobj(new CUser_object);
    
        CRef<CObject_id> uo_oi(new CObject_id);
        uo_oi->SetStr(ASN1_DEFLINE_LABEL);
        uobj->SetType(*uo_oi);
        
        CRef<CUser_field> uf(new CUser_field);
        
        CRef<CObject_id> uf_oi(new CObject_id);
        uf_oi->SetStr(ASN1_DEFLINE_LABEL);
        uf->SetLabel(*uf_oi);
        
        vector< vector<char>* > & strs = uf->SetData().SetOss();
        uf->SetNum(1);
        
        strs.push_back(new vector<char>);
        strs[0]->swap(hdr_data);
        
        uobj->SetData().push_back(uf);
        
        asndef = new CSeqdesc;
        asndef->SetUser(*uobj); // or something.
    }
    
    return asndef;
}

CRef<CBioseq>
CSeqDBVol::GetBioseq(Uint4                 oid,
                     bool                  have_oidlist,
                     Uint4                 memb_bit,
                     Uint4                 pref_gi,
                     CRef<CSeqDBTaxInfo>   tax_info,
                     CSeqDBLockHold      & locked) const
{
    CRef<CBioseq> null_result;
    
    // Get the defline set.  Probably this is like the function above
    // that gets asn, but split it up more when returning it.
    
    CRef<CBlast_def_line_set> defline_set(x_GetHdrText(oid, locked));
    CRef<CBlast_def_line>     defline;
    list< CRef< CSeq_id > >   seqids;
    
    if (defline_set.Empty() ||
        (! defline_set->CanGet())) {
        return null_result;
    }
        
    defline = defline_set->Get().front();
        
    if (! defline->CanGetSeqid()) {
        return null_result;
    }
        
    seqids = defline->GetSeqid();
    
    // Get length & sequence.
    
    const char * seq_buffer = 0;
    
    Int4 length = x_GetSequence(oid, & seq_buffer, false, locked, false);
    
    if (length < 1) {
        return null_result;
    }
    
    // If protein, we set bsp->mol = Seq_mol_aa, seq_data_type =
    // Seq_code_ncbistdaa; then we write the buffer into the byte
    // store (or equivalent).
    //
    // Nucleotide sequences require more work:
    // a. Try to get ambchars
    // b. If there are any, convert sequence to 4 byte rep.
    // c. Otherwise write to a byte store.
    // d. Set mol = Seq_mol_na;
    
    CRef<CBioseq> bioseq(new CBioseq);
    
    CSeq_inst & seqinst = bioseq->SetInst();
    
    bool is_prot = (x_GetSeqType() == kSeqTypeProt);
    
    if (is_prot) {
        s_SeqDBWriteSeqDataProt(seqinst, seq_buffer, length);
    } else {
        // nucl
        vector<Int4> ambchars;
        
        if (! x_GetAmbChar(oid, ambchars, locked)) {
            return null_result;
        }
        
        if (ambchars.empty()) {
            // keep as 2 bit
            s_SeqDBWriteSeqDataNucl(seqinst, seq_buffer, length);
        } else {
            // translate to 4 bit
            s_SeqDBWriteSeqDataNucl(seqinst, seq_buffer, length, ambchars);
        }
        
        // mol = na
        seqinst.SetMol(CSeq_inst::eMol_na);
    }
    
    if (seq_buffer) {
        seq_buffer = 0;
    }
    
    // Set the length, id (Seq_id), and repr (== raw).
    
    seqinst.SetLength(length);
    seqinst.SetRepr(CSeq_inst::eRepr_raw);
    bioseq->SetId().swap(seqids);
    
    // If the format is binary, we get the defline and chain it onto
    // the bsp->desc list; then we read and append taxonomy names to
    // the list (x_GetTaxonomy()).
    
    if (defline_set.NotEmpty()) {
        // Convert defline to string.
        
        string description;
        
        s_GetBioseqTitle(defline_set, description);
        
        CRef<CSeqdesc> desc1(new CSeqdesc);
        desc1->SetTitle().swap(description);
        
        CRef<CSeqdesc> desc2( x_GetAsnDefline(oid, locked) );
        
        CSeq_descr & seq_desc_set = bioseq->SetDescr();
        seq_desc_set.Set().push_back(desc1);
        
        if (! desc2.Empty()) {
            seq_desc_set.Set().push_back(desc2);
        }
    }
    
    CRef<CSeqdesc> tax = x_GetTaxonomy(oid,
                                       have_oidlist,
                                       memb_bit,
                                       pref_gi,
                                       tax_info,
                                       locked);
    
    if (tax.NotEmpty()) {
        CSeq_descr & seqdesc = bioseq->SetDescr();
        seqdesc.Set().push_back(tax);
    }
    
    return bioseq;
}

char * CSeqDBVol::x_AllocType(Uint4 length, ESeqDBAllocType alloc_type, CSeqDBLockHold & locked) const
{
    // Specifying an allocation type of zero uses the atlas to do the
    // allocation.  This is not intended to be visible to the end
    // user, so it is not enumerated in seqdbcommon.hpp.
    
    char * retval = 0;
    
    switch(alloc_type) {
    case eMalloc:
        retval = (char*) malloc(length);
        break;
        
// MemNew is not available.
//     case eMemNew:
//         retval = (char*) MemNew(length);
//         break;
        
    case eNew:
        retval = new char[length];
        break;
        
    default:
        retval = m_Atlas.Alloc(length, locked);
    }
    
    return retval;
}

Int4 CSeqDBVol::GetAmbigSeq(Int4               oid,
                            char            ** buffer,
                            Uint4              nucl_code,
                            ESeqDBAllocType    alloc_type,
                            CSeqDBLockHold   & locked) const
{
    char * buf1 = 0;
    Int4 baselen = x_GetAmbigSeq(oid, & buf1, nucl_code, alloc_type, locked);
    
    *buffer = buf1;
    return baselen;
}

Int4 CSeqDBVol::x_GetAmbigSeq(Int4               oid,
                              char            ** buffer,
                              Uint4              nucl_code,
                              ESeqDBAllocType    alloc_type,
                              CSeqDBLockHold   & locked) const
{
    Int4 base_length = -1;
    
    if (kSeqTypeProt == m_Idx.GetSeqType()) {
        if (alloc_type == ESeqDBAllocType(0)) {
            base_length = x_GetSequence(oid, (const char**) buffer, true, locked, false);
        } else {
            const char * buf2(0);
            
            base_length = x_GetSequence(oid, & buf2, false, locked, false);
            
            char * obj = x_AllocType(base_length, alloc_type, locked);
            
            memcpy(obj, buf2, base_length);
            
            *buffer = obj;
        }
    } else {
        vector<char> buffer_na8;
        
        {
            // The code in this block is derived from GetBioseq() and
            // s_SeqDBWriteSeqDataNucl().
            
            // Get the length and the (probably mmapped) data.
            
            const char * seq_buffer = 0;
            
            base_length = x_GetSequence(oid, & seq_buffer, false, locked, false);
            
            if (base_length < 1) {
                NCBI_THROW(CSeqDBException, eFileErr,
                           "Error: could not get sequence.");
            }
            
            // Get ambiguity characters.
            
            vector<Int4> ambchars;
            
            if (! x_GetAmbChar(oid, ambchars, locked) ) {
                NCBI_THROW(CSeqDBException, eFileErr,
                           "File error: could not get ambiguity data.");
            }
            
            // Combine and translate to 4 bits-per-character encoding.
            
            bool sentinel = (nucl_code == kSeqDBNuclBlastNA8);
            
            s_SeqDBMapNA2ToNA8(seq_buffer, buffer_na8, base_length, sentinel);
            s_SeqDBRebuildDNA_NA8(buffer_na8, ambchars);
            
            if (sentinel) {
                // Translate bytewise, in place.
                s_SeqDBMapNcbiNA8ToBlastNA8(buffer_na8);
            }
        }
        
        int bytelen = buffer_na8.size();
        char * uncomp_buf = x_AllocType(bytelen, alloc_type, locked);
        
        for(int i = 0; i < bytelen; i++) {
            uncomp_buf[i] = buffer_na8[i];
        }
        
        *buffer = uncomp_buf;
    }
    
    return base_length;
}

Int4 CSeqDBVol::x_GetSequence(Int4             oid,
                              const char    ** buffer,
                              bool             keep,
                              CSeqDBLockHold & locked,
                              bool             can_release) const
{
    TIndx start_offset = 0;
    TIndx end_offset   = 0;
    
    Int4 length = -1;
    
    if (! m_Idx.GetSeqStartEnd(oid, start_offset, end_offset))
        return -1;
    
    char seqtype = m_Idx.GetSeqType();
    
    if (kSeqTypeProt == seqtype) {
        // Subtract one, for the inter-sequence null.
        
        end_offset --;
        
        length = end_offset - start_offset;
        
        *buffer = m_Seq.GetRegion(start_offset, end_offset, keep, locked);
    } else if (kSeqTypeNucl == seqtype) {
        // The last byte is partially full; the last two bits of
        // the last byte store the number of nucleotides in the
        // last byte (0 to 3).
        
        *buffer = m_Seq.GetRegion(start_offset, end_offset, keep, locked);
        
        // If we are returning a hold on the sequence (keep), and the
        // caller does not need the lock after this (can_release) we
        // can let go of the lock (because the hold will prevent GC of
        // the underlying data).  This will allow the following data
        // access to occur outside of the locked duration - lowering
        // contention in the nucleotide case.
        
        if (keep && can_release) {
            m_Atlas.Unlock(locked);
        }
        
        Int4 whole_bytes = end_offset - start_offset - 1;
        
        char last_char = (*buffer)[whole_bytes];
        
        Int4 remainder = last_char & 3;
        length = (whole_bytes * 4) + remainder;
    }
    
    return length;
}

list< CRef<CSeq_id> > CSeqDBVol::GetSeqIDs(Uint4 oid, CSeqDBLockHold & locked) const
{
    list< CRef< CSeq_id > > seqids;
    
    CRef<CBlast_def_line_set> defline_set(x_GetHdrText(oid, locked));
    
    if ((! defline_set.Empty()) && defline_set->CanGet()) {
        CRef<CBlast_def_line> defline = defline_set->Get().front();
        
        if (defline->CanGetSeqid()) {
            seqids = defline->GetSeqid();
        }
    }
    
    return seqids;
}

Uint8 CSeqDBVol::GetTotalLength(void) const
{
    return m_Idx.GetTotalLength();
}

CRef<CBlast_def_line_set>
CSeqDBVol::GetHdr(Uint4 oid, CSeqDBLockHold & locked) const
{
    return x_GetHdrText(oid, locked);
}

CRef<CBlast_def_line_set>
CSeqDBVol::x_GetHdrText(Uint4 oid, CSeqDBLockHold & locked) const
{
    CRef<CBlast_def_line_set> nullret;
    
    TIndx hdr_start = 0;
    TIndx hdr_end   = 0;
    
    if (! m_Idx.GetHdrStartEnd(oid, hdr_start, hdr_end)) {
        return nullret;
    }
    
    const char * asn_region = m_Hdr.GetRegion(hdr_start, hdr_end, locked);
    
    // Now; create an ASN.1 object from the memory chunk provided
    // here.
        
    if (! asn_region) {
        return nullret;
    }
    
    // Get the data as a string, then as a stringstream, then build
    // the actual asn.1 object.  How much 'extra' work is done here?
    // Perhaps a dedicated ASN.1 memory-stream object would be of some
    // benefit, particularly in the "called 100000 times" cases.
    
    istringstream asndata( string(asn_region, asn_region + (hdr_end - hdr_start)) );
    
    auto_ptr<CObjectIStream> inpstr(CObjectIStream::Open(eSerial_AsnBinary, asndata));
    
    CRef<objects::CBlast_def_line_set> phil(new objects::CBlast_def_line_set);
    
    *inpstr >> *phil;
    
    
    return phil;
}

void CSeqDBVol::x_GetHdrBinary(Uint4 oid, vector<char> & hdr_data, CSeqDBLockHold & locked) const
{
    hdr_data.clear();
    
    TIndx hdr_start = 0;
    TIndx hdr_end   = 0;
    
    if (! m_Idx.GetHdrStartEnd(oid, hdr_start, hdr_end)) {
        return;
    }
    
    const char * asn_region = m_Hdr.GetRegion(hdr_start, hdr_end, locked);
    
    // If empty, nothing to do.
    
    if (! asn_region) {
        return;
    }
    
    // Return the binary byte data as a string.
    
    const char * asn_region_end = asn_region + (hdr_end - hdr_start);
    
    hdr_data.assign(asn_region, asn_region_end);
}

bool CSeqDBVol::x_GetAmbChar(Uint4 oid, vector<Int4> & ambchars, CSeqDBLockHold & locked) const
{
    TIndx start_offset = 0;
    TIndx end_offset   = 0;
    
    bool ok = m_Idx.GetAmbStartEnd(oid, start_offset, end_offset);
    
    if (! ok) {
        return false;
    }
    
    Int4 length = Int4(end_offset - start_offset);
    
    if (0 == length)
        return true;
    
    Int4 total = length / 4;
    
    Int4 * buffer =
        (Int4*) m_Seq.GetRegion(start_offset,
                                start_offset + (total * 4),
                                false,
                                locked);
    
    // This makes no sense...
    total &= 0x7FFFFFFF;
    
    ambchars.resize(total);
    
    for(int i = 0; i<total; i++) {
	ambchars[i] = SeqDB_GetStdOrd((const Uint4 *)(& buffer[i]));
    }
    
    return true;
}

Uint4 CSeqDBVol::GetNumSeqs(void) const
{
    return m_Idx.GetNumSeqs();
}

string CSeqDBVol::GetTitle(void) const
{
    return m_Idx.GetTitle();
}

string CSeqDBVol::GetDate(void) const
{
    return m_Idx.GetDate();
}

Uint4 CSeqDBVol::GetMaxLength(void) const
{
    return m_Idx.GetMaxLength();
}

bool CSeqDBVol::PigToOid(Uint4 pig, Uint4 & oid, CSeqDBLockHold & locked) const
{
    if (m_IsamPig.Empty()) {
        return false;
    }
    
    return m_IsamPig->PigToOid(pig, oid, locked);
}

bool CSeqDBVol::GetPig(Uint4 oid, Uint4 & pig, CSeqDBLockHold & locked) const
{
    pig = (Uint4) -1;
    
    if (m_IsamPig.Empty()) {
        return false;
    }
    
    CRef<CBlast_def_line_set> BDLS = x_GetHdrText(oid, locked);
    
    if (BDLS.Empty() || (! BDLS->CanGet())) {
        return false;
    }
    
    typedef list< CRef< CBlast_def_line > >::const_iterator TI1;
    typedef list< int >::const_iterator TI2;
    
    TI1 it1 = BDLS->Get().begin();
    
    for(; it1 != BDLS->Get().end();  it1++) {
        if ((*it1)->CanGetOther_info()) {
            TI2 it2 = (*it1)->GetOther_info().begin();
            TI2 it2end = (*it1)->GetOther_info().end();
            
            for(; it2 != it2end;  it2++) {
                if (*it2 != -1) {
                    pig = *it2;
                    return true;
                }
            }
        }
    }
    
    return false;
}

bool CSeqDBVol::GiToOid(Uint4 gi, Uint4 & oid, CSeqDBLockHold & locked) const
{
    if (m_IsamGi.Empty()) {
        return false;
    }
    
    return m_IsamGi->GiToOid(gi, oid, locked);
}

bool CSeqDBVol::GetGi(Uint4 oid, Uint4 & gi, CSeqDBLockHold & locked) const
{
    gi = (Uint4) -1;
    
    if (m_IsamGi.Empty()) {
        return false;
    }
    
    CRef<CBlast_def_line_set> BDLS = x_GetHdrText(oid, locked);
    
    if (BDLS.Empty() || (! BDLS->CanGet())) {
        return false;
    }
    
    typedef list< CRef< CBlast_def_line > >::const_iterator TI1;
    typedef list< CRef< CSeq_id > >::const_iterator TI2;
    
    TI1 it1 = BDLS->Get().begin();
    
    // Iterate over all blast def lines in the set
    
    for(; it1 != BDLS->Get().end();  it1++) {
        if ((*it1)->CanGetSeqid()) {
            TI2 it2 = (*it1)->GetSeqid().begin();
            TI2 it2end = (*it1)->GetSeqid().end();
            
            // Iterate within each defline
            
            for(; it2 != it2end;  it2++) {
                if ((*it2)->IsGi()) {
                    gi = (*it2)->GetGi();
                    return true;
                }
            }
        }
    }
    
    return false;
}

void CSeqDBVol::UnLease()
{
    m_Idx.UnLease();
    m_Seq.UnLease();
    m_Hdr.UnLease();
    
    if (m_IsamPig.NotEmpty()) {
        m_IsamPig->UnLease();
    }
    if (m_IsamGi.NotEmpty()) {
        m_IsamGi->UnLease();
    }
    if (m_IsamStr.NotEmpty()) {
        m_IsamStr->UnLease();
    }
}

END_NCBI_SCOPE

