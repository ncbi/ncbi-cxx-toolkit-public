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

#include <ncbi_pch.hpp>
#include "seqdbvol.hpp"

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
}

char CSeqDBVol::GetSeqType(void) const
{
    return x_GetSeqType();
}

char CSeqDBVol::x_GetSeqType(void) const
{
    return m_Idx.GetSeqType();
}

Int4 CSeqDBVol::GetSeqLength(Uint4 oid, bool approx, CSeqDBLockHold & locked) const
{
    Uint8 start_offset = 0;
    Uint8 end_offset   = 0;
    
    Int8 length = -1;
    
    if (! m_Idx.GetSeqStartEnd(oid, start_offset, end_offset, locked))
        return -1;
    
    char seqtype = m_Idx.GetSeqType();
    
    if (kSeqTypeProt == seqtype) {
        // Subtract one, for the inter-sequence null.
        length = end_offset - start_offset - 1;
    } else if (kSeqTypeNucl == seqtype) {
        Int4 whole_bytes = Int4(end_offset - start_offset - 1);
        
        if (approx) {
            // Same principle as below - but use lower bits of oid
            // instead of fetching the actual last byte.  this should
            // correct for bias, unless sequence length modulo 4 has a
            // significant statistical bias, which seems unlikely to
            // me.
            
            length = (whole_bytes * 4) + (oid & 0x03);
        } else {
            // The last byte is partially full; the last two bits of
            // the last byte store the number of nucleotides in the
            // last byte (0 to 3).
            
            char amb_char = 0;
            
            m_Seq.ReadBytes(& amb_char, end_offset - 1, end_offset, locked);
            
            Int4 remainder = amb_char & 3;
            length = (whole_bytes * 4) + remainder;
        }
    }
    
    return length;
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
            buf8bit.push_back(expanded[ ++table_offset ]);
        }
    }
    
    if (sentinel_bytes) {
        buf8bit.push_back((unsigned char)(0));
    }
    
    _ASSERT(base_length == (buf8bit.size() - sreserve));
}

#if 0
static vector<Uint1>
s_SeqDBMapNcbiNA4ToBlastNA4Setup(void)
{
    vector<Uint1> translated;
    translated.resize(256);
    
    // This mapping stolen., er, courtesy of blastkar.c.
    
    Uint1 trans_ncbi4na_to_blastna[] = {
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
    
    for(int i = 0; i<256; i++) {
        int a1 = (i >> 4) & 0xF;
        int a2 = i & 0xF;
        
        int b1 = trans_ncbi4na_to_blastna[a1];
        int b2 = trans_ncbi4na_to_blastna[a2];
        
        translated[i] = (b1 << 4) | b2;
    }
    
    return translated;
}

static void
s_SeqDBMapNcbiNA4ToBlastNA4(vector<char> & buf)
{
    static vector<Uint1> trans = s_SeqDBMapNcbiNA4ToBlastNA4Setup();
    
    for(Uint4 i = 0; i < buf.size(); i++) {
        buf[i] = trans[ unsigned(buf[i]) & 0xFF ];
    }
}
#endif

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
    return (ambchars[i] >> 28);
}

inline Uint4 s_ResLenOld(const vector<Int4> & ambchars, Uint4 i)
{
    return (ambchars[i] >> 24) & 0xF;
}

inline Uint4 s_ResPosOld(const vector<Int4> & ambchars, Uint4 i)
{
    return ambchars[i];
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
//             if (!rem) {
//            	buf4bit[index] = (buf4bit[index] & 0x0F) + char_l;
//             	rem = 1;
//             } else {
//            	buf4bit[index] = (buf4bit[index] & 0xF0) + char_r;
//             	rem = 0;
//                 index++;
//             }
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

CRef<CBioseq>
CSeqDBVol::GetBioseq(Int4 oid,
                     bool /*use_objmgr,*/,
                     bool /*insert_ctrlA*/,
                     CSeqDBLockHold & locked) const
{
    // 1. Declare variables.
    
    CRef<CBioseq> null_result;
    
    // 2. if we can't find the seq_num (get_link), we are done.
    
    //xx  Not relevant - the layer above this would do that before
    //xx  This specific 'CSeqDB' object was involved.
    
    // 3. get the descriptor - i.e. sip, defline.  Probably this
    // is like the function above that gets asn, but split it up
    // more when returning it.
    
    // QUESTIONS: What do we actually want?  The defline & seqid, or
    // the defline set and seqid set?  From readdb.c it looks like the
    // single items are the prized artifacts.
    
    CRef<CBlast_def_line_set> defline_set(x_GetHdr(oid, locked));
    CRef<CBlast_def_line>     defline;
    list< CRef< CSeq_id > >   seqids;
    
    {
        if (defline_set.Empty() ||
            (! defline_set->CanGet())) {
            return null_result;
        }
        
    
        defline = defline_set->Get().front();
        
        if (! defline->CanGetSeqid()) {
            // Not sure if this is necessary - if it turns out we can
            // handle a null, this should be taken back out.
            return null_result;
        }
        
        // Do we want the list, or just the first CRef<>??  For now, get
        // the whole list.
        
        seqids = defline->GetSeqid();
    }
    
    // 4. If insert_ctrlA is FALSE, we convert each "^A" to " >";
    // Otherwise, we just copy the defline across. (inline func?)
    
    //xx Ignoring ctrl-A issue for now (as per Tom's insns.)
    
    // 5. Get length & sequence (_get_sequence).  That should be
    // mimicked before this.
    
    const char * seq_buffer = 0;
    
    Int4 length = x_GetSequence(oid, & seq_buffer, false, locked);
    
    if (length < 1) {
        return null_result;
    }
    
    // 6. If using obj mgr, BioseqNew() is called; otherwise
    // MemNew().  --> This is the BioseqPtr, bsp.
    
    //byte_store = BSNew(0);
    
    // 7. A byte store is created (BSNew()).
    
    //xx No allocation is done; instead we just call *Set() etc.
    
    // 8. If protein, we set bsp->mol = Seq_mol_aa, seq_data_type
    // = Seq_code_ncbistdaa; then we write the buffer into the
    // byte store.
    
    // 9. Nucleotide sequences require more pain, suffering.
    // a. Try to get ambchars
    // b. If there are any, convert sequence to 4 byte rep.
    // c. Otherwise write to a byte store.
    // d. Set mol = Seq_mol_na;
    
    // 10. Stick the byte store into the bsp->seq_data
    
    CRef<CBioseq> bioseq(new CBioseq);
    
    CSeq_inst & seqinst = bioseq->SetInst();
    //CSeq_data & seqdata = seqinst->SetSeq_data();
    
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
    
    // 11. Set the length, id (Seq_id), and repr (== raw).
    
    seqinst.SetLength(length);
    seqinst.SetRepr(CSeq_inst::eRepr_raw);
    bioseq->SetId().swap(seqids);
    
    // 12. Add the new_defline to the list at bsp->descr if
    // non-null, with ValNodeAddString().
    
    // 13. If the format is not text (si), we get the defline and
    // chain it onto the bsp->desc list; then we read and append
    // taxonomy names to the list (readdb_get_taxonomy_names()).
    
    if (defline_set.NotEmpty()) {
        // Convert defline to ... string.
        
        //1. Have a defline.. maybe.  Want to add this as the title.(?)
        // A. How to add a description item:
        
        string description;
        
        s_GetDescrFromDefline(defline_set, description);
        
        CRef<CSeqdesc> desc1(new CSeqdesc);
        desc1->SetTitle().swap(description);
        
        CSeq_descr & seqdesc = bioseq->SetDescr();
        seqdesc.Set().push_back(desc1);
    }
    
    // I'm going to let the taxonomy slide for now....
    
    //cerr << "Taxonomy, etc." << endl;

    // 14. Return the bsp.
    
    // Everything seems eerily quiet so far, so...
    
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
        
// MemNew is not available yet.
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
            base_length = x_GetSequence(oid, (const char**) buffer, true, locked);
        } else {
            const char * buf2(0);
            
            base_length = x_GetSequence(oid, & buf2, false, locked);
            
            char * obj = x_AllocType(base_length, alloc_type, locked);
            
            memcpy(obj, buf2, base_length);
            
            *buffer = obj;
        }
    } else {
        vector<char> buffer_na8;
        
        {
            // The code in this block is a few excerpts from
            // GetBioseq() and s_SeqDBWriteSeqDataNucl().
            
            // Get the length and the (probably mmapped) data.
            
            const char * seq_buffer = 0;
            
            base_length = x_GetSequence(oid, & seq_buffer, false, locked);
            
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

Int4 CSeqDBVol::x_GetSequence(Int4 oid, const char ** buffer, bool keep, CSeqDBLockHold & locked) const
{
    Uint8 start_offset = 0;
    Uint8 end_offset   = 0;
    
    Int4 length = -1;
    
    if (! m_Idx.GetSeqStartEnd(oid, start_offset, end_offset, locked))
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
    
    CRef<CBlast_def_line_set> defline_set(x_GetHdr(oid, locked));
    
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

CRef<CBlast_def_line_set> CSeqDBVol::GetHdr(Uint4 oid, CSeqDBLockHold & locked) const
{
    return x_GetHdr(oid, locked);
}

CRef<CBlast_def_line_set> CSeqDBVol::x_GetHdr(Uint4 oid, CSeqDBLockHold & locked) const
{
    CRef<CBlast_def_line_set> nullret;
    
    Uint8 hdr_start = 0;
    Uint8 hdr_end   = 0;
    
    if (! m_Idx.GetHdrStartEnd(oid, hdr_start, hdr_end, locked)) {
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

bool CSeqDBVol::x_GetAmbChar(Uint4 oid, vector<Int4> ambchars, CSeqDBLockHold & locked) const
{
    Uint8 start_offset = 0;
    Uint8 end_offset   = 0;
    
    bool ok = m_Idx.GetAmbStartEnd(oid, start_offset, end_offset, locked);
    
    if (! ok) {
        return false;
    }
    
    Int4 length = Int4(end_offset - start_offset);
    
    if (0 == length)
        return true;
    
    Int4 total = length / 4;
    
    Int4 * buffer = (Int4*) m_Seq.GetRegion(start_offset, start_offset + (total * 4), false, locked);
    
    // This makes no sense...
    total &= 0x7FFFFFFF;
    
    ambchars.resize(total);
    
    for(int i = 0; i<total; i++) {
	ambchars[i] = SeqDB_GetStdOrd((const unsigned char *)(& buffer[i]));
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

END_NCBI_SCOPE

