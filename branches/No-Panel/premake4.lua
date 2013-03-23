-- to generate Visual Studio files in vs directory, run:
-- premake4 vs2010 or premake4 vs2008
solution "everything"
  configurations { "Debug", "Release" }
  location "vs" -- this is where generated solution/project files go

  -- Symbols - generate .pdb files
  -- StaticRuntime - statically link crt
  -- ExtraWarnings - set max compiler warnings level
  -- FatalWarnings - compiler warnigs are errors'
  flags {
   "Symbols", "StaticRuntime", "ExtraWarnings", "FatalWarnings",
   "NoRTTI", "Unicode", "NoExceptions"
  }

  -- those are inherited by projects that follow
  configuration "Debug"
    targetdir "dbg" -- this is where the .exe/.lib etc. files wil end up
    defines { "_DEBUG", "DEBUG" }

  configuration "Release"
     targetdir "rel"
     flags { "Optimize" }
     defines { "NDEBUG" }
     -- 4189 - variable not used, happens with CrashIf() macros that are no-op
     --        in release builds
     buildoptions { "/wd4189" }

  configuration {"vs*"}
    -- defines { "_WIN32", "WIN32", "WINDOWS", "_CRT_SECURE_NO_WARNINGS" }
    defines { "_WIN32", "WIN32", "WINDOWS" }
    -- 4800 - int -> bool coversion
    -- 4127 - conditional expression is constant
    -- 4100 - unreferenced formal parameter
    -- 4244 - possible loss of data due to conversion
    -- 4480 - non-standard (deriving enum from u16)
    -- 4706 - assignment within conditional expression TODO: code should be fixed instead
    -- 4505 - unused function
    buildoptions {
        "/wd4800", "/wd4127", "/wd4100", "/wd4244", "/wd4480", "/wd4706",
        "/wd4505"
    }

  project "efi"
    kind "ConsoleApp"
    language "C++"
    files {
      "tools/efi/*.h",
      "tools/efi/*.cpp",
      "src/utils/BaseUtil*",
      "src/utils/BitManip.h",
      "src/utils/Dict*",
      "src/utils/StrUtil*",
    }
    excludes
    {
      "src/utils/*_ut.cpp",
    }
    includedirs { "src/utils", "src/utils/msvc" }
    links { }

    --configuration {"vs*"}
      -- Note: don't understand why this is needed
      --linkoptions {"/NODEFAULTLIB:\"msvcrt.lib\""}

--[[
  project "utils"
    kind "StaticLib"
    language "C++"
    files { "src/utils/*.cpp", "src/utils/*.h" }
    excludes
    {
      "src/utils/*_ut.cpp",
      "src/utils/Zip*",
      "src/utils/HtmlWindow*",
      "src/utils/LzmaSimpleArchive*",
      "src/utils/Experiments*",
      "src/utils/UtilTests.cpp",
      "src/utils/Touch*",
    }
    includedirs { "src/utils", "src/utils/msvc" }
--]]


  project "sertxt_test"
    kind "ConsoleApp"
    language "C++"
    files {
      "tools/sertxt_test/*.h",
      "tools/sertxt_test/*.cpp",
      "tools/sertxt_test/*.txt",
      "src/utils/BaseUtil*",
      "src/utils/BitManip.h",
      "src/utils/Dict*",
      "src/utils/FileUtil*",
      "src/utils/StrUtil*",
    }
    excludes
    {
      "src/utils/*_ut.cpp",
    }
    includedirs { "src/utils", "src/utils/msvc" }
    links { "Shlwapi" }

  project "serini_test"
    kind "ConsoleApp"
    language "C++"
    files {
      "tools/serini_test/*",
      "tools/sertxt_test/*.h",
      "tools/sertxt_test/SettingsSumatra.cpp",
      "src/utils/BaseUtil.*",
      "src/utils/FileUtil.*",
      "src/utils/StrUtil.*",
      "src/utils/IniParser.*",
    }
    includedirs { "src/utils", "src/utils/msvc" }
    links { "Shlwapi" }
