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
* Author:  Igor Filippov
*
* File Description:
*   Simple program to analyze possible shift
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <sstream>
#include <iomanip>

#include <cstdlib>
#include <iostream>

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifile.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objectio.hpp>
#include <serial/streamiter.hpp>
#include <util/compress/stream_util.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objects/variation/Variation.hpp>
#include <objects/variation/VariantPlacement.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/misc/sequence_macros.hpp>

#include <util/line_reader.hpp>
#include <objtools/data_loaders/genbank/gbloader.hpp>
#include <objtools/data_loaders/blastdb/bdbloader.hpp>
#include <connect/services/neticache_client.hpp>
#include <corelib/rwstream.hpp>

#include <objmgr/util/sequence.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/util/seq_loc_util.hpp>

#include <objtools/variation/variation_utils.hpp>

#include <misc/hgvs/hgvs_reader.hpp>
#include <misc/hgvs/variation_util2.hpp>
#include <objects/genomecoll/genome_collection__.hpp>
#include <objects/genomecoll/genomic_collections_cli.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(objects);
using namespace std;


class CAnalyzeShiftApp : public CNcbiApplication 
{
private:
    virtual void Init(void);
    virtual int Run ();
    virtual void Exit(void);
    bool ProcessHGVS(string &expression, CRef<CScope> scope, CHgvsReader &reader, int &pos_left, int &pos_right, bool &is_shifted, bool &is_different, int &dleft, int &dright);
};


void CAnalyzeShiftApp::Init(void)
{
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),"Analyze Shift in HGVS objects");
    arg_desc->AddDefaultKey("i", "input", "Input file",CArgDescriptions::eInputFile, "-", CArgDescriptions::fPreOpen);
    arg_desc->AddDefaultKey("o", "output", "Output file",CArgDescriptions::eOutputFile,"-",CArgDescriptions::fPreOpen);
    SetupArgDescriptions(arg_desc.release());
}

void CAnalyzeShiftApp::Exit(void)
{
}


bool CAnalyzeShiftApp::ProcessHGVS(string &expression, CRef<CScope> scope, CHgvsReader &reader, int &pos_left, int &pos_right, bool &is_shifted, bool &is_different, int &dleft, int &dright)
{
  vector<CRef<CSeq_annot> > annots;
  stringstream line;
  line << expression;

  try {
      reader.ReadSeqAnnots(annots, line);
  } catch(...) {return false;}
  if (annots.size() != 1) return false;
  CRef<CSeq_annot> a(new CSeq_annot);
  a->Assign(*annots.front());
// cout <<  MSerial_AsnText << *a;
  if (a->GetData().GetFtable().empty() || a->GetData().GetFtable().size() != 1) return false;
  if (!a->GetData().GetFtable().front()->IsSetLocation()) return false;
  int type = CVariation_inst::eType_identity;
  if (a->GetData().GetFtable().front()->GetData().GetVariation().GetData().IsSet())
  {
      for (CVariation_ref::TData::TSet::TVariations::const_iterator inst = a->GetData().GetFtable().front()->GetData().GetVariation().GetData().GetSet().GetVariations().begin(); 
           inst != a->GetData().GetFtable().front()->GetData().GetVariation().GetData().GetSet().GetVariations().end(); ++inst)
          if ((*inst)->IsSetData() && (*inst)->GetData().IsInstance() && (*inst)->IsSetData() && (*inst)->GetData().GetInstance().IsSetType() && (*inst)->GetData().GetInstance().GetType() != CVariation_inst::eType_identity)
              type = (*inst)->GetData().GetInstance().GetType();
  }
  else if (a->GetData().GetFtable().front()->GetData().GetVariation().GetData().IsInstance() && a->GetData().GetFtable().front()->GetData().GetVariation().GetData().GetInstance().IsSetType() 
           && a->GetData().GetFtable().front()->GetData().GetVariation().GetData().GetInstance().GetType() != CVariation_inst::eType_identity)
      type = a->GetData().GetFtable().front()->GetData().GetVariation().GetData().GetInstance().GetType();
  else
      return false;
  if (type != CVariation_inst::eType_ins && type != CVariation_inst::eType_del)
      return false;

  CSeq_loc &orig_loc = a->SetData().SetFtable().front()->SetLocation();

  int orig_left = pos_left;
  int orig_right = pos_right;
  if (orig_loc.IsInt())
  {
      orig_left = orig_loc.GetInt().GetFrom();
      orig_right = orig_loc.GetInt().GetTo();
  }
  else if (orig_loc.IsPnt())
  {
      orig_left = orig_loc.GetPnt().GetPoint();
      orig_right = orig_left;
  }
  if (orig_left != pos_left || orig_right != pos_right)
      is_different = true;

  if (type == CVariation_inst::eType_ins && orig_loc.IsInt())
  {
      int pos = orig_loc.GetInt().GetTo();
      CSeq_point pnt;
      pnt.SetPoint(pos);
      if (orig_loc.GetInt().IsSetStrand())
          pnt.SetStrand( orig_loc.GetInt().GetStrand() );
      pnt.SetId().Assign(orig_loc.GetInt().GetId());
      orig_loc.SetPnt().Assign(pnt);
      orig_left = pos;
      orig_right = pos;
  }

  CVariationNormalization::NormalizeVariation(*a,CVariationNormalization::eDbSnp,*scope);
  const CSeq_loc &loc = a->GetData().GetFtable().front()->GetLocation();
  string ref;
  if (a->GetData().GetFtable().front()->GetData().GetVariation().GetData().IsSet())
  {
      for (CVariation_ref::TData::TSet::TVariations::const_iterator inst = a->GetData().GetFtable().front()->GetData().GetVariation().GetData().GetSet().GetVariations().begin(); 
           inst != a->GetData().GetFtable().front()->GetData().GetVariation().GetData().GetSet().GetVariations().end(); ++inst)
          if ( (*inst)->IsSetData() && (*inst)->GetData().IsInstance() && (*inst)->GetData().GetInstance().IsSetDelta() && (*inst)->GetData().GetInstance().GetDelta().size() == 1 && 
               (*inst)->GetData().GetInstance().GetDelta().front()->IsSetSeq() && (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().IsLiteral() &&
               (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral().IsSetSeq_data() && (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().IsIupacna())
              ref = (*inst)->GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().GetIupacna().Get();
  }
  else if (a->GetData().GetFtable().front()->GetData().GetVariation().GetData().IsInstance() &&  a->GetData().GetFtable().front()->GetData().GetVariation().GetData().GetInstance().IsSetDelta()
           && a->GetData().GetFtable().front()->GetData().GetVariation().GetData().GetInstance().GetDelta().size() == 1  
           && a->GetData().GetFtable().front()->GetData().GetVariation().GetData().GetInstance().GetDelta().front()->IsSetSeq()
           && a->GetData().GetFtable().front()->GetData().GetVariation().GetData().GetInstance().GetDelta().front()->GetSeq().IsLiteral()
           && a->GetData().GetFtable().front()->GetData().GetVariation().GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral().IsSetSeq_data()
           && a->GetData().GetFtable().front()->GetData().GetVariation().GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().IsIupacna())
      ref = a->GetData().GetFtable().front()->GetData().GetVariation().GetData().GetInstance().GetDelta().front()->GetSeq().GetLiteral().GetSeq_data().GetIupacna().Get();
  else
      return false;

  if (ref.empty()) return false;

  if (loc.IsPnt())
  {
      pos_left = loc.GetPnt().GetPoint();
      pos_right = pos_left;
  }
  else  if (loc.IsInt())
  {
      pos_left = loc.GetInt().GetFrom();
      pos_right = loc.GetInt().GetTo();
  }
  else return false;

  if (pos_left != orig_left || pos_right != orig_right)
  {
      dleft = orig_left - pos_left;
      dright = pos_right - orig_right;
      is_shifted = true;
  }

  return true;
}


int CAnalyzeShiftApp::Run() 
{

    CArgs args = GetArgs();
    CNcbiIstream& istr = args["i"].AsInputFile();
    CNcbiOstream& ostr = args["o"].AsOutputFile();

    CRef<CObjectManager> object_manager = CObjectManager::GetInstance();
    CRef<CScope> scope(new CScope(*object_manager));
    CGBDataLoader::RegisterInObjectManager(*object_manager, NULL, CObjectManager::eDefault, 2);
    scope->AddDefaults();
    
    variation::CVariationUtil util(*scope);
    CGenomicCollectionsService gcservice;
    CConstRef<CGC_Assembly> assembly( gcservice.GetAssembly("GCF_000001405.22", CGCClient_GetAssemblyRequest::eLevel_scaffold)  );
    CHgvsReader reader(*assembly);


    CStreamLineReader line_reader(istr);
    while(!line_reader.AtEOF())
    {
        string line = *(++line_reader);
        if (!line.empty())
        {
            vector<string> tokens;
            NStr::Tokenize(line,"|",tokens);
            string snp_id = tokens[0];
            string hgvs = tokens[1];
            string gi = tokens[2];
            string start = tokens[3];
            string stop	 = tokens[4];
            string isClinical = tokens[5];
            int pos_left = NStr::StringToInt(start);
            int pos_right = NStr::StringToInt(stop);
            bool is_shifted = false;
            bool is_different = false;
            int dleft = 0;
            int dright = 0;
            if (ProcessHGVS(hgvs,scope,reader,pos_left,pos_right,is_shifted,is_different,dleft,dright))
            {
                ostr <<snp_id<<"|"<<hgvs<<"|"<<isClinical<<"|"<<is_different<<"|"<<is_shifted<<"|";
                if (is_shifted)
                    ostr  << dleft<<"|"<<dright<<"|"<<pos_left<<"|"<<pos_right<<"|";
                ostr<<endl;
            }
            else
                ostr << line <<"ERROR_HGVS_PROCESSING"<<endl;
        }
    }

    return 0;
}

int main(int argc, const char* argv[])
{
    CAnalyzeShiftApp AnalyzeShiftApp;
    return AnalyzeShiftApp.AppMain(argc, argv);
}


