Step by step instructions to build the C++ toolkit with Codewarrior on Macintosh OSX.

1. Unpack the C++ toolkit in your home directory in a folder named ncbi_cxx. If you need to name it something else or put it somewhere else you can specify that at the top of the build scripts (see below).

2. Install third party libraries:

Download and unpack the source code for the following third party libraries: FLTK (www.fltk.org), Berkeley DB (www.sleepycat.com), Sqlite (www.sqlite.org). Put them in your home directory, as siblings of the C++ toolkit directory, or inside the C++ toolkit directory.  It doesn't matter what you name them and you do not have to build these libraries.  The build scripts will find them and build them.

3. Set the macintosh file/creator types on everything:

From a command line (Terminal application) cd to the c++ toolkit home directory and type:
sh ./compilers/mac_prj/settypes.sh 

4. Run the build scripts:

Use an AppleScript editor to open and run the AppleScript files makeLibs.met (first) and makeApps.met (second).  You must use a script editor capable of opening a script file larger than 32K, such as Apple's Script Editor v2.0, or Smile.  Script Editor v1.9 will not work since makeLibs.met just got too big.  The command line tool osascript also works. 

These scripts create and populate Codewarrior projects, then tell Codewarrior to build the libraries and applications from these projects.  There are currently 128 libraries projects and 6 application projects and a number of demo application projects.

We  build all the libraries and most of the applications including the Genome Workbench (gbench).  We only build a few of the many test or demo apps.  We also build the fltk library's GUI editor, fluid.

All apps are built as application bundles except gbench_plugin_scan and datatool which are built as command line apps.  Any of the applications can be built as command line apps by tweaking the build scripts or the Codewarrior projects. 

Projects include targets to compile with BSD/Apple headers and libraries, and with MSL headers and libraries, but plugins (for gbench) built with MSL do not link properly, and so, beause of lack of interest to keep up with changes, some source code does not currently compile with MSL.  

Bottom Line: Do not compile the MSL targets, only the BSD ones.

The targets to be compiled can be controlled by including an empty file or folder in the compilers:mac_prj folder with the name 'Build' followed by the keywords of the targets you want built.  The keywords are: MSL, BSD, Debug and Final.  For example, to build only the BSD Debug targets use: "Build BSD Debug", to build both BSD debug and release (final) versions: "Build BSD".  If you have no such empty file or folder the scripts prompt for which targets to build.  You have to build libraries for a particular target first before building the applications for that target, else the applications will not link.

There are several script properties at the beginning of the AppleScripts (makeLibs.met and makeApps.met) which are meant to be edited to customize the build.

If you install the C++ toolkit under a different name than "ncbi_cxx" or in a different location than your home directory, you can edit the  script's properties, pRootFolderName and pRootFolderPath , to override these defaults.  Note that these paths, and those mentioned below, must be entered in mac format (e.g. disk:Users:username: ) not Unix format (e.g. /Users/username/ ).  The disk name and its following colon may be omitted. 

The scripts normally halt on any Codewarrior compilation errors.  If you want them to continue and save errors, set the script property, pSaveContinueOnErrors  to true.  Compilation errors for a project will be saved in a file in the same folder as the project being built, with a name in the following format: projectName-targetNumber.errs (e.g. xncbi-2.errs ). 

The scripts can be rerun and will update any existing projects, adding missing sources files and libraries.  The scripts will not delete extra files or libraries.  To start fresh, in the compilers/mac_prj directory delete the directories: lib, bin and plugins.


5. Register Genome Workbench's plugins.

The first time you run gbench or after you create new plugins you must register them. There are two ways to do this:

5.1 from the command line:

  # cd to the compilers/mac_prj subdirectory of the c++ toolkit, e.g.
  cd ~/ncbi_cxx/compilers/mac_prj
  # run the plugin registration program
  ./bin/gbench_plugin_scan ./plugins

5.2 From within gbench.

  # start the gbench program (see step 6)
  Settings->Manage Plugins->Rescan
  
  
6. Run Genome Workbench

from the command line in ~/ncbi_cxx/compilers/mac_prj/bin/
./gbench # optimised version
./gbench_D # debug version

Or open the file gbench.mcp in Codewarrior and run it under Codewarrior's debugger.


Genome Workbench stores various configuration and work files in the user's folder: ~/Library/Application Support/gbench. 

CFM builds are not supported.  OS 8 or 9 are not supported.  We know Mac OS 10.2 and 10.3 work.  We think 10.1 still works. 

[see also http://www.ncbi.nlm.nih.gov/books/bv.fcgi?call=bv.View..ShowSection&rid=toolkit.section.ad_hoc#metrowerks_codewarrior for a slightly older version of these notes.]


Author: Robert G. Smith, rsmith@ncbi.nlm.nih.gov
