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
#include "common/misc.h"
#include "common/restext.h"

#include "graphics/engine/engine.h"

#include "ui/controls/control.h"

#include <string>


namespace Ui
{

class CTarget : public CControl
{
public:
    CTarget();
    ~CTarget();

    bool        Create(Math::Point pos, Math::Point dim, int icon, EventType eventMsg) override;

    bool        EventProcess(const Event &event) override;
    void        Draw() override;
    bool        GetTooltip(Math::Point pos, std::string &name) override;

protected:
    CObject*    DetectFriendObject(Math::Point pos);
};


}
