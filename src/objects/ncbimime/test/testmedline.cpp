#define Ncbi_mime_asn11 0
#define Medline1 1

#if Ncbi_mime_asn11
#include <objects/ncbimime/Ncbi_mime_asn1.hpp>
#define Struct1 CNcbi_mime_asn1
#define Module1 ncbimime
#define File1 "ncbimime"
#endif
#if Medline1
#include <objects/medline/Medline_entry.hpp>
#define Struct1 CMedline_entry
#define Module1 medline
#define File1 "medline"
#endif
#include <objects/medlars/Medlars_entry.hpp>
#define Struct2 CMedlars_entry
#define Module2 medlars
#define File2 "medlars"

#include <corelib/ncbistre.hpp>
#include <serial/serial.hpp>
#include <serial/objistrasn.hpp>
#include <serial/objostrasn.hpp>


BEGIN_NCBI_SCOPE

using namespace objects;

extern "C"
int main(void)
{
    SetDiagStream(&NcbiCerr);

    Struct1 object1;
    try {
        CNcbiIfstream in(File1 ".ent");
        CObjectIStreamAsn objIn(in);
        objIn >> object1;
    }
    catch (exception& exc) {
        ERR_POST("Exception: " << exc.what());
    }
    try {
        CNcbiOfstream out(File1 ".out");
        CObjectOStreamAsn objOut(out);
        objOut << object1;
    }
    catch (exception& exc) {
        ERR_POST("Exception: " << exc.what());
    }
    Struct2 object2;
    try {
        CNcbiIfstream in(File2 ".ent");
        CObjectIStreamAsn objIn(in);
        objIn >> object2;
    }
    catch (exception& exc) {
        ERR_POST("Exception: " << exc.what());
    }
    try {
        CNcbiOfstream out(File2 ".out");
        CObjectOStreamAsn objOut(out);
        objOut << object2;
    }
    catch (exception& exc) {
        ERR_POST("Exception: " << exc.what());
    }
}

END_NCBI_SCOPE
