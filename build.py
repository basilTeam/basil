#!/usr/bin/env python

# This is a hand-written Python script that manages Basil's build system. It should be
# compatible with Python 2.7 and all versions of Python 3. We're using it to avoid
# any niche build-system dependencies (Python is pretty common) while still having greater
# or easier configurability than something like GNU Make.
#
# Run ./build.py --help for usage info!

#######################################################################
#                       - Platform Detection -                        #
#                                                                     #
# Populate a list of supported tools and figure out which one to use. #
#######################################################################

def cmd_exists(cmd): # see if a given command is available on the system PATH
    from distutils.spawn import find_executable
    return find_executable(cmd) is not None

# First, we'll find out what OS we're on.

import platform
OS = platform.system().strip()
if OS.startswith("MINGW"): OS = "MinGW"
if OS.startswith("MSYS"): OS = "MSYS"

# Next, we'll assemble a list of supported compilers across all platforms, and choose
# the first one that's available.

SUPPORTED_COMPILERS = []

SUPPORTED_COMPILERS.append("clang++") # default to clang
for i in range(13, 4, -1): SUPPORTED_COMPILERS.append("clang++-" + str(i)) # clang++ 5 and higher support C++17

SUPPORTED_COMPILERS.append("g++")
for i in range(11, 6, -1): SUPPORTED_COMPILERS.append("g++-" + str(i))  # g++ 7 and higher support C++17 to the degree we need

SUPPORTED_COMPILERS.append("i++") # intel C compiler

SUPPORTED_COMPILERS.append("cl") # msvc toolchain

CXX = None
for cxx in SUPPORTED_COMPILERS:
    if cmd_exists(cxx):
        CXX = cxx
        break

# We'll do likewise for linkers.

SUPPORTED_LINKERS = ["ld", "link"]
LD = None
for ld in SUPPORTED_LINKERS:
    if cmd_exists(ld):
        LD = ld
        break

# ...and static library tools.

SUPPORTED_ARCHIVERS = ["ar", "lib"]
AR = None
for ar in SUPPORTED_ARCHIVERS:
    if cmd_exists(ar):
        AR = ar
        break;

# Finally, we'll parse the program arguments, overriding our defaults if specified.

PRODUCTS = ["basil-release", "basil-debug", "librt-static", "librt-dynamic", "jasmine-release", "jasmine-debug", "test"]

import sys
import argparse
parser = argparse.ArgumentParser(description="Build an artifact from the Basil or Jasmine projects.")
parser.add_argument("-v", "--verbose", action='store_true', help="Enable additional output from the build script.")
parser.add_argument("--cxx", default=CXX, help="Override the default C++ compiler.")
parser.add_argument("--ld", default=LD, help="Override the default linker.")
parser.add_argument("--clean", action='store_true', help="Scrub all compiler artifacts from the current directory before building.")
parser.add_argument("--cxxflags", default="", help="Define additional C++ compiler flags to use.")
parser.add_argument("--ldflags", default="", help="Define additional C++ linker flags to use.")
parser.add_argument("target", choices=PRODUCTS, default="basil-debug", metavar="target", nargs='?', help="Specifies the desired product you'd like to build. (Options are " + ", ".join(PRODUCTS) + ")")
args = parser.parse_args(sys.argv[1:])

CXX = args.cxx

# Try and determine the overall compiler family from the executable name. 

if "clang" in CXX:
    CXXTYPE = "clang"
elif "g++" in CXX:
    CXXTYPE = "gcc"
elif "icpc" in CXX or "icpx" in CXX:
    CXXTYPE = "intel"
elif "cl" in CXX:
    CXXTYPE = "msvc"
else:
    print("Couldn't determine C++ compiler family for '" + CXX + "': supported compilers are Clang, GCC, and MSVC.")

LD = args.ld
VERBOSE = args.verbose
TARGET = args.target
CLEAN = args.clean

if not CXX:
    print("No valid C++ compiler could be automatically detected! Consider explicitly specifying one using '--cxx'.")
    sys.exit(1)
if not cmd_exists(CXX):
    print("Could not resolve C++ compiler '" + CXX + "'.")
    sys.exit(1)

if not LD:
    print("No valid linker could be automatically detected! Consider explicitly specifying one using '--ld'.")
    sys.exit(1)
if not cmd_exists(LD):
    print("Could not resolve linker '" + LD + "'.")
    sys.exit(1)

if VERBOSE: print("Detected OS '" + OS + "'.")
if VERBOSE: print("Using C++ compiler '" + CXX + "' in compiler family '" + CXXTYPE + "'.")
if VERBOSE: print("Using linker '" + LD + "'.")

####################################################################################
#                                - Select Process -                                #
#                                                                                  #
# Choose compiler flags, linker flags, and commands to run based on the toolchain. #
####################################################################################

CXXFLAGS = []
LDFLAGS = []
LDLIBS = []

if CXXTYPE in {"clang", "gcc", "intel"}:
    CXXFLAGS += [
        "-I.", "-Iutil", "-Ijasmine", "-Icompiler", "-Itest",
        "-std=c++17", 
        "-ffunction-sections", "-fdata-sections", "-ffast-math", "-fno-rtti", "-finline-functions",
        "-fPIC", "-fomit-frame-pointer", "-fmerge-all-constants", "-fno-exceptions", "-fno-threadsafe-statics",
        "-Wall", "-Wno-unused", "-Wno-comment"
    ]
    if OS == "Linux":
        if "librt" not in TARGET: CXXFLAGS.append("-DINCLUDE_UTF8_LOOKUP_TABLE")
        LDFLAGS += ["-Wl,--gc-sections"]
        LDLIBS += ["-lc"]
        if CXXTYPE == "intel": LDLIBS += ["-limf", "-lirc"]
    elif OS == "Darwin":
        if "librt" not in TARGET: CXXFLAGS.append("-DINCLUDE_UTF8_LOOKUP_TABLE")
        LDFLAGS += ["-nodefaultlibs", "-Wl,--gc-sections"]
        LDLIBS += ["-lc"]
        if CXXTYPE == "intel": LDLIBS += ["-limf", "-lirc"]
    if "release" in TARGET:
        CXXFLAGS += ["-fno-unwind-tables", "-fno-asynchronous-unwind-tables", "-Os", "-DBASIL_RELEASE"]
    elif "debug" in TARGET:
        CXXFLAGS += ["-g3", "-O0"]
    elif "librt" in TARGET:
        CXXFLAGS += ["-nostdlib", "-fno-builtin", "-fno-unwind-tables", "-fno-asynchronous-unwind-tables", "-Os", "-DBASIL_RELEASE"]
    if TARGET == "librt-dynamic":
        LDFLAGS.append("-shared")
elif CXXTYPE == "msvc":
    CXXFLAGS += [
        "/I .", "/I util", "/I jasmine", "/I compiler", "/I test",
        "/std:c++17", "/Gw", "/GL", 
        "/W0"
    ]
    if "release" in TARGET:
        CXXFLAGS += ["/Os", "/Oy", "/O1", "/DBASIL_RELEASE"]
    elif "debug" in TARGET:
        CXXFLAGS += ["/Z7", "/Od"]
    if TARGET == "librt-dynamic":
        LDFLAGS.append("/Ld")

CXXFLAG_STRING = " ".join(CXXFLAGS) + " " + args.cxxflags
LDFLAG_STRING = " ".join(LDFLAGS) + " " + args.ldflags
LDLIB_STRING = " ".join(LDLIBS)

def cxx_compile_object_cmd(src, obj):
    if CXXTYPE in {"clang", "gcc", "intel"}: DEST = "-c " + src + " -o " + obj
    elif CXXTYPE == "msvc": DEST = "/c " + src + " /Fo" + obj
    return " ".join(['"' + CXX + '"', CXXFLAG_STRING, DEST])

def cxx_compile_all_cmd(inputs, product):
    inputs_string = " ".join(inputs)
    if CXXTYPE in {"clang", "gcc", "intel"}: DEST = "-o " + product
    elif CXXTYPE == "msvc": DEST = "/Fe" + product
    return " ".join(['"' + CXX + '"', CXXFLAG_STRING, LDFLAG_STRING, inputs_string, DEST, LDLIB_STRING])

def ar_lib_cmd(objs, product):
    if AR == 'ar':
        ARFLAGS = "r " + product
    elif AR == 'lib':
        ARFLAGS = "/out" + product
    return '"' + AR + "\" " + ARFLAGS + " " + " ".join(objs)

def strip_cmd(target):
    if OS in {"Linux", "MSYS", "MinGW"}:
        strip_args = " ".join([
            "-g", 
            "-R .gnu.version",
			"-R .gnu.hash",
			"-R .note",
			"-R .note.ABI-tag",
			"-R .note.gnu.build-id",
			"-R .comment",
			"--strip-unneeded"
        ])
        return "strip " + strip_args + " " + target
    elif OS == "Darwin":
        return "strip " + target

if VERBOSE:
    print("Using the following C++ compiler flags: " + CXXFLAG_STRING)
    print("Using the following linker flags: " + LDFLAG_STRING + " " + LDLIB_STRING)

##########################################################################
#                        - Detect Changed Files -                        #
#                                                                        #
# Compose a list of project sources and detect which need to be updated. #
##########################################################################

import glob
import os
import shutil

# First, we find all sources in all source directories.

COMPILER_SRCS = glob.glob("compiler/*.cpp")
JASMINE_SRCS = glob.glob("jasmine/*.cpp")
UTIL_SRCS = glob.glob("util/*.cpp")
RUNTIME_SRCS = glob.glob("runtime/*.cpp")

# We compute object file names for each C++ source.

OBJ_EXT = ".obj" if CXXTYPE == "msvc" else ".o"

COMPILER_OBJS = {src : os.path.splitext(src)[0] + OBJ_EXT for src in COMPILER_SRCS}
JASMINE_OBJS = {src : os.path.splitext(src)[0] + OBJ_EXT for src in JASMINE_SRCS}
UTIL_OBJS = {src : os.path.splitext(src)[0] + OBJ_EXT for src in UTIL_SRCS}
RUNTIME_OBJS = {src : os.path.splitext(src)[0] + OBJ_EXT for src in RUNTIME_SRCS}

# This table defines our desired target pattern based on the selected target and our
# current operating system.

PRODUCTS_BY_TARGET = {
    "librt-static": {
        "Windows": "bin\\librt.lib", 
        "Linux": "bin/librt.a", 
        "Darwin": "bin/librt.a", 
        "MSYS": "bin/librt.a",
        "MinGW": "bin/librt.a"
    }[OS],
    "librt-dynamic": {
        "Windows": "bin\\librt.dll",
        "Linux": "bin/librt.so",
        "Darwin": "bin/librt.dylib",
        "MSYS": "bin/librt.dll",
        "MinGW": "bin/librt.dll"
    }[OS],
    "jasmine-debug": {
        "Windows": "bin\\jasmine.exe",
        "Linux": "bin/jasmine",
        "Darwin": "bin/jasmine",
        "MSYS": "bin/jasmine.exe",
        "MinGW": "bin/jasmine.exe"
    }[OS],
    "jasmine-release": {
        "Windows": "bin\\jasmine.exe",
        "Linux": "bin/jasmine",
        "Darwin": "bin/jasmine",
        "MSYS": "bin/jasmine.exe",
        "MinGW": "bin/jasmine.exe"
    }[OS],
    "basil-debug": {
        "Windows": "bin\\basil.exe",
        "Linux": "bin/basil",
        "Darwin": "bin/basil",
        "MSYS": "bin/basil.exe",
        "MinGW": "bin/basil.exe"
    }[OS],
    "basil-release": {
        "Windows": "bin\\basil.exe",
        "Linux": "bin/basil",
        "Darwin": "bin/basil",
        "MSYS": "bin/basil.exe",
        "MinGW": "bin/basil.exe"
    }[OS],
    "test": "" # tests use different targets
}

TEST_PRODUCTS = [os.path.splitext(test)[0] + ".test" for test in glob.glob("test/**/*.cpp")]

# This table defines which objects we need to build our desired target.

if "basil" not in TARGET:
    if "compiler/main.cpp" in COMPILER_OBJS: del COMPILER_OBJS["compiler/main.cpp"]
    if "compiler\\main.cpp" in COMPILER_OBJS: del COMPILER_OBJS["compiler\\main.cpp"]

if "jasmine" not in TARGET: 
    if "jasmine/main.cpp" in JASMINE_OBJS: del JASMINE_OBJS["jasmine/main.cpp"]
    if "jasmine\\main.cpp" in JASMINE_OBJS: del JASMINE_OBJS["jasmine\\main.cpp"]

OBJS_BY_TARGET = {
    "librt-static": [RUNTIME_OBJS],
    "librt-dynamic": [RUNTIME_OBJS],
    "jasmine-debug": [JASMINE_OBJS, UTIL_OBJS],
    "jasmine-release": [JASMINE_SRCS, UTIL_OBJS],
    "basil-debug": [COMPILER_OBJS, JASMINE_OBJS, RUNTIME_OBJS, UTIL_OBJS],
    "basil-release": [COMPILER_OBJS, JASMINE_OBJS, RUNTIME_OBJS, UTIL_OBJS],
    "test": [COMPILER_OBJS, JASMINE_OBJS, RUNTIME_OBJS, UTIL_OBJS]
}

# Now that we've computed all the object and target names, let's clean up any existing ones
# if specified in the arguments.

if CLEAN:
    for obj in COMPILER_OBJS.values(): 
        if os.path.exists(obj): 
            if VERBOSE: print("Removing '" + obj + "'.")
            os.remove(obj)
    for obj in JASMINE_OBJS.values(): 
        if os.path.exists(obj): 
            if VERBOSE: print("Removing '" + obj + "'.")
            os.remove(obj)
    for obj in RUNTIME_OBJS.values(): 
        if os.path.exists(obj): 
            if VERBOSE: print("Removing '" + obj + "'.")
            os.remove(obj)
    for obj in UTIL_OBJS.values(): 
        if os.path.exists(obj): 
            if VERBOSE: print("Removing '" + obj + "'.")
            os.remove(obj)
    for test in TEST_PRODUCTS: 
        if os.path.exists(test): 
            if VERBOSE: print("Removing '" + test + "'.")
            os.remove(test)
    shutil.rmtree("bin/")

# We determine which objects are needed by our target...

OBJS = {}
for srcpack in OBJS_BY_TARGET[TARGET]: 
    OBJS.update(srcpack)

# ...and of those, which need to be recompiled, either due to a newer source file or missing
# object file.

RECOMPILED_OBJS = { src: OBJS[src] for src in OBJS if not os.path.exists(OBJS[src]) or os.path.getmtime(src) > os.path.getmtime(OBJS[src]) }
if VERBOSE and "release" not in TARGET:
    for src in RECOMPILED_OBJS:
        if not os.path.exists(OBJS[src]):
            reason = "artifact '" + OBJS[src] + "' does not exist."
        else:
            reason = "source file is newer than object file."
        print("Recompiling '" + src + "': " + reason)

##########################################################
#              - Create and Run Task List -              #
#                                                        #
# Assemble a list of compilation tasks and execute them. #
##########################################################

if not os.path.exists("bin"):
    os.mkdir("bin")
if os.path.isfile("bin"):
    print("Cannot create output directory './bin/' - a file already exists with that name.")
    sys.exit(1)

TASKS = []
PRODUCT = PRODUCTS_BY_TARGET[TARGET]

if TARGET in {"librt-static", "librt-dynamic", "basil-debug", "jasmine-debug", "test"}:
    for src in RECOMPILED_OBJS:
        TASKS.append(cxx_compile_object_cmd(src, RECOMPILED_OBJS[src]))

if TARGET in {"basil-debug", "jasmine-debug", "librt-dynamic"}:
    if not os.path.exists(PRODUCT) or len(TASKS):  # we only need to re-link if we recompiled any of the sub-objects
        TASKS.append(cxx_compile_all_cmd(OBJS.values(), PRODUCT))

if TARGET in {"basil-release", "jasmine-release"}:
    TASKS.append(cxx_compile_all_cmd(OBJS.keys(), PRODUCT))
    TASKS.append(strip_cmd(PRODUCT))

if TARGET == "librt-static":
    if os.path.exists(PRODUCT): os.remove(PRODUCT)
    TASKS.append(ar_lib_cmd(OBJS.values(), PRODUCT))

if TARGET in {"test"}:
    for test_exec in TEST_PRODUCTS:
        srcs = {os.path.splitext(test_exec)[0] + ".cpp", "test/test.cpp"}
        srcs = srcs.union(OBJS.values())
        if not os.path.exists(test_exec) or len(RECOMPILED_OBJS) > 0: 
            TASKS.append(cxx_compile_all_cmd(srcs, test_exec))
    for test_exec in TEST_PRODUCTS:
        TASKS.append(test_exec)

errors = 0
for task in TASKS:
    if VERBOSE: print(task)
    if task: result = os.system(task)
    if task and result: errors += result

if errors == 0:
    if VERBOSE: print("Successfully built '" + PRODUCT + "'.")
else:
    sys.exit(1)