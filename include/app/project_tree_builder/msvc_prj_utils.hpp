#ifndef MSVC_PRJ_UTILS_HEADER
#define MSVC_PRJ_UTILS_HEADER

#include "VisualStudioProject.hpp"

#include <corelib/ncbienv.hpp>

BEGIN_NCBI_SCOPE
//------------------------------------------------------------------------------

/// Creates CVisualStudioProject class instanse from file.
///
/// @param file_path
///   Path to file load from.
/// @return
///   Created on heap CVisualStudioProject instance or NULL
///   if failed.
CVisualStudioProject * LoadFromXmlFile(const string& file_path);


/// Save CVisualStudioProject class instanse to file.
///
/// @param file_path
///   Path to file project will be saved to.
/// @param project
///   Project to save.
void SaveToXmlFile  (const string& file_path, 
                     const CVisualStudioProject& project);

/// Generate pseudo-GUID.
string GenerateSlnGUID(void);

/// Get extension for source file without extension.
///
/// @param file_path
///   Source file full path withour extension.
/// @return
///   Extension of source file (".cpp" or ".c") 
///   if such file exist. Empty string string if there is no
///   such file.
string SourceFileExt(const string& file_path);

//------------------------------------------------------------------------------        
END_NCBI_SCOPE

#endif // MSVC_PRJ_UTILS_HEADER