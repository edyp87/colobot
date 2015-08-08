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

#include "common/perf_counters.h"

#include "app/system.h"

CPerformanceCounters::CPerformanceCounters(CSystemUtils* systemUtils)
    : m_systemUtils(systemUtils),
      m_performanceCounters(),
      m_performanceCountersData()
{
    for (int i = 0; i < PCNT_MAX; ++i)
    {
        m_performanceCounters[i][0] = m_systemUtils->CreateTimeStamp();
        m_performanceCounters[i][1] = m_systemUtils->CreateTimeStamp();
    }
}

CPerformanceCounters::~CPerformanceCounters()
{
    for (int i = 0; i < PCNT_MAX; ++i)
    {
        m_systemUtils->DestroyTimeStamp(m_performanceCounters[i][0]);
        m_systemUtils->DestroyTimeStamp(m_performanceCounters[i][1]);
    }
}

void CPerformanceCounters::StartCounter(PerformanceCounter counter)
{
    m_systemUtils->GetCurrentTimeStamp(m_performanceCounters[counter][0]);
}

void CPerformanceCounters::StopCounter(PerformanceCounter counter)
{
    m_systemUtils->GetCurrentTimeStamp(m_performanceCounters[counter][1]);
}

float CPerformanceCounters::GetCounterData(PerformanceCounter counter) const
{
    return m_performanceCountersData[counter];
}

void CPerformanceCounters::ResetCounter(PerformanceCounter counter)
{
    StartCounter(counter);
    StopCounter(counter);
}

void CPerformanceCounters::UpdateCounterData(PerformanceCounter counter)
{
    // TODO: Make those fractions again?
    long long diff = m_systemUtils->TimeStampExactDiff(m_performanceCounters[counter][0],
                                                       m_performanceCounters[counter][1]);

    m_performanceCountersData[counter] = static_cast<float>(diff) / 1e6f;
}
