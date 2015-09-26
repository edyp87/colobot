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

#pragma once

#include "common/event.h"
#include "common/error.h"

#include "object/auto/auto.h"
#include "object/object_type.h"

class COldObject;
class CLevelParserLine;

class CAutoDelegate : public CAuto
{
public:
    //TODO: COldObject is being passed only for CAuto but we need CAuto interface also.
    //      Need to extract this interface to CInteractiveObject or sth.
    CAutoDelegate(COldObject* oldObject, std::unique_ptr<CAuto> object);
    ~CAutoDelegate();

    void    DeleteObject(bool bAll=false) override;

    void    Init() override;
    void    Start(int param) override;
    bool    EventProcess(const Event &event) override;
    Error   IsEnded() override;
    bool    Abort() override;

    Error   StartAction(int param) override;

    bool    SetType(ObjectType type) override;
    bool    SetValue(int rank, float value) override;
    bool    SetString(char *string) override;

    bool    CreateInterface(bool bSelect) override;
    Error   GetError() override;

    bool    GetBusy() override;
    void    SetBusy(bool bBuse) override;
    void    InitProgressTotal(float total) override;
    void    EventProgress(float rTime) override;

    bool    GetMotor() override;
    void    SetMotor(bool bMotor) override;

    bool    Write(CLevelParserLine* line) override;
    bool    Read(CLevelParserLine* line) override;

private:
    std::unique_ptr<CAuto> m_object;
};
