/* $Id$
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
* Author: Azat Badretdin
*
* File Description:
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include "read_blast_result.hpp"

int CReadBlastApp::short_proteins()
{
  int nshort=0;

  // m_verbosity_threshold = 300;
  if(PrintDetails()) NcbiCerr << "short_proteins starts: " << NcbiEndl;
  TSimpleSeqs& seqs=m_simple_seqs;  // now calculated in CopyGenestoforgotthename
  NON_CONST_ITERATE(TSimpleSeqs, seq, seqs)
    {
    int len=0;
    if(seq->type != "CDS") continue;
    // TSimpleSeq* debug_seq=&*seq;
    ITERATE(TSimplePairs, e2, seq->exons)
      {
      len+= e2->to - e2->from + 1;
      }
    len/=3;
    bool is_short = len<m_shortProteinThreshold;

    if(is_short)
      {
      bool edge = false;
      bool edge_from = false;
      bool edge_to   = false;
      bool fuzzy_edge = false;
// get the nucle seq
      const CBioseq& seq_nu = get_nucleotide_seq(*(seq->seq));
      int len_nu = seq_nu.GetInst().GetLength();
// check if it is at the edge,  and that edge is fuzzy
      ITERATE(TSimplePairs, e2, seq->exons)
        {
        if(e2->from < 3) { edge=true; edge_from=true; if (e2->fuzzy_from) fuzzy_edge = true; }
        if(e2->to > len_nu - 4 ) { edge=true; edge_to=true; if (e2->fuzzy_to) fuzzy_edge = true; }
        }
      if(edge_from && edge_to && seq->exons.size()>1 ) { edge=false; } // annotation crossing the origin of a  circular molecule, not really an edge
      strstream bufferstr; 
      strstream misc;
      misc << "Annotation is too short " << seq->name << '\0';
      int from, to;
      from = seq->exons[0].from;
      to = seq->exons[seq->exons.size()-1].to; 

      
      ENa_strand strand = seq->exons[0].strand;
      string range = strand == eNa_strand_plus ? printed_range(from, to) : printed_range(to, from);
      string diag_name = seq->name;
      bufferstr << "Short protein " << diag_name << " " << range << '\0';
      problemStr problem = {eShortProtein, bufferstr.str(), misc.str(), "", "", from, to, strand};
      if(!edge) 
        {
        m_diag[diag_name].problems.push_back(problem);
        if(PrintDetails()) NcbiCerr << "short_proteins: adding problem:" << "\t"
               << diag_name << "\t"
               << "eShortProtein" << "\t"
               << bufferstr.str() << "\t"
               << NcbiEndl;
        CBioseq_set::TSeq_set* seqs = get_parent_seqset(*(seq->seq));
        if(seqs!=NULL) append_misc_feature(*seqs, diag_name, eShortProtein);
        }
      else if (!fuzzy_edge)
        {
        NcbiCerr << "WARNING: short annotation on the edge does not have corresponding fuzzy ends " << diag_name << " " << range << NcbiEndl;
        }
      else
        {
        NcbiCerr << "INFO: short annotation on the edge with corresponding fuzzy end " << diag_name << " " << range << NcbiEndl;
        }
      nshort++;
      }
    }
 return nshort;  
}
