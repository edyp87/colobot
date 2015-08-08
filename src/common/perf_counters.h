/*
 * This file is part of the Colobot: Gold Edition source code
 * Copyright (C) 2001-2015, Daniel Roux, EPSITEC SA & TerranovaTeam
 * http://epsiteс.ch; http://colobot.info; http://github.com/colobot
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

class CSystemUtils;
struct SystemTimeStamp;

/**
 * \enum PerformanceCounter
 * \brief Type of counter testing performance
 */
enum PerformanceCounter
{
    // Main (render) thread

    PCNT_EVENT_RECIEVING,       //! < recieving events from SDL

    PCNT_PRERENDER,             //! < update before render
    PCNT_RENDER_ALL,            //! < the whole rendering process
    PCNT_RENDER_PARTICLE,       //! < rendering the particles in 3D
    PCNT_RENDER_WATER,          //! < rendering the water
    PCNT_RENDER_TERRAIN,        //! < rendering the terrain
    PCNT_RENDER_OBJECTS,        //! < rendering the 3D objects
    PCNT_RENDER_INTERFACE,      //! < rendering 2D interface
    PCNT_RENDER_SHADOW_MAP,     //! < rendering shadow map

    PCNT_ALL_MAIN,              //! < all counters together on main thread

    // Update thread

    PCNT_EVENT_PROCESSING,      //! < event processing (except update events)

    PCNT_UPDATE_ALL,            //! < the whole frame update process
    PCNT_UPDATE_ENGINE,         //! < frame update in CEngine
    PCNT_UPDATE_PARTICLE,       //! < frame update in CParticle
    PCNT_UPDATE_GAME,           //! < frame update in CRobotMain

    PCNT_ALL_UPDATE,            //! < all counters together on update thread

    //! Total counters
    PCNT_MAX
};

class CPerformanceCounters
{
public:
    CPerformanceCounters(CSystemUtils* systemUtils);
    ~CPerformanceCounters();

    //! Management of performance counters
    //@{
    void        StartCounter(PerformanceCounter counter);
    void        StopCounter(PerformanceCounter counter);
    float       GetCounterData(PerformanceCounter counter) const;
    //@}

    //! Resets all performance counters to zero
    void ResetCounter(PerformanceCounter counter);
    //! Updates performance counters from gathered timer data
    void UpdateCounterData(PerformanceCounter counter);

protected:
    CSystemUtils*    m_systemUtils;

    SystemTimeStamp* m_performanceCounters[PCNT_MAX][2];
    float            m_performanceCountersData[PCNT_MAX];
};
