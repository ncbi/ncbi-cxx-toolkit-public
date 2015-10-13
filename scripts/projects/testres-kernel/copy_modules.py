#!/opt/python-2.7/bin/python
import os
import shutil
import sys
import re

MODULES_PATH = "src/internal/cppcore/testres/modules"
MODULES_DEST_PATH = "modules"
GENERATE_CONFIG = "generate_config"
DEFAULT_RUN = "default_run"

try:
    src_dir = sys.argv[1]
    bin_dir = sys.argv[2]
    install_dir = sys.argv[3]

    with open("/home/zakharov/pr_args.txt", "w") as f:
        f.writelines([src_dir, "\n", bin_dir, "\n", install_dir])

    #locate all modules
    modules_dir = os.path.join(src_dir, MODULES_PATH)
    modules_dest_dir = os.path.join(install_dir, MODULES_DEST_PATH)

    for dir_entry in os.listdir(modules_dir):
        module_src_dir = os.path.join(modules_dir, dir_entry)
        if os.path.isdir(module_src_dir):
            gen_conf_path_bin = os.path.join(
                bin_dir,
                dir_entry + "_" + GENERATE_CONFIG)
            def_run_path_bin = os.path.join(
                bin_dir,
                dir_entry + "_" + DEFAULT_RUN)

            # modules may still reside in the src directory (if non-compilable)
            gen_conf_path_src = os.path.join(
                module_src_dir,
                dir_entry + "_" + GENERATE_CONFIG)
            def_run_path_src = os.path.join(
                module_src_dir,
                dir_entry + "_" + DEFAULT_RUN)

            gen_conf_path = None
            def_run_path = None
            mod_dir_path = None         # TestRes module support directory to be copied into the release

            if (os.path.exists(gen_conf_path_bin) and os.path.exists(def_run_path_bin)):
                # taking modules from binary
                gen_conf_path = gen_conf_path_bin
                def_run_path = def_run_path_bin
                mod_dir_path = module_src_dir
            else:
                # modules in src
                if os.path.exists(gen_conf_path_src) and os.path.exists(def_run_path_src):
                    gen_conf_path = gen_conf_path_src
                    def_run_path = def_run_path_src
                    mod_dir_path = module_src_dir

                    #delete Id svn keyword
                    for root, dirs, files in os.walk(mod_dir_path):
                        for fname in files:
                            fpath = os.path.join(root, fname)
                            result = []
                            modified = False

                            with open(fpath) as f:
                                for line in f:
                                    if "$Id" not in line:
                                        result.append(line)
                                    else:
                                        modified = True

                            if modified:
                                with open(fname, "w") as f:
                                    f.writelines(result)

            if gen_conf_path and def_run_path:
                if mod_dir_path:
                    re_fileskip = re.compile(r"(Makefile)|\.((cpp)|(hpp)|(in)|(app)|(lib))$")
                    # filtered shutil.copytree
                    for root, dirs, files in os.walk(mod_dir_path):
                        for directory in dir:
                            os.makedirs(os.path.join(root, directory))
                        file_pathes = [x for x in files if not re_fileskip.match(x)]
                    shutil.copytree(mod_dir_path, module_dest_dir)

                module_dest_dir = os.path.join(modules_dest_dir, dir_entry)
                os.makedirs(module_dest_dir)
                gen_conf_dest_path = os.path.join(
                    module_dest_dir,
                    GENERATE_CONFIG)
                def_run_dest_path = os.path.join(module_dest_dir, DEFAULT_RUN)
                shutil.copy(gen_conf_path, gen_conf_dest_path)
                shutil.copy(def_run_path, def_run_dest_path)
except Exception as excpt:
    print excpt
    exit(1)

exit(0)
