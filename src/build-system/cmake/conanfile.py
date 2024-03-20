from conan import ConanFile
from conan.errors import ConanInvalidConfiguration, ConanException
from conan.tools.build import check_min_cppstd
import subprocess

class NCBIToolkitWithConanRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"

    @property
    def _min_cppstd(self):
        return 17

    def configure(self):
        self.options["abseil/*"].shared = False
        self.options["grpc/*"].shared = False
        self.options["protobuf/*"].shared = False
        self.options["boost/*"].shared = False
        self.options["libxml2/*"].shared = False
        self.options["pcre/*"].shared = False
        self.options["ncbicrypt/*"].shared = False
#
        self.options["grpc/*"].cpp_plugin = True
        self.options["grpc/*"].csharp_plugin = False
        self.options["grpc/*"].node_plugin = False
        self.options["grpc/*"].objective_c_plugin = False
        self.options["grpc/*"].php_plugin = False
        self.options["grpc/*"].python_plugin = False
        self.options["grpc/*"].ruby_plugin = False
#
#boost/*:header_only = True
        self.options["boost/*"].header_only = False
#boost/*:without_atomic = True
#boost/*:without_chrono = True
#boost/*:without_container = True
#boost/*:without_context = True
        self.options["boost/*"].without_contract = True
        self.options["boost/*"].without_coroutine = True
#boost/*:without_date_time = True
#boost/*:without_exception = True
        self.options["boost/*"].without_fiber = True
#boost/*:without_filesystem = True
        self.options["boost/*"].without_graph = True
        self.options["boost/*"].without_graph_parallel = True
#boost/*:without_iostreams = True
        self.options["boost/*"].without_json = True
        self.options["boost/*"].without_locale = True
        self.options["boost/*"].without_log = True
        self.options["boost/*"].without_math = True
        self.options["boost/*"].without_mpi = True
        self.options["boost/*"].without_nowide = True
        self.options["boost/*"].without_program_options = True
        self.options["boost/*"].without_python = True
#boost/*:without_random = True
#boost/*:without_regex = True
#boost/*:without_serialization = True
        self.options["boost/*"].without_stacktrace = True
#boost/*:without_system = True
#boost/*:without_test = True
#boost/*:without_thread = True
#boost/*:without_timer = True
        self.options["boost/*"].without_type_erasure = True
        self.options["boost/*"].without_wave = True

    def requirements(self):
        res = subprocess.run(["conan", "remote", "list"], 
            stdout = subprocess.PIPE, universal_newlines = True, encoding="utf-8")
        pos = res.stdout.find("ncbi.nlm.nih.gov")
        NCBIfound = pos > 0
        if NCBIfound:
            print("NCBI artifactory is found")
        else:
            print("NCBI artifactory is not found")

        if self.settings.os == "Linux":
            self.requires("backward-cpp/1.6")
        self.requires("boost/1.82.0")
        self.requires("bzip2/1.0.8")
        if self.settings.os == "Linux":
            self.requires("cassandra-cpp-driver/2.15.3")
        self.requires("giflib/5.2.1")
        self.requires("grpc/1.50.1")
        if self.settings.os == "Linux" or NCBIfound:
            self.requires("libdb/5.3.28")
        self.requires("libjpeg/9e")
        self.requires("libnghttp2/1.54.0")
        self.requires("libpng/1.6.39")
        self.requires("libtiff/4.5.0")
        if self.settings.os == "Linux":
            self.requires("libunwind/1.8.0")
        self.requires("libuv/1.46.0")
        self.requires("libxml2/2.11.6")
        self.requires("libxslt/1.1.34")
        self.requires("lmdb/0.9.29")
        self.requires("lzo/2.10")
        self.requires("openssl/1.1.1s")
        self.requires("pcre/8.45")
        self.requires("sqlite3/3.42.0")
        self.requires("zlib/[>=1.2.11 <2]")
        self.requires("zstd/1.5.5")

        if NCBIfound:
            self.requires("ncbicrypt/20230516")
            if self.settings.os == "Linux":
                self.requires("ncbi-fastcgi/2.4.2")
            self.requires("ncbi-vdb/3.1.0")

    def validate(self):
        if self.settings.compiler.cppstd:
            check_min_cppstd(self, self._min_cppstd)
        if self.settings.os not in ["Linux", "Macos", "Windows"]:
            raise ConanInvalidConfiguration("This operating system is not supported")

