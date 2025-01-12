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
 * \file common/key.h
 * \brief Key-related macros and enums
 */

#pragma once


#include <SDL_keysym.h>

/* Key definitions are specially defined here so that it is clear in other parts of the code
  that these are used. It is to avoid having SDL-related enum values or #defines lying around
  unchecked. With this approach it will be easier to maintain the code later on. */

// Key symbol defined as concatenation to SDLK_...
// If need arises, it can be changed to custom function or anything else
#define KEY(x) SDLK_ ## x


// Key modifier defined as concatenation to KMOD_...
// If need arises, it can be changed to custom function or anything else
#define KEY_MOD(x) KMOD_ ## x

/**
 * \enum VirtualKmod
 * \brief Virtual key codes generated on kmod presses
 *
 * These are provided here because left and right pair of keys generate different codes.
 */
enum VirtualKmod
{
    VIRTUAL_KMOD_CTRL  = SDLK_LAST + 100, //! < control (left or right)
    VIRTUAL_KMOD_SHIFT = SDLK_LAST + 101, //! < shift (left or right)
    VIRTUAL_KMOD_ALT   = SDLK_LAST + 102, //! < alt (left or right)
    VIRTUAL_KMOD_META  = SDLK_LAST + 103  //! < win key (left or right)
};

// Just syntax sugar
// So it is the same as other macros
#define VIRTUAL_KMOD(x) VIRTUAL_KMOD_ ## x

//! Converts individual codes to virtual keys if needed
unsigned int GetVirtualKey(unsigned int key);

// Virtual key code generated on joystick button presses
// num is number of joystick button
#define VIRTUAL_JOY(num) (SDLK_LAST + 200 + num)

//! Special value for invalid key bindings
const unsigned int KEY_INVALID = SDLK_LAST + 1000;

/**
 * \enum InputSlot
 * \brief Available slots for input bindings
 * NOTE: When adding new values, remember to also update keyTable in input.cpp and their descriptions in restext.cpp
 */
enum InputSlot
{
    INPUT_SLOT_LEFT,
    INPUT_SLOT_RIGHT,
    INPUT_SLOT_UP,
    INPUT_SLOT_DOWN,
    INPUT_SLOT_GUP,
    INPUT_SLOT_GDOWN,
    INPUT_SLOT_CAMERA,
    INPUT_SLOT_DESEL,
    INPUT_SLOT_ACTION,
    INPUT_SLOT_NEAR,
    INPUT_SLOT_AWAY,
    INPUT_SLOT_NEXT,
    INPUT_SLOT_HUMAN,
    INPUT_SLOT_QUIT,
    INPUT_SLOT_HELP,
    INPUT_SLOT_PROG,
    INPUT_SLOT_VISIT,
    INPUT_SLOT_SPEED05,
    INPUT_SLOT_SPEED10,
    INPUT_SLOT_SPEED15,
    INPUT_SLOT_SPEED20,
    INPUT_SLOT_SPEED30,
    INPUT_SLOT_SPEED40,
    INPUT_SLOT_SPEED60,
    INPUT_SLOT_CAMERA_UP,
    INPUT_SLOT_CAMERA_DOWN,
    INPUT_SLOT_PAUSE,

    INPUT_SLOT_MAX
};

/**
 * \enum JoyAxisSlot
 * \brief Slots for joystick axes inputs
 */
enum JoyAxisSlot
{
    JOY_AXIS_SLOT_X,
    JOY_AXIS_SLOT_Y,
    JOY_AXIS_SLOT_Z,

    JOY_AXIS_SLOT_MAX
};
