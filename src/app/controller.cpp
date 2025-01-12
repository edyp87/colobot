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

#include "app/controller.h"


#include "common/make_unique.h"

#include "level/robotmain.h"



CController::CController(CApplication* app, bool configLoaded)
 : m_app(app)
{
    m_main = MakeUnique<CRobotMain>();
    if (configLoaded)
        m_main->LoadConfigFile();
    else
        m_main->CreateConfigFile();
}

CController::~CController()
{
}

CApplication* CController::GetApplication()
{
    return m_app;
}

CRobotMain* CController::GetRobotMain()
{
    return m_main.get();
}

void CController::StartApp()
{
    m_main->ChangePhase(PHASE_WELCOME1);
}

void CController::StartGame(LevelCategory cat, int chap, int lvl)
{
    m_main->SetLevel(cat, chap, lvl);
    m_main->ChangePhase(PHASE_SIMUL);
}

void CController::ProcessEvent(Event& event)
{
    m_main->ProcessEvent(event);
}
