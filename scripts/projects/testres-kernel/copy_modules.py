#!/opt/python-2.7/bin/python
import os
import shutil
import sys

MODULES_PATH = "src/internal/cppcore/testres/modules"
MODULES_DEST_PATH = "modules"
GENERATE_CONFIG = "generate_config"
DEFAULT_RUN = "default_run"

try:
    src_dir = sys.argv[1]
    bin_dir = sys.argv[2]
    install_dir = sys.argv[3]

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

            if (os.path.exists(gen_conf_path_bin) and os.path.exists(def_run_path_bin)):
                gen_conf_path = gen_conf_path_bin
                def_run_path = def_run_path_bin
            else:
                if os.path.exists(gen_conf_path_src) and os.path.exists(def_run_path_src):
                    gen_conf_path = gen_conf_path_src
                    def_run_path = def_run_path_src

                    #delete Id svn keyword
                    for fname in [gen_conf_path, gen_conf_path_src]:
                        result = []
                        modified = False

                        with open(fname) as f:
                            for line in f:
                                if "$Id" not in line:
                                    result.append(line)
                                else:
                                    modified = True

                        if modified:
                            with open(fname, "w") as f:
                                f.writelines(result)

            if gen_conf_path and def_run_path:
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
