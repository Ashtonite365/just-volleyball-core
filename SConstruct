#!/usr/bin/env python



import os
env = SConscript("thirdparty/godot-cpp/SConstruct")
env.Append(CPPPATH=["src"])
if env["platform"] == "windows" and env.get("use_mingw"):
    env.Append(LINKFLAGS=["-static", "-static-libgcc", "-static-libstdc++"])
if env["platform"] == "linux":
    env.Append(LINKFLAGS=["-Wl,--no-undefined"])

common = sorted(str(f) for f in Glob("src/gate/*.cpp")) + ["src/common/jvm_common.cpp", "src/common/jv_prof.cpp"]
libs = {
    "jvm_core": Glob("src/core/*.cpp") + Glob("src/api/*.cpp"),



    "jvm_loader": Glob("src/textures/*.cpp") + Glob("src/models/*.cpp") + Glob("src/loader/*.cpp"),
    "jvm_shots": Glob("src/shots/*.cpp"),
    "jvm_goofy": Glob("src/goofy/*.cpp"),
}
out = []
for name, srcs in libs.items():
    e = env.Clone()
    if name == "jvm_core" and ARGUMENTS.get("jv_dev", "no") == "yes":
        e.Append(CPPDEFINES=["JVM_DEV_BUILD"])
    objs = [e.SharedObject("build/%s/%s/%s" % (env["platform"], name, os.path.basename(str(s)).replace(".cpp", "")), s) for s in list(srcs) + common]
    lib = e.SharedLibrary("bin/{}/{}{}{}".format(env["platform"], name, env["suffix"], env["SHLIBSUFFIX"]), objs)
    out.append(lib)
Default(out)
