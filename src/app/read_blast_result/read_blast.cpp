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
* Author: Azat Badretdin
*
* File Description:
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include "read_blast_result.hpp"


int CReadBlastApp::ReadBlast(const char *file, map<string, blastStr>& blastMap)
/* original: Dima Dernovoy */
{
#define         MAXSTR    322

    char *s, *rest;
    static char str[MAXSTR+1];
    if(PrintDetails()) NcbiCerr<<"ReadBlast()" << NcbiEndl;

    FILE *fpt = fopen( file, "r" );

    int defline = -1;
    long qLen = 0;
    string qName; 

    CTypeIterator<CBioseq> seq; 
    // use only first id of the set, since it is submission that have only new sequences.
    CBioseq::TId::iterator id;

    int ihit=0;
    seq  = Begin(); 
    skip_toprot(seq);
    if(!seq) return 1;
    id  = seq->SetId().begin();
    string prefix = CSeq_id::GetStringDescr (*seq, CSeq_id::eFormat_FastA);
//    string prefix = GetStringDescr (*seq);
    if(PrintDetails()) NcbiCerr<< "CReadBlastApp::ReadBlast: "
        << "CSeq_id::GetStringDescr for prefix = "
        <<  CSeq_id::GetStringDescr (*seq, CSeq_id::eFormat_FastA)
        << ", "
        << "GetStringDescr for prefix = "
        <<  GetStringDescr (*seq)
        << NcbiEndl;


// this was before Dima's decision to drop contig ids as part of locus tags or protein ids
/*
    string::size_type ipos = prefix.find('_');
    if(ipos != string::npos) prefix.erase(ipos+1);
*/
    string::size_type ipos = prefix.rfind('|');
    if(ipos != string::npos) prefix.erase(ipos+1);
    if(PrintDetails()) NcbiCerr<< "CReadBlastApp::ReadBlast: " 
        << "prefix = "
        << prefix
        << NcbiEndl;

    IncreaseVerbosity();
    while(fgets(str, MAXSTR, fpt))
    {
        if((s=strstr(str, "Query= "))!=NULL)
// new sequence, new blast "file"
        {
            ihit=0;
           
            s = next_w(s);
// printing the name of the sequence 
            qName.erase(); //////////////////////
            qLen = 0;      
            while(!isspace(*s)&& *s) 
              {
              qName+= *s; s++;
              }
//            ipos = qName.rfind('|');
//            if(ipos!=string::npos) qName.erase(0,ipos+1);
            if(m_usemap)
              {
              if( m_tagmap.find(qName) != m_tagmap.end()) 
                 {
                 if(PrintDetails()) NcbiCerr<< "CReadBlastApp::ReadBlast: " 
                                       << "before applying tagmap = "
                                       << qName 
                                       << NcbiEndl;
                 qName = prefix+m_tagmap[qName];
                 if(PrintDetails()) NcbiCerr<< "CReadBlastApp::ReadBlast: " 
                                       << "after applying tagmap = "
                                       << qName 
                                       << NcbiEndl;
                 }
              }

            defline = 0;

            do // long protein names are wrapped by BLAST, need to skip the elongation
              {
              fgets(str, MAXSTR, fpt);
              s = strstr(str, "letters)");
              } while (!s);
            s = strstr(str, "(");
            qLen = strtol(s+1, &rest, 10); ////////////////////
            blastMap[qName].qLen  = qLen;
            blastMap[qName].qName = qName;
            if(PrintDetails()) NcbiCerr<< "CReadBlastApp::ReadBlast: " 
                                       << "final qname = "
                                       << qName << ":" << qLen << NcbiEndl;

            continue;
        } // end reading query part, header of the file

// hits
        IncreaseVerbosity();
        if((defline < m_n_best_hit ) && (str[0] == '>'))
        {
            str[0] = ' ';
            blastMap[qName].hits.resize(ihit+1);
            long gi =0; 
	    list<long> sbjGIs;            /////////////////////
            if((s=strstr(str,"gi|")) !=NULL)
               {
               gi=atoi(s+3);
               sbjGIs.push_back(gi);
               if(PrintDetails()) NcbiCerr<< "First gi :" << gi << NcbiEndl;
               }

            // read ALL until "Length = "
           // different gi ids

            char bigbuf[9900];  *bigbuf = '\0';
      
            IncreaseVerbosity();
            while( (s=strstr(str, "Length = ")) == NULL)
            {
               strcat( bigbuf, str );
               fgets(str, MAXSTR, fpt);
               if((s=strstr(str,"gi|")) !=NULL)
                 {
                 gi=atoi(s+3);
                 sbjGIs.push_back(gi);
                 if(PrintDetails()) NcbiCerr<< "gi :" << gi << NcbiEndl;
                 }
            }         
            DecreaseVerbosity();
            blastMap[qName].hits[ihit].sbjGIs = sbjGIs;

            // sbjLen is read after "Length =" 
            long sbjLen = strtol(s+8, &rest, 10);      //////////////////////
            blastMap[qName].hits[ihit].sbjLen = sbjLen;
            if(PrintDetails()) NcbiCerr<< "sbjLen :" << sbjLen << NcbiEndl;

            ++defline;
            
            s = bigbuf;        // point to the beginning of the chunk containing gi's
            for(int k=1; k < 2; ++s) if(*s ==' ') ++k; // skip one space to the  protein name
//            for(int k=1; k < 5; ++s) if(*s =='|') ++k; // skip 4 pipes to the  protein name
// skip to the name
            s = skip_space(s);


// name ends w/ the eol
            string sbjName;                            /////////////////////////
            while(*s && (*s != '\n')) 
               {
               sbjName+=*s; s++;
               }
            while((ipos = sbjName.find_last_of("\r\n")) != string::npos)
               {
               sbjName.erase(ipos,1);
               }
            // NcbiCerr << "Debug: " << sbjName.c_str() << NcbiEndl;
            if(PrintDetails()) NcbiCerr<< "sbjName :" << sbjName.c_str() << NcbiEndl;
            blastMap[qName].hits[ihit].sbjName = sbjName;

            if(*s == '\n') {  ++s;  }

            // s = skip_space(s);
            // while(*s && isspace(*s)) ++s;

            if(strstr(s,"gi|")!= s) // if there is no gi| in the beginning
// print next line. How often does this happen?
               while(*s && (*s != '\n')) {s++; }

// Skip to the Score line
            while(strstr(str, "Score = ")== NULL) fgets(str, MAXSTR, fpt);

            s = strstr(str, "Score = "); s+=strlen("Score = ");
            double bitscore = atof(s);                  //////////////////////////
            blastMap[qName].hits[ihit].bitscore = bitscore;
            if(PrintDetails()) NcbiCerr<< "bitscore:" << bitscore << NcbiEndl;
// chomp ??
            if(str[strlen(str)-1] == '\n') str[strlen(str)-1] = ' ';

            s = strstr(str, "Expect = "); s+=strlen("Expect = ");
            double eval = atof(s);	           ////////////////////////////
            blastMap[qName].hits[ihit].eval = eval;

            fgets(str, MAXSTR, fpt); 
            s = strstr(str, "Identities = "); s+=strlen("Identities = "); 
            long nident = atoi(s);                ////////////////////////////
            blastMap[qName].hits[ihit].nident = nident;
            s = strstr(s, "/"); s++;
            long alilen = atoi(s);               ////////////////////////////
            blastMap[qName].hits[ihit].alilen = alilen;
            s = strstr(s, "("); s++;
            double pident = atof(s);              ///////////////////////////
            blastMap[qName].hits[ihit].pident = pident;
 
            s = strstr(str, "Positives = "); s+=strlen("Positives = "); 
            long npos = atoi(s);                ////////////////////////////
            blastMap[qName].hits[ihit].npos = npos;
            s = strstr(s, "/"); s++;
            // int alilen2 = atoi(s);
            s = strstr(s, "("); s++;
            int ppos = atoi(s);                ////////////////////////////
            blastMap[qName].hits[ihit].ppos = ppos;

// Skip to the alignment 
            while(strstr(str, "Query: ")== NULL) fgets(str, MAXSTR, fpt);
            string alignment;                    ////////////////////////////
            alignment+=str;
          
            // long q_start = strtol(str+6, &rest, 10);

            char label[10], seqal[100];
            long sbjstart =0 ,sbjend, ret;              ////////////////////
            long q_start, q_end;                 ///////////////////////////
// read first subkect line of the alignment
            ret = sscanf(str, "%s%ld%s%ld", label, &q_start, seqal, &q_end);
            if(ret != 4) { printf("\nERROR line: %s", str); return 0;}

            fgets(str, MAXSTR, fpt);
            alignment+=str;

            while(strstr(str, "Sbjct: ")== NULL) 
              {
              fgets(str, MAXSTR, fpt);
              alignment+=str;
              }

            while(1)
              {
               long  tmpstart;
               ret = sscanf(str, "%s%ld%s%ld", label, &tmpstart, seqal, &sbjend);
               if(ret != 4) { printf("\nERROR line: %s", str); return 0;}

               if(sbjstart == 0)  sbjstart = tmpstart;         

               fgets(str, MAXSTR, fpt); // skip empty line
               alignment+=str;
               fgets(str, MAXSTR, fpt); // new Query-line or Next >gi line !
               alignment+=str;
               
               if(strstr(str, "Query: ")) 
                 {
                   ret = sscanf(str, "%s%ld%s%ld", label, &tmpstart, seqal, &q_end);
                   if(ret != 4) { printf("\nERROR line: %s", str); return 0;}
                   fgets(str, MAXSTR, fpt);
                   alignment+=str;
                   fgets(str, MAXSTR, fpt); // new Sbjt-line
                   alignment+=str;
                 }
               else
                   break;
              }
         blastMap[qName].hits[ihit].alignment = alignment;
         blastMap[qName].hits[ihit].sbjstart = sbjstart;
         blastMap[qName].hits[ihit].sbjend   = sbjend  ;
         blastMap[qName].hits[ihit].q_start = q_start;
         blastMap[qName].hits[ihit].q_end   = q_end  ;
         if(PrintDetails()) NcbiCerr<< "endhit" << NcbiEndl;

         ihit++;

         } // end of hits
        DecreaseVerbosity();
    } // while
   DecreaseVerbosity();
   fclose(fpt);
 return 1;
}

int CReadBlastApp::StoreBlast(map<string, blastStr>& blastMap)
{
 if(PrintDetails()) NcbiCerr<< "Apply BLAST map into ASN.1 mem" << NcbiEndl;

 CTypeIterator<CBioseq> seq;
 IncreaseVerbosity();
 for(seq=Begin(); seq; ++seq)
   {
   skip_toprot(seq);
   if(!seq) break;
   CBioseq::TAnnot& annotList = seq->SetAnnot(); // Igor says that one will get you RW access

   string qname = CSeq_id::GetStringDescr (*seq, CSeq_id::eFormat_FastA);
   // string qname = GetStringDescr (*seq);
   if(PrintDetails()) NcbiCerr<< "Nameseq:" << qname << NcbiEndl;
   if(blastMap.find(qname) == blastMap.end() )
      {
      NcbiCerr << "WARNING: sequence " << qname << " does not have blast results, might be a naming convention mismatch" << NcbiEndl;
      continue;  //  no blast results for this seq
      }
   if(PrintDetails()) NcbiCerr<< "Have BLAST record:" << qname << NcbiEndl;

   if(PrintDetails() && blastMap[qname].hits.size()) NcbiCerr<< "Have BLAST hits:" << qname << NcbiEndl;
   if(m_previous_genome.size() && m_parent.size())
     {
     if(PrintDetails())
        {
        NcbiCerr<< "Removing some hits from previous version of the same genome:" ;
        ITERATE(list<long>, pr, m_previous_genome)
          {
          NcbiCerr << " " << *pr;
          }
        NcbiCerr<<NcbiEndl;
        }
     }
   else
     {
     if(PrintDetails())
        NcbiCerr<< "previous genome parameters are default:" << m_previous_genome.size() << ":"
                << m_parent.size() << NcbiEndl;
     }
   IncreaseVerbosity();
   for(unsigned int ihit=0; ihit < blastMap[qname].hits.size(); ihit++)
     {
     bool prev_version=false;
     if(m_previous_genome.size() && m_parent.size())
     for(list<long>::const_iterator mygi = blastMap[qname].hits[ihit].sbjGIs.begin();
         mygi!=blastMap[qname].hits[ihit].sbjGIs.end();
         mygi++)
       {
       if(find(m_previous_genome.begin(),m_previous_genome.end(),m_parent[*mygi]) != m_previous_genome.end())
         {
         prev_version=true;
         if(PrintDetails())
           {
           NcbiCerr << "CReadBlastApp::StoreBlast: blast hit to the protein from previous version: "
                  << *mygi << " << " << m_parent[*mygi] << NcbiEndl;
           }
         continue;
         }
       }
     if(prev_version)
       {
       continue;
       }
     CRef < CSeq_annot > annot (new CSeq_annot);
     annotList.push_back(annot);

     {
     CRef<CAnnotdesc> desc(new CAnnotdesc);
     desc->SetName(blastMap[qname].hits[ihit].sbjName);
     annot->SetDesc().Set().push_back(desc);
     }
     {
     CRef<CAnnotdesc> desc(new CAnnotdesc);
     desc->SetComment(blastMap[qname].hits[ihit].alignment);
     desc->SetComment("alignment skipped");
     annot->SetDesc().Set().push_back(desc);
     }


     CSeq_annot::C_Data::TAlign& alignList = annot->SetData().SetAlign();
     CRef < CSeq_align > align (new CSeq_align);
     alignList.push_back(align);

     align->SetType(CSeq_align::eType_partial);

     CSeq_align::C_Segs::TDenseg& denseg = align->SetSegs().SetDenseg();  // just formally
     denseg.SetDim(2);
     denseg.SetNumseg(1);
     CDense_seg::TIds& id_vector       = denseg.SetIds(); //
       {
       CRef < CSeq_id > qid = *(seq->SetId().begin());
       id_vector.push_back(qid);
       CRef < CSeq_id > sid ( new CSeq_id ) ; //
         {
         sid->SetGi(1);
         }
       id_vector.push_back(sid);
       }
     CDense_seg::TStarts& start_vector = denseg.SetStarts();  //
       {
       start_vector.push_back(1);
       start_vector.push_back(1);
       }
     CDense_seg::TLens& len_vector     = denseg.SetLens();  //
       {
       len_vector.push_back(1);
       len_vector.push_back(1);
       }

     CSeq_align::TBounds& bounds = align->SetBounds();
     CRef < CSeq_loc > qBounds   (new CSeq_loc);
     bounds.push_back(qBounds);
     CRef < CSeq_loc > sbjBounds (new CSeq_loc);
     bounds.push_back(sbjBounds);
     qBounds->SetInt().SetFrom(blastMap[qname].hits[ihit].q_start); //++++
     qBounds->SetInt().SetTo  (blastMap[qname].hits[ihit].q_end);    //++++
     qBounds->SetInt().SetId().SetGi(1);
     sbjBounds->SetInt().SetFrom(blastMap[qname].hits[ihit].sbjstart); //++++
     sbjBounds->SetInt().SetTo  (blastMap[qname].hits[ihit].sbjend); //++++
     sbjBounds->SetInt().SetId().SetGi(1);

     CSeq_align::TScore& scores  = align->SetScore();

     scores.resize(8);
     int iscore=-1;

     iscore++; scores[iscore]=new CScore;       //
     scores[iscore]->SetId().SetStr("sbjLen");
     scores[iscore]->SetValue().SetInt(blastMap[qname].hits[ihit].sbjLen);
     iscore++; scores[iscore]=new CScore;       //
     scores[iscore]->SetId().SetStr("score");
     scores[iscore]->SetValue().SetReal(blastMap[qname].hits[ihit].bitscore);
     iscore++; scores[iscore]=new CScore;       //
     scores[iscore]->SetId().SetStr("e_value");
     scores[iscore]->SetValue().SetReal(blastMap[qname].hits[ihit].eval);
     iscore++; scores[iscore]=new CScore;       //
     scores[iscore]->SetId().SetStr("num_ident");
     scores[iscore]->SetValue().SetInt(blastMap[qname].hits[ihit].nident);
     iscore++; scores[iscore]=new CScore;       //
     scores[iscore]->SetId().SetStr("alilen");
     scores[iscore]->SetValue().SetInt(blastMap[qname].hits[ihit].alilen);
     iscore++; scores[iscore]=new CScore;       //
     scores[iscore]->SetId().SetStr("per_ident");
     scores[iscore]->SetValue().SetReal(blastMap[qname].hits[ihit].pident);
     iscore++; scores[iscore]=new CScore;       //
     scores[iscore]->SetId().SetStr("num_sim");
     scores[iscore]->SetValue().SetInt(blastMap[qname].hits[ihit].npos);
     iscore++; scores[iscore]=new CScore;       //
     scores[iscore]->SetId().SetStr("per_sim");
     scores[iscore]->SetValue().SetReal(blastMap[qname].hits[ihit].ppos);
     scores.resize(scores.size()+blastMap[qname].hits[ihit].sbjGIs.size());

     for(list<long>::const_iterator gii=blastMap[qname].hits[ihit].sbjGIs.begin();
       gii!=blastMap[qname].hits[ihit].sbjGIs.end(); ++gii)
       {
       iscore++; scores[iscore]=new CScore;       //
       scores[iscore]->SetId().SetStr("use_this_gi");
       scores[iscore]->SetValue().SetInt(*gii);
       }
     } // hits
   DecreaseVerbosity();
   if(PrintDetails()) NcbiCerr<< "End sequence hits and sequence" << NcbiEndl;
   }
 DecreaseVerbosity();
 if(PrintDetails()) NcbiCerr<< "End Apply BLAST map into ASN.1 mem" << NcbiEndl;

 return 0;
}

int CReadBlastApp::ProcessCDD(map<string, blastStr>& blastMap)
{
 if(PrintDetails()) NcbiCerr<< "Process CDD map" << NcbiEndl;
 CTypeIterator<CBioseq> seq;

 IncreaseVerbosity();
 for(seq=Begin(); seq; ++seq)
   {
   skip_toprot(seq);
   if(!seq) break;
   string qname = CSeq_id::GetStringDescr (*seq, CSeq_id::eFormat_FastA);
   //string qname = GetStringDescr (*seq);
   if(PrintDetails()) NcbiCerr<< "Nameseq:" << qname << NcbiEndl;
   if(blastMap.find(qname) == blastMap.end() ) continue;  //  no blast results for this seq
   if(PrintDetails()) NcbiCerr<< "Have BLAST record:" << qname << NcbiEndl;
   if(PrintDetails() && blastMap[qname].hits.size()) NcbiCerr<< "Have BLAST hits:" << qname << NcbiEndl;

   int numQcoversHits=0; // query covers the domain
   int numScoversHits=0; // domain covers the query
   int numHits=0;
   IncreaseVerbosity();
   const blastStr& thisStr=blastMap.find(qname)->second;
   // const blastStr& thisStr = blastMap[qname];
   int qLen = blastMap[qname].qLen;
   vector<problemStr> problems;
   for(unsigned int ihit=0; ihit < thisStr.hits.size(); ihit++)
     {
     const hitStr& thisHitStr = thisStr.hits[ihit];
     int q_ali_len = thisHitStr.q_end - thisHitStr.q_start+1;
     int s_ali_len = thisHitStr.sbjend - thisHitStr.sbjstart+1;
     int sLen      = thisHitStr.sbjLen;
     string sname = thisHitStr.sbjName;
     if(sname.find(" PRK")!=string::npos) continue; // skip PRK's which are not domains
     if(thisHitStr.eval>m_eThreshold) continue;

     if((double)q_ali_len/qLen < m_partThreshold  &&
        (double)s_ali_len/sLen > m_entireThreshold  )
        {
        numQcoversHits++;
        }
     else if
       ((double)s_ali_len/sLen < m_partThreshold  &&
        (double)q_ali_len/qLen > m_entireThreshold  )
        {
        char bufferchar[2048];  memset(bufferchar, 0, 2048);
        strstream buffer(bufferchar, 2048, IOS_BASE::out);
        buffer.flush();
        buffer << "Query" << "\t"
               << qname << "\t"
               << qLen << "\t"
               << thisHitStr.q_start << "\t"
               << thisHitStr.q_end << NcbiEndl;
        buffer << "Subject" << "\t"
               << sname << "\t"
               << sLen << "\t"
               << thisHitStr.sbjstart << "\t"
               << thisHitStr.sbjend << NcbiEndl;
        string debug_test = buffer.str();
/*
        strstream misc_feat;
        misc_feat << buffer.str() << '\0';
*/
        problemStr problem = { ePartial, buffer.str(), "","", "", -1, -1, eNa_strand_unknown};
        problems.push_back(problem);
        if(PrintDetails())
         {
         NcbiCerr << buffer.str();
         }
        numScoversHits++;
        }
     numHits++;
     }
   DecreaseVerbosity();
   if(numScoversHits && !numQcoversHits)
     {
     ITERATE(vector<problemStr>, problem, problems)
       {
       m_diag[qname].problems.push_back(*problem);
       }
     char bufferchar[2048];  memset(bufferchar, 0, 2048);
     strstream buffer(bufferchar, 2048, IOS_BASE::out);
     buffer.flush();
     NcbiCerr << qname  << "(" << qLen << ")"
                 << " is a potential partial annotation." << NcbiEndl;
     NcbiCerr << "Out of "
                 << numHits << " hits to CDD db, "
                 << numScoversHits << " cover the query and in only "
                 << numQcoversHits << " cases query covers the subject"
                 << NcbiEndl;
     problemStr problem = { ePartial, "", buffer.str(),"", "", -1, -1, eNa_strand_unknown};
     m_diag[qname].problems.push_back(problem);
     }
   }
 DecreaseVerbosity();
 return 0;
}

