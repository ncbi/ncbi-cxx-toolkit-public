/* $Id$
 * =========================================================================
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
 * =========================================================================
 *
 *
 * Author: J. Chen
 *
 * $Log: cUtilib.cpp,v $
 * Revision 1.5  2005/12/20 23:01:39  chenj
 * 'Error: ' -> 'Error Message(s): '
 *
 * Revision 1.4  2005/03/08 19:34:19  chenj
 * many new functions were added
 *
 * Revision 1.3  2004/12/06 22:40:54  chenj
 * Added ReqId2Str()
 *
 * Revision 1.2  2004/10/01 11:27:44  thiessen
 * MemFree -> delete; remove C-Toolkit dependency
 *
 * Revision 1.1  2004/09/30 22:15:00  thiessen
 * checkin Jie's helper library
 *
 * Revision 1.1  2004/09/29 18:34:30  chenj
 * Common utility lib: libcjlib.a
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbimisc.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/enumvalues.hpp>


#include <objtools/discrepancy_report/hUtilib.hpp>

#include <sstream>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
USING_SCOPE(std);


static 	ostringstream output;
static	string	strtmp;
static 	string	iStr = "1234567890";
static	string	lchStr = "abcdefghijklmnopqrstuvwxyz";
static	string	uchStr = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

void MultiTokenize(vector <string>& arr, const string& src, const string& delim)
{
   vector <string> tmp;
   tmp = NStr::Tokenize(src, delim, tmp);
   for (unsigned i=0; i< tmp.size(); i++)
        if (!tmp[i].empty()) arr.push_back(tmp[i]);

}  // MultiTokenize()

unsigned CntOfSubstrInStr(const string& str, const string& thisStr)
{
  if (str.empty()) return 0;
  strtmp = str;
  unsigned cnt = 0;

  size_t pos;
  while ( (pos = strtmp.find(thisStr)) != string::npos ) {

	cnt++;
	strtmp = strtmp.substr(pos+1, strtmp.size());	
  }

  return cnt;

} // CntofSubstrInStr



void RmvNonDigitsAtEnds(string& str)
{

   if (str.empty()) return;
   str = str.substr(str.find_first_of(iStr));


   if (str.empty()) return;
   str = str.substr(0, str.find_last_of(iStr));

} //RmvNonDigitsAtEnds



bool RmChar2Comma(string& str, const string& mved_str)
{

   string::size_type pos;

   while ((pos = str.find(mved_str)) != string::npos) {

      // rmv the begining char.
      if (!pos || pos == str.size()-1 || (pos && str[pos-1] == ',') || 
		(pos < str.size() -1 && str[pos+1] == ',') )
		str.erase(pos, mved_str.size());
      else str.replace(pos, mved_str.size(), ",");
   }

   return true;
}



string Vec2ListSepByComma (vector <unsigned>& vec)
{

  	output.str("");
	for (unsigned i=0; i< vec.size(); i++)
		output  << vec[i] << ",";
	return (output.str().substr(0, output.str().size()-1));

}	// Vec2ListSepByComma



string Vec2ListSepByComma (vector <string>& vec)
{

        output.str("");
        for (unsigned i=0; i< vec.size(); i++)
                output  << vec[i] << ",";
        return (output.str().substr(0, output.str().size()-1));

} 



static string qto_str = "\"\' ";

void GetIniValue(string& output, string& input)
{

        size_t pos = input.find('=');
        strtmp = input.substr(pos+1);

        pos = strtmp.find_first_not_of(qto_str);
        if (pos == string::npos) output = "";
        else {

                unsigned pos2 = strtmp.find_last_not_of(qto_str);
                output =  strtmp.substr(pos, pos2-pos+1);
        }

	return;
}


void GetIniValue(char* output, string& input)
{
	size_t pos = input.find('=');
	strtmp = input.substr(pos+1);

	pos = strtmp.find_first_not_of(qto_str);
	if (pos == string::npos) {
		output[0] = '\0';
		return;
	}
	else {
		unsigned pos2 = strtmp.find_last_not_of(qto_str);
		strcpy(output, (strtmp.substr(pos, pos2-pos+1)).c_str());
		return;
	}
}


string GetTodaysDate(void)
{
  int month, day, year;
  struct tm *now_tm;
  time_t now_t;
  char  date[100];

  vector<string> Months;
  Months.reserve(12);
  Months.push_back("JAN");
  Months.push_back("FEB");
  Months.push_back("MAR");
  Months.push_back("APR");
  Months.push_back("MAY");
  Months.push_back("JUN");
  Months.push_back("JUL");
  Months.push_back("AUG");
  Months.push_back("SEP");
  Months.push_back("OCT");
  Months.push_back("NOV");
  Months.push_back("DEC");

  /* Retrieve the current date */

  time (&now_t);

  /* Break it down into month, day, year and so on */
  now_tm = gmtime (&now_t);
  month = now_tm->tm_mon;
  day = now_tm->tm_mday;
  year = now_tm->tm_year;
  year = year - 100;

  if (day < 10)
	sprintf(date, "0%d-%s-0%d", day, Months[month].c_str(), year);
  else 
	sprintf(date, "%d-%s-0%d", day, Months[month].c_str(), year);

  return (string)date;

} // end of GetTodaysDate



string ReadF2String(const string& FName)
{

    ifstream inf(FName.c_str());
    char *buf;

    string strtmp = "No input file: " + FName + " (in ReadFString)";
    if (!inf) throw(CGeneralExc(strtmp));
    inf.seekg(0, ios::end);
    int len = inf.tellg();
    buf = new char [len+1];  // just for sure the space is enought

    inf.seekg(0, ios::beg);
    inf.read(buf, len);
    inf.close();

    strtmp = "";
    strtmp.assign(buf, len);

    delete [] buf;
    return strtmp;

}       // ReadF2String()




string GetTime()
{

    	time_t tp;
    	struct tm* now_time;

        time (&tp);
        now_time = gmtime(&tp);  // GreenWitch time

	return string(asctime(now_time));

} 	// GetTime()



bool isInt(const string& str)
{
     if (str.empty() || str.size() > 10) return false;
 
     unsigned i=0;
     if (str[0] == '+' || str[0] == '-') i=1;
     for (i; i< str.size(); i++) if (!isdigit(str[i])) return false;

     return true;
}


int isInt(char *str)
{
   if (!strlen(str) || strlen(str) > 10) return 0;
  
   unsigned i=0;
   if (str[0] == '+' || str[0] == '-') i=1;
   for (i; i< strlen(str); i++) if (!isdigit(str[i])) return 0;	

   return 1;
}




void StrCut(char* s1, char* s2, short loc1, short loc2)
{

	short i;
	for (i=loc1; i<=loc2; i++) s1[i-loc1] = s2[i-1];
	s1[i-loc1] = '\0';

} /* end of StrCut() */

END_NCBI_SCOPE
