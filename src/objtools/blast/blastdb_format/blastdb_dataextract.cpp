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
 * Author: Christiam Camacho
 *
 */

/** @file blastdb_dataextract.cpp
 *  Defines classes which extract data from a BLAST database
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <objtools/blast/blastdb_format/invalid_data_exception.hpp>
#include <objtools/blast/blastdb_format/blastdb_dataextract.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <corelib/ncbiutil.hpp>
#include <util/sequtil/sequtil_manip.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

// Standard CRC32 table.
static Uint4 CRCTable[256]={
           0UL, 1996959894UL, 3993919788UL, 2567524794UL,  124634137UL, 1886057615UL, 3915621685UL, 2657392035UL, 
   249268274UL, 2044508324UL, 3772115230UL, 2547177864UL,  162941995UL, 2125561021UL, 3887607047UL, 2428444049UL, 
   498536548UL, 1789927666UL, 4089016648UL, 2227061214UL,  450548861UL, 1843258603UL, 4107580753UL, 2211677639UL, 
   325883990UL, 1684777152UL, 4251122042UL, 2321926636UL,  335633487UL, 1661365465UL, 4195302755UL, 2366115317UL, 
   997073096UL, 1281953886UL, 3579855332UL, 2724688242UL, 1006888145UL, 1258607687UL, 3524101629UL, 2768942443UL, 
   901097722UL, 1119000684UL, 3686517206UL, 2898065728UL,  853044451UL, 1172266101UL, 3705015759UL, 2882616665UL, 
   651767980UL, 1373503546UL, 3369554304UL, 3218104598UL,  565507253UL, 1454621731UL, 3485111705UL, 3099436303UL, 
   671266974UL, 1594198024UL, 3322730930UL, 2970347812UL,  795835527UL, 1483230225UL, 3244367275UL, 3060149565UL, 
  1994146192UL,   31158534UL, 2563907772UL, 4023717930UL, 1907459465UL,  112637215UL, 2680153253UL, 3904427059UL, 
  2013776290UL,  251722036UL, 2517215374UL, 3775830040UL, 2137656763UL,  141376813UL, 2439277719UL, 3865271297UL,
  1802195444UL,  476864866UL, 2238001368UL, 4066508878UL, 1812370925UL,  453092731UL, 2181625025UL, 4111451223UL,
  1706088902UL,  314042704UL, 2344532202UL, 4240017532UL, 1658658271UL,  366619977UL, 2362670323UL, 4224994405UL,
  1303535960UL,  984961486UL, 2747007092UL, 3569037538UL, 1256170817UL, 1037604311UL, 2765210733UL, 3554079995UL,
  1131014506UL,  879679996UL, 2909243462UL, 3663771856UL, 1141124467UL,  855842277UL, 2852801631UL, 3708648649UL,
  1342533948UL,  654459306UL, 3188396048UL, 3373015174UL, 1466479909UL,  544179635UL, 3110523913UL, 3462522015UL,
  1591671054UL,  702138776UL, 2966460450UL, 3352799412UL, 1504918807UL,  783551873UL, 3082640443UL, 3233442989UL, 
  3988292384UL, 2596254646UL,   62317068UL, 1957810842UL, 3939845945UL, 2647816111UL,   81470997UL, 1943803523UL, 
  3814918930UL, 2489596804UL,  225274430UL, 2053790376UL, 3826175755UL, 2466906013UL,  167816743UL, 2097651377UL,
  4027552580UL, 2265490386UL,  503444072UL, 1762050814UL, 4150417245UL, 2154129355UL,  426522225UL, 1852507879UL,
  4275313526UL, 2312317920UL,  282753626UL, 1742555852UL, 4189708143UL, 2394877945UL,  397917763UL, 1622183637UL,
  3604390888UL, 2714866558UL,  953729732UL, 1340076626UL, 3518719985UL, 2797360999UL, 1068828381UL, 1219638859UL,
  3624741850UL, 2936675148UL,  906185462UL, 1090812512UL, 3747672003UL, 2825379669UL,  829329135UL, 1181335161UL,
  3412177804UL, 3160834842UL,  628085408UL, 1382605366UL, 3423369109UL, 3138078467UL,  570562233UL, 1426400815UL,
  3317316542UL, 2998733608UL,  733239954UL, 1555261956UL, 3268935591UL, 3050360625UL,  752459403UL, 1541320221UL, 
  2607071920UL, 3965973030UL, 1969922972UL,   40735498UL, 2617837225UL, 3943577151UL, 1913087877UL,   83908371UL, 
  2512341634UL, 3803740692UL, 2075208622UL,  213261112UL, 2463272603UL, 3855990285UL, 2094854071UL,  198958881UL,
  2262029012UL, 4057260610UL, 1759359992UL,  534414190UL, 2176718541UL, 4139329115UL, 1873836001UL,  414664567UL,
  2282248934UL, 4279200368UL, 1711684554UL,  285281116UL, 2405801727UL, 4167216745UL, 1634467795UL,  376229701UL,
  2685067896UL, 3608007406UL, 1308918612UL,  956543938UL, 2808555105UL, 3495958263UL, 1231636301UL, 1047427035UL,
  2932959818UL, 3654703836UL, 1088359270UL,  936918000UL, 2847714899UL, 3736837829UL, 1202900863UL,  817233897UL,
  3183342108UL, 3401237130UL, 1404277552UL,  615818150UL, 3134207493UL, 3453421203UL, 1423857449UL,  601450431UL,
  3009837614UL, 3294710456UL, 1567103746UL,  711928724UL, 3020668471UL, 3272380065UL, 1510334235UL,  755167117UL };

// Initialises CRC32 table for calculating of checksums.
void CRC32_init(void) {
    Uint4 crc, CRC32_POLYNOMIAL = 0xEDB88320L;
    int i,j;

    /* CRC32_init is called only if initialized == 0, which apparently never happens */
    static int initialized = -1;
    if( initialized != 0) 
        return;
  
    for( i = 0; i <= 255 ; i++ )
    { 
        crc = i; 
        for( j = 8 ; j > 0; j-- )
        { 
            if( crc & 1 ) 
                crc = ( crc >> 1 ) ^ CRC32_POLYNOMIAL; 
            else 
                crc >>= 1; 
        }
        if( !initialized)
            CRCTable[i] = crc;
        else
        {
            assert( CRCTable[i] == crc );
        }
    } 
    initialized = 1;
}

// NOTE: All letters are converted to upper case automatically.
void CRC32( Uint4 *crc, Uchar byte)                              
{                                                           
      Uint4 temp1;                                              
      Uint4 temp2;                                              
                                                                
      temp1 = ( *crc >> 8 ) & 0x00FFFFFFL;
      temp2 = CRCTable[ ( (int) *crc ^ toupper(byte) ) & 0xff ];
      *crc = temp1 ^ temp2;
}                                                           

// Calculates hash for a buffer in IUPACna (NCBIeaa for proteins) format.
// NOTE: if sequence is in a different format, the function below can be modified to convert
// each byte into IUPACna encoding on the fly.  
int GetHash(const char* buffer, int length)
{
    Uint4 hash  = 0xFFFFFFFFL;
    int ii;

    CRC32_init();

    for(ii = 0; ii < length; ii++) {
        if (buffer[ii] != '\n')
            CRC32( &hash, buffer[ii]);
    }
    return hash;
}

string CGiExtractor::Extract(CBlastDBSeqId& id, CSeqDB& blastdb)
{
    int gi = CBlastDBSeqId::kInvalid;
    if (id.IsGi()) {
        gi = id.GetGi();
    } else {
        blastdb.OidToGi(COidExtractor().ExtractOID(id, blastdb), gi);
    }
    return NStr::IntToString(gi);
}

string CPigExtractor::Extract(CBlastDBSeqId& id, CSeqDB& blastdb)
{
    int pig = CBlastDBSeqId::kInvalid;
    if (id.IsPig()) {
        pig = id.GetPig();
    } else {
        blastdb.OidToPig(COidExtractor().ExtractOID(id, blastdb), pig);
    }
    return NStr::IntToString(pig);
}

string COidExtractor::Extract(CBlastDBSeqId& id, CSeqDB& blastdb)
{
    return NStr::IntToString(ExtractOID(id, blastdb));
}

int COidExtractor::ExtractOID(CBlastDBSeqId& id, CSeqDB& blastdb)
{
    int retval = CBlastDBSeqId::kInvalid;
    if (id.IsOID()) {
        retval = id.GetOID();
    } else if (id.IsGi()) {
        blastdb.GiToOid(id.GetGi(), retval);
        // Verify that the GI of interest is among the Seq-ids this oid
        // retrieves 
        list< CRef<CSeq_id> > filtered_ids = blastdb.GetSeqIDs(retval);
        bool found = false;
        ITERATE(list< CRef<CSeq_id> >, seqid, filtered_ids) {
            if ((*seqid)->IsGi() && ((*seqid)->GetGi() == id.GetGi())) {
                found = true;
                break;
            }
        }
        if ( !found ) {
            retval = CBlastDBSeqId::kInvalid;
        }
    } else if (id.IsPig()) {
        blastdb.PigToOid(id.GetPig(), retval);
    } else if (id.IsStringId()) {
        vector<int> oids;
        blastdb.AccessionToOids(id.GetStringId(), oids);
        if ( !oids.empty() ) {
            retval = oids.front();
        }
    }
    if (retval == CBlastDBSeqId::kInvalid) {
        NCBI_THROW(CSeqDBException, eArgErr, 
                   "Entry not found in BLAST database");
    }
    id.SetOID(retval);

    return retval;
}

int CHashExtractor::ExtractHash(CBlastDBSeqId& id, CSeqDB& blastdb) {
    const int kOid = COidExtractor().ExtractOID(id, blastdb);
    string seq;
    blastdb.GetSequenceAsString(kOid, seq);
    return GetHash(seq.c_str(), seq.size());
}

string CHashExtractor::Extract(CBlastDBSeqId& id, CSeqDB& blastdb) 
{
    return NStr::IntToString(ExtractHash(id, blastdb));
}

TSeqPos
CSeqLenExtractor::ExtractLength(CBlastDBSeqId& id, CSeqDB& blastdb)
{
    return blastdb.GetSeqLength(COidExtractor().ExtractOID(id, blastdb));
}

string CSeqLenExtractor::Extract(CBlastDBSeqId& id, CSeqDB& blastdb)
{
    return NStr::IntToString(ExtractLength(id, blastdb));
}

string CTaxIdExtractor::Extract(CBlastDBSeqId& id, CSeqDB& blastdb)
{
    return NStr::IntToString(ExtractTaxID(id, blastdb));
}

int CTaxIdExtractor::ExtractTaxID(CBlastDBSeqId& id, CSeqDB& blastdb)
{
    const int kOid = COidExtractor().ExtractOID(id, blastdb);
    int retval = CBlastDBSeqId::kInvalid;
    map<int, int> gi2taxid;

    blastdb.GetTaxIDs(kOid, gi2taxid);
    if (id.IsGi()) {
        retval = gi2taxid[id.GetGi()];
    } else {
        retval = gi2taxid.begin()->second;
    }
    return retval;
}

string CSeqDataExtractor::Extract(CBlastDBSeqId& id, CSeqDB& blastdb)
{
    string retval;
    const int kOid = COidExtractor().ExtractOID(id, blastdb);
    try {
        blastdb.GetSequenceAsString(kOid, retval, m_SeqRange);
        CSeqDB::TSequenceRanges masked_ranges;
        CMaskingDataExtractor mask_extractor(m_FiltAlgoIds);
        mask_extractor.GetMaskedRegions(id, blastdb, masked_ranges);
        ITERATE(CSeqDB::TSequenceRanges, mask, masked_ranges) {
            transform(&retval[mask->first], &retval[mask->second],
                      &retval[mask->first], (int (*)(int))tolower);
        }
        if (m_Strand == eNa_strand_minus) {
            CSeqManip::ReverseComplement(retval, CSeqUtil::e_Iupacna, 
                                         0, retval.size());
        }
    } catch (CSeqDBException& e) {
        //FIXME: change the enumeration when it's availble
        if (e.GetErrCode() == CSeqDBException::eArgErr ||
            e.GetErrCode() == CSeqDBException::eFileErr/*eOutOfRange*/) {
            NCBI_THROW(CInvalidDataException, eInvalidRange, e.GetMsg());
        }
        throw;
    }
    return retval;
}

/// Auxiliary function to retrieve a Bioseq from the BLAST database
/// @param id BLAST DB sequence identifier [in]
/// @param blastdb BLAST database to fetch data from [in]
/// @param get_sequence should the sequence data be retrieved?
static CRef<CBioseq>
s_GetBioseq(CBlastDBSeqId& id, CSeqDB& blastdb, bool get_sequence = false,
            bool get_target_gi_only = false)
{
    const int kOid = COidExtractor().ExtractOID(id, blastdb);
    if (get_target_gi_only) _ASSERT(id.IsGi());
    const int kTargetGi = get_target_gi_only ? id.GetGi() : 0;

    CRef<CBioseq> retval;
    try {
        retval = get_sequence 
            ? blastdb.GetBioseq(kOid, kTargetGi) 
            : blastdb.GetBioseqNoData(kOid, kTargetGi);
    } catch (const CSeqDBException& e) {
        // this happens when CSeqDB detects a GI that doesn't belong to a
        // filtered database (e.g.: swissprot as a subset of nr)
        if (e.GetMsg().find("oid headers do not contain target gi")) {
            NCBI_THROW(CSeqDBException, eArgErr, 
                       "Entry not found in BLAST database");
        }
    }
    return retval;
}

string CTitleExtractor::Extract(CBlastDBSeqId& id, CSeqDB& blastdb)
{
    string retval;
    CRef<CBioseq> bioseq = s_GetBioseq(id, blastdb, false, m_ShowTargetOnly);
    if (bioseq->CanGetDescr()) {
        ITERATE(CSeq_descr::Tdata, seq_desc, bioseq->GetDescr().Get()) {
            if ((*seq_desc)->IsTitle()) {
                retval.assign((*seq_desc)->GetTitle());
                break;
            }
        }
    }

    if (retval.empty()) { 
        // Get the Bioseq's ID
        retval = CSeq_id::GetStringDescr(*bioseq, CSeq_id::eFormat_FastA);
    }

    return retval;
}

string CAccessionExtractor::Extract(CBlastDBSeqId& id, CSeqDB& blastdb)
{
    CRef<CBioseq> bioseq = s_GetBioseq(id, blastdb, false, m_ShowTargetOnly);
    _ASSERT(bioseq->CanGetId());
    CRef<CSeq_id> theId = FindBestChoice(bioseq->GetId(), CSeq_id::WorstRank);

    string retval;
    theId->GetLabel(&retval, CSeq_id::eContent);
    return retval;
}

string 
CCommonTaxonomicNameExtractor::Extract(CBlastDBSeqId& id, CSeqDB& blastdb)
{
    const int kTaxID = CTaxIdExtractor().ExtractTaxID(id, blastdb);
    SSeqDBTaxInfo tax_info;
    blastdb.GetTaxInfo(kTaxID, tax_info);
    _ASSERT(kTaxID == tax_info.taxid);
    return tax_info.common_name;
}

string 
CScientificNameExtractor::Extract(CBlastDBSeqId& id, CSeqDB& blastdb)
{
    const int kTaxID = CTaxIdExtractor().ExtractTaxID(id, blastdb);
    SSeqDBTaxInfo tax_info;
    blastdb.GetTaxInfo(kTaxID, tax_info);
    _ASSERT(kTaxID == tax_info.taxid);
    return tax_info.scientific_name;
}

CConstRef<CSeq_loc>
CMaskingDataExtractor::GetMaskedRegions(CBlastDBSeqId& id, CSeqDB& blastdb,
                                        CRef<CSeq_id> seqid)
{
    CSeqDB::TSequenceRanges masked_ranges;
    GetMaskedRegions(id, blastdb, masked_ranges);
    CRef<CSeq_loc> retval;
    if (masked_ranges.empty()) {
        return retval;
    }
    retval.Reset(new CSeq_loc);
    ITERATE(CSeqDB::TSequenceRanges, itr, masked_ranges) {
        // CSeq_loc assumes closed range while TSeqRange assumes half open
        // therefore, translation is necessary here.
        CRef<CSeq_loc> mask(new CSeq_loc(*seqid, itr->first, itr->second -1));
        retval->SetMix().Set().push_back(mask);
    }
    return CConstRef<CSeq_loc>(retval.GetPointer());
}

static int lessthan(const void *x1, const void *x2) {
    const int &r1 = *(int *)x1;
    const int &r2 = *(int *)x2;
    return (r2 < r1);
}

void 
CMaskingDataExtractor::GetMaskedRegions(CBlastDBSeqId& id, CSeqDB& blastdb,
                                        CSeqDB::TSequenceRanges& ranges)
{
    ranges.clear();
    if (m_AlgoIds.empty()) {
        return;
    }
    const int kOid = COidExtractor().ExtractOID(id, blastdb);

    // Only support the 1st algo for now.
    blastdb.GetMaskData(kOid, m_AlgoIds[0], ranges);
}


string
CMaskingDataExtractor::Extract(CBlastDBSeqId& id, CSeqDB& blastdb)
{
    static const string kNoMasksFound("none");
#if ((defined(NCBI_COMPILER_WORKSHOP) && (NCBI_COMPILER_VERSION <= 550))  ||  \
     defined(NCBI_COMPILER_MIPSPRO))
    return kNoMasksFound;
#else
    CSeqDB::TSequenceRanges masked_ranges;
    GetMaskedRegions(id, blastdb, masked_ranges);
    if (masked_ranges.empty()) {
        return kNoMasksFound;
    }
    CNcbiOstrstream out;
    ITERATE(CSeqDB::TSequenceRanges, range, masked_ranges) {
        out << range->first << "-" << range->second << ";";
    }
    return CNcbiOstrstreamToString(out);
#endif
}

CFastaExtractor::CFastaExtractor(TSeqPos line_width, 
             TSeqRange range /* = TSeqRange() */,
             objects::ENa_strand strand /* = objects::eNa_strand_other */,
             bool target_only /* = false */,
             bool ctrl_a /* = false */,
             const vector<int>& filt_algo_ids /* = vector<int>() */)
    : CSeqDataExtractor(range, strand, filt_algo_ids), 
    m_FastaOstream(m_OutputStream), m_ShowTargetOnly(target_only),
    m_UseCtrlA(ctrl_a)
{
    m_FastaOstream.SetWidth(line_width);
    m_FastaOstream.SetAllFlags(CFastaOstream::fKeepGTSigns);
}

/// Hacky function to replace ' >' for Ctrl-A's in the Bioseq's title
static void s_ReplaceCtrlAsInTitle(CRef<CBioseq> bioseq)
{
    static const string kTarget(" >gi|");
    static const string kCtrlA = string(1, '\001') + string("gi|");
    NON_CONST_ITERATE(CSeq_descr::Tdata, desc, bioseq->SetDescr().Set()) {
        if ((*desc)->Which() == CSeqdesc::e_Title) {
            NStr::ReplaceInPlace((*desc)->SetTitle(), kTarget, kCtrlA);
            break;
        }
    }
}

string CFastaExtractor::Extract(CBlastDBSeqId& id, CSeqDB& blastdb)
{
    CRef<CBioseq> bioseq = s_GetBioseq(id, blastdb, true, m_ShowTargetOnly);
    if (m_UseCtrlA) {
        s_ReplaceCtrlAsInTitle(bioseq);
    }
    CRef<CSeq_id> seqid = FindBestChoice(bioseq->GetId(), CSeq_id::BestRank);

    // Handle the case when a sequence range is provided
    CRef<CSeq_loc> range;
    if (m_SeqRange.NotEmpty() || m_Strand != eNa_strand_other) {
        if (m_SeqRange.NotEmpty()) {
            range.Reset(new CSeq_loc(*seqid, m_SeqRange.GetFrom(),
                                     m_SeqRange.GetTo(), m_Strand));
            m_FastaOstream.ResetFlag(CFastaOstream::fSuppressRange);
        } else {
            TSeqPos length = CSeqLenExtractor().ExtractLength(id, blastdb);
            range.Reset(new CSeq_loc(*seqid, 0, length-1, m_Strand));
            m_FastaOstream.SetFlag(CFastaOstream::fSuppressRange);
        }
    }

    // Handle any requests for masked FASTA
    static const CFastaOstream::EMaskType kMaskType = CFastaOstream::eSoftMask;
    CMaskingDataExtractor mask_extractor(m_FiltAlgoIds);
    m_FastaOstream.SetMask(kMaskType, 
                           mask_extractor.GetMaskedRegions(id, blastdb, seqid));

    m_OutputStream.str(""); // clean the buffer
    try { m_FastaOstream.Write(*bioseq, range); }
    catch (const CObjmgrUtilException& e) {
        if (e.GetErrCode() == CObjmgrUtilException::eBadLocation) {
            NCBI_THROW(CInvalidDataException, eInvalidRange, 
                       "Invalid sequence range");
        }
    }
    return m_OutputStream.str();
}

END_NCBI_SCOPE
