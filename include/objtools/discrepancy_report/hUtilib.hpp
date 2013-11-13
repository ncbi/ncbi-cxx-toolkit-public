// $Id$
//
// Author: J. Chen
//
// $Log: hUtilib.hpp,v $
// Revision 1.5  2005/05/24 21:25:51  chenj
// calss CGeneralExc added to handle exception
//
// Revision 1.4  2005/03/08 19:34:19  chenj
// many new functions were added
//
// Revision 1.3  2004/12/06 22:40:54  chenj
// Added ReqId2Str()
//
// Revision 1.2  2004/10/01 11:27:44  thiessen
// MemFree -> delete; remove C-Toolkit dependency
//
// Revision 1.1  2004/09/30 22:15:00  thiessen
// checkin Jie's helper library
//
// Revision 1.1  2004/09/29 18:34:30  chenj
// Common utility lib: libcjlib.a
//
//


#ifndef _UTILS_
#define _UTILS_

#include <cgi/ncbicgir.hpp>
#include <cgi/cgiapp.hpp>
#include <cgi/ncbires.hpp>
#include <corelib/ncbistl.hpp>
#include <cgi/cgictx.hpp>
#include <cgi/ncbicgi.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbiobj.hpp>
#include <corelib/ncbifile.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <serial/objostr.hpp>
#include <serial/objistr.hpp>
#include <serial/serial.hpp>
#include <serial/serialdef.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_util.h>

#include <string>
#include <iostream>
#include <memory>
#include <exception>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

void MultiTokenize(vector <string>& arr, const string& src, const string& delim);

unsigned CntOfSubstrInStr(const string& str, const string& thisStr);

void RmvNonDigitsAtEnds(string& str);

template < class T >
void Str2NumLs(const string& in_str, list <T>& ls)
{
    string::size_type pos0=0, pos1=0;
    while ( (pos1 = in_str.find(",", pos0)) != string::npos) {

	ls.push_back( atoi(in_str.substr(pos0, pos1-pos0).data()) );
	pos0 = pos1+1;
    }
    
    if (pos0 <= in_str.size()) 
	ls.push_back( atoi(in_str.substr(pos0, in_str.size()-pos0 + 1).data()) );
}


bool RmChar2Comma(string& str, const string& moved_ch);


template <class AsnClass>
string Blob2Str(const AsnClass& blob, ESerialDataFormat datafm = eSerial_AsnText)
{

        CNcbiOstrstream oss;
        auto_ptr <CObjectOStream> oos (CObjectOStream::Open(datafm, oss, eNoOwnership));
        *oos << blob;
        oos->Close();
        return ( CNcbiOstrstreamToString(oss) );

};


template <class AsnClass>
bool Str2Blob(const string& str, AsnClass& blob, ESerialDataFormat datafm=eSerial_AsnText)
{
	CNcbiIstrstream iss(str.data(), str.size());
        auto_ptr <CObjectIStream> ois (CObjectIStream::Open(datafm, iss));
	try { 
                *ois >> blob;
                ois->Close();
        }
        catch (CSerialException&) {

           ois->Close();
           return false;
        }

        return true;
};



template <class AsnClass>
void OutBlob(const AsnClass& blob, string file_nm, ESerialDataFormat datafm =eSerial_AsnText)
{
	auto_ptr <CObjectOStream> oos (CObjectOStream::Open(datafm, file_nm));
	*oos << blob;
        oos->Close();
};


template <class AsnClass>
void ReadInBlob(AsnClass& blob, string file_nm, ESerialDataFormat datafm=eSerial_AsnText)
{
        auto_ptr <CObjectIStream> ois (CObjectIStream::Open(datafm, file_nm));
        *ois >> blob;
        ois->Close();
};


string Vec2ListSepByComma(vector <string>& vec);

string Vec2ListSepByComma(vector <unsigned>& vec);

void GetIniValue(string& output, string& input);
void GetIniValue(char * output, string& input);


string ReadF2String(const string& FName);

class CGeneralExc : public exception 
{

	string exc_msg;

   public:

        CGeneralExc (string excep) { exc_msg = excep; };
        ~CGeneralExc () throw() {};

        const char *what() const throw() { return exc_msg.c_str(); }

};


string GetTodaysDate(void);




int isInt(char *str);

bool isInt(const string& str);

void StrCut(char* s1, char* s2, short loc1, short loc2);

template <class T>
T* NewDataType(unsigned cnt)
{
   T* pResult = (T*) new T [cnt];
   if (!pResult) 
	THROWS("No enough momery to new a pointer\n");

   return pResult;
}

string GetTime();

END_NCBI_SCOPE

#endif
