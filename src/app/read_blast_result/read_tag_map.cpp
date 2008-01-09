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
* Author of the template:  Aaron Ucko
*
* File Description:
*   Simple program demonstrating the use of serializable objects (in this
*   case, biological sequences).  Does NOT use the object manager.
*
* Modified: Azat Badretdinov
*   reads seq-submit file, blast file and optional tagmap file to produce list of potential candidates
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include "read_blast_result.h"

int CReadBlastApp::ReadTagMap(const char *file)
{
  ifstream in(file);
 string key, value;
  while(!in.eof())
    {
    in >> key >> value;
    if(!in.eof()) m_tagmap[key]=value;
    }

  return m_tagmap.size();
}


/*
* ===========================================================================
*
* $Log: read_tag_map.cpp,v $
* Revision 1.1  2007/09/20 14:40:44  badrazat
* read routines to their own files
*
* Revision 1.4  2007/07/25 12:40:41  badrazat
* read_tag_map.cpp: renaming some routines
* read_tag_map.cpp: attempt at smarting up RemoveInterim: nothing has been done
* read_blast_result.cpp: second dump of -out asn does not happen, first (debug) dump if'd out
*
* Revision 1.3  2007/07/24 16:15:55  badrazat
* added general tag str to the methods of copying location to seqannots
*
* Revision 1.2  2007/07/19 20:59:26  badrazat
* SortSeqs is done for all seqsets
*
* Revision 1.1  2007/06/21 16:21:31  badrazat
* split
*
* Revision 1.20  2007/06/20 18:28:41  badrazat
* regular checkin
*
* Revision 1.19  2007/05/16 18:57:49  badrazat
* fixed feature elimination
*
* Revision 1.18  2007/05/04 19:42:56  badrazat
* *** empty log message ***
*
* Revision 1.17  2006/11/03 15:22:29  badrazat
* flatfiles genomica location starts with 1, not 0 as in case of ASN.1 file
* changed corresponding flatfile locations in the TRNA output
*
* Revision 1.16  2006/11/03 14:47:50  badrazat
* changes
*
* Revision 1.15  2006/11/02 16:44:44  badrazat
* various changes
*
* Revision 1.14  2006/10/25 12:06:42  badrazat
* added a run over annotations for the seqset in case of submit input
*
* Revision 1.13  2006/10/17 18:14:38  badrazat
* modified output for frameshifts according to chart
*
* Revision 1.12  2006/10/17 16:47:02  badrazat
* added modifications to the output ASN file:
* addition of frameshifts
* removal of frameshifted CDSs
*
* removed product names from misc_feature record and
* added common subject info instead
*
* Revision 1.11  2006/10/02 12:50:15  badrazat
* checkin
*
* Revision 1.9  2006/09/08 19:24:23  badrazat
* made a change for Linux
*
* Revision 1.8  2006/09/07 14:21:20  badrazat
* added support of external tRNA annotation input
*
* Revision 1.7  2006/09/01 13:17:23  badrazat
* init
*
* Revision 1.6  2006/08/21 17:32:12  badrazat
* added CheckMissingRibosomalRNA
*
* Revision 1.5  2006/08/11 19:36:09  badrazat
* update
*
* Revision 1.4  2006/05/09 15:08:51  badrazat
* new file cut_blast_output_qnd and some changes to read_tag_map.cpp
*
* Revision 1.3  2006/03/29 19:44:21  badrazat
* same borders are included now in complete overlap calculations
*
* Revision 1.2  2006/03/29 17:17:32  badrazat
* added id extraction from whole product record in cdregion annotations
*
* Revision 1.1  2006/03/22 13:32:59  badrazat
* init
*
* Revision 1000.1  2004/06/01 18:31:56  gouriano
* PRODUCTION: UPGRADED [GCC34_MSVC7] Dev-tree R1.3
*
* Revision 1.3  2004/05/21 21:41:41  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.2  2003/03/10 18:48:48  kuznets
* iterate->ITERATE
*
* Revision 1.1  2002/04/18 16:05:13  ucko
* Add centralized tree for sample apps.
*
*
* ===========================================================================
*/
