## C++ CGI application sample

| [GIT FCGI](https://bitbucket.ncbi.nlm.nih.gov/projects/CXX/repos/cxx-fast-cgi-sample/browse) | [GIT CGI](https://bitbucket.ncbi.nlm.nih.gov/projects/CXX/repos/cxx-cgi-sample/browse) | SVN FCGI | [SVN CGI](https://svn.ncbi.nlm.nih.gov/viewvc/toolkit/trunk/c%2B%2B/src/sample/app/deployable_cgi/) |

This is C++ CGI Application sample which is buildable and deployable using export\_project and TeamCity.

See proper documentation on build process [here](https://confluence.ncbi.nlm.nih.gov/display/CT/Build+Framework).

## Build in TeamCity:
See "[Build CGI Sample SVN](https://teamcity.ncbi.nlm.nih.gov/viewType.html?buildTypeId=CXX_CToolkitProductsCIDemo_ExportProject_BuildCgiSampleSvn)" in TeamCity.

* Base build configuraiton on [CxxBuildTemplateSvn](https://teamcity.ncbi.nlm.nih.gov/admin/editBuild.html?id=template:CxxBuildTemplateSvn);
* Add Toolkit dependency via another templatge: `CXX Dependency: Pre-Built-<Something>`. [Example Metastable](https://teamcity.ncbi.nlm.nih.gov/admin/editBuild.html?id=template:CxxDependencyPreBuiltMetastable);
* Set SVN\_BASE\_DIR to either `%SVN_BASE_DIR_TRUNK_INTERNAL%` (the default) or `%SVN_BASE_DIR_TRUNK_PUBLIC%`;
* After adding VCS Root modify checkout rule to be `%VCS_CHECKOUT_RULES%`;
* In TeamCity properties update `PROJECT` to be your `<project-name>`.

## Quality
See "[Sample SVN CGI report in sonarqube]()".

If you want code coverage report you must create separate TeamCity configuration to avoid leaking coverage-enabled binaries to PRODs.

The following TeamCity parameters affect quality check:

* COVERAGE - enables coverage report by unit tests;
* RUN\_UNIT\_TESTS - runs unit tests;
* RUN\_CLANG\_STATIC - runs clang static analyzer on your code;
* RUN\_VALGRIND - runs valgrind memcheck on your code;
* RATS\_REPORT - runs "Rough Auditing Tool for Security" on your code;
* SONAR\_PUBLISH - this enables publishing quality check results to sonarqube.

## Deploying
See "[Deploy SVN CGI sample]()" in TeamCity.

* Create a project based on the template "[Deploy Project](https://teamcity.ncbi.nlm.nih.gov/admin/editBuild.html?id=template:DeployProject)";
* Add snapshot and artifact dependencies to code compilation project which you created earlier;
* Artifact rules for artifact dependency `dist=>dist`;
* Make sure `env.CD_TYPE` is `nomad`;
* If necessary set `env.CD_ENTRY_POINT` to `profile://permanent/development-aws`;
* Obtain consul token from the CI/CD team;
* Only use aliases which registered with the consul token.

