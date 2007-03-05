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
* Authors:  Paul Thiessen
*
* File Description:
*       main and code for non-windowed (scripted) Cn3D
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbitime.hpp> // avoids some 'CurrentTime' conflict later on...
#include <corelib/ncbiapp.hpp>
#include <serial/objostr.hpp>

#include <objects/mmdb1/Biostruc.hpp>
#include <objects/ncbimime/Ncbi_mime_asn1.hpp>
#include <objects/ncbimime/Biostruc_seq.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqloc/Seq_id.hpp>

#include <algorithm>
#include <vector>

#ifdef __WXMSW__
#include <windows.h>
#include <wx/msw/winundef.h>
#endif
#include <wx/wx.h>
#include <wx/filesys.h>
#include <wx/fs_zip.h>

#include "asn_reader.hpp"
#include "cn3d_tools.hpp"
#include "structure_set.hpp"
#include "opengl_renderer.hpp"
#include "cn3d_cache.hpp"
#include "data_manager.hpp"
#include "structure_window.hpp"
#include "cn3d_png.hpp"
#include "chemical_graph.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

#define ERRORTHROW(stream) do { \
    CNcbiOstrstream oss; \
    oss << stream; \
    NCBI_THROW(CException, eUnknown, ((string) CNcbiOstrstreamToString(oss))); \
} while (0)
    
void DisplayDiagnostic(const SDiagMessage& diagMsg)
{
    string errMsg;
    diagMsg.Write(errMsg);

    // severe errors go to cout, too
    if (diagMsg.m_Severity >= eDiag_Error && diagMsg.m_Severity != eDiag_Trace)
        NcbiCout << errMsg;

    // info messages and less severe errors go to cout
    else
        NcbiCout << errMsg;
}

// other placeholders
bool IsWindowedMode(void) { return false; }
wxFrame * GlobalTopWindow(void) { ERRORMSG("Shouldn't call GlobalTopWindow() in no-window mode"); return NULL; }
void SetDialogSevereErrors(bool) { }
void RaiseLogWindow(void) { ERRORMSG("Shouldn't call RaiseLogWindow() in no-window mode"); }

// no-window application object
class Cn3DNoWin : public CNcbiApplication
{
private:

public:
    void Init(void);
    int Run(void);
    void Exit(void);
};

void Cn3DNoWin::Init(void)
{
    // turn off some Mesa-related env vars, irrelevant to off-screen rendering and not platform-indepdendent
    setenv("MESA_NO_3DNOW", "true", 1);
    setenv("MESA_NO_SSE", "true", 1);
    
    // setup the diagnostic stream
    SetDiagHandler(DisplayDiagnostic, NULL, NULL);
    SetDiagPostLevel(eDiag_Info);   // report all messages
    SetDiagTrace(eDT_Default);      // trace messages only when DIAG_TRACE env. var. is set
    UnsetDiagTraceFlag(eDPF_Location);
#ifdef _DEBUG
    SetDiagPostFlag(eDPF_File);
    SetDiagPostFlag(eDPF_Line);
#else
    UnsetDiagTraceFlag(eDPF_File);
    UnsetDiagTraceFlag(eDPF_Line);
#endif

    // C++ object verification
    CSerialObject::SetVerifyDataGlobal(eSerialVerifyData_Always);
    CObjectIStream::SetVerifyDataGlobal(eSerialVerifyData_Always);
    CObjectOStream::SetVerifyDataGlobal(eSerialVerifyData_Always);

    // set up argument processing
    CArgDescriptions *argDescr = new CArgDescriptions(false);
    argDescr->SetUsageContext(GetArguments().GetProgramName(), "No-Window Cn3D");
    
    // get data from file or network
    argDescr->AddOptionalKey("f", "file", "Ncbi-mime-asn1, Biostruc, or Cdd ASN.1 data file", argDescr->eString);
    argDescr->AddOptionalKey("d", "id", "MMDB or PDB ID", argDescr->eString);
    
    // model, for network or Biostruc load
    argDescr->AddOptionalKey("o", "model", "Model type to use for coordinates", argDescr->eString);
    argDescr->SetConstraint("o", (new CArgAllow_Strings)->Allow("alpha")->Allow("single")->Allow("PDB"));
    
    // controls for output
    argDescr->AddKey("p", "pngfile", "Output PNG file name", argDescr->eString);
    argDescr->AddKey("w", "width", "Output width in pixels", argDescr->eInteger);
    argDescr->AddKey("h", "height", "Output height in pixels", argDescr->eInteger);
    argDescr->AddFlag("i", "Create interlaced PNG");
    
    // use favorite style
    argDescr->AddOptionalKey("s", "style", "Favorite style", argDescr->eString);
    
    SetupArgDescriptions(argDescr);
}

void Cn3DNoWin::Exit(void)
{
    SetDiagHandler(NULL, NULL, NULL);
    SetDiagStream(&cout);

    // remove dictionary
    DeleteStandardDictionary();
}

int Cn3DNoWin::Run(void)
{
    TRACEMSG("Hello! Running build from " << __DATE__);
    int status = 1;
    
    try {
    
        // validate arguments
        const CArgs& args = GetArgs();
        if (!(args["f"].HasValue() || args["d"].HasValue()) || (args["f"].HasValue() && args["d"].HasValue()))
            ERRORTHROW("Command line: Must supply one (and only one) of -f or -d");
        EModel_type model = eModel_type_ncbi_all_atom;
        if (args["o"].HasValue()) {
            if (args["o"].AsString() == "alpha")
                model = eModel_type_ncbi_backbone;
            else if (args["o"].AsString() == "PDB")
                model = eModel_type_pdb_model;
        }
        string favorite(args["s"].HasValue() ? args["s"].AsString() : kEmptyStr);

        // setup dirs
        SetUpWorkingDirectories(GetArguments().GetProgramName().c_str());

        // read dictionary
        wxString dictFile = wxString(GetDataDir().c_str()) + "bstdt.val";
        LoadStandardDictionary(dictFile.c_str());

        // set up registry 
        LoadRegistry();

        // local structure set and renderer
        StructureSet *sset = NULL;
        OpenGLRenderer renderer(NULL);

        // load data from file
        if (args["f"].HasValue()) {
            // if -o is present, assume this is a Biostruc file
            if (args["o"].HasValue()) {
                CNcbi_mime_asn1 *mime = CreateMimeFromBiostruc(args["f"].AsString(), model);
                if (!mime || !LoadDataOnly(&sset, &renderer, NULL, mime, favorite))
                    ERRORTHROW("Failed to load Biostruc file " << args["f"].AsString());
            } else {
                if (!LoadDataOnly(&sset, &renderer, args["f"].AsString().c_str(), NULL, favorite, model))
                    ERRORTHROW("Failed to load file " << args["f"].AsString());
            }
        }

        // else network fetch
        else {
            CNcbi_mime_asn1 *mime = LoadStructureViaCache(args["d"].AsString(), model);
            if (!mime || !LoadDataOnly(&sset, &renderer, NULL, mime, favorite))
                ERRORTHROW("Failed to load from network with id " << args["d"].AsString());
        }

        if (!sset)
            ERRORTHROW("Somehow ended up with NULL sset");
        auto_ptr < StructureSet > sset_ap(sset); // so we can be sure it's deleted
        
        // export PNG image
        if (!ExportPNG(NULL, &renderer, args["p"].AsString(), args["w"].AsInteger(), args["h"].AsInteger(), args["i"].HasValue()))
            ERRORTHROW("PNG export failed");
        
        TRACEMSG("Done!");
        status = 0;
    
    } catch (ncbi::CException& ce) {
        ERRORMSG("Caught CException: " << ce.GetMsg());
    } catch (std::exception& e) {
        ERRORMSG("Caught exception: " << e.what());
    } catch (...) {
        ERRORMSG("Caught unknown exception");
    }

    return status;
}

END_SCOPE(Cn3D)
 
USING_SCOPE(Cn3D);

int main(int argc, char *argv[])
{
    return Cn3DNoWin().AppMain(argc, argv, 0, eDS_Default, NULL);
}
