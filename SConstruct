import sys, os
opts = Options('config.py')

# default to using sdl-config if not on windows
opts.Add('SDL_INCLUDE_PATH', 'include path for SDL', '')
opts.Add('SDL_LIB_PATH', 'lib path for SDL', '')
opts.Add('SAMPLERATE_LIB_PATH', 'include path for boost', '')
opts.Add('SAMPLERATE_INCLUDE_PATH', 'include path for boost', '')

if sys.platform == 'win32':
    env = Environment(options = opts, ENV=os.environ)
else:
    env = Environment(options = opts)

env.Append(CPPPATH="#include")

ApiVer = '0.0.1'

if sys.platform != "win32":
    env.ParseConfig('pkg-config --cflags sdl samplerate adplug')
    env.Append(CCFLAGS=["-Wall", "-Wextra", "-pedantic", "-O3", "-Wno-long-long", "-Wno-variadic-macros", "-fPIC", "-Werror"])
    env.Append(SHLINKFLAGS="-Wl,-soname=libeastwood.so.%d" % int(ApiVer.split(".")[0]))
    env.Append(CCFLAGS=["-ggdb"])

else:
    env.Append(LIBS = ["SDLmain"])
    env.Append(LINKFLAGS = ["/SUBSYSTEM:CONSOLE", "/DEBUG"])
    env.Append(CCFLAGS = ["/O2", "/EHsc", "/MD", "/DEBUG", "/Zi", "/GR"])

    env.Append(CPPPATH = [ "${SDL_INCLUDE_PATH}",
        "${SAMPLERATE_INCLUDE_PATH}"])
      
    env.Append(LIBPATH = [ "${SDL_LIB_PATH}",
        "${SAMPLERATE_LIB_PATH}"])

env.Append(LIBS = [ "SDL",
                        "samplerate",
                        ])



Export('env')

SConscript('src/SConscript')

major, minor, micro = [int(c) for c in ApiVer.split('.')]
os.system('mv libeastwood.so libeastwood.so.%s' % ApiVer)
os.system('ln -s libeastwood.so.%s libeastwood.so.%d' % (ApiVer, major))
os.system('ln -s libeastwood.so.%s libeastwood.so' % ApiVer)

