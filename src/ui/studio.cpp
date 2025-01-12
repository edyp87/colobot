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


#include "ui/studio.h"

#include "CBot/CBotDll.h"

#include "app/app.h"
#include "app/pausemanager.h"

#include "common/event.h"
#include "common/logger.h"
#include "common/misc.h"
#include "common/settings.h"

#include "common/resources/resourcemanager.h"

#include "level/robotmain.h"
#include "level/player_profile.h"

#include "graphics/engine/camera.h"
#include "graphics/engine/engine.h"

#include "object/object.h"

#include "object/interface/programmable_object.h"

#include "script/cbottoken.h"
#include "script/script.h"

#include "sound/sound.h"

#include "ui/controls/check.h"
#include "ui/controls/color.h"
#include "ui/controls/control.h"
#include "ui/controls/edit.h"
#include "ui/controls/group.h"
#include "ui/controls/image.h"
#include "ui/controls/interface.h"
#include "ui/controls/key.h"
#include "ui/controls/label.h"
#include "ui/controls/list.h"
#include "ui/controls/map.h"
#include "ui/controls/shortcut.h"
#include "ui/controls/target.h"
#include "ui/controls/window.h"

#include <stdio.h>


namespace Ui
{


// Object's constructor.

CStudio::CStudio()
{
    m_app       = CApplication::GetInstancePointer();
    m_sound     = m_app->GetSound();
    m_event     = m_app->GetEventQueue();
    m_engine    = Gfx::CEngine::GetInstancePointer();
    m_main      = CRobotMain::GetInstancePointer();
    m_interface = m_main->GetInterface();
    m_camera    = m_main->GetCamera();
    m_pause     = m_engine->GetPauseManager();
    m_settings  = CSettings::GetInstancePointer();

    m_program = nullptr;
    m_script = nullptr;

    m_bEditMaximized = false;
    m_bEditMinimized = false;

    m_time      = 0.0f;
    m_bRealTime = true;
    m_bRunning  = false;
    m_fixInfoTextTime = 0.0f;
    m_dialog = SD_NULL;
    m_editCamera = Gfx::CAM_TYPE_NULL;
}

// Object's destructor.

CStudio::~CStudio()
{
}


// Management of an event.

bool CStudio::EventProcess(const Event &event)
{
    CWindow*    pw;
    CEdit*      edit;
    CSlider*    slider;

    if ( m_dialog != SD_NULL )  // dialogue exists?
    {
        return EventDialog(event);
    }

    if ( event.type == EVENT_FRAME )
    {
        EventFrame(event);
    }

    pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW3));
    if ( pw == nullptr )  return false;

    edit = static_cast<CEdit*>(pw->SearchControl(EVENT_STUDIO_EDIT));
    if ( edit == nullptr )  return false;

    if ( event.type == pw->GetEventTypeClose() )
    {
        m_event->AddEvent(Event(EVENT_STUDIO_OK));
    }

    if ( event.type == EVENT_STUDIO_EDIT )  // text modifief?
    {
        ColorizeScript(edit);
    }

    if ( event.type == EVENT_STUDIO_LIST )  // list clicked?
    {
        m_main->StartDisplayInfo(m_helpFilename, -1);
    }

    if ( event.type == EVENT_STUDIO_NEW )  // new?
    {
        m_script->New(edit, "");
    }

    if ( event.type == EVENT_STUDIO_OPEN )  // open?
    {
        StartDialog(SD_OPEN);
    }
    if ( event.type == EVENT_STUDIO_SAVE )  // save?
    {
        StartDialog(SD_SAVE);
    }

    if ( event.type == EVENT_STUDIO_UNDO )  // undo?
    {
        edit->Undo();
    }
    if ( event.type == EVENT_STUDIO_CUT )  // cut?
    {
        edit->Cut();
    }
    if ( event.type == EVENT_STUDIO_COPY )  // copy?
    {
        edit->Copy();
    }
    if ( event.type == EVENT_STUDIO_PASTE )  // paste?
    {
        edit->Paste();
    }

    if ( event.type == EVENT_STUDIO_SIZE )  // size?
    {
        slider = static_cast< CSlider* >(pw->SearchControl(EVENT_STUDIO_SIZE));
        if ( slider == nullptr )  return false;
        m_settings->SetFontSize(9.0f+slider->GetVisibleValue()*15.0f);
        ViewEditScript();
    }

    if ( event.type == EVENT_STUDIO_TOOL &&  // instructions?
         m_dialog == SD_NULL )
    {
        m_main->StartDisplayInfo(SATCOM_HUSTON, false);
    }
    if ( event.type == EVENT_STUDIO_HELP &&  // help?
         m_dialog == SD_NULL )
    {
        m_main->StartDisplayInfo(SATCOM_PROG, false);
    }

    if ( event.type == EVENT_STUDIO_COMPILE )  // compile?
    {
        if ( m_script->GetScript(edit) )  // compile
        {
            std::string res;
            GetResource(RES_TEXT, RT_STUDIO_COMPOK, res);
            SetInfoText(res, false);
        }
        else
        {
            std::string error;
            m_script->GetError(error);
            SetInfoText(error, false);
        }
    }

    if ( event.type == EVENT_STUDIO_RUN )  // run/stop?
    {
        if ( m_script->IsRunning() )
        {
            m_event->AddEvent(Event(EVENT_OBJECT_PROGSTOP));
        }
        else
        {
            if ( m_script->GetScript(edit) )  // compile
            {
                SetInfoText("", false);

                m_event->AddEvent(Event(EVENT_OBJECT_PROGSTART));
            }
            else
            {
                std::string error;
                m_script->GetError(error);
                SetInfoText(error, false);
            }
        }
    }

    if ( event.type == EVENT_STUDIO_REALTIME )  // real time?
    {
        m_bRealTime = !m_bRealTime;
        m_script->SetStepMode(!m_bRealTime);
        UpdateFlux();
        UpdateButtons();
    }

    if ( event.type == EVENT_STUDIO_STEP )  // step?
    {
        m_script->Step();
    }

    if ( event.type == EVENT_WINDOW3 )  // window is moved?
    {
        m_editActualPos = m_editFinalPos = pw->GetPos();
        m_editActualDim = m_editFinalDim = pw->GetDim();
        m_settings->SetWindowPos(m_editActualPos);
        m_settings->SetWindowDim(m_editActualDim);
        AdjustEditScript();
    }
    if ( event.type == pw->GetEventTypeReduce() )
    {
        if ( m_bEditMinimized )
        {
            m_editFinalPos = m_settings->GetWindowPos();
            m_editFinalDim = m_settings->GetWindowDim();
            m_bEditMinimized = false;
            m_bEditMaximized = false;
        }
        else
        {
            m_editFinalPos.x =  0.00f;
            m_editFinalPos.y = -0.44f;
            m_editFinalDim.x =  1.00f;
            m_editFinalDim.y =  0.50f;
            m_bEditMinimized = true;
            m_bEditMaximized = false;
        }
        m_main->SetEditFull(m_bEditMaximized);
        pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW3));
        if ( pw != nullptr )
        {
            pw->SetMaximized(m_bEditMaximized);
            pw->SetMinimized(m_bEditMinimized);
        }
    }
    if ( event.type == pw->GetEventTypeFull() )
    {
        if ( m_bEditMaximized )
        {
            m_editFinalPos = m_settings->GetWindowPos();
            m_editFinalDim = m_settings->GetWindowDim();
            m_bEditMinimized = false;
            m_bEditMaximized = false;
        }
        else
        {
            m_editFinalPos.x = 0.00f;
            m_editFinalPos.y = 0.00f;
            m_editFinalDim.x = 1.00f;
            m_editFinalDim.y = 1.00f;
            m_bEditMinimized = false;
            m_bEditMaximized = true;
        }
        m_main->SetEditFull(m_bEditMaximized);
        pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW3));
        if ( pw != nullptr )
        {
            pw->SetMaximized(m_bEditMaximized);
            pw->SetMinimized(m_bEditMinimized);
        }
    }

    return true;
}


// Evolves value with time elapsed.

float Evolution(float final, float actual, float time)
{
    float   value;

    value = actual + (final-actual)*time;

    if ( final > actual )
    {
        if ( value > final )  value = final;  // does not exceed
    }
    else
    {
        if ( value < final )  value = final;  // does not exceed
    }

    return value;
}

// Makes the studio evolve as time elapsed.

bool CStudio::EventFrame(const Event &event)
{
    CWindow*    pw;
    CEdit*      edit;
    CList*      list;
    float       time;
    int         cursor1, cursor2, iCursor1, iCursor2;

    m_time += event.rTime;
    m_fixInfoTextTime -= event.rTime;

    pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW3));
    if ( pw == nullptr )  return false;

    edit = static_cast< CEdit* >(pw->SearchControl(EVENT_STUDIO_EDIT));
    if ( edit == nullptr )  return false;

    list = static_cast< CList* >(pw->SearchControl(EVENT_STUDIO_LIST));
    if ( list == nullptr )  return false;

    if (m_script->IsRunning() && (!m_script->GetStepMode() || m_script->IsContinue()))
    {
        if (m_editorPause != nullptr)
        {
            m_pause->DeactivatePause(m_editorPause);
            m_editorPause = nullptr;
        }
    }
    else
    {
        if (m_editorPause == nullptr)
        {
            m_editorPause = m_pause->ActivatePause(PAUSE_EDITOR);
        }
    }

    if ( !m_script->IsRunning() && m_bRunning )  // stop?
    {
        m_bRunning = false;
        UpdateFlux();  // stop
        AdjustEditScript();
        std::string res;
        GetResource(RES_TEXT, RT_STUDIO_PROGSTOP, res);
        SetInfoText(res, false);

        m_event->AddEvent(Event(EVENT_OBJECT_PROGSTOP));
    }

    if ( m_script->IsRunning() && !m_bRunning )  // starting?
    {
        m_bRunning = true;
        UpdateFlux();  // run
        AdjustEditScript();
    }
    UpdateButtons();

    if ( m_bRunning )
    {
        m_script->GetCursor(cursor1, cursor2);
        edit->GetCursor(iCursor1, iCursor2);
        if ( cursor1 != iCursor1 ||
             cursor2 != iCursor2 )  // cursors change?
        {
            edit->SetCursor(cursor1, cursor2);  // shows it on the execution
            edit->ShowSelect();
        }

        m_script->UpdateList(list);  // updates the list of variables
    }
    else
    {
        SearchToken(edit);
    }

    if ( m_editFinalPos.x != m_editActualPos.x ||
         m_editFinalPos.y != m_editActualPos.y ||
         m_editFinalDim.x != m_editActualDim.x ||
         m_editFinalDim.y != m_editActualDim.y )
    {
        time = event.rTime*20.0f;
        m_editActualPos.x = Evolution(m_editFinalPos.x, m_editActualPos.x, time);
        m_editActualPos.y = Evolution(m_editFinalPos.y, m_editActualPos.y, time);
        m_editActualDim.x = Evolution(m_editFinalDim.x, m_editActualDim.x, time);
        m_editActualDim.y = Evolution(m_editFinalDim.y, m_editActualDim.y, time);
        AdjustEditScript();
    }

    return true;
}


// Indicates whether a character is part of a word.

bool IsToken(int character)
{
    char    c;

    c = tolower(GetNoAccent(character));

    return ( (c >= 'a' && c <= 'z') ||
             (c >= '0' && c <= '9') ||
             c == '_' );
}

// Seeks if the cursor is on a keyword.

void CStudio::SearchToken(CEdit* edit)
{
    ObjectType  type;
    int         len, cursor1, cursor2, i, character, level;
    char*       text;
    char        token[100];

    text = edit->GetText();
    len  = edit->GetTextLength();
    edit->GetCursor(cursor1, cursor2);

    i = cursor1;
    if ( i > 0 )
    {
        character = static_cast< unsigned char > (text[i-1]);
        if ( !IsToken(character) )
        {
            level = 1;
            while ( i > 0 )
            {
                character = static_cast< unsigned char > (text[i-1]);
                if ( character == ')' )
                {
                    level ++;
                }
                else if ( character == '(' )
                {
                    level --;
                    if ( level == 0 )  break;
                }
                i --;
            }
            if ( level > 0 )
            {
                m_helpFilename = "";
                SetInfoText("", true);
                return;
            }
            while ( i > 0 )
            {
                character = static_cast< unsigned char > (text[i-1]);
                if ( IsToken(character) )  break;
                i --;
            }
        }
    }

    while ( i > 0 )
    {
        character = static_cast< unsigned char > (text[i-1]);
        if ( !IsToken(character) )  break;
        i --;
    }
    cursor2 = i;

    while ( i < len )
    {
        character = static_cast< unsigned char > (text[i]);
        if ( !IsToken(character) )  break;
        i ++;
    }
    cursor1 = i;
    len = cursor1-cursor2;

    if ( len > 100-1 )  len = 100-1;
    for ( i=0 ; i<len ; i++ )
    {
        token[i] = text[cursor2+i];
    }
    token[i] = 0;

    m_helpFilename = GetHelpFilename(token);
    if ( m_helpFilename.length() == 0 )
    {
        for ( i=0 ; i<OBJECT_MAX ; i++ )
        {
            type = static_cast< ObjectType >(i);
            text = const_cast<char *>(GetObjectName(type));
            if ( text[0] != 0 )
            {
                if ( strcmp(token, text) == 0 )
                {
                    m_helpFilename = GetHelpFilename(type);
                    SetInfoText(std::string(token), true);
                    return;
                }
            }
            text = const_cast<char *>(GetObjectAlias(type));
            if ( text[0] != 0 )
            {
                if ( strcmp(token, text) == 0 )
                {
                    m_helpFilename = GetHelpFilename(type);
                    SetInfoText(std::string(token), true);
                    return;
                }
            }
        }
    }

    text = const_cast<char *>(GetHelpText(token));
    if ( text[0] == 0 && m_helpFilename.length() > 0 )
    {
        SetInfoText(std::string(token), true);
    }
    else
    {
        SetInfoText(text, true);
    }
}

// Colors the text according to syntax.

void CStudio::ColorizeScript(CEdit* edit)
{
    m_script->ColorizeScript(edit);
}


// Starts editing a program.

void CStudio::StartEditScript(CScript *script, std::string name, Program* program)
{
    Math::Point     pos, dim;
    CWindow*    pw;
    CEdit*      edit;
    CButton*    button;
    CSlider*    slider;
    CList*      list;

    m_script  = script;
    m_program = program;

    m_main->SetEditLock(true, true);
    m_main->SetEditFull(false);
    m_main->SetSpeed(1.0f);
    m_editCamera = m_camera->GetType();
    m_camera->SetType(Gfx::CAM_TYPE_EDIT);

    m_bRunning = m_script->IsRunning();
    m_bRealTime = m_bRunning;
    m_script->SetStepMode(!m_bRealTime);

    pw = static_cast<CWindow*>(m_interface->SearchControl(EVENT_WINDOW6));
    if (pw != nullptr) pw->ClearState(STATE_VISIBLE);

    pos = m_editFinalPos = m_editActualPos = m_settings->GetWindowPos();
    dim = m_editFinalDim = m_editActualDim = m_settings->GetWindowDim();
    pw = m_interface->CreateWindows(pos, dim, 8, EVENT_WINDOW3);
    if (pw == nullptr)
        return;

    pw->SetState(STATE_SHADOW);
    pw->SetRedim(true);  // before SetName!
    pw->SetMovable(true);
    pw->SetClosable(true);

    std::string res;
    GetResource(RES_TEXT, RT_STUDIO_TITLE, res);
    pw->SetName(res);

    pw->SetMinDim(Math::Point(0.49f, 0.50f));
    pw->SetMaximized(m_bEditMaximized);
    pw->SetMinimized(m_bEditMinimized);
    m_main->SetEditFull(m_bEditMaximized);

    edit = pw->CreateEdit(pos, dim, 0, EVENT_STUDIO_EDIT);
    if (edit == nullptr)
        return;

    edit->SetState(STATE_SHADOW);
    edit->SetInsideScroll(false);
//? if ( m_bRunning )  edit->SetEdit(false);
    edit->SetMaxChar(EDITSTUDIOMAX);
    edit->SetFontType(Gfx::FONT_COURIER);
    edit->SetFontStretch(1.0f);
    edit->SetDisplaySpec(true);
    edit->SetAutoIndent(m_engine->GetEditIndentMode());

    m_script->PutScript(edit, name.c_str());
    ColorizeScript(edit);

    ViewEditScript();

    list = pw->CreateList(pos, dim, 1, EVENT_STUDIO_LIST, 1.2f);
    list->SetState(STATE_SHADOW);
    list->SetFontType(Gfx::FONT_COURIER);
    list->SetSelectCap(false);
    list->SetFontSize(Gfx::FONT_SIZE_SMALL*0.85f);
//? list->SetFontStretch(1.0f);

    button = pw->CreateButton(pos, dim, 56, EVENT_STUDIO_NEW);
    button->SetState(STATE_SHADOW);
    button = pw->CreateButton(pos, dim, 57, EVENT_STUDIO_OPEN);
    button->SetState(STATE_SHADOW);
    button = pw->CreateButton(pos, dim, 58, EVENT_STUDIO_SAVE);
    button->SetState(STATE_SHADOW);
    button = pw->CreateButton(pos, dim, 59, EVENT_STUDIO_UNDO);
    button->SetState(STATE_SHADOW);
    button = pw->CreateButton(pos, dim, 60, EVENT_STUDIO_CUT);
    button->SetState(STATE_SHADOW);
    button = pw->CreateButton(pos, dim, 61, EVENT_STUDIO_COPY);
    button->SetState(STATE_SHADOW);
    button = pw->CreateButton(pos, dim, 62, EVENT_STUDIO_PASTE);
    button->SetState(STATE_SHADOW);
    slider = pw->CreateSlider(pos, dim, 0, EVENT_STUDIO_SIZE);
    slider->SetState(STATE_SHADOW);
    slider->SetVisibleValue((m_settings->GetFontSize()-9.0f)/15.0f);
    pw->CreateGroup(pos, dim, 19, EVENT_LABEL1);  // SatCom logo
    button = pw->CreateButton(pos, dim, 128+57, EVENT_STUDIO_TOOL);
    button->SetState(STATE_SHADOW);
    button = pw->CreateButton(pos, dim, 128+60, EVENT_STUDIO_HELP);
    button->SetState(STATE_SHADOW);

    button = pw->CreateButton(pos, dim, -1, EVENT_STUDIO_OK);
    button->SetState(STATE_SHADOW);
    button = pw->CreateButton(pos, dim, 61, EVENT_STUDIO_CLONE);
    button->SetState(STATE_SHADOW);
    button = pw->CreateButton(pos, dim, -1, EVENT_STUDIO_CANCEL);
    button->SetState(STATE_SHADOW);
    button = pw->CreateButton(pos, dim, 64+23, EVENT_STUDIO_COMPILE);
    button->SetState(STATE_SHADOW);
    button = pw->CreateButton(pos, dim, 21, EVENT_STUDIO_RUN);
    button->SetState(STATE_SHADOW);
    button = pw->CreateButton(pos, dim, 64+22, EVENT_STUDIO_REALTIME);
    button->SetState(STATE_SHADOW);
    button = pw->CreateButton(pos, dim, 64+29, EVENT_STUDIO_STEP);
    button->SetState(STATE_SHADOW);

    if (!m_program->runnable)
    {
        GetResource(RES_TEXT, RT_PROGRAM_EXAMPLE, res);
        SetInfoText(res, false);
    }
    else if (m_program->readOnly)
    {
        GetResource(RES_TEXT, RT_PROGRAM_READONLY, res);
        SetInfoText(res, false);
    }

    UpdateFlux();
    UpdateButtons();
    AdjustEditScript();
}

// Repositions all the editing controls.

void CStudio::AdjustEditScript()
{
    CWindow*    pw;
    CEdit*      edit;
    CButton*    button;
    CGroup*     group;
    CSlider*    slider;
    CList*      list;
    Math::Point     wpos, wdim, pos, dim, ppos, ddim;
    float       hList;

    wpos = m_editActualPos;
    wdim = m_editActualDim;

    pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW3));
    if ( pw != nullptr )
    {
        pw->SetPos(wpos);
        pw->SetDim(wdim);
        wdim = pw->GetDim();
    }

    if ( m_bRunning ) hList = 80.0f/480.0f;
    else              hList = 20.0f/480.0f;

    pos.x = wpos.x+0.01f;
    pos.y = wpos.y+0.09f+hList;
    dim.x = wdim.x-0.02f;
    dim.y = wdim.y-0.22f-hList;
    edit = static_cast< CEdit* >(pw->SearchControl(EVENT_STUDIO_EDIT));
    if ( edit != nullptr )
    {
        edit->SetPos(pos);
        edit->SetDim(dim);
    }

    pos.x = wpos.x+0.01f;
    pos.y = wpos.y+0.09f;
    dim.x = wdim.x-0.02f;
    dim.y = hList;
    list = static_cast< CList* >(pw->SearchControl(EVENT_STUDIO_LIST));
    if ( list != nullptr )
    {
        list->SetPos(pos);
        list->SetDim(dim);
    }

    dim.x = 0.04f;
    dim.y = 0.04f*1.5f;
    dim.y = 25.0f/480.0f;

    pos.y = wpos.y+wdim.y-dim.y-0.06f;
    pos.x = wpos.x+0.01f;
    button = static_cast< CButton* >(pw->SearchControl(EVENT_STUDIO_NEW));
    if ( button != nullptr )
    {
        button->SetPos(pos);
        button->SetDim(dim);
    }
    pos.x = wpos.x+0.05f;
    button = static_cast< CButton* >(pw->SearchControl(EVENT_STUDIO_OPEN));
    if ( button != nullptr )
    {
        button->SetPos(pos);
        button->SetDim(dim);
    }
    pos.x = wpos.x+0.09f;
    button = static_cast< CButton* >(pw->SearchControl(EVENT_STUDIO_SAVE));
    if ( button != nullptr )
    {
        button->SetPos(pos);
        button->SetDim(dim);
    }
    pos.x = wpos.x+0.14f;
    button = static_cast< CButton* >(pw->SearchControl(EVENT_STUDIO_UNDO));
    if ( button != nullptr )
    {
        button->SetPos(pos);
        button->SetDim(dim);
    }
    pos.x = wpos.x+0.19f;
    button = static_cast< CButton* >(pw->SearchControl(EVENT_STUDIO_CUT));
    if ( button != nullptr )
    {
        button->SetPos(pos);
        button->SetDim(dim);
    }
    pos.x = wpos.x+0.23f;
    button = static_cast< CButton* >(pw->SearchControl(EVENT_STUDIO_COPY));
    if ( button != nullptr )
    {
        button->SetPos(pos);
        button->SetDim(dim);
    }
    pos.x = wpos.x+0.27f;
    button = static_cast< CButton* >(pw->SearchControl(EVENT_STUDIO_PASTE));
    if ( button != nullptr )
    {
        button->SetPos(pos);
        button->SetDim(dim);
    }
    pos.x = wpos.x+0.32f;
    slider = static_cast< CSlider* >(pw->SearchControl(EVENT_STUDIO_SIZE));
    if ( slider != nullptr )
    {
        ppos = pos;
        ddim.x = dim.x*0.7f;
        ddim.y = dim.y;
        ppos.y -= 3.0f/480.0f;
        ddim.y += 6.0f/480.0f;
        slider->SetPos(ppos);
        slider->SetDim(ddim);
    }
    pos.x = wpos.x+0.36f;
    group = static_cast< CGroup* >(pw->SearchControl(EVENT_LABEL1));
    if ( group != nullptr )
    {
        group->SetPos(pos);
        group->SetDim(dim);
    }
    pos.x = wpos.x+0.40f;
    button = static_cast< CButton* >(pw->SearchControl(EVENT_STUDIO_TOOL));
    if ( button != nullptr )
    {
        button->SetPos(pos);
        button->SetDim(dim);
    }
    pos.x = wpos.x+0.44f;
    button = static_cast< CButton* >(pw->SearchControl(EVENT_STUDIO_HELP));
    if ( button != nullptr )
    {
        button->SetPos(pos);
        button->SetDim(dim);
    }

    pos.y = wpos.y+0.02f;
    pos.x = wpos.x+0.01f;
    dim.x = 80.0f/640.0f;
    dim.y = 25.0f/480.0f;
    button = static_cast< CButton* >(pw->SearchControl(EVENT_STUDIO_OK));
    if ( button != nullptr )
    {
        button->SetPos(pos);
        button->SetDim(dim);
    }
    dim.x = 25.0f/480.0f;
    dim.y = 25.0f/480.0f;
    pos.x = wpos.x+0.01f+0.125f+0.005f;
    button = static_cast< CButton* >(pw->SearchControl(EVENT_STUDIO_CLONE));
    if ( button != nullptr )
    {
        button->SetPos(pos);
        button->SetDim(dim);
    }
    dim.x = 80.0f/640.0f - 25.0f/480.0f;
    dim.y = 25.0f/480.0f;
    pos.x = wpos.x+0.01f+0.125f+0.005f+(25.0f/480.0f)+0.005f;
    button = static_cast< CButton* >(pw->SearchControl(EVENT_STUDIO_CANCEL));
    if ( button != nullptr )
    {
        button->SetPos(pos);
        button->SetDim(dim);
    }
    pos.x = wpos.x+0.28f;
    dim.x = dim.y*0.75f;
    button = static_cast< CButton* >(pw->SearchControl(EVENT_STUDIO_COMPILE));
    if ( button != nullptr )
    {
        button->SetPos(pos);
        button->SetDim(dim);
    }
    pos.x = wpos.x+0.28f+dim.x*1;
    button = static_cast< CButton* >(pw->SearchControl(EVENT_STUDIO_RUN));
    if ( button != nullptr )
    {
        button->SetPos(pos);
        button->SetDim(dim);
    }
    pos.x = wpos.x+0.28f+dim.x*2;
    button = static_cast< CButton* >(pw->SearchControl(EVENT_STUDIO_REALTIME));
    if ( button != nullptr )
    {
        button->SetPos(pos);
        button->SetDim(dim);
    }
    pos.x = wpos.x+0.28f+dim.x*3;
    button = static_cast< CButton* >(pw->SearchControl(EVENT_STUDIO_STEP));
    if ( button != nullptr )
    {
        button->SetPos(pos);
        button->SetDim(dim);
    }
}

// Ends edition of a program.

bool CStudio::StopEditScript(bool bCancel)
{
    CWindow*    pw;
    CEdit*      edit;

    pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW3));
    if ( pw == nullptr )  return false;

    if ( !bCancel && !m_script->IsRunning() )
    {
        edit = static_cast< CEdit* >(pw->SearchControl(EVENT_STUDIO_EDIT));
        if ( edit != nullptr )
        {
            if ( !m_script->GetScript(edit) )  // compile
            {
                std::string error;
                m_script->GetError(error);
                SetInfoText(error, false);
                return false;
            }
        }
    }
    m_script->SetStepMode(false);

    m_interface->DeleteControl(EVENT_WINDOW3);

    pw = static_cast<CWindow*>(m_interface->SearchControl(EVENT_WINDOW6));
    if (pw != nullptr) pw->SetState(STATE_VISIBLE);

    m_pause->DeactivatePause(m_editorPause);
    m_editorPause = nullptr;
    m_sound->MuteAll(false);
    m_main->SetEditLock(false, true);
    m_camera->SetType(m_editCamera);
    return true;
}

// Specifies the message to display.
// The messages are not clickable 8 seconds,
// even if a message was clickable poster before.

void CStudio::SetInfoText(std::string text, bool bClickable)
{
    if ( bClickable && m_fixInfoTextTime > 0.0f )  return;
    if ( !bClickable )  m_fixInfoTextTime = 8.0f;

    CWindow* pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW3));
    if ( pw == nullptr )  return;

    CList* list = static_cast< CList* >(pw->SearchControl(EVENT_STUDIO_LIST));
    if ( list == nullptr )  return;

    list->Flush();  // just text
    list->SetItemName(0, text.c_str());

    if ( text[0] == 0 )  bClickable = false;
    list->SetSelectCap(bClickable);

    if ( bClickable )
    {
        std::string res;
        GetResource(RES_TEXT, RT_STUDIO_LISTTT, res);
        list->SetTooltip(res);
        list->SetState(STATE_ENABLE);
    }
    else
    {
        list->SetTooltip("");
//?     list->ClearState(STATE_ENABLE);
        list->SetState(STATE_ENABLE, text[0] != 0);
    }
}


// Changing the size of a editing program.

void CStudio::ViewEditScript()
{
    CWindow*    pw;
    CEdit*      edit;
    Math::IntPoint dim;

    pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW3));
    if ( pw == nullptr )  return;

    edit = static_cast< CEdit* >(pw->SearchControl(EVENT_STUDIO_EDIT));
    if ( edit == nullptr )  return;

    dim = m_engine->GetWindowSize();
    edit->SetFontSize(m_settings->GetFontSize()/(dim.x/640.0f));
}


// Updates the operating mode.

void CStudio::UpdateFlux()
{
    if ( m_bRunning )
    {
        if ( m_bRealTime )  // run?
        {
            m_pause->DeactivatePause(m_editorPause);
            m_editorPause = nullptr;
        }
        else    // step by step?
        {
            if (m_editorPause == nullptr)
            {
                m_editorPause = m_pause->ActivatePause(PAUSE_EDITOR);
            }
        }
    }
    else    // stop?
    {
        if (m_editorPause == nullptr)
        {
            m_editorPause = m_pause->ActivatePause(PAUSE_EDITOR);
        }
    }
}

// Updates the buttons.

void CStudio::UpdateButtons()
{
    CWindow*    pw;
    CEdit*      edit;
    CButton*    button;

    pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW3));
    if ( pw == nullptr )  return;

    edit = static_cast< CEdit* >(pw->SearchControl(EVENT_STUDIO_EDIT));
    if ( edit == nullptr )  return;

    if ( m_bRunning )
    {
        edit->SetIcon(1);  // red background
        edit->SetEditCap(false);  // just to see
        edit->SetHighlightCap(true);
    }
    else
    {
        edit->SetIcon(m_program->readOnly ? 1 : 0);  // standard background
        edit->SetEditCap(!m_program->readOnly);
        edit->SetHighlightCap(true);
    }


    button = static_cast< CButton* >(pw->SearchControl(EVENT_STUDIO_CLONE));
    if ( button == nullptr )  return;
    button->SetState(STATE_ENABLE, m_program->runnable && !m_bRunning);


    button = static_cast< CButton* >(pw->SearchControl(EVENT_STUDIO_COMPILE));
    if ( button == nullptr )  return;
    button->SetState(STATE_ENABLE, m_program->runnable && !m_bRunning);

    button = static_cast< CButton* >(pw->SearchControl(EVENT_STUDIO_RUN));
    if ( button == nullptr )  return;
    button->SetIcon(m_bRunning?8:21);  // stop/run
    button->SetState(STATE_ENABLE, m_program->runnable || m_bRunning);

    button = static_cast< CButton* >(pw->SearchControl(EVENT_STUDIO_REALTIME));
    if ( button == nullptr )  return;
    button->SetIcon(m_bRealTime?64+22:64+21);
    button->SetState(STATE_ENABLE, (!m_bRunning || !m_script->IsContinue()));

    button = static_cast< CButton* >(pw->SearchControl(EVENT_STUDIO_STEP));
    if ( button == nullptr )  return;
    button->SetState(STATE_ENABLE, (m_bRunning && !m_bRealTime && !m_script->IsContinue()));


    button = static_cast< CButton* >(pw->SearchControl(EVENT_STUDIO_NEW));
    if ( button == nullptr )  return;
    button->SetState(STATE_ENABLE, !m_program->readOnly);

    button = static_cast< CButton* >(pw->SearchControl(EVENT_STUDIO_OPEN));
    if ( button == nullptr )  return;
    button->SetState(STATE_ENABLE, !m_program->readOnly);

    button = static_cast< CButton* >(pw->SearchControl(EVENT_STUDIO_UNDO));
    if ( button == nullptr )  return;
    button->SetState(STATE_ENABLE, !m_program->readOnly);

    button = static_cast< CButton* >(pw->SearchControl(EVENT_STUDIO_CUT));
    if ( button == nullptr )  return;
    button->SetState(STATE_ENABLE, !m_program->readOnly);

    button = static_cast< CButton* >(pw->SearchControl(EVENT_STUDIO_PASTE));
    if ( button == nullptr )  return;
    button->SetState(STATE_ENABLE, !m_program->readOnly);
}


// Beginning of the display of a dialogue.

void CStudio::StartDialog(StudioDialog type)
{
    CWindow*    pw;
    CButton*    pb;
    CCheck*     pc;
    CLabel*     pla;
    CList*      pli;
    CEdit*      pe;
    Math::Point     pos, dim;
    std::string name;

    m_dialog = type;

    pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW0));
    if ( pw != nullptr )  pw->ClearState(STATE_ENABLE);

    pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW1));
    if ( pw != nullptr )  pw->ClearState(STATE_ENABLE);

    pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW3));
    if ( pw != nullptr )  pw->ClearState(STATE_ENABLE);

    pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW3));
    if ( pw != nullptr )  pw->ClearState(STATE_ENABLE);

    pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW4));
    if ( pw != nullptr )  pw->ClearState(STATE_ENABLE);

    pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW5));
    if ( pw != nullptr )  pw->ClearState(STATE_ENABLE);

    //pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW6));
    //if ( pw != nullptr )  pw->ClearState(STATE_VISIBLE);

    pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW7));
    if ( pw != nullptr )  pw->ClearState(STATE_ENABLE);

    pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW8));
    if ( pw != nullptr )  pw->ClearState(STATE_ENABLE);

    if ( m_dialog == SD_OPEN ||
         m_dialog == SD_SAVE )
    {
        pos = m_settings->GetIOPos();
        dim = m_settings->GetIODim();
    }
//? pw = m_interface->CreateWindows(pos, dim, 8, EVENT_WINDOW9);
    pw = m_interface->CreateWindows(pos, dim, m_dialog==SD_OPEN?14:13, EVENT_WINDOW9);
    pw->SetState(STATE_SHADOW);
    pw->SetMovable(true);
    pw->SetClosable(true);
    pw->SetMinDim(Math::Point(320.0f/640.0f, (121.0f+18.0f*4)/480.0f));
    if ( m_dialog == SD_OPEN )  GetResource(RES_TEXT, RT_IO_OPEN, name);
    if ( m_dialog == SD_SAVE )  GetResource(RES_TEXT, RT_IO_SAVE, name);
    pw->SetName(name);

    pos = Math::Point(0.0f, 0.0f);
    dim = Math::Point(0.0f, 0.0f);

    if ( m_dialog == SD_OPEN ||
         m_dialog == SD_SAVE )
    {
        GetResource(RES_TEXT, RT_IO_LIST, name);
        pla = pw->CreateLabel(pos, dim, 0, EVENT_DIALOG_LABEL1, name);
        pla->SetTextAlign(Gfx::TEXT_ALIGN_LEFT);

        pli = pw->CreateList(pos, dim, 0, EVENT_DIALOG_LIST);
        pli->SetState(STATE_SHADOW);

        GetResource(RES_TEXT, RT_IO_NAME, name);
        pla = pw->CreateLabel(pos, dim, 0, EVENT_DIALOG_LABEL2, name);
        pla->SetTextAlign(Gfx::TEXT_ALIGN_LEFT);

        pe = pw->CreateEdit(pos, dim, 0, EVENT_DIALOG_EDIT);
        pe->SetState(STATE_SHADOW);

        GetResource(RES_TEXT, RT_IO_DIR, name);
        pla = pw->CreateLabel(pos, dim, 0, EVENT_DIALOG_LABEL3, name);
        pla->SetTextAlign(Gfx::TEXT_ALIGN_LEFT);

        pc = pw->CreateCheck(pos, dim, 0, EVENT_DIALOG_CHECK1);
        GetResource(RES_TEXT, RT_IO_PRIVATE, name);
        pc->SetName(name);
        pc->SetState(STATE_SHADOW);

        pc = pw->CreateCheck(pos, dim, 0, EVENT_DIALOG_CHECK2);
        GetResource(RES_TEXT, RT_IO_PUBLIC, name);
        pc->SetName(name);
        pc->SetState(STATE_SHADOW);

        pb = pw->CreateButton(pos, dim, -1, EVENT_DIALOG_OK);
        pb->SetState(STATE_SHADOW);
        if ( m_dialog == SD_OPEN )  GetResource(RES_TEXT, RT_IO_OPEN, name);
        if ( m_dialog == SD_SAVE )  GetResource(RES_TEXT, RT_IO_SAVE, name);
        pb->SetName(name);

        pb = pw->CreateButton(pos, dim, -1, EVENT_DIALOG_CANCEL);
        pb->SetState(STATE_SHADOW);
        GetResource(RES_EVENT, EVENT_DIALOG_CANCEL, name);
        pb->SetName(name);

        AdjustDialog();
        UpdateDialogList();
        UpdateDialogPublic();
        UpdateDialogAction();

        if ( m_dialog == SD_SAVE )
        {
            SetFilenameField(pe, m_script->GetFilename());
            UpdateChangeEdit();
        }

        pe->SetCursor(999, 0);  // selects all
        m_interface->SetFocus(pe);
    }

    m_main->SetSatComLock(true);  // impossible to use the SatCom
}

// End of the display of a dialogue.

void CStudio::StopDialog()
{
    CWindow*    pw;

    if ( m_dialog == SD_NULL )  return;
    m_dialog = SD_NULL;

    pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW0));
    if ( pw != nullptr )  pw->SetState(STATE_ENABLE);

    pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW1));
    if ( pw != nullptr )  pw->SetState(STATE_ENABLE);

    pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW2));
    if ( pw != nullptr )  pw->SetState(STATE_ENABLE);

    pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW3));
    if ( pw != nullptr )  pw->SetState(STATE_ENABLE);

    pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW4));
    if ( pw != nullptr )  pw->SetState(STATE_ENABLE);

    pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW5));
    if ( pw != nullptr )  pw->SetState(STATE_ENABLE);

    //pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW6));
    //if ( pw != nullptr )  pw->SetState(STATE_VISIBLE);

    pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW7));
    if ( pw != nullptr )  pw->SetState(STATE_ENABLE);

    pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW8));
    if ( pw != nullptr )  pw->SetState(STATE_ENABLE);

    m_interface->DeleteControl(EVENT_WINDOW9);
    m_main->SetSatComLock(false);  // possible to use the SatCom
}

// Adjust all controls of dialogue after a change in geometry.

void CStudio::AdjustDialog()
{
    CWindow*    pw;
    CButton*    pb;
    CCheck*     pc;
    CLabel*     pla;
    CList*      pli;
    CEdit*      pe;
    Math::Point     wpos, wdim, ppos, ddim;
    int         nli, nch;
    char        name[100];

    pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW9));
    if ( pw == nullptr )  return;

    wpos = pw->GetPos();
    wdim = pw->GetDim();
    pw->SetPos(wpos);  // to move the buttons on the titlebar

    if ( m_dialog == SD_OPEN ||
         m_dialog == SD_SAVE )
    {
        ppos.x = wpos.x+10.0f/640.0f;
        ppos.y = wpos.y+wdim.y-55.0f/480.0f;
        ddim.x = wdim.x-20.0f/640.0f;
        ddim.y = 20.0f/480.0f;
        pla = static_cast< CLabel* >(pw->SearchControl(EVENT_DIALOG_LABEL1));
        if ( pla != nullptr )
        {
            pla->SetPos(ppos);
            pla->SetDim(ddim);
        }

        nli = static_cast<int>((wdim.y-120.0f/480.0f)/(18.0f/480.0f));
        ddim.y = nli*18.0f/480.0f+9.0f/480.0f;
        ppos.y = wpos.y+wdim.y-48.0f/480.0f-ddim.y;
        pli = static_cast< CList* >(pw->SearchControl(EVENT_DIALOG_LIST));
        if ( pli != nullptr )
        {
            pli->SetPos(ppos);
            pli->SetDim(ddim);
            pli->SetTabs(0, ddim.x-(50.0f+130.0f+16.0f)/640.0f);
            pli->SetTabs(1,  50.0f/640.0f, Gfx::TEXT_ALIGN_RIGHT);
            pli->SetTabs(2, 130.0f/640.0f);
//?         pli->ShowSelect();
        }

        ppos.y = wpos.y+30.0f/480.0f;
        ddim.x = 50.0f/640.0f;
        ddim.y = 20.0f/480.0f;
        pla = static_cast< CLabel* >(pw->SearchControl(EVENT_DIALOG_LABEL2));
        if ( pla != nullptr )
        {
            pla->SetPos(ppos);
            pla->SetDim(ddim);
        }

        ppos.x += 50.0f/640.0f;
        ppos.y = wpos.y+36.0f/480.0f;
        ddim.x = wdim.x-170.0f/640.0f;
        pe = static_cast< CEdit* >(pw->SearchControl(EVENT_DIALOG_EDIT));
        if ( pe != nullptr )
        {
            pe->SetPos(ppos);
            pe->SetDim(ddim);

            nch = static_cast< int >((ddim.x*640.0f-22.0f)/8.0f);
            pe->GetText(name, 100);
            pe->SetMaxChar(nch);
            name[nch] = 0;  // truncates the text according to max
            pe->SetText(name);
        }

        ppos.x = wpos.x+10.0f/640.0f;
        ppos.y = wpos.y+5.0f/480.0f;
        ddim.x = 50.0f/640.0f;
        ddim.y = 16.0f/480.0f;
        pla = static_cast< CLabel* >(pw->SearchControl(EVENT_DIALOG_LABEL3));
        if ( pla != nullptr )
        {
            pla->SetPos(ppos);
            pla->SetDim(ddim);
        }

        ppos.x += 50.0f/640.0f;
        ppos.y = wpos.y+12.0f/480.0f;
        ddim.x = 70.0f/640.0f;
        pc = static_cast< CCheck* >(pw->SearchControl(EVENT_DIALOG_CHECK1));
        if ( pc != nullptr )
        {
            pc->SetPos(ppos);
            pc->SetDim(ddim);
        }

        ppos.x += 80.0f/640.0f;
        pc = static_cast< CCheck* >(pw->SearchControl(EVENT_DIALOG_CHECK2));
        if ( pc != nullptr )
        {
            pc->SetPos(ppos);
            pc->SetDim(ddim);
        }

        ppos.x = wpos.x+wdim.x-100.0f/640.0f;
        ppos.y = wpos.y+34.0f/480.0f;
        ddim.x = 90.0f/640.0f;
        ddim.y = 23.0f/480.0f;
        pb = static_cast< CButton* >(pw->SearchControl(EVENT_DIALOG_OK));
        if ( pb != nullptr )
        {
            pb->SetPos(ppos);
            pb->SetDim(ddim);
        }

        ppos.y -= 26.0f/480.0f;
        pb = static_cast< CButton* >(pw->SearchControl(EVENT_DIALOG_CANCEL));
        if ( pb != nullptr )
        {
            pb->SetPos(ppos);
            pb->SetDim(ddim);
        }
    }
}

// Management of the event of a dialogue.

bool CStudio::EventDialog(const Event &event)
{
    CWindow*    pw;
    Math::Point     wpos, wdim;

    pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW9));
    if ( pw == nullptr )  return false;

    if ( event.type == EVENT_WINDOW9 )  // window is moved?
    {
        wpos = pw->GetPos();
        wdim = pw->GetDim();
        m_settings->SetIOPos(wpos);
        m_settings->SetIODim(wdim);
        AdjustDialog();
    }

    if ( m_dialog == SD_OPEN ||
         m_dialog == SD_SAVE )
    {
        if ( event.type == EVENT_DIALOG_LIST )
        {
            UpdateChangeList();
        }
        if ( event.type == EVENT_DIALOG_EDIT )
        {
            UpdateChangeEdit();
        }

        if ( event.type == EVENT_DIALOG_CHECK1 )  // private?
        {
            m_settings->SetIOPublic(false);
            UpdateDialogPublic();
            UpdateDialogList();
        }
        if ( event.type == EVENT_DIALOG_CHECK2 )  // public?
        {
            m_settings->SetIOPublic(true);
            UpdateDialogPublic();
            UpdateDialogList();
        }
    }

    if ( event.type == EVENT_DIALOG_OK ||
         (event.type == EVENT_KEY_DOWN && event.GetData<KeyEventData>()->key == KEY(RETURN)) )
    {
        if ( m_dialog == SD_OPEN )
        {
            if ( !ReadProgram() )  return true;
        }
        if ( m_dialog == SD_SAVE )
        {
            if ( !WriteProgram() )  return true;
        }

        StopDialog();
        return true;
    }

    if ( event.type == EVENT_DIALOG_CANCEL ||
         (event.type == EVENT_KEY_DOWN && event.GetData<KeyEventData>()->key == KEY(ESCAPE)) ||
         event.type == pw->GetEventTypeClose() )
    {
        StopDialog();
        return true;
    }

    return true;
}

// Updates the name after a click in the list.

void CStudio::UpdateChangeList()
{
    CWindow*    pw;
    CList*      pl;
    CEdit*      pe;

    pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW9));
    if ( pw == nullptr )  return;
    pl = static_cast< CList* >(pw->SearchControl(EVENT_DIALOG_LIST));
    if ( pl == nullptr )  return;
    pe = static_cast< CEdit* >(pw->SearchControl(EVENT_DIALOG_EDIT));
    if ( pe == nullptr )  return;

    std::string name = pl->GetItemName(pl->GetSelect());
    name = name.substr(0, name.find_first_of("\t"));
    SetFilenameField(pe, name);
    pe->SetCursor(999, 0);  // selects all
    m_interface->SetFocus(pe);

    UpdateDialogAction();
}

void CStudio::SetFilenameField(CEdit* edit, const std::string& filename)
{
    std::string name = filename;
    if (name.length() > static_cast<unsigned int>(edit->GetMaxChar()))
    {
        if (name.substr(name.length()-4) == ".txt")
            name = name.substr(0, name.length()-4);
        if (name.length() > static_cast<unsigned int>(edit->GetMaxChar()))
        {
            GetLogger()->Warn("Tried to load too long filename!\n");
            name = name.substr(0, edit->GetMaxChar());  // truncates according to max length
        }
    }
    edit->SetText(name.c_str());
}

// Updates the list after a change in name.

void CStudio::UpdateChangeEdit()
{
    CWindow*    pw;
    CList*      pl;

    pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW9));
    if ( pw == nullptr )  return;
    pl = static_cast< CList* >(pw->SearchControl(EVENT_DIALOG_LIST));
    if ( pl == nullptr )  return;

    pl->SetSelect(-1);

    UpdateDialogAction();
}

// Updates the action button.

void CStudio::UpdateDialogAction()
{
    CWindow*    pw;
    CEdit*      pe;
    CButton*    pb;
    char        name[100];
    int         len, i;
    bool        bError;

    pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW9));
    if ( pw == nullptr )  return;
    pe = static_cast< CEdit* >(pw->SearchControl(EVENT_DIALOG_EDIT));
    if ( pe == nullptr )  return;
    pb = static_cast< CButton* >(pw->SearchControl(EVENT_DIALOG_OK));
    if ( pb == nullptr )  return;

    pe->GetText(name, 100);
    len = strlen(name);
    if ( len == 0 )
    {
        bError = true;
    }
    else
    {
        bError = false;
        for ( i=0 ; i<len ; i++ )
        {
            if ( name[i] == '*'  ||
                 name[i] == '?'  ||
                 name[i] == ':'  ||
                 name[i] == '<'  ||
                 name[i] == '>'  ||
                 name[i] == '"'  ||
                 name[i] == '|'  ||
                 name[i] == '/'  ||
                 name[i] == '\\' )  bError = true;
        }
    }

    pb->SetState(STATE_ENABLE, !bError);
}

// Updates the buttons private/public.

void CStudio::UpdateDialogPublic()
{
    CWindow*    pw;
    CCheck*     pc;
    CLabel*     pl;

    pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW9));
    if ( pw == nullptr )  return;

    pc = static_cast< CCheck* >(pw->SearchControl(EVENT_DIALOG_CHECK1));
    if ( pc != nullptr )
    {
        pc->SetState(STATE_CHECK, !m_settings->GetIOPublic());
    }

    pc = static_cast< CCheck* >(pw->SearchControl(EVENT_DIALOG_CHECK2));
    if ( pc != nullptr )
    {
        pc->SetState(STATE_CHECK, m_settings->GetIOPublic());
    }

    pl = static_cast< CLabel* >(pw->SearchControl(EVENT_DIALOG_LABEL1));
    if ( pl != nullptr )
    {
        // GetResource(RES_TEXT, RT_IO_LIST, name); // TODO: unused?
        pl->SetName(SearchDirectory(false).c_str(), false);
    }
}

// Fills the list with all programs saved.

void CStudio::UpdateDialogList()
{
    CWindow*        pw;
    CList*          pl;
    int             i = 0;
    char            time[100];

    pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW9));
    if ( pw == nullptr )  return;
    pl = static_cast< CList* >(pw->SearchControl(EVENT_DIALOG_LIST));
    if ( pl == nullptr )  return;
    pl->Flush();

    if (!CResourceManager::DirectoryExists(SearchDirectory(false)))
        return;

    std::vector<std::string> programs = CResourceManager::ListFiles(SearchDirectory(false));
    for (auto& prog : programs)
    {
        std::ostringstream temp;
        TimeToAscii(CResourceManager::GetLastModificationTime(SearchDirectory(false) + prog), time);
        temp << prog << '\t' << CResourceManager::GetFileSize(SearchDirectory(false) + prog) << "  \t" << time;
        pl->SetItemName(i++, temp.str().c_str());
    }
}

// Constructs the name of the folder or open/save.
// If the folder does not exist, it will be created.
std::string CStudio::SearchDirectory(bool bCreate)
{
    std::string dir;
    if ( m_settings->GetIOPublic() )
    {
        dir = "program";
    }
    else
    {
        dir = m_main->GetPlayerProfile()->GetSaveFile("program");
    }

    if ( bCreate )
    {
        if (!CResourceManager::DirectoryExists(dir))
            CResourceManager::CreateDirectory(dir);
    }
    return dir+"/";
}

// Reads a new program.

bool CStudio::ReadProgram()
{
    CWindow*    pw;
    CEdit*      pe;
    char        filename[100];
    char        dir[100];
    char*       p;

    pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW9));
    if ( pw == nullptr )  return false;

    pe = static_cast< CEdit* >(pw->SearchControl(EVENT_DIALOG_EDIT));
    if ( pe == nullptr )  return false;
    pe->GetText(filename, 100);
    if ( filename[0] == 0 )  return false;

    p = strstr(filename, ".txt");
    if ( p == nullptr || p != filename+strlen(filename)-4 )
    {
        strcat(filename, ".txt");
    }
    strcpy(dir, SearchDirectory(true).c_str());
    strcat(dir, filename);

    pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW3));
    if ( pw == nullptr )  return false;
    pe = static_cast< CEdit* >(pw->SearchControl(EVENT_STUDIO_EDIT));
    if ( pe == nullptr )  return false;

    if ( !pe->ReadText(dir) )  return false;

    m_script->SetFilename(filename);
    ColorizeScript(pe);
    return true;
}

// Writes the current program.

bool CStudio::WriteProgram()
{
    CWindow*    pw;
    CEdit*      pe;
    char        filename[100];
    char        dir[100];
    char*       p;

    pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW9));
    if ( pw == nullptr )  return false;

    pe = static_cast< CEdit* >(pw->SearchControl(EVENT_DIALOG_EDIT));
    if ( pe == nullptr )  return false;
    pe->GetText(filename, 100);
    if ( filename[0] == 0 )  return false;

    p = strstr(filename, ".txt");
    if ( p == nullptr || p != filename+strlen(filename)-4 )
    {
        strcat(filename, ".txt");
    }
    strcpy(dir, SearchDirectory(true).c_str());
    strcat(dir, filename);

    pw = static_cast< CWindow* >(m_interface->SearchControl(EVENT_WINDOW3));
    if ( pw == nullptr )  return false;
    pe = static_cast< CEdit* >(pw->SearchControl(EVENT_STUDIO_EDIT));
    if ( pe == nullptr )  return false;

    if ( !pe->WriteText(std::string(dir)) )  return false;

    m_script->SetFilename(filename);
    return true;
}

}
