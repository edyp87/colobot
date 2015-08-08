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

#include "common/thread/sdl_cond_wrapper.h"
#include "common/thread/sdl_mutex_wrapper.h"

class CSignal
{
public:
    CSignal(CSDLMutexWrapper* mutex)
        : m_mutex(mutex)
    {
        m_cond = MakeUnique<CSDLCondWrapper>();
        m_condition = false;
    }

    ~CSignal()
    {}

    //! Waits for signal. Assumes the mutex is locked
    void Wait()
    {
        while(!m_condition)
            SDL_CondWait(**m_cond, **m_mutex);
        m_condition = false;
    }

    //! Signals the other thread
    void Signal()
    {
        m_condition = true;
        SDL_CondSignal(**m_cond);
    }

    //! Clears the condition flag
    void Clear()
    {
        m_condition = false;
    }

private:
    CSDLMutexWrapper* m_mutex;
    std::unique_ptr<CSDLCondWrapper> m_cond;
    bool m_condition;
};
