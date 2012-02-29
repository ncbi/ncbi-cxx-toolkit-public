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
* Author:  David McElhany
*
* File Description:
*   A commented demo program to illustrate how to use a serial hook.
* 
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <serial/objistr.hpp>
#include <objects/general/Date_std.hpp>

USING_NCBI_SCOPE;
USING_SCOPE(ncbi::objects);

// This class implements a read hook for class members.
//
// A read hook is created by passing a new instance of this class to a
// "set hook" method.  Hooks may be created as global or local.  Global hooks
// apply to all streams, whereas local hooks are associated with a specific
// stream.  Thus, the "set hook" methods for creating class member read hooks
// are:
//     SetGlobalReadHook()
//     SetLocalReadHook()
// Note: Global hooks are deprecated, so please use local hooks if possible.
//
// This class must override the virtual method ReadClassMember().  See the
// comment for the ReadClassMember() method below for more details.
//
// In principle, multiple instances of this hook class could be used to provide
// the same hook processing for more than one entity.  However, it is probably
// best to create a separate class for each "thing" you want to hook and
// process.
//
// You should adopt a meaningful naming convention for your hook classes.
// In this example, the convention is C<mode><context>Hook_<object>__<member>
// where:   <mode>=(Read|Write|Copy|Skip)
//          <context>=(Obj|CM|CV)  --  object, class member, or choice variant
// and hyphens in ASN.1 object types are replaced with underscores.
//
// Note:  Since this is a read hook, ReadClassMember() will only be called when
// reading from the stream.  If the stream is being skipped, ReadClassMember()
// will not be called.  If you want to use a hook to read a specific type of
// class member while skipping everything else, use a skip hook and call
// DefaultRead() from within the SkipClassMember() method.
//
// Note: This example is a read hook, which means that the input stream is
// being read when the hook is called.  Hooks for other processing modes
// (Write, Skip, and Copy) are similarly created by inheriting from the
// respecitive base classes.  It is also a ClassMember hook.  Hooks for
// other structural contexts (Object and ChoiceVariant) a similarly derived
// from the appropriate base.
class CReadCMHook_Date_std__year : public CReadClassMemberHook
{
public:
    // Implement the hook method.
    //
    // Once the read hook has been set, ReadClassMember() will be called
    // whenever the specified class member is encountered while
    // reading a hooked input stream.  Without the hook, the encountered
    // class member would have been automatically read.  With the hook, it is
    // now the responsibility of the ReadClassMember() method to remove the
    // class member from the input stream and process it as desired.  It can
    // either read it or skip it to remove it from the stream.  This is
    // easily done by calling DefaultRead() or DefaultSkip() from within
    // ReadClassMember().  Subsequent processing is up to the application.
    virtual void ReadClassMember(CObjectIStream& in,
                                 const CObjectInfoMI& passed_info)
    {
        // Perform any pre-read processing here.
        //NcbiCout << "In ReadClassMember() hook, before reading." << NcbiEndl;

        // You must call DefaultRead() (or perform an equivalent operation)
        // if you want the object to be read into memory.  You could also
        // call DefaultSkip() if you wanted to skip the hooked object while
        // reading everything else.
        DefaultRead(in, passed_info);

        // Perform any post-read processing here.  Once the object has been
        // read, its data can be used for processing.
        CNcbiOstrstream oss;
        oss << MSerial_AsnText << passed_info.GetClassObject();
        string s = CNcbiOstrstreamToString(oss);
        NcbiCout << s << NcbiEndl;
    }
};

int main(int argc, char** argv)
{
    // Create some ASN.1 data that can be parsed by this code sample.
    char asn[] = "Date-std ::= { year 1998 }";

    // Setup an input stream, based on the sample ASN.1.
    CNcbiIstrstream iss(asn);
    auto_ptr<CObjectIStream> in(CObjectIStream::Open(eSerial_AsnText, iss));

    ////////////////////////////////////////////////////
    // Create a hook for the 'year' class member of Date-std objects.
    // The year class member was aribtrarily chosen to illustrate the
    // use of hooks - many other entities would work equally well.

    // Get data structures that model the type information for Date-std
    // objects and their 'year' class members.
    // The type information will be used to recognize and forward 'year'
    // class members of Date-std objects found in the stream to the hook.
    CObjectTypeInfo stdInfo = CType<CDate_std>();
    CObjectTypeInfoMI memberInfo = stdInfo.FindMember("year");

    // Set a local hook for Date-std 'year' class members.  This involves
    // creating an instance of the hook class and passing that hook to the
    // "set hook" method, which registers the hook to be called when a hooked
    // type is encountered in the stream.
    memberInfo.SetLocalReadHook(*in, new CReadCMHook_Date_std__year());

    // Read from the input stream, storing data in the object.  At this point,
    // the hook is in place so simply reading from the input stream will
    // cause the hook to be triggered whenever the 'year' class member is
    // encountered.
    in->Read(stdInfo);

    return 0;
}
