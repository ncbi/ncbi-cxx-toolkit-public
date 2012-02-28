
#ifndef CU_CDREADWRITEASN_HPP
#define CU_CDREADWRITEASN_HPP

#include <corelib/ncbiargs.hpp>
#include <corelib/ncbistre.hpp>

#include <serial/serial.hpp>
#include <serial/objistrasn.hpp>
#include <serial/objistrasnb.hpp>
#include <serial/objostrasn.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objectio.hpp>


//BEGIN_NCBI_SCOPE

//BEGIN_objects_SCOPE // namespace ncbi::objects::
USING_NCBI_SCOPE;
//USING_SCOPE(cd_utils);

//------------------------------------------------------------------------------------
// create a new copy of a C++ ASN data object (copied from Paul's asn_converter.hpp)
//------------------------------------------------------------------------------------
template < class ASNClassOld, class ASNClassNew >
//NCBI_CDUTILS_EXPORT 
static void TransformASNObject(ASNClassNew& newObject,const ASNClassOld& originalObject, std::string *err)
{
    err->erase();
    try {
        // create output stream and load object into it
        ncbi::CNcbiStrstream asnIOstream;
        ncbi::CObjectOStreamAsnBinary outObject(asnIOstream);
        outObject << originalObject;

        // create input stream and load into new object
        ncbi::CObjectIStreamAsnBinary inObject(asnIOstream);
        //newObject.reset(new ASNClassNew());
        inObject >> newObject;

    } catch (exception& e) {
        *err = e.what();
    }
}

//------------------------------------------------------------------------------------
// create a new copy of a C++ ASN data object (copied from Paul's asn_converter.hpp)
//------------------------------------------------------------------------------------
template < class ASNClass >
//NCBI_CDUTILS_EXPORT 
static ASNClass * CopyASNObject(const ASNClass& originalObject, std::string *err)
{
    err->erase();
    auto_ptr<ASNClass> newObject;
    try {
        // create output stream and load object into it
        ncbi::CNcbiStrstream asnIOstream;
        ncbi::CObjectOStreamAsnBinary outObject(asnIOstream);
        outObject << originalObject;

        // create input stream and load into new object
        ncbi::CObjectIStreamAsnBinary inObject(asnIOstream);
        newObject.reset(new ASNClass());
        inObject >> *newObject;

    } catch (exception& e) {
        *err = e.what();
        return NULL;
    }
    return newObject.release();
}


//------------------------------------------------------------------------------------
// a utility function for reading different types of ASN data from a file
// (copied from Paul's asn_reader.hpp)
//------------------------------------------------------------------------------------
template < class ASNClass >
//NCBI_CDUTILS_EXPORT 
static bool ReadASNFromFile(const char *filename, ASNClass *ASNobject, bool isBinary, std::string *err)
{
    err->erase();
    // initialize the binary input stream
    ncbi::CNcbiIfstream inStream(filename, IOS_BASE::in | IOS_BASE::binary);
//    auto_ptr<ncbi::CNcbiIstream> inStream;
//    inStream.reset(new ncbi::CNcbiIfstream(filename, IOS_BASE::in | IOS_BASE::binary));
    if (!inStream) {
        *err = "Cannot open file for reading";
        return false;
    }
    auto_ptr<ncbi::CObjectIStream> inObject;
    if (isBinary) {
        // Associate ASN.1 binary serialization methods with the input
        inObject.reset(new ncbi::CObjectIStreamAsnBinary(inStream));
    } else {
        // Associate ASN.1 text serialization methods with the input
        inObject.reset(new ncbi::CObjectIStreamAsn(inStream));
    }
    // Read the asn data
    try {
        *inObject >> *ASNobject;
    } catch (exception& e) {
        *err = e.what();
        return false;
    }
    inObject->Close();
    inStream.close();
    return true;
}

template < class ASNClass >
//NCBI_CDUTILS_EXPORT 
static bool ReadASNFromStream(ncbi::CNcbiIstream& is, ASNClass *ASNobject, bool isBinary, std::string *err)
{
    err->erase();
    if (!is.good()) {
        *err = "Input stream is bad.";
        return false;
    }
    auto_ptr<ncbi::CObjectIStream> inObject;
    if (isBinary) {
        // Associate ASN.1 binary serialization methods with the input
        inObject.reset(new ncbi::CObjectIStreamAsnBinary(is));
    } else {
        // Associate ASN.1 text serialization methods with the input
        inObject.reset(new ncbi::CObjectIStreamAsn(is));
    }
    // Read the asn data
    try {
        *inObject >> *ASNobject;
    } catch (exception& e) {
        *err = e.what();
        return false;
    }
    return true;
}

//------------------------------------------------------------------------------------
// for writing ASN data (copied from Paul's asn_reader.hpp)
//------------------------------------------------------------------------------------
template < class ASNClass >
//NCBI_CDUTILS_EXPORT 
static bool WriteASNToFile(const char *filename, const ASNClass& ASNobject, bool isBinary,
    std::string *err, ncbi::EFixNonPrint fixNonPrint = ncbi::eFNP_Default)
{
    err->erase();
    // initialize a binary output stream
    ncbi::CNcbiOfstream outStream(filename, IOS_BASE::out | IOS_BASE::binary);
//    auto_ptr<ncbi::CNcbiOstream> outStream;
//    outStream.reset(new ncbi::CNcbiOfstream(filename, IOS_BASE::out | IOS_BASE::binary));
    if (!outStream) {
        *err = "Cannot open file for writing";
        return false;
    }
    auto_ptr<ncbi::CObjectOStream> outObject;
    if (isBinary) {
        // Associate ASN.1 binary serialization methods with the input
        outObject.reset(new ncbi::CObjectOStreamAsnBinary(outStream, fixNonPrint));
    } else {
        // Associate ASN.1 text serialization methods with the input
        outObject.reset(new ncbi::CObjectOStreamAsn(outStream, fixNonPrint));
    }
    // write the asn data
    try {
        *outObject << ASNobject;
    } catch (exception& e) {
        *err = e.what();
        return false;
    }
    outObject->Close();
    outStream.close();
    return true;
}

template < class ASNClass >
//NCBI_CDUTILS_EXPORT 
static bool WriteASNToStream(ncbi::CNcbiOstream& os, const ASNClass& ASNobject, bool isBinary,
    std::string *err, ncbi::EFixNonPrint fixNonPrint = ncbi::eFNP_Default)
{
    err->erase();
    if (!(os.good())) {
        *err = "Stream for writing is bad";
        return false;
    }
    auto_ptr<ncbi::CObjectOStream> outObject;
    if (isBinary) {
        // Associate ASN.1 binary serialization methods with the input
        outObject.reset(new ncbi::CObjectOStreamAsnBinary(os, fixNonPrint));
    } else {
        // Associate ASN.1 text serialization methods with the input
        outObject.reset(new ncbi::CObjectOStreamAsn(os, fixNonPrint));
    }
    // write the asn data
    try {
        *outObject << ASNobject;
    } catch (exception& e) {
        *err = e.what();
        return false;
    }
    return true;
}
 
template < class ASNContainerClass >
class NCBI_CDUTILS_EXPORT CObjectIStreamHelper
{
private:
    auto_ptr<CObjectIStream> inStream;

public:
    CObjectIStreamHelper(const string& filename, bool isBinary)
    {
        inStream.reset(CObjectIStream::Open((isBinary ? eSerial_AsnBinary : eSerial_AsnText), filename));
        if (!inStream->InGoodState())
            NCBI_USER_THROW("input stream not InGoodState()");
        inStream->SkipFileHeader(ASNContainerClass::GetTypeInfo());
    }

    CObjectIStream& GetInStream(void) { return *inStream; }
};


template < class ASNContainerClass , class ASNElementClass >
class NCBI_CDUTILS_EXPORT ASNInputContainerStream
{
private:
    CObjectIStreamHelper < ASNContainerClass > inStream;
    CIStreamContainerIterator containerIterator;

public:
    // ctor will throw an exception if stream opening fails for any reason
    ASNInputContainerStream(const string& filename, bool isBinary) :
        inStream(filename, isBinary),
        containerIterator(inStream.GetInStream(), CObjectTypeInfo(ASNContainerClass::GetTypeInfo()))
    {
    }

    bool ReadElement(ASNElementClass *element)
    {
        if (containerIterator.HaveMore()) {
            element->Reset();
            containerIterator >> *element;
            ++containerIterator;
            return true;
        } else
            return false;
    }
};


//END_objects_SCOPE // namespace ncbi::objects::

//END_NCBI_SCOPE

#endif
