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
 * Author:  J. Chen
 *
 * File Description:
 *   GetSequenceQualFromBioseq
 *
 * Remark:
 *   This code was originally generated by application DATATOOL
 *   using the following specifications:
 *   'macro.asn'.
 */

// standard includes
#include <ncbi_pch.hpp>

// generated includes
#include <objects/macro/Molinfo_field.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Seq_inst.hpp>

// generated classes

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

// destructor
CMolinfo_field::~CMolinfo_field(void)
{
}

string CMolinfo_field :: GetSequenceQualFromBioseq (CConstRef <CBioseq> bioseq) const
{
  if (bioseq.Empty()) return kEmptyStr;
  const CMolInfo* molinfo = 0;
  if (bioseq->CanGetDescr()) {
     ITERATE (list <CRef <CSeqdesc> >, it, bioseq->GetDescr().Get()) {
        if ((*it)->IsMolinfo()) {
           molinfo = &((*it)->GetMolinfo());
        }
     }
  }

  string rval(kEmptyStr);
  switch (Which()) {
    case e_Molecule:
      if (molinfo) {
        rval = CMolInfo::ENUM_METHOD_NAME(EBiomol)()
                                ->FindName(molinfo->GetBiomol(), true);
      }
      break;
    case e_Technique:
      if (molinfo) {
        rval = CMolInfo::ENUM_METHOD_NAME(ETech)()
                                ->FindName(molinfo->GetTech(), true);
      }
      break;
    case e_Completedness:
      if (molinfo) {
        rval = CMolInfo::ENUM_METHOD_NAME(ECompleteness)()
                                   ->FindName(molinfo->GetCompleteness(), true);
      }
      break;
    case e_Mol_class:
        rval = CSeq_inst::ENUM_METHOD_NAME(EMol)()
                              ->FindName(bioseq->GetInst().GetMol(), true);
        if (rval == "dna" || rval == "rna") {
              rval = NStr::ToUpper(rval);
        }
        else if (rval == "aa") {
              rval = "protein";
        }
        else if (rval == "na") {
              rval = "nucleotide";
        }
        break;
    case e_Topology:
        if (bioseq->GetInst().CanGetTopology()) {
            rval = CSeq_inst::ENUM_METHOD_NAME(ETopology)()
                          ->FindName(bioseq->GetInst().GetTopology(), true);
        }
        break;
    case e_Strand:
        if (bioseq->GetInst().CanGetStrand()) {
           rval = CSeq_inst::ENUM_METHOD_NAME(EStrand)()
                              ->FindName(bioseq->GetInst().GetStrand(), true);
           if (rval == "ss") {
              rval = "single";
           }
           else if (rval == "ds") {
              rval = "double";
           }
        }
        break;
    default: break;
  }
  if (rval == "unknown" || rval == "not-set") rval = kEmptyStr;
  return rval;
};

END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE

/* Original file checksum: lines: 57, chars: 1732, CRC32: db1a7fa6 */
