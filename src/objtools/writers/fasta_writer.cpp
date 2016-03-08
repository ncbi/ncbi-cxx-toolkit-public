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
 * Authors:  Sergiy Gotvyanskyy
 *
 * File Description:  Write object as a hierarchy of FASTA objects
 *
 */

#include <ncbi_pch.hpp>
#include <objtools/writers/fasta_writer.hpp>

#include <corelib/ncbifile.hpp>

#include <objmgr/util/sequence.hpp>
//#include <objmgr/object_manager.hpp>
//#include <objmgr/scope.hpp>

BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)

CFastaOstreamEx::CFastaOstreamEx(const string& dir, const string& filename_without_ext)
: m_filename_without_ext(filename_without_ext),
  m_Flags(-1)
{        
    m_dir = CDir::AddTrailingPathSeparator(dir);
}

CFastaOstreamEx::~CFastaOstreamEx()
{
    NON_CONST_ITERATE(vector<TStreams>, it, m_streams)
    {
        delete it->m_fasta_stream; it->m_fasta_stream = 0;
        delete it->m_ostream; it->m_ostream = 0;
    }
}

void CFastaOstreamEx::x_GetNewFilename(string& filename, E_FileSection sel)
{
    filename = m_dir;
    filename += m_filename_without_ext;
    const char* suffix = 0;
    switch (sel)
    {
    case eFS_nucleotide:
        suffix = "";
        break;
    case eFS_CDS:
        suffix = "_cds_from_genomic";
        break;
    case eFS_RNA:
        suffix = "_rna_from_genomic";
        break;
    default:
        _ASSERT(0);
    }
    filename.append(suffix);
    const char* ext = 0;
    switch (sel)
    {
    case eFS_nucleotide:
        ext = ".fsa";
        break;
    case eFS_CDS:
    case eFS_RNA:
        ext = ".fna";
        break;          
    default:
        _ASSERT(0);
    }
    filename.append(ext);
}

CNcbiOstream* CFastaOstreamEx::x_GetOutputStream(const string& filename, E_FileSection sel)
{
    return new CNcbiOfstream(filename.c_str());
}

CFastaOstream* CFastaOstreamEx::x_GetFastaOstream(CNcbiOstream& ostr, E_FileSection sel)
{
    CFastaOstream* fstr = new CFastaOstream(ostr);
    if (m_Flags != -1)
       fstr->SetAllFlags(m_Flags);
    return fstr;
}

CFastaOstreamEx::TStreams& CFastaOstreamEx::x_GetStream(E_FileSection sel)
{
    if (m_streams.size() <= sel)
    {
        m_streams.resize(sel + 1);
    }
    TStreams& res = m_streams[sel];
    if (res.m_filename.empty())
    { 
        x_GetNewFilename(res.m_filename, sel);
    }
    if (res.m_ostream == 0)
    {
        res.m_ostream = x_GetOutputStream(res.m_filename, sel);
    }
    if (res.m_fasta_stream == 0)
    {
        res.m_fasta_stream = x_GetFastaOstream(*res.m_ostream, sel);
    }
    return res;
}

void CFastaOstreamEx::Write(const CSeq_entry_Handle& handle, const CSeq_loc* location)
{
    for (CBioseq_CI it(handle); it; ++it) {
        if (location) {
            CSeq_loc loc2;
            loc2.SetWhole().Assign(*it->GetSeqId());
            int d = sequence::TestForOverlap
                (*location, loc2, sequence::eOverlap_Interval,
                kInvalidSeqPos, &handle.GetScope());
            if (d < 0) {
                continue;
            }
        }
        x_Write(*it, location);
    }
}

void CFastaOstreamEx::x_Write(const CBioseq_Handle& handle, const CSeq_loc* location)
{
    E_FileSection sel = eFS_nucleotide;
    if (handle.CanGetInst_Mol())
    {
        CSeq_inst::EMol mol = handle.GetInst_Mol();
        switch (mol)
        {
        case ncbi::objects::CSeq_inst_Base::eMol_dna:
            sel = eFS_RNA;
            break;
        case ncbi::objects::CSeq_inst_Base::eMol_rna:
            sel = eFS_RNA;
            break;
        case ncbi::objects::CSeq_inst_Base::eMol_aa:
            sel = eFS_CDS;
            break;
        case ncbi::objects::CSeq_inst_Base::eMol_na:
            break;
        default:
            break;
        }
    }
    TStreams& res = x_GetStream(sel);
    res.m_fasta_stream->Write(handle, location);
}

END_SCOPE(objects)
END_NCBI_SCOPE
