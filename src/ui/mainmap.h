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

#include "graphics/core/color.h"

#include "math/point.h"

#include "ui/controls/interface.h"


namespace Ui
{

class CMainMap
{
public:
    CMainMap();
    ~CMainMap();

    void        UpdateMap();
    void        CreateMap();
    void        SetFixImage(const char *filename);
    void        FloorColorMap(Gfx::Color floor, Gfx::Color water);
    void        ShowMap(bool bShow);
    void        DimMap();
    float       GetZoomMap();
    void        ZoomMap(float zoom);
    void        ZoomMap();
    void        MapEnable(bool bEnable);
    bool        GetFixImage();
    CObject*    DetectMap(Math::Point pos, bool &bInMap);
    void        SetHighlight(CObject* pObj);
    void        SetToy(bool bToy);
    void        SetFixParam(float zoom, float ox, float oy, float angle, int mode, bool bDebug);

protected:
    void        CenterMap();

protected:
    CEventQueue*    m_event;
    Gfx::CEngine*   m_engine;
    CInterface*     m_interface;

    bool            m_bFixImage;
};

}
