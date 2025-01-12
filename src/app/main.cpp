/*
 * This file is part of the Colobot: Gold Edition source code
 * Copyright (C) 2001-2015, Daniel Roux, EPSITEC SA & TerranovaTeam
 * http://epsitec.ch; http://colobot.info; http://github.com/colobot
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see http://gnu.org/licenses
 */

/**
 * \file app/main.cpp
 * \brief Entry point of application - main() function
 */

#include "common/config.h"

#include "app/app.h"
#include "app/signal_handlers.h"
#include "app/system.h"
#if PLATFORM_WINDOWS
    #include "app/system_windows.h"
#endif

#include "common/logger.h"
#include "common/make_unique.h"
#include "common/restext.h"

#include "common/resources/resourcemanager.h"

#if PLATFORM_WINDOWS
    #include <windows.h>
#endif

#include <memory>
#include <vector>

/* Doxygen main page */

/**

\mainpage

Doxygen documentation of Colobot project

\section Intro Introduction

The source code released by Epitec was sparsely documented. This documentation, written from scratch,
will aim to describe the various components of the code.

Currently, the only documented classes are the ones written from scratch or the old ones rewritten
to match the new code.
In time, the documentation will be extended to cover every major part of the code.

\section Structure Code structure

The source code was split from the original all-in-one directory to subdirectories,
each containing one major part of the project.
The current layout is the following:
 - src/CBot - separate library with CBot language
 - src/app - class CApplication and everything concerned with SDL plus other system-dependent
   code such as displaying a message box, finding files, etc.
 - src/common - shared structs, enums, defines, etc.; should not have any external dependencies
 - src/graphics/core - abstract interface of graphics device (abstract CDevice class)
   (split from old src/graphics/common)
 - src/graphics/engine - main graphics engine based on abstract graphics device; is composed
   of CEngine class and associated classes implementing the 3D engine (split from old src/graphics/common)
 - src/graphics/opengl - concrete implementation of CDevice class in OpenGL: CGLDevice
 - src/graphics/d3d - in (far) future - perhaps a newer implementation in DirectX (9? 10?)
 - src/math - mathematical structures and functions
 - src/object - non-graphical game engine, that is robots, buildings, etc.
 - src/ui - 2D user interface (menu, buttons, check boxes, etc.)
 - src/sound - sound and music engine written using fmod library
 - src/physics - physics engine
 - src/script - link with the CBot library
*/

//! Entry point to the program
extern "C"
{

int SDL_MAIN_FUNC(int argc, char *argv[])
{
    CLogger logger; // single instance of logger

    // Workaround for character encoding in argv on Windows
    #if PLATFORM_WINDOWS
    int wargc = 0;
    wchar_t** wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (wargv == nullptr)
    {
        logger.Error("CommandLineToArgvW failed\n");
        return 1;
    }

    std::vector<std::vector<char>> windowsArgs;
    for (int i = 0; i < wargc; i++)
    {
        std::wstring warg = wargv[i];
        std::string arg = CSystemUtilsWindows::UTF8_Encode(warg);
        std::vector<char> argVec(arg.begin(), arg.end());
        argVec.push_back('\0');
        windowsArgs.push_back(std::move(argVec));
    }

    auto windowsArgvPtrs = MakeUniqueArray<char*>(wargc);
    for (int i = 0; i < wargc; i++)
        windowsArgvPtrs[i] = windowsArgs[i].data();

    argv = windowsArgvPtrs.get();

    LocalFree(wargv);
    #endif

    logger.Info("%s starting\n", COLOBOT_FULLNAME);

    auto systemUtils = CSystemUtils::Create(); // platform-specific utils
    systemUtils->Init();

    CSignalHandlers::Init(systemUtils.get());

    CResourceManager manager(argv[0]);

    // Initialize static string arrays
    InitializeRestext();
    InitializeEventTypeTexts();

    int code = 0;
    CApplication app(systemUtils.get()); // single instance of the application

    ParseArgsStatus status = app.ParseArguments(argc, argv);
    if (status == PARSE_ARGS_FAIL)
    {
        systemUtils->SystemDialog(SDT_ERROR, "COLOBOT - Fatal Error", "Invalid commandline arguments!\n");
        return app.GetExitCode();
    }
    else if (status == PARSE_ARGS_HELP)
    {
        return app.GetExitCode();
    }

    if (! app.Create())
    {
        code = app.GetExitCode();
        if (code != 0 && !app.GetErrorMessage().empty())
        {
            systemUtils->SystemDialog(SDT_ERROR, "COLOBOT - Fatal Error", app.GetErrorMessage());
        }
        logger.Info("Didn't run main loop. Exiting with code %d\n", code);
        return code;
    }

    code = app.Run();

    logger.Info("Exiting with code %d\n", code);

    return code;
}

} // extern "C"
