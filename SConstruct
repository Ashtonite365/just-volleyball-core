#!/usr/bin/env python

import os
env = SConscript("thirdparty/godot-cpp/SConstruct")
env.Append(CPPPATH=["src"])
if env["platform"] == "windows" and env.get("use_mingw"):
    env.Append(LINKFLAGS=["-static", "-static-libgcc", "-static-libstdc++"])
if env["platform"] == "linux":
    env.Append(LINKFLAGS=["-Wl,--no-undefined"])

common = sorted(str(f) for f in Glob("src/gate/*.cpp")) + ["src/common/jvm_common.cpp", "src/common/jv_prof.cpp"]
e = env.Clone()
if ARGUMENTS.get("jv_dev", "no") == "yes":
    e.Append(CPPDEFINES=["JVM_DEV_BUILD"])
objs = [
    e.SharedObject(
        "build/%s/%s/%s" % (env["platform"], "jvm_core", os.path.basename(str(s)).replace(".cpp", "")),
        s,
    )
    for s in list(Glob("src/core/*.cpp")) + list(Glob("src/api/*.cpp")) + common
]
lib = e.SharedLibrary(
    "bin/{}/{}{}{}".format(env["platform"], "jvm_core", env["suffix"], env["SHLIBSUFFIX"]),
    objs,
)
Default(lib)
