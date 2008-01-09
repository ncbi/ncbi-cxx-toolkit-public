#include <ncbi_pch.hpp>
#include <string.h>

#include <corelib/ncbienv.hpp>


#include <string>
#include <istream>

USING_SCOPE(ncbi);

#include "tbl.h"

bool Ctbl::Read(istream& in)
{
  bool result=false;
  while(!in.eof())
    {
    CRef < Ctbl_feat > feat ( new Ctbl_feat );
    if(!feat->Read(in))
      break; 
    m_feat.push_back(feat);
    result=true;
    }

  return result;
}

bool Ctbl_feat::Read(istream& in)
{
  char buf[1024];
  in.getline(buf, 1024); 
  if(in.eof()) return false;
  if(strncmp(buf, ">Feature ", sizeof(">Feature ")) )
    {
    NcbiCerr << "FATAL: Ctbl_feat::Read(): wrong format I" << NcbiEndl;
    throw;
    }
  istrstream bufStr(buf);
  string dummy, seqid, table_name; /////////////////////
  bufStr >>  dummy >> seqid >> table_name;
  m_table_name = table_name;
  m_seqid      = seqid;
  while(!in.eof())
    {
    CRef < Ctbl_seg > seg ( new Ctbl_seg );
    if(!seg->Read(in))
      break; 
    else
      m_seg.push_back(seg);
    }

  return true; 
  
}

bool Ctbl_seg::Read(istream& in)
{
  char buf[1024];
  vector < string > tabs;
  tabs.clear();

//
  in.getline(buf, 1024); 
  if(in.eof()) return false;

  char *c=buf;
  char *tok;
  while(tok=strtok(c,"\t"))
    {
    tabs.push_back(tok);
    c=NULL;
    }
// check if correct line
  if(tabs.size()<3 || !tabs[0].size() || !tabs[1].size() ||  !tabs[2].size() ) return false;
  if(tabs[0][0]=='<')
    {
    m_fuzzy_from=true;
    tabs[0].erase(0,1);
    }
  m_from = atoi(tabs[0].c_str());
  if(tabs[1][0]=='>')
    {
    m_fuzzy_to=true;
    tabs[1].erase(0,1);
    }
  m_to = atoi(tabs[0].c_str());
  m_key = tabs[2];
  while(!in.eof())
    {
    int pos = in.tellg();
    in.getline(buf, 1024); 
    if(in.eof()) break;
// end of segment, rewind back
    tabs.clear();
    c=buf;
    while(tok=strtok(c,"\t"))
      {
      tabs.push_back(tok);
      c=NULL;
      }
    if(!tabs[0].size()) 
      {
      in.seekg(pos);
      return true;
      }
    m_qual[tabs[3]]=tabs[4];
    }
  return true;
}
