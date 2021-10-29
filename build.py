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

SUPPORTED_COMPILERS.append("msvc++")

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

PRODUCTS = ["basil-release", "basil-debug", "rt-static", "rt-dynamic", "jasmine-release", "jasmine-debug", "test"]

import sys
import argparse
parser = argparse.ArgumentParser(description="Build an artifact from the Basil or Jasmine projects.")
parser.add_argument("-v", "--verbose", action='store_true', help="Enable additional output from the build script.")
parser.add_argument("--cxx", default=CXX, help="Override the default C++ compiler.")
parser.add_argument("--ld", default=LD, help="Override the default linker.")
parser.add_argument("--clean", action='store_true', help="Scrub all compiler artifacts from the current directory before building.")
parser.add_argument("--cxxflags", default="", help="Define additional C++ compiler flags to use.")
parser.add_argument("--ldflags", default="", help="Define additional C++ linker flags to use.")
parser.add_argument("target", choices=PRODUCTS, default="basil-debug", metavar="target", nargs='?', help="Specifies the desired product you'd like to build.")
args = parser.parse_args(sys.argv[1:])

CXX = args.cxx

# Try and determine the overall compiler family from the executable name. 

if "clang" in CXX:
    CXXTYPE = "clang"
elif "g++" in CXX:
    CXXTYPE = "gcc"
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

if CXXTYPE in {"clang", "gcc"}:
    CXXFLAGS += [
        "-I.", "-Iutil", "-Ijasmine", "-Icompiler", "-Itest",
        "-std=c++17", 
        "-ffunction-sections", "-fdata-sections", "-ffast-math", "-fno-rtti", "-finline-functions",
        "-fPIC", "-fomit-frame-pointer", "-fmerge-all-constants", "-fno-exceptions", "-fno-threadsafe-statics",
        "-Wall", "-Wno-unused", "-Wno-comment", "-DINCLUDE_UTF8_LOOKUP_TABLE" 
    ]
    if OS == "Linux":
        LDFLAGS += ["-nodefaultlibs", "-Wl,--gc-sections"]
        LDLIBS += ["-lc"]
    elif OS == "Darwin":
        LDFLAGS += ["-nodefaultlibs", "-Wl,--gc-sections"]
        LDLIBS += ["-lc"]
    if "release" in TARGET:
        CXXFLAGS += ["-fno-unwind-tables", "-fno-asynchronous-unwind-tables", "-Os", "-DBASIL_RELEASE"]
    elif "debug" in TARGET:
        CXXFLAGS += ["-g3", "-O0"]
    if TARGET == "librt-dynamic":
        LDFLAGS.append("-shared")

CXXFLAG_STRING = " ".join(CXXFLAGS) + " " + args.cxxflags
LDFLAG_STRING = " ".join(LDFLAGS) + " " + args.ldflags
LDLIB_STRING = " ".join(LDLIBS)

def cxx_compile_object_cmd(src, obj):
    return " ".join([CXX, CXXFLAG_STRING, "-c", src, "-o", obj])

def cxx_compile_all_cmd(inputs, product):
    inputs_string = " ".join(inputs)
    return " ".join([CXX, CXXFLAG_STRING, inputs_string, "-o", product])

def ar_lib_cmd(objs, product):
    if AR == 'ar':
        ARFLAGS = "r " + product
    elif AR == 'lib':
        ARFLAGS = "/out " + product
    return AR + " " + ARFLAGS + " " + " ".join(objs)

def strip_cmd(target):
    if OS == "Linux":
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

# First, we find all sources in all source directories.

COMPILER_SRCS = glob.glob("compiler/*.cpp")
JASMINE_SRCS = glob.glob("jasmine/*.cpp")
UTIL_SRCS = glob.glob("util/*.cpp")
RUNTIME_SRCS = glob.glob("runtime/*.cpp")

# We compute object file names for each C++ source.

COMPILER_OBJS = {src : os.path.splitext(src)[0] + ".o" for src in COMPILER_SRCS}
JASMINE_OBJS = {src : os.path.splitext(src)[0] + ".o" for src in JASMINE_SRCS}
UTIL_OBJS = {src : os.path.splitext(src)[0] + ".o" for src in UTIL_SRCS}
RUNTIME_OBJS = {src : os.path.splitext(src)[0] + ".o" for src in RUNTIME_SRCS}

# This table defines our desired target pattern based on the selected target and our
# current operating system.

PRODUCTS_BY_TARGET = {
    "librt-static": {
        "Windows": "bin/librt.lib", 
        "Linux": "bin/librt.a", 
        "Darwin": "bin/librt.a", 
        "MSYS": "bin/librt.a",
        "MinGW": "bin/librt.a"
    }[OS],
    "librt-dynamic": {
        "Windows": "bin/librt.dll",
        "Linux": "bin/librt.so",
        "Darwin": "bin/librt.dylib",
        "MSYS": "bin/librt.dll",
        "MinGW": "bin/librt.dll"
    }[OS],
    "jasmine-debug": "bin/jasmine",
    "jasmine-release": "bin/jasmine",
    "basil-debug": "bin/basil",
    "basil-release": "bin/basil"
}

# This table defines which objects we need to build our desired target.

if "jasmine" not in TARGET: del JASMINE_OBJS["jasmine/main.cpp"]

OBJS_BY_TARGET = {
    "librt-static": [RUNTIME_OBJS],
    "librt-dynamic": [RUNTIME_OBJS],
    "jasmine-debug": [JASMINE_OBJS, UTIL_OBJS],
    "jasmine-release": [JASMINE_SRCS, UTIL_OBJS],
    "basil-debug": [COMPILER_OBJS, JASMINE_OBJS, UTIL_OBJS],
    "basil-release": [COMPILER_OBJS, JASMINE_OBJS, UTIL_OBJS]
}

# Now that we've computed all the object and target names, let's clean up any existing ones
# if specified in the arguments.

if CLEAN:
    for src in OBJS: os.remove(OBJS[src])
    os.rmdir("bin/")

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

if TARGET in {"librt-static", "librt-dynamic", "basil-debug", "jasmine-debug"}:
    for src in RECOMPILED_OBJS:
        TASKS.append(cxx_compile_object_cmd(src, RECOMPILED_OBJS[src]))

if TARGET in {"basil-debug", "jasmine-debug", "librt-dynamic"}:
    TASKS.append(cxx_compile_all_cmd(OBJS.values(), PRODUCT))

if TARGET in {"basil-release", "jasmine-release"}:
    TASKS.append(cxx_compile_all_cmd(OBJS.keys(), PRODUCT))
    TASKS.append(strip_cmd(PRODUCT))

if TARGET == "librt-static":
    TASKS.append(ar_lib_cmd(OBJS.values(), PRODUCT))

errors = 0
for task in TASKS:
    if VERBOSE: print(task)
    errors += os.system(task)

if errors == 0:
    if VERBOSE: print("Successfully built '" + PRODUCT + "'.")
else:
    sys.exit(1)