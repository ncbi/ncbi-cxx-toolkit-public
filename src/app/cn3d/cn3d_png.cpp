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
*      save structure window to PNG file
*
* ===========================================================================
*/

#ifdef _MSC_VER
#pragma warning(disable:4018)   // disable signed/unsigned mismatch warning in MSVC
#endif

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbi_limits.h>

// need GL headers for off-screen rendering
#if defined(__WXMSW__)
#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>

#elif defined(__WXGTK__)
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glx.h>
#include <gdk/gdkx.h>

#elif defined(__WXMAC__)
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#endif

#include <wx/platform.h>
#if defined(__WXGTK__) && defined(__LINUX__)
// use system headers/libs for linux builds
#include <png.h>
#else
// otherwise, use libs built into wxWindows
#include "../cn3d/png.h"
#endif

#include "cn3d_png.hpp"
#include "cn3d_glcanvas.hpp"
#include "opengl_renderer.hpp"
#include "progress_meter.hpp"
#include "cn3d_tools.hpp"
#include "messenger.hpp"


////////////////////////////////////////////////////////////////////////////////////////////////
// The following is taken unmodified from wxDesigner's C++ code from png_dialog.wdr
////////////////////////////////////////////////////////////////////////////////////////////////

#include <wx/image.h>
#include <wx/statline.h>
#include <wx/spinbutt.h>
#include <wx/spinctrl.h>
#include <wx/splitter.h>
#include <wx/listctrl.h>
#include <wx/treectrl.h>
#include <wx/notebook.h>
#include <wx/grid.h>

// Declare window functions

#define ID_TEXT 10000
#define ID_B_BROWSE 10001
#define ID_T_NAME 10002
#define ID_T_WIDTH 10003
#define ID_T_HEIGHT 10004
#define ID_C_ASPECT 10005
#define ID_C_INTERLACE 10006
#define ID_B_OK 10007
#define ID_B_CANCEL 10008
wxSizer *SetupPNGOptionsDialog( wxPanel *parent, bool call_fit = TRUE, bool set_sizer = TRUE );

////////////////////////////////////////////////////////////////////////////////////////////////

// fix jmpbuf symbol problem on aix
#if defined(_AIX43) && defined(jmpbuf)
#undef jmpbuf
#endif

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

static ProgressMeter *progressMeter = NULL;
static const int PROGRESS_RESOLUTION = 100, MAX_BUFFER_PIXELS = 1000000;
static int nRows;

class PNGOptionsDialog : private wxDialog
{
public:
    PNGOptionsDialog(wxWindow *parent);
    bool Activate(int initialWidth, int initialHeight, bool initialInterlaced);
    bool GetValues(wxString *outputFilename, int *width, int *height, bool *interlaced);

private:
    double initialAspect;
    bool dontProcessChange;

    // event callbacks
    void OnButton(wxCommandEvent& event);
    void OnChangeSize(wxCommandEvent& event);
    void OnCheckbox(wxCommandEvent& event);
    void OnCloseWindow(wxCloseEvent& event);

    DECLARE_EVENT_TABLE()
};

#define DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(var, id, type) \
    type *var; \
    var = wxDynamicCast(FindWindow(id), type); \
    if (!var) { \
        ERRORMSG("Can't find window with id " << id); \
        return; \
    }

#define DECLARE_AND_FIND_WINDOW_RETURN_RESULT_ON_ERR(var, id, type, errResult) \
    type *var; \
    var = wxDynamicCast(FindWindow(id), type); \
    if (!var) { \
        ERRORMSG("Can't find window with id " << id); \
        return errResult; \
    }

// to make sure values are legitimate integers; use wxString::ToDouble to avoid parsing
// strings as octal or hexadecimal...
#define GET_AND_IS_VALID_SIZE(textctrl, var) \
    (textctrl->GetValue().ToDouble(&var) && var >= 1 && fmod(var, 1.0) == 0.0 && var <= kMax_Int)

BEGIN_EVENT_TABLE(PNGOptionsDialog, wxDialog)
    EVT_BUTTON      (-1,    PNGOptionsDialog::OnButton)
    EVT_TEXT        (-1,    PNGOptionsDialog::OnChangeSize)
    EVT_CHECKBOX    (-1,    PNGOptionsDialog::OnCheckbox)
    EVT_CLOSE       (       PNGOptionsDialog::OnCloseWindow)
END_EVENT_TABLE()

PNGOptionsDialog::PNGOptionsDialog(wxWindow *parent) :
    wxDialog(parent, -1, "Export Options", wxPoint(50, 50), wxDefaultSize,
        wxCAPTION | wxSYSTEM_MENU | wxFRAME_NO_TASKBAR), // not resizable
    dontProcessChange(false)
{
    // construct the panel
    wxPanel *panel = new wxPanel(this, -1);
    wxSizer *topSizer = SetupPNGOptionsDialog(panel, false);

    // call sizer stuff
    topSizer->Fit(panel);
    SetClientSize(topSizer->GetMinSize());
}

void PNGOptionsDialog::OnButton(wxCommandEvent& event)
{
    switch (event.GetId()) {
        case ID_B_BROWSE: {
            DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tName, ID_T_NAME, wxTextCtrl)
            wxString filename = wxFileSelector(
                "Choose a filename for output", GetUserDir().c_str(), "",
                ".png", "All Files|*.*|PNG files (*.png)|*.png",
                wxSAVE | wxOVERWRITE_PROMPT);
            if (filename.size() > 0) tName->SetValue(filename);
            break;
        }
        case ID_B_OK: {
            wxString str;
            int w, h;
            bool i;
            if (GetValues(&str, &w, &h, &i))
                EndModal(wxOK);
            else
                wxBell();
            break;
        }
        case ID_B_CANCEL:
            EndModal(wxCANCEL);
            break;
    }
}

void PNGOptionsDialog::OnChangeSize(wxCommandEvent& event)
{
    // avoid recursive calls to this, since SetValue() triggers this event; only process size fields
    if (dontProcessChange || !(event.GetId() == ID_T_WIDTH || event.GetId() == ID_T_HEIGHT)) return;

    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tWidth, ID_T_WIDTH, wxTextCtrl)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tHeight, ID_T_HEIGHT, wxTextCtrl)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(cAspect, ID_C_ASPECT, wxCheckBox)
    DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(cInterlace, ID_C_INTERLACE, wxCheckBox)

    // post refreshes for both, in case background color changes
    tWidth->Refresh(true);
    tHeight->Refresh(true);

    double w, h;
    if (!GET_AND_IS_VALID_SIZE(tWidth, w)) {
        tWidth->SetBackgroundColour(*wxRED);
        return;
    }
    tWidth->SetBackgroundColour(*wxWHITE);
    if (!GET_AND_IS_VALID_SIZE(tHeight, h)) {
        tHeight->SetBackgroundColour(*wxRED);
        return;
    }
    tHeight->SetBackgroundColour(*wxWHITE);

    // adjust to aspect ratio if indicated
    dontProcessChange = true;
    if (cAspect->GetValue()) {
        wxString num;
        if (event.GetId() == ID_T_WIDTH) {
            h = floor(w / initialAspect + 0.5);
            num.Printf("%i", (int) h);
            tHeight->SetValue(num);
        } else {
            w = floor(h * initialAspect + 0.5);
            num.Printf("%i", (int) w);
            tWidth->SetValue(num);
        }
    }
    dontProcessChange = false;

    // only allow interlacing if image is small enough
    if (w * h > MAX_BUFFER_PIXELS) {
        if (cInterlace->IsEnabled()) {
            cInterlace->Enable(false);
            cInterlace->SetValue(false);
        }
    } else {
        if (!cInterlace->IsEnabled()) {
            cInterlace->Enable(true);
            cInterlace->SetValue(true);
        }
    }
}

void PNGOptionsDialog::OnCheckbox(wxCommandEvent& event)
{
    if (event.GetId() == ID_C_ASPECT) {
        // adjust height when aspect checkbox is turned on
        DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(cAspect, ID_C_ASPECT, wxCheckBox)
        if (cAspect->GetValue()) {

            DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tWidth, ID_T_WIDTH, wxTextCtrl)
            DECLARE_AND_FIND_WINDOW_RETURN_ON_ERR(tHeight, ID_T_HEIGHT, wxTextCtrl)
            double w, h;

            if (GET_AND_IS_VALID_SIZE(tWidth, w)) {
                wxString num;
                h = floor(w / initialAspect + 0.5);
                num.Printf("%i", (int) h);
                tHeight->SetValue(num);
            }
        }
    }
}

void PNGOptionsDialog::OnCloseWindow(wxCloseEvent& event)
{
    EndModal(wxCANCEL);
}

bool PNGOptionsDialog::Activate(int initialWidth, int initialHeight, bool initialInterlaced)
{
    DECLARE_AND_FIND_WINDOW_RETURN_RESULT_ON_ERR(tName, ID_T_NAME, wxTextCtrl, false)
    DECLARE_AND_FIND_WINDOW_RETURN_RESULT_ON_ERR(tWidth, ID_T_WIDTH, wxTextCtrl, false)
    DECLARE_AND_FIND_WINDOW_RETURN_RESULT_ON_ERR(tHeight, ID_T_HEIGHT, wxTextCtrl, false)
    DECLARE_AND_FIND_WINDOW_RETURN_RESULT_ON_ERR(cAspect, ID_C_ASPECT, wxCheckBox, false)
    DECLARE_AND_FIND_WINDOW_RETURN_RESULT_ON_ERR(cInterlace, ID_C_INTERLACE, wxCheckBox, false)

    dontProcessChange = true;
    wxString num;
    num.Printf("%i", initialWidth);
    tWidth->SetValue(num);
    num.Printf("%i", initialHeight);
    tHeight->SetValue(num);
    initialAspect = ((double) initialWidth) / initialHeight;
    cAspect->SetValue(true);
    dontProcessChange = false;

    if (initialWidth*initialHeight > MAX_BUFFER_PIXELS) {
        cInterlace->Enable(false);
        cInterlace->SetValue(false);
    } else
        cInterlace->SetValue(initialInterlaced);

    // bring up file selector first
    wxCommandEvent browse(wxEVT_COMMAND_BUTTON_CLICKED, ID_B_BROWSE);
    OnButton(browse);
    if (tName->GetValue().size() == 0) return false;    // cancelled

    // return dialog result
    return (ShowModal() == wxOK);
}

bool PNGOptionsDialog::GetValues(wxString *outputFilename, int *width, int *height, bool *interlaced)
{
    DECLARE_AND_FIND_WINDOW_RETURN_RESULT_ON_ERR(tName, ID_T_NAME, wxTextCtrl, false)
    DECLARE_AND_FIND_WINDOW_RETURN_RESULT_ON_ERR(tWidth, ID_T_WIDTH, wxTextCtrl, false)
    DECLARE_AND_FIND_WINDOW_RETURN_RESULT_ON_ERR(tHeight, ID_T_HEIGHT, wxTextCtrl, false)
    DECLARE_AND_FIND_WINDOW_RETURN_RESULT_ON_ERR(cInterlace, ID_C_INTERLACE, wxCheckBox, false)

    *outputFilename = tName->GetValue();
    double val;
    if (!GET_AND_IS_VALID_SIZE(tWidth, val)) return false;
    *width = (int) val;
    if (!GET_AND_IS_VALID_SIZE(tHeight, val)) return false;
    *height = (int) val;
    *interlaced = (cInterlace->IsEnabled() && cInterlace->GetValue());
    return true;
}


static bool GetOutputParameters(wxString *outputFilename, int *width, int *height, bool *interlaced)
{
    PNGOptionsDialog dialog(NULL);
    bool ok = dialog.Activate(*width, *height, *interlaced);
    if (ok) dialog.GetValues(outputFilename, width, height, interlaced);
    return ok;
}

// callback used by PNG library to report errors
static void writepng_error_handler(png_structp png_ptr, png_const_charp msg)
{
    ERRORMSG("PNG library error: " << msg);
}

#ifdef __WXGTK__
static bool gotAnXError;
int X_error_handler(Display *dpy, XErrorEvent *error)
{
    gotAnXError = true;
    return 0;
}
#endif

// called after each row is written to the file
static void write_row_callback(png_structp png_ptr, png_uint_32 row, int pass)
{
    if (!progressMeter) return;
    int progress = 0;

    if (nRows < 0) { /* if negative, then we're doing interlacing */
        double start, end;
        switch (pass + 1) {
            case 1: start = 0;  end = 1;  break;
            case 2: start = 1;  end = 2;  break;
            case 3: start = 2;  end = 3;  break;
            case 4: start = 3;  end = 5;  break;
            case 5: start = 5;  end = 7;  break;
            case 6: start = 7;  end = 11; break;
            case 7: start = 11; end = 15; break;
            default: return;    // png lib gives final pass=7,row=0 to signal completion
        }
        progress = (int) (1.0 * PROGRESS_RESOLUTION *
            ((start / 15) + (((double) row) / (-nRows)) * ((end - start) / 15)));
    } else { /* not interlaced */
        progress = (int) (1.0 * PROGRESS_RESOLUTION * row / nRows);
    }

    progressMeter->SetValue(progress);
}

bool ExportPNG(Cn3DGLCanvas *glCanvas)
{
#if !defined(__WXMSW__) && !defined(__WXGTK__) && !defined(__WXMAC__)
    ERRORMSG("PNG export not (yet) implemented on this platform");
    return false;
#endif

    if (!glCanvas || !glCanvas->renderer) {
        ERRORMSG("ExportPNG() - bad glCanvas parameter");
        return false;
    }

    bool success = false, shareDisplayLists = true, interlaced = true;
    int outputWidth, outputHeight, bufferHeight, bytesPerPixel = 3, nChunks = 1;
    wxString filename;
    FILE *out = NULL;
    unsigned char *rowStorage = NULL;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;

    outputWidth = glCanvas->GetClientSize().GetWidth();		// initial size
    outputHeight = glCanvas->GetClientSize().GetHeight();
    if (!GetOutputParameters(&filename, &outputWidth, &outputHeight, &interlaced))
        return true; // cancelled

    try {
        INFOMSG("saving PNG file '" << filename.c_str() << "'");

        // need to avoid any GL calls in glCanvas while off-screen rendering is happening; so
        // temporarily prevent glCanvas from responding to window resize/exposure, etc.
        glCanvas->SuspendRendering(true);

        int windowViewport[4];
        glCanvas->renderer->GetViewport(windowViewport);
        TRACEMSG("window viewport: x,y: " << windowViewport[0] << ',' << windowViewport[1]
            << " size: " << windowViewport[2] << ',' << windowViewport[3]);
        INFOMSG("output size: " << outputWidth << ',' << outputHeight);

        // decide whether the in-memory image buffer can fit the whole drawing,
        // or whether we need to split it up into separate horizontal chunks
        bufferHeight = outputHeight;
        if (outputWidth * outputHeight > MAX_BUFFER_PIXELS) {
            bufferHeight = MAX_BUFFER_PIXELS / outputWidth;
            nChunks = outputHeight / bufferHeight;              // whole chunks +
            if (outputHeight % bufferHeight != 0) ++nChunks;    // partially occupied chunk
            interlaced = false;
        }

        // create and show progress meter
        wxString message;
        message.Printf("Writing PNG file %s (%ix%i)",
            (wxString(wxFileNameFromPath(filename.c_str()))).c_str(),
            outputWidth, outputHeight);
        progressMeter = new ProgressMeter(NULL, message, "Saving...", PROGRESS_RESOLUTION);

        // open the output file for writing
        out = fopen(filename.c_str(), "wb");
        if (!out) throw "can't open file for writing";

#if defined(__WXMSW__)
        HDC hdc = NULL, current_hdc = NULL;
        HGLRC hglrc = NULL, current_hglrc = NULL;
        HGDIOBJ current_hgdiobj = NULL;
        HBITMAP hbm = NULL;
        PIXELFORMATDESCRIPTOR pfd;
        int nPixelFormat;

        current_hglrc = wglGetCurrentContext(); // save to restore later
        current_hdc = wglGetCurrentDC();

        // create bitmap of same color type as current rendering context
        hbm = CreateCompatibleBitmap(current_hdc, outputWidth, bufferHeight);
        if (!hbm) throw "failed to create rendering BITMAP";
        hdc = CreateCompatibleDC(current_hdc);
        if (!hdc) throw "CreateCompatibleDC failed";
        current_hgdiobj = SelectObject(hdc, hbm);
        if (!current_hgdiobj) throw "SelectObject failed";

        memset(&pfd, 0, sizeof(PIXELFORMATDESCRIPTOR));
        pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_BITMAP | PFD_SUPPORT_OPENGL;  // NOT doublebuffered, to save memory
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = GetDeviceCaps(current_hdc, BITSPIXEL);
        pfd.iLayerType = PFD_MAIN_PLANE;
        nPixelFormat = ChoosePixelFormat(hdc, &pfd);
        if (!nPixelFormat) {
            ERRORMSG("ChoosePixelFormat failed");
            throw GetLastError();
        }
        if (!SetPixelFormat(hdc, nPixelFormat, &pfd)) {
            ERRORMSG("SetPixelFormat failed");
            throw GetLastError();
        }
        hglrc = wglCreateContext(hdc);
        if (!hglrc) {
            ERRORMSG("wglCreateContext failed");
            throw GetLastError();
        }
        // try to share display lists with regular window, to save memory and draw time
        if (!wglShareLists(current_hglrc, hglrc)) {
            WARNINGMSG("wglShareLists failed: " << GetLastError());
            shareDisplayLists = false;
        }
        if (!wglMakeCurrent(hdc, hglrc)) {
            ERRORMSG("wglMakeCurrent failed");
            throw GetLastError();
        }

#elif defined(__WXGTK__)
        GLint glSize;
        int nAttribs, attribs[20];
        XVisualInfo *visinfo = NULL;
        bool localVI = false;
        Pixmap xPixmap = 0;
        GLXContext currentCtx = NULL, glCtx = NULL;
        GLXPixmap glxPixmap = 0;
        GLXDrawable currentXdrw = 0;
        Display *display;
        int (*currentXErrHandler)(Display *, XErrorEvent *) = NULL;

        currentCtx = glXGetCurrentContext(); // save current context info
        currentXdrw = glXGetCurrentDrawable();
        display = GDK_DISPLAY();

        currentXErrHandler = XSetErrorHandler(X_error_handler);
        gotAnXError = false;

        // first, try to get a non-doublebuffered visual, to economize on memory
        nAttribs = 0;
        attribs[nAttribs++] = GLX_USE_GL;
        attribs[nAttribs++] = GLX_RGBA;
        attribs[nAttribs++] = GLX_RED_SIZE;
        glGetIntegerv(GL_RED_BITS, &glSize);
        attribs[nAttribs++] = glSize;
        attribs[nAttribs++] = GLX_GREEN_SIZE;
        attribs[nAttribs++] = glSize;
        attribs[nAttribs++] = GLX_BLUE_SIZE;
        attribs[nAttribs++] = glSize;
        attribs[nAttribs++] = GLX_DEPTH_SIZE;
        glGetIntegerv(GL_DEPTH_BITS, &glSize);
        attribs[nAttribs++] = glSize;
        attribs[nAttribs++] = None;
        visinfo = glXChooseVisual(display, DefaultScreen(display), attribs);

        // if that fails, just revert to the one used for the regular window
        if (visinfo)
            localVI = true;
        else
            visinfo = (XVisualInfo *) (glCanvas->m_vi);

        // create pixmap
        xPixmap = XCreatePixmap(display,
            RootWindow(display, DefaultScreen(display)),
            outputWidth, bufferHeight, visinfo->depth);
        if (!xPixmap) throw "failed to create Pixmap";
        glxPixmap = glXCreateGLXPixmap(display, visinfo, xPixmap);
        if (!glxPixmap) throw "failed to create GLXPixmap";
        if (gotAnXError) throw "Got an X error creating GLXPixmap";

        // try to share display lists with "regular" context
	// ... seems to be too flaky - fails on Linux/Mesa, Solaris
//        glCtx = glXCreateContext(display, visinfo, currentCtx, False);
//        if (!glCtx || !glXMakeCurrent(display, glxPixmap, glCtx)) {
//            WARNINGMSG("failed to make GLXPixmap rendering context with shared display lists");
//            if (glCtx) glXDestroyContext(display, glCtx);
            shareDisplayLists = false;

            // try to create context without shared lists
            glCtx = glXCreateContext(display, visinfo, NULL, False);
            if (!glCtx || !glXMakeCurrent(display, glxPixmap, glCtx))
                throw "failed to make GLXPixmap rendering context without shared display lists";
//        }
        if (gotAnXError) throw "Got an X error setting GLX context";

#elif defined(__WXMAC__)
        unsigned char *base = NULL;
        GLint attrib[20], glSize;
        int na = 0;
        AGLPixelFormat fmt = NULL;
        AGLContext ctx = NULL, currentCtx;

    	currentCtx = aglGetCurrentContext();

    	// Mac pixels seem to always be 32-bit
    	bytesPerPixel = 4;

    	base = new unsigned char[outputWidth * bufferHeight * bytesPerPixel];
    	if (!base) throw "failed to allocate image buffer";

    	// create an off-screen rendering context (NOT doublebuffered)
    	attrib[na++] = AGL_OFFSCREEN;
    	attrib[na++] = AGL_RGBA;
    	glGetIntegerv(GL_RED_BITS, &glSize);
        attrib[na++] = AGL_RED_SIZE;
        attrib[na++] = glSize;
        attrib[na++] = AGL_GREEN_SIZE;
        attrib[na++] = glSize;
        attrib[na++] = AGL_BLUE_SIZE;
        attrib[na++] = glSize;
    	glGetIntegerv(GL_DEPTH_BITS, &glSize);
        attrib[na++] = AGL_DEPTH_SIZE;
        attrib[na++] = glSize;
        attrib[na++] = AGL_NONE;

        if ((fmt=aglChoosePixelFormat(NULL, 0, attrib)) == NULL)
        	throw "aglChoosePixelFormat failed";
        // try to share display lists with current "regular" context
        if ((ctx=aglCreateContext(fmt, currentCtx)) == NULL) {
            WARNINGMSG("aglCreateContext with shared lists failed");
        	shareDisplayLists = false;
        	if ((ctx=aglCreateContext(fmt, NULL)) == NULL)
        	    throw "aglCreateContext without shared lists failed";
        }

        // attach off-screen buffer to this context
        if (!aglSetOffScreen(ctx, outputWidth, bufferHeight, bytesPerPixel * outputWidth, base))
        	throw "aglSetOffScreen failed";
        if (!aglSetCurrentContext(ctx))
        	throw "aglSetCurrentContext failed";

        glCanvas->renderer->RecreateQuadric();	// Macs have context-sensitive quadrics...
#endif

        TRACEMSG("interlaced: " << interlaced << ", nChunks: " << nChunks
            << ", buffer height: " << bufferHeight << ", shared: " << shareDisplayLists);

        // allocate a row of pixel storage
        rowStorage = new unsigned char[outputWidth * bytesPerPixel];
        if (!rowStorage) throw "failed to allocate pixel row buffer";

        /* set up the PNG writing (see libpng.txt) */
        png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, writepng_error_handler, NULL);
        if (!png_ptr) throw "can't create PNG write structure";
        info_ptr = png_create_info_struct(png_ptr);
        if (!info_ptr) throw "can't create PNG info structure";
        if (setjmp(png_ptr->jmpbuf)) throw "setjmp failed";
        png_init_io(png_ptr, out);

        // sets callback that's called by PNG after each written row
        png_set_write_status_fn(png_ptr, write_row_callback);

        // set various PNG parameters
        png_set_compression_level(png_ptr, Z_BEST_COMPRESSION);
        png_set_IHDR(png_ptr, info_ptr, outputWidth, outputHeight,
            8, PNG_COLOR_TYPE_RGB,
            interlaced ? PNG_INTERLACE_ADAM7 : PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
        png_write_info(png_ptr, info_ptr);

        // set up camera so that it's the same view as it is in the window
        glCanvas->renderer->Init();
        glViewport(0, 0, outputWidth, outputHeight);
        glCanvas->renderer->NewView();

        // Redraw the model into the new off-screen context, then use glReadPixels
        // to retrieve pixel data. It's much easier to use glReadPixels rather than
        // trying to read directly from the off-screen buffer, since GL can do all
        // the potentially tricky work of translating from whatever pixel format
        // the buffer uses into "regular" RGB byte triples. If fonts need scaling,
        // have to reconstruct lists regardless of whether display lists are shared.
        if (!shareDisplayLists || outputHeight != glCanvas->GetClientSize().GetHeight()) {
            glCanvas->SetGLFontFromRegistry(((double) outputHeight) / glCanvas->GetClientSize().GetHeight());
            glCanvas->renderer->Construct();
        }
        glPixelStorei(GL_PACK_ALIGNMENT, 1);

        // Write image row by row to avoid having to allocate another full image's
        // worth of memory. Do multiple passes for interlacing, if necessary. Note
        // that PNG's rows are stored top down, while GL's are bottom up.
        if (interlaced) {

            // need to draw only once
            glCanvas->renderer->Display();

            int pass, r;
            nRows = -outputHeight; // signal to monitor that we're interlacing
            if (png_set_interlace_handling(png_ptr) != 7) throw "confused by unkown PNG interlace scheme";
            for (pass = 1; pass <= 7; ++pass) {
                for (int i = outputHeight - 1; i >= 0; --i) {
                    r = outputHeight - i - 1;
                    // when interlacing, only certain rows are actually read
                    // during certain passes - avoid unncessary reads
                    if (
                        ((pass == 1 || pass == 2) && (r % 8 == 0)) ||
                        ((pass == 3) && ((r - 4) % 8 == 0)) ||
                        ((pass == 4) && (r % 4 == 0)) ||
                        ((pass == 5) && ((r - 2) % 4 == 0)) ||
                        ((pass == 6) && (r % 2 == 0)) ||
                        ((pass == 7) && ((r - 1) % 2 == 0))
                    )
                        glReadPixels(0, i, outputWidth, 1, GL_RGB, GL_UNSIGNED_BYTE, rowStorage);
                    // ... but still have to call this for each row in each pass */
                    png_write_row(png_ptr, rowStorage);
                }
            }
        }

        // not interlaced, but possibly chunked
        else {
            int bufferRow, bufferRowStart;
            nRows = outputHeight;
            for (int chunk = nChunks - 1; chunk >= 0; --chunk) {

                // set viewport for this chunk and redraw
                if (nChunks > 1) {
                    TRACEMSG("drawing chunk #" << (chunk + 1));
                    glViewport(0, -chunk*bufferHeight, outputWidth, outputHeight);
                }
                glCanvas->renderer->Display();

                // only draw "visible" part of top chunk
                if (chunk == nChunks - 1)
                    bufferRowStart = outputHeight - 1 - bufferHeight * (nChunks - 1);
                else
                    bufferRowStart = bufferHeight - 1;

                // dump chunk to PNG file
                for (bufferRow = bufferRowStart; bufferRow >= 0; --bufferRow) {
                    glReadPixels(0, bufferRow, outputWidth, 1, GL_RGB, GL_UNSIGNED_BYTE, rowStorage);
                    png_write_row(png_ptr, rowStorage);
                }
            }
        }

        // finish up PNG write
        png_write_end(png_ptr, info_ptr);
        success = true;

        // general cleanup
        if (out) fclose(out);
        if (rowStorage) delete rowStorage;
        if (png_ptr) {
            if (info_ptr)
                png_destroy_write_struct(&png_ptr, &info_ptr);
            else
                png_destroy_write_struct(&png_ptr, NULL);
        }
        if (progressMeter) {
            progressMeter->Close(true);
            progressMeter->Destroy();
            progressMeter = NULL;
        }

    // platform-specific cleanup, restore context
#if defined(__WXMSW__)
        if (current_hdc && current_hglrc) wglMakeCurrent(current_hdc, current_hglrc);
        if (hglrc) wglDeleteContext(hglrc);
        if (hbm) DeleteObject(hbm);
        if (hdc) DeleteDC(hdc);

#elif defined(__WXGTK__)
        gotAnXError = false;
        if (glCtx) {
            glXMakeCurrent(display, currentXdrw, currentCtx);
            glXDestroyContext(display, glCtx);
        }
        if (glxPixmap) glXDestroyGLXPixmap(display, glxPixmap);
        if (xPixmap) XFreePixmap(display, xPixmap);
        if (localVI && visinfo) XFree(visinfo);
        if (gotAnXError) WARNINGMSG("Got an X error destroying GLXPixmap context");
        XSetErrorHandler(currentXErrHandler);

#elif defined(__WXMAC__)
        aglSetCurrentContext(currentCtx);
        if (ctx) aglDestroyContext(ctx);
        if (fmt) aglDestroyPixelFormat(fmt);
        if (base) delete[] base;
        glCanvas->renderer->RecreateQuadric();	// Macs have context-sensitive quadrics...
#endif

    } catch (const char *err) {
        ERRORMSG("Error creating PNG: " << err);
    } catch (exception& e) {
        ERRORMSG("Uncaught exception while creating PNG: " << e.what());
    }

    // reset font after "regular" context restore
    glCanvas->SuspendRendering(false);
    if (outputHeight != glCanvas->GetClientSize().GetHeight()) {
        glCanvas->SetCurrent();
        glCanvas->SetGLFontFromRegistry(1.0);
        if (shareDisplayLists)
            GlobalMessenger()->PostRedrawAllStructures();
    }
    glCanvas->Refresh(false);

    return success;
}

END_SCOPE(Cn3D)


////////////////////////////////////////////////////////////////////////////////////////////////
// The following is taken unmodified from wxDesigner's C++ code from png_dialog.wdr
////////////////////////////////////////////////////////////////////////////////////////////////

wxSizer *SetupPNGOptionsDialog( wxPanel *parent, bool call_fit, bool set_sizer )
{
    wxBoxSizer *item0 = new wxBoxSizer( wxVERTICAL );

    wxStaticBox *item2 = new wxStaticBox( parent, -1, "Output Settings" );
    wxStaticBoxSizer *item1 = new wxStaticBoxSizer( item2, wxVERTICAL );

    wxFlexGridSizer *item3 = new wxFlexGridSizer( 1, 0, 0, 0 );
    item3->AddGrowableCol( 1 );

    wxStaticText *item4 = new wxStaticText( parent, ID_TEXT, "File name:", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item4, 0, wxALIGN_CENTRE|wxALL, 5 );

    item3->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item5 = new wxButton( parent, ID_B_BROWSE, "Browse", wxDefaultPosition, wxDefaultSize, 0 );
    item3->Add( item5, 0, wxALIGN_CENTRE|wxLEFT|wxRIGHT|wxTOP, 5 );

    item1->Add( item3, 0, wxGROW|wxALIGN_CENTER_VERTICAL, 5 );

    wxTextCtrl *item6 = new wxTextCtrl( parent, ID_T_NAME, "", wxDefaultPosition, wxDefaultSize, 0 );
    item1->Add( item6, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxLEFT|wxRIGHT|wxBOTTOM, 5 );

    wxFlexGridSizer *item7 = new wxFlexGridSizer( 2, 0, 0, 0 );
    item7->AddGrowableCol( 1 );
    item7->AddGrowableRow( 1 );

    wxStaticText *item8 = new wxStaticText( parent, ID_TEXT, "Width:", wxDefaultPosition, wxDefaultSize, 0 );
    item7->Add( item8, 0, wxALIGN_CENTER_VERTICAL|wxLEFT|wxRIGHT|wxTOP, 5 );

    item7->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxStaticText *item9 = new wxStaticText( parent, ID_TEXT, "Height:", wxDefaultPosition, wxDefaultSize, 0 );
    item7->Add( item9, 0, wxALIGN_CENTER_VERTICAL|wxLEFT|wxRIGHT|wxTOP, 5 );

    wxTextCtrl *item10 = new wxTextCtrl( parent, ID_T_WIDTH, "", wxDefaultPosition, wxDefaultSize, 0 );
    item7->Add( item10, 0, wxALIGN_CENTRE|wxLEFT|wxRIGHT|wxBOTTOM, 5 );

    wxStaticText *item11 = new wxStaticText( parent, ID_TEXT, "x", wxDefaultPosition, wxDefaultSize, 0 );
    item7->Add( item11, 0, wxALIGN_CENTRE|wxLEFT|wxRIGHT|wxBOTTOM, 5 );

    wxTextCtrl *item12 = new wxTextCtrl( parent, ID_T_HEIGHT, "", wxDefaultPosition, wxDefaultSize, 0 );
    item7->Add( item12, 0, wxALIGN_CENTRE|wxLEFT|wxRIGHT|wxBOTTOM, 5 );

    item1->Add( item7, 0, wxGROW|wxALIGN_CENTER_VERTICAL|wxALL, 5 );

    wxBoxSizer *item13 = new wxBoxSizer( wxHORIZONTAL );

    wxCheckBox *item14 = new wxCheckBox( parent, ID_C_ASPECT, "Maintain original aspect ratio", wxDefaultPosition, wxDefaultSize, 0 );
    item14->SetValue( TRUE );
    item13->Add( item14, 0, wxALIGN_CENTRE|wxALL, 5 );

    item13->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxCheckBox *item15 = new wxCheckBox( parent, ID_C_INTERLACE, "Interlaced", wxDefaultPosition, wxDefaultSize, 0 );
    item15->SetValue( TRUE );
    item13->Add( item15, 0, wxALIGN_CENTRE|wxALL, 5 );

    item1->Add( item13, 0, wxALIGN_CENTRE|wxLEFT|wxRIGHT|wxTOP, 5 );

    item0->Add( item1, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxBoxSizer *item16 = new wxBoxSizer( wxHORIZONTAL );

    wxButton *item17 = new wxButton( parent, ID_B_OK, "OK", wxDefaultPosition, wxDefaultSize, 0 );
    item17->SetDefault();
    item16->Add( item17, 0, wxALIGN_CENTRE|wxALL, 5 );

    item16->Add( 20, 20, 0, wxALIGN_CENTRE|wxALL, 5 );

    wxButton *item18 = new wxButton( parent, ID_B_CANCEL, "Cancel", wxDefaultPosition, wxDefaultSize, 0 );
    item16->Add( item18, 0, wxALIGN_CENTRE|wxALL, 5 );

    item0->Add( item16, 0, wxALIGN_CENTRE|wxALL, 5 );

    if (set_sizer)
    {
        parent->SetAutoLayout( TRUE );
        parent->SetSizer( item0 );
        if (call_fit)
        {
            item0->Fit( parent );
            item0->SetSizeHints( parent );
        }
    }

    return item0;
}

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.21  2004/05/21 21:41:39  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.20  2004/03/15 17:30:06  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.19  2004/02/19 17:04:50  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.18  2003/12/04 15:49:39  thiessen
* fix stereo and PNG export problems on Mac
*
* Revision 1.17  2003/11/15 16:08:35  thiessen
* add stereo
*
* Revision 1.16  2003/03/13 14:26:18  thiessen
* add file_messaging module; split cn3d_main_wxwin into cn3d_app, cn3d_glcanvas, structure_window, cn3d_tools
*
* Revision 1.15  2003/02/03 19:20:03  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.14  2002/10/21 15:11:29  thiessen
* don't share lists in wxGTK build
*
* Revision 1.13  2002/10/18 20:33:54  thiessen
* workaround for linux/Mesa bug
*
* Revision 1.12  2002/10/18 15:42:59  thiessen
* work around png header issue for linux
*
* Revision 1.11  2002/10/11 17:21:39  thiessen
* initial Mac OSX build
*
* Revision 1.10  2002/08/15 22:13:14  thiessen
* update for wx2.3.2+ only; add structure pick dialog; fix MultitextDialog bug
*
* Revision 1.9  2002/04/21 12:21:21  thiessen
* minor fixes for AIX
*
* Revision 1.8  2001/10/25 17:16:43  thiessen
* add PNG output to Mac version
*
* Revision 1.7  2001/10/25 14:19:44  thiessen
* move png.h to top
*
* Revision 1.6  2001/10/25 00:06:29  thiessen
* fix concurrent rendering problem in wxMSW PNG output
*
* Revision 1.5  2001/10/24 22:02:02  thiessen
* fix wxGTK concurrent rendering problem
*
* Revision 1.4  2001/10/24 17:07:30  thiessen
* add PNG output for wxGTK
*
* Revision 1.3  2001/10/24 11:25:20  thiessen
* fix wxString problem
*
* Revision 1.2  2001/10/23 20:10:23  thiessen
* fix scaling of fonts in high-res PNG output
*
* Revision 1.1  2001/10/23 13:53:38  thiessen
* add PNG export
*
*/
