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
 * \file app/system_other.h
 * \brief Fallback code for other systems
 */

#include "app/system.h"

#include <SDL.h>

#include <iostream>


struct SystemTimeStamp
{
    Uint32 sdlTicks;

    SystemTimeStamp()
    {
        sdlTicks = 0;
    }
};

class CSystemUtilsOther : public CSystemUtils
{
public:
    void Init() override;
    SystemDialogResult SystemDialog(SystemDialogType type, const std::string& title, const std::string& message) override;

    void GetCurrentTimeStamp(SystemTimeStamp *stamp) override;
    long long TimeStampExactDiff(SystemTimeStamp *before, SystemTimeStamp *after) override;

    void Usleep(int usec) override;
};

