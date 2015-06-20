/*
 * This file is part of the Colobot: Gold Edition source code
 * Copyright (C) 2001-2014, Daniel Roux, EPSITEC SA & TerranovaTeam
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


#include "object/object.h"

#include "CBot/CBotDll.h"

#include "app/app.h"

#include "common/global.h"
#include "common/iman.h"
#include "common/restext.h"

#include "graphics/engine/lightman.h"
#include "graphics/engine/lightning.h"
#include "graphics/engine/modelmanager.h"
#include "graphics/engine/particle.h"
#include "graphics/engine/pyro.h"
#include "graphics/engine/terrain.h"

#include "math/geometry.h"

#include "object/auto/auto.h"
#include "object/auto/autojostle.h"
#include "object/brain.h"
#include "object/motion/motion.h"
#include "object/motion/motionvehicle.h"
#include "object/robotmain.h"
#include "object/objman.h"
#include "object/level/parserline.h"
#include "object/level/parserparam.h"
#include "object/level/parserexceptions.h"

#include "physics/physics.h"

#include "script/cbottoken.h"
#include "script/cmdtoken.h"

#include <boost/lexical_cast.hpp>



#define ADJUST_ONBOARD  0       // 1 -> adjusts the camera ONBOARD
#define ADJUST_ARM  0           // 1 -> adjusts the manipulator arm
const float VIRUS_DELAY     = 60.0f;        // duration of virus infection
const float LOSS_SHIELD     = 0.24f;        // loss of the shield by shot
const float LOSS_SHIELD_H   = 0.10f;        // loss of the shield for humans
const float LOSS_SHIELD_M   = 0.02f;        // loss of the shield for the laying

#if ADJUST_ONBOARD
static float debug_x = 0.0f;
static float debug_y = 0.0f;
static float debug_z = 0.0f;
#endif

#if ADJUST_ARM
static float debug_arm1 = 0.0f;
static float debug_arm2 = 0.0f;
static float debug_arm3 = 0.0f;
#endif




// Updates the class Object.

void uObject(CBotVar* botThis, void* user)
{
    CObject*    object = static_cast<CObject*>(user);
    CObject*    power;
    CObject*    fret;
    CPhysics*   physics;
    CBotVar     *pVar, *pSub;
    ObjectType  type;
    Math::Vector    pos;
    float       value;
    int         iValue;

    if ( object == 0 )  return;

    physics = object->GetPhysics();

    // Updates the object's type.
    pVar = botThis->GetItemList();  // "category"
    type = object->GetType();
    pVar->SetValInt(type, object->GetName());

    // Updates the position of the object.
    pVar = pVar->GetNext();  // "position"
    if ( object->GetTruck() == 0 )
    {
        pos = object->GetPosition(0);
        pos.y -= object->GetWaterLevel();  // relative to sea level!
        pSub = pVar->GetItemList();  // "x"
        pSub->SetValFloat(pos.x/g_unit);
        pSub = pSub->GetNext();  // "y"
        pSub->SetValFloat(pos.z/g_unit);
        pSub = pSub->GetNext();  // "z"
        pSub->SetValFloat(pos.y/g_unit);
    }
    else    // object transported?
    {
        pSub = pVar->GetItemList();  // "x"
        pSub->SetInit(IS_NAN);
        pSub = pSub->GetNext();  // "y"
        pSub->SetInit(IS_NAN);
        pSub = pSub->GetNext();  // "z"
        pSub->SetInit(IS_NAN);
    }

    // Updates the angle.
    pos = object->GetAngle(0);
    pos += object->GetInclinaison();
    pVar = pVar->GetNext();  // "orientation"
    pVar->SetValFloat(360.0f-Math::Mod(pos.y*180.0f/Math::PI, 360.0f));
    pVar = pVar->GetNext();  // "pitch"
    pVar->SetValFloat(pos.z*180.0f/Math::PI);
    pVar = pVar->GetNext();  // "roll"
    pVar->SetValFloat(pos.x*180.0f/Math::PI);

    // Updates the energy level of the object.
    pVar = pVar->GetNext();  // "energyLevel"
    value = object->GetEnergy();
    pVar->SetValFloat(value);

    // Updates the shield level of the object.
    pVar = pVar->GetNext();  // "shieldLevel"
    value = object->GetShield();
    pVar->SetValFloat(value);

    // Updates the temperature of the reactor.
    pVar = pVar->GetNext();  // "temperature"
    if ( physics == 0 )  value = 0.0f;
    else                 value = 1.0f-physics->GetReactorRange();
    pVar->SetValFloat(value);

    // Updates the height above the ground.
    pVar = pVar->GetNext();  // "altitude"
    if ( physics == 0 )  value = 0.0f;
    else                 value = physics->GetFloorHeight();
    pVar->SetValFloat(value/g_unit);

    // Updates the lifetime of the object.
    pVar = pVar->GetNext();  // "lifeTime"
    value = object->GetAbsTime();
    pVar->SetValFloat(value);

    // Updates the material of the object.
    pVar = pVar->GetNext();  // "material"
    iValue = object->GetMaterial();
    pVar->SetValInt(iValue);

    // Updates the type of battery.
    pVar = pVar->GetNext();  // "energyCell"
    power = object->GetPower();
    if ( power == 0 )  pVar->SetPointer(0);
    else               pVar->SetPointer(power->GetBotVar());

    // Updates the transported object's type.
    pVar = pVar->GetNext();  // "load"
    fret = object->GetFret();
    if ( fret == 0 )  pVar->SetPointer(0);
    else              pVar->SetPointer(fret->GetBotVar());

    pVar = pVar->GetNext();  // "id"
    value = object->GetID();
    pVar->SetValInt(value);
}




// Object's constructor.

CObject::CObject()
{
    m_app         = CApplication::GetInstancePointer();
    m_sound       = m_app->GetSound();
    m_engine      = Gfx::CEngine::GetInstancePointer();
    m_lightMan    = m_engine->GetLightManager();
    m_water       = m_engine->GetWater();
    m_particle    = m_engine->GetParticle();
    m_main        = CRobotMain::GetInstancePointer();
    m_terrain     = m_main->GetTerrain();
    m_camera      = m_main->GetCamera();
    m_physics     = nullptr;
    m_brain       = nullptr;
    m_motion      = nullptr;
    m_auto        = nullptr;
    m_runScript   = nullptr;

    m_type = OBJECT_FIX;
    m_id = ++g_id;
    m_option = 0;
    m_name[0] = 0;
    m_partiReactor  = -1;
    m_shadowLight   = -1;
    m_effectLight   = -1;
    m_linVibration  = Math::Vector(0.0f, 0.0f, 0.0f);
    m_cirVibration  = Math::Vector(0.0f, 0.0f, 0.0f);
    m_inclinaison   = Math::Vector(0.0f, 0.0f, 0.0f);
    m_lastParticle = 0.0f;

    m_power = 0;
    m_fret  = 0;
    m_truck = 0;
    m_truckLink = 0;
    m_energy   = 1.0f;
    m_capacity = 1.0f;
    m_shield   = 1.0f;
    m_range    = 0.0f;
    m_transparency = 0.0f;
    m_lastEnergy = 999.9f;
    m_bHilite = false;
    m_bSelect = false;
    m_bSelectable = true;
    m_bCheckToken = true;
    m_bVisible = true;
    m_bEnable = true;
    m_bGadget = false;
    m_bProxyActivate = false;
    m_bTrainer = false;
    m_bToy = false;
    m_bManual = false;
    m_bFixed = false;
    m_bClip = true;
    m_bShowLimit = false;
    m_showLimitRadius = 0.0f;
    m_aTime = 0.0f;
    m_shotTime = 0.0f;
    m_bVirusMode = false;
    m_virusTime = 0.0f;
    m_lastVirusParticle = 0.0f;
    m_totalDesectList = 0;
    m_bLock  = false;
    m_bIgnoreBuildCheck = false;
    m_bExplo = false;
    m_bCargo = false;
    m_bBurn  = false;
    m_bDead  = false;
    m_bFlat  = false;
    m_gunGoalV = 0.0f;
    m_gunGoalH = 0.0f;
    m_shieldRadius = 0.0f;
    m_defRank = -1;
    m_magnifyDamage = 1.0f;
    m_proxyDistance = 60.0f;
    m_param = 0.0f;

    memset(&m_character, 0, sizeof(m_character));
    m_character.wheelFront = 1.0f;
    m_character.wheelBack  = 1.0f;
    m_character.wheelLeft  = 1.0f;
    m_character.wheelRight = 1.0f;

    m_resetCap      = RESET_NONE;
    m_bResetBusy    = false;
    m_resetPosition = Math::Vector(0.0f, 0.0f, 0.0f);
    m_resetAngle    = Math::Vector(0.0f, 0.0f, 0.0f);
    m_resetRun      = nullptr;

    m_cameraType = Gfx::CAM_TYPE_BACK;
    m_cameraDist = 50.0f;
    m_bCameraLock = false;

    m_infoTotal = 0;
    m_infoReturn = NAN;
    m_bInfoUpdate = false;

    for (int i=0 ; i<OBJECTMAXPART ; i++ )
    {
        m_objectPart[i].bUsed = false;
    }
    m_totalPart = 0;

    for (int i=0 ; i<4 ; i++ )
    {
        m_partiSel[i] = -1;
    }

    for (int i=0 ; i<OBJECTMAXCMDLINE ; i++ )
    {
        m_cmdLine[i] = NAN;
    }

    FlushCrashShere();
    m_globalSpherePos = Math::Vector(0.0f, 0.0f, 0.0f);
    m_globalSphereRadius = 0.0f;
    m_jotlerSpherePos = Math::Vector(0.0f, 0.0f, 0.0f);
    m_jotlerSphereRadius = 0.0f;

    CBotClass* bc = CBotClass::Find("object");
    if ( bc != 0 )
    {
        bc->AddUpdateFunc(uObject);
    }

    m_botVar = CBotVar::Create("", CBotTypResult(CBotTypClass, "object"));
    m_botVar->SetUserPtr(this);
    m_botVar->SetIdent(m_id);

    CObjectManager::GetInstancePointer()->AddObject(this);
}

// Object's destructor.

CObject::~CObject()
{
    if ( m_botVar != 0 )
    {
        m_botVar->SetUserPtr(OBJECTDELETED);
        delete m_botVar;
        m_botVar = nullptr;
    }

    delete m_physics;
    m_physics = nullptr;
    delete m_brain;
    m_brain = nullptr;
    delete m_motion;
    m_motion = nullptr;
    delete m_auto;
    m_auto = nullptr;
    
    CObjectManager::GetInstancePointer()->DeleteObject(this);

    m_app = nullptr;
}


// Removes an object.
// If bAll = true, it does not help,
// because all objects in the scene are quickly destroyed!

void CObject::DeleteObject(bool bAll)
{
    CObject*    pObj;
    Gfx::CPyro* pPyro;

    if ( m_botVar != 0 )
    {
        m_botVar->SetUserPtr(OBJECTDELETED);
    }

    if ( m_camera->GetControllingObject() == this )
    {
        m_camera->SetControllingObject(0);
    }
    
    CInstanceManager* iMan = CInstanceManager::GetInstancePointer();
    
    for(auto it : CObjectManager::GetInstancePointer()->GetAllObjects())
    {
        pObj = it.second;

        pObj->DeleteDeselList(this);
    }

    if ( !bAll )
    {
#if 0
        type = m_camera->GetType();
        if ( (type == Gfx::CAM_TYPE_BACK   ||
              type == Gfx::CAM_TYPE_FIX    ||
              type == Gfx::CAM_TYPE_EXPLO  ||
              type == Gfx::CAM_TYPE_ONBOARD) &&
             m_camera->GetControllingObject() == this )
        {
            pObj = m_main->SearchNearest(GetPosition(0), this);
            if ( pObj == 0 )
            {
                m_camera->SetControllingObject(0);
                m_camera->SetType(Gfx::CAM_TYPE_FREE);
            }
            else
            {
                m_camera->SetControllingObject(pObj);
                m_camera->SetType(Gfx::CAM_TYPE_BACK);
            }
        }
#endif
        for (int i=0 ; i<1000000 ; i++ )
        {
            pPyro = static_cast<Gfx::CPyro*>(iMan->SearchInstance(CLASS_PYRO, i));
            if ( pPyro == 0 )  break;

            pPyro->CutObjectLink(this);  // the object no longer exists
        }

        if ( m_bSelect )
        {
            SetSelect(false);
        }

        if ( m_type == OBJECT_BASE     ||
             m_type == OBJECT_FACTORY  ||
             m_type == OBJECT_REPAIR   ||
             m_type == OBJECT_DESTROYER||
             m_type == OBJECT_DERRICK  ||
             m_type == OBJECT_STATION  ||
             m_type == OBJECT_CONVERT  ||
             m_type == OBJECT_TOWER    ||
             m_type == OBJECT_RESEARCH ||
             m_type == OBJECT_RADAR    ||
             m_type == OBJECT_INFO     ||
             m_type == OBJECT_ENERGY   ||
             m_type == OBJECT_LABO     ||
             m_type == OBJECT_NUCLEAR  ||
             m_type == OBJECT_PARA     ||
             m_type == OBJECT_SAFE     ||
             m_type == OBJECT_HUSTON   ||
             m_type == OBJECT_START    ||
             m_type == OBJECT_END      )  // building?
        {
            m_terrain->DeleteBuildingLevel(GetPosition(0));  // flattens the field
        }
    }

    m_type = OBJECT_NULL;  // invalid object until complete destruction

    if ( m_partiReactor != -1 )
    {
        m_particle->DeleteParticle(m_partiReactor);
        m_partiReactor = -1;
    }

    if ( m_shadowLight != -1 )
    {
        m_lightMan->DeleteLight(m_shadowLight);
        m_shadowLight = -1;
    }

    if ( m_effectLight != -1 )
    {
        m_lightMan->DeleteLight(m_effectLight);
        m_effectLight = -1;
    }

    if ( m_physics != 0 )
    {
        m_physics->DeleteObject(bAll);
    }

    if ( m_brain != 0 )
    {
        m_brain->DeleteObject(bAll);
    }

    if ( m_motion != 0 )
    {
        m_motion->DeleteObject(bAll);
    }

    if ( m_auto != 0 )
    {
        m_auto->DeleteObject(bAll);
    }

    for (int i=0 ; i<OBJECTMAXPART ; i++ )
    {
        if ( m_objectPart[i].bUsed )
        {
            m_objectPart[i].bUsed = false;
            m_engine->DeleteObject(m_objectPart[i].object);

            if ( m_objectPart[i].masterParti != -1 )
            {
                m_particle->DeleteParticle(m_objectPart[i].masterParti);
                m_objectPart[i].masterParti = -1;
            }
        }
    }

    if ( m_bShowLimit )
    {
        m_main->FlushShowLimit(0);
        m_bShowLimit = false;
    }

    if ( !bAll )  m_main->CreateShortcuts();
}

// Simplifies a object (he was the brain, among others).

void CObject::Simplify()
{
    if ( m_brain != 0 )
    {
        m_brain->StopProgram();
    }
    m_main->SaveOneScript(this);

    if ( m_physics != 0 )
    {
        m_physics->DeleteObject();
        delete m_physics;
        m_physics = 0;
    }

    if ( m_brain != 0 )
    {
        m_brain->DeleteObject();
        delete m_brain;
        m_brain = 0;
    }

    if ( m_motion != 0 )
    {
        m_motion->DeleteObject();
        delete m_motion;
        m_motion = 0;
    }

    if ( m_auto != 0 )
    {
        m_auto->DeleteObject();
        delete m_auto;
        m_auto = 0;
    }

    m_main->CreateShortcuts();
}


// Detonates an object, when struck by a shot.
// If false is returned, the object is still screwed.
// If true is returned, the object is destroyed.

bool CObject::ExploObject(ExploType type, float force, float decay)
{
    Gfx::PyroType    pyroType;
    Gfx::CPyro*      pyro;
    float       loss, shield;

    if ( type == EXPLO_BURN )
    {
        if ( m_type == OBJECT_MOBILEtg ||
             m_type == OBJECT_TEEN28    ||  // cylinder?
             m_type == OBJECT_METAL    ||
             m_type == OBJECT_POWER    ||
             m_type == OBJECT_ATOMIC   ||
             m_type == OBJECT_TNT      ||
             m_type == OBJECT_SCRAP1   ||
             m_type == OBJECT_SCRAP2   ||
             m_type == OBJECT_SCRAP3   ||
             m_type == OBJECT_SCRAP4   ||
             m_type == OBJECT_SCRAP5   ||
             m_type == OBJECT_BULLET   ||
             m_type == OBJECT_EGG      )  // object that isn't burning?
        {
            type = EXPLO_BOUM;
            force = 1.0f;
            decay = 1.0f;
        }
    }

    if ( type == EXPLO_BOUM )
    {
        if ( m_shotTime < 0.5f )  return false;
        m_shotTime = 0.0f;
    }

    if ( m_type == OBJECT_HUMAN && m_bDead )  return false;

    // Calculate the power lost by the explosion.
    if ( force == 0.0f )
    {
        if ( m_type == OBJECT_HUMAN )
        {
            loss = LOSS_SHIELD_H;
        }
        else if ( m_type == OBJECT_MOTHER )
        {
            loss = LOSS_SHIELD_M;
        }
        else
        {
            loss = LOSS_SHIELD;
        }
    }
    else
    {
        loss = force;
    }
    loss *= m_magnifyDamage;
    loss *= decay;

    // Decreases the power of the shield.
    shield = GetShield();
    shield -= loss;
    if ( shield < 0.0f )  shield = 0.0f;
    SetShield(shield);

    if ( shield > 0.0f )  // not dead yet?
    {
        if ( type == EXPLO_WATER )
        {
            if ( m_type == OBJECT_HUMAN )
            {
                pyroType = Gfx::PT_SHOTH;
            }
            else
            {
                pyroType = Gfx::PT_SHOTW;
            }
        }
        else
        {
            if ( m_type == OBJECT_HUMAN )
            {
                pyroType = Gfx::PT_SHOTH;
            }
            else if ( m_type == OBJECT_MOTHER )
            {
                pyroType = Gfx::PT_SHOTM;
            }
            else
            {
                pyroType = Gfx::PT_SHOTT;
            }
        }
    }
    else    // completely dead?
    {
        if ( type == EXPLO_BURN )  // burning?
        {
            if ( m_type == OBJECT_MOTHER ||
                 m_type == OBJECT_ANT    ||
                 m_type == OBJECT_SPIDER ||
                 m_type == OBJECT_BEE    ||
                 m_type == OBJECT_WORM   ||
                 m_type == OBJECT_BULLET )
            {
                pyroType = Gfx::PT_BURNO;
                SetBurn(true);
            }
            else if ( m_type == OBJECT_HUMAN )
            {
                pyroType = Gfx::PT_DEADG;
            }
            else
            {
                pyroType = Gfx::PT_BURNT;
                SetBurn(true);
            }
            SetVirusMode(false);
        }
        else if ( type == EXPLO_WATER )
        {
            if ( m_type == OBJECT_HUMAN )
            {
                pyroType = Gfx::PT_DEADW;
            }
            else
            {
                pyroType = Gfx::PT_FRAGW;
            }
        }
        else    // explosion?
        {
            if ( m_type == OBJECT_ANT    ||
                 m_type == OBJECT_SPIDER ||
                 m_type == OBJECT_BEE    ||
                 m_type == OBJECT_WORM   )
            {
                pyroType = Gfx::PT_EXPLOO;
            }
            else if ( m_type == OBJECT_MOTHER ||
                      m_type == OBJECT_NEST   ||
                      m_type == OBJECT_BULLET )
            {
                pyroType = Gfx::PT_FRAGO;
            }
            else if ( m_type == OBJECT_HUMAN )
            {
                pyroType = Gfx::PT_DEADG;
            }
            else if ( m_type == OBJECT_BASE     ||
                      m_type == OBJECT_DERRICK  ||
                      m_type == OBJECT_FACTORY  ||
                      m_type == OBJECT_STATION  ||
                      m_type == OBJECT_CONVERT  ||
                      m_type == OBJECT_REPAIR   ||
                      m_type == OBJECT_DESTROYER||
                      m_type == OBJECT_TOWER    ||
                      m_type == OBJECT_NEST     ||
                      m_type == OBJECT_RESEARCH ||
                      m_type == OBJECT_RADAR    ||
                      m_type == OBJECT_INFO     ||
                      m_type == OBJECT_ENERGY   ||
                      m_type == OBJECT_LABO     ||
                      m_type == OBJECT_NUCLEAR  ||
                      m_type == OBJECT_PARA     ||
                      m_type == OBJECT_SAFE     ||
                      m_type == OBJECT_HUSTON   ||
                      m_type == OBJECT_START    ||
                      m_type == OBJECT_END      )  // building?
            {
                pyroType = Gfx::PT_FRAGT;
            }
            else if ( m_type == OBJECT_MOBILEtg ||
                      m_type == OBJECT_TEEN28    ||  // cylinder?
                      m_type == OBJECT_TEEN31    )   // basket?
            {
                pyroType = Gfx::PT_FRAGT;
            }
            else
            {
                pyroType = Gfx::PT_EXPLOT;
            }
        }

        loss = 1.0f;
    }

    pyro = new Gfx::CPyro();
    pyro->Create(pyroType, this, loss);

    if ( shield == 0.0f )  // dead?
    {
        if ( m_brain != 0 )
        {
            m_brain->StopProgram();
        }
        m_main->SaveOneScript(this);
    }

    if ( shield > 0.0f )  return false;  // not dead yet

    if ( GetSelect() )
    {
        SetSelect(false);  // deselects the object
        m_camera->SetType(Gfx::CAM_TYPE_EXPLO);
        m_main->DeselectAll();
    }
    DeleteDeselList(this);

    if ( m_botVar != 0 )
    {
        if ( m_type == OBJECT_STONE   ||
             m_type == OBJECT_URANIUM ||
             m_type == OBJECT_METAL   ||
             m_type == OBJECT_POWER   ||
             m_type == OBJECT_ATOMIC  ||
             m_type == OBJECT_BULLET  ||
             m_type == OBJECT_BBOX    ||
             m_type == OBJECT_TNT     ||
             m_type == OBJECT_SCRAP1  ||
             m_type == OBJECT_SCRAP2  ||
             m_type == OBJECT_SCRAP3  ||
             m_type == OBJECT_SCRAP4  ||
             m_type == OBJECT_SCRAP5  )  // (*)
        {
            m_botVar->SetUserPtr(OBJECTDELETED);
        }
    }

    return true;
}

// (*)  If a robot or cosmonaut dies, the subject must continue to exist,
//  so that programs of the ants continue to operate as usual.


// Initializes a new part.

void CObject::InitPart(int part)
{
    m_objectPart[part].bUsed      = true;
    m_objectPart[part].object     = -1;
    m_objectPart[part].parentPart = -1;

    m_objectPart[part].position   = Math::Vector(0.0f, 0.0f, 0.0f);
    m_objectPart[part].angle.y    = 0.0f;
    m_objectPart[part].angle.x    = 0.0f;
    m_objectPart[part].angle.z    = 0.0f;
    m_objectPart[part].zoom       = Math::Vector(1.0f, 1.0f, 1.0f);

    m_objectPart[part].bTranslate = true;
    m_objectPart[part].bRotate    = true;
    m_objectPart[part].bZoom      = false;

    m_objectPart[part].matTranslate.LoadIdentity();
    m_objectPart[part].matRotate.LoadIdentity();
    m_objectPart[part].matTransform.LoadIdentity();
    m_objectPart[part].matWorld.LoadIdentity();;

    m_objectPart[part].masterParti = -1;
}

// Creates a new part, and returns its number.
// Returns -1 on error.

int CObject::CreatePart()
{
    int     i;

    for ( i=0 ; i<OBJECTMAXPART ; i++ )
    {
        if ( m_objectPart[i].bUsed )  continue;

        InitPart(i);
        UpdateTotalPart();
        return i;
    }
    return -1;
}

// Removes part.

void CObject::DeletePart(int part)
{
    if ( !m_objectPart[part].bUsed )  return;

    if ( m_objectPart[part].masterParti != -1 )
    {
        m_particle->DeleteParticle(m_objectPart[part].masterParti);
        m_objectPart[part].masterParti = -1;
    }

    m_objectPart[part].bUsed = false;
    m_engine->DeleteObject(m_objectPart[part].object);
    UpdateTotalPart();
}

void CObject::UpdateTotalPart()
{
    int     i;

    m_totalPart = 0;
    for ( i=0 ; i<OBJECTMAXPART ; i++ )
    {
        if ( m_objectPart[i].bUsed )
        {
            m_totalPart = i+1;
        }
    }
}


// Specifies the number of the object of a part.

void CObject::SetObjectRank(int part, int objRank)
{
    if ( !m_objectPart[part].bUsed )  // object not created?
    {
        InitPart(part);
        UpdateTotalPart();
    }
    m_objectPart[part].object = objRank;
}

// Returns the number of part.

int CObject::GetObjectRank(int part)
{
    if ( !m_objectPart[part].bUsed )  return -1;
    return m_objectPart[part].object;
}

// Specifies what is the parent of a part.
// Reminder: Part 0 is always the father of all
// and therefore the main part (eg the chassis of a car).

void CObject::SetObjectParent(int part, int parent)
{
    m_objectPart[part].parentPart = parent;
}


// Specifies the type of the object.

void CObject::SetType(ObjectType type)
{
    m_type = type;
    strcpy(m_name, GetObjectName(m_type));

    if ( m_type == OBJECT_MOBILErs )
    {
        m_param = 1.0f;  // shield up to default
    }

    if ( m_type == OBJECT_ATOMIC )
    {
        m_capacity = 10.0f;
    }
    else
    {
        m_capacity = 1.0f;
    }

    if ( m_type == OBJECT_MOBILEwc ||
         m_type == OBJECT_MOBILEtc ||
         m_type == OBJECT_MOBILEfc ||
         m_type == OBJECT_MOBILEic ||
         m_type == OBJECT_MOBILEwi ||
         m_type == OBJECT_MOBILEti ||
         m_type == OBJECT_MOBILEfi ||
         m_type == OBJECT_MOBILEii ||
         m_type == OBJECT_MOBILErc )  // cannon vehicle?
    {
        m_cameraType = Gfx::CAM_TYPE_ONBOARD;
    }
}

ObjectType CObject::GetType()
{
    return m_type;
}

char* CObject::GetName()
{
    return m_name;
}


// Choosing the option to use.

void CObject::SetOption(int option)
{
    m_option = option;
}

int CObject::GetOption()
{
    return m_option;
}


// Management of the unique identifier of an object.

void CObject::SetID(int id)
{
    m_id = id;

    if ( m_botVar != 0 )
    {
        m_botVar->SetIdent(m_id);
    }
}

int CObject::GetID()
{
    return m_id;
}


// Saves all the parameters of the object.

bool CObject::Write(CLevelParserLine* line)
{
    Math::Vector    pos;
    Info        info;
    float       value;
    int         i;

    line->AddParam("camera", new CLevelParserParam(GetCameraType()));

    if ( GetCameraLock() )
        line->AddParam("cameraLock", new CLevelParserParam(GetCameraLock()));

    if ( GetEnergy() != 0.0f )
        line->AddParam("energy", new CLevelParserParam(GetEnergy()));

    if ( GetCapacity() != 1.0f )
        line->AddParam("capacity", new CLevelParserParam(GetCapacity()));
    
    if ( GetShield() != 1.0f )
        line->AddParam("shield", new CLevelParserParam(GetShield()));

    if ( GetRange() != 1.0f )
        line->AddParam("range", new CLevelParserParam(GetRange()));
    
    if ( !GetSelectable() )
        line->AddParam("selectable", new CLevelParserParam(GetSelectable()));

    if ( !GetEnable() )
        line->AddParam("enable", new CLevelParserParam(GetEnable()));

    if ( GetFixed() )
        line->AddParam("fixed", new CLevelParserParam(GetFixed()));

    if ( !GetClip() )
        line->AddParam("clip", new CLevelParserParam(GetClip()));

    if ( GetLock() )
        line->AddParam("lock", new CLevelParserParam(GetLock()));

    if ( GetProxyActivate() )
    {
        line->AddParam("proxyActivate", new CLevelParserParam(GetProxyActivate()));
        line->AddParam("proxyDistance", new CLevelParserParam(GetProxyDistance()/g_unit));
    }

    if ( GetMagnifyDamage() != 1.0f )
        line->AddParam("magnifyDamage", new CLevelParserParam(GetMagnifyDamage()));

    if ( GetGunGoalV() != 0.0f )
        line->AddParam("aimV", new CLevelParserParam(GetGunGoalV()));
    
    if ( GetGunGoalH() != 0.0f )
        line->AddParam("aimH", new CLevelParserParam(GetGunGoalH()));

    if ( GetParam() != 0.0f )
        line->AddParam("param", new CLevelParserParam(GetParam()));

    if ( GetResetCap() != 0 )
    {
        line->AddParam("resetCap", new CLevelParserParam(static_cast<int>(GetResetCap())));
        line->AddParam("resetPos", new CLevelParserParam(GetResetPosition()/g_unit));
        line->AddParam("resetAngle", new CLevelParserParam(GetResetAngle()/(Math::PI/180.0f)));
        line->AddParam("resetRun", new CLevelParserParam(m_brain->GetProgramIndex(GetResetRun())));
    }

    if ( m_bVirusMode )
        line->AddParam("virusMode", new CLevelParserParam(m_bVirusMode));

    if ( m_virusTime != 0.0f )
        line->AddParam("virusTime", new CLevelParserParam(m_virusTime));

    // Puts information in terminal (OBJECT_INFO).
    for ( i=0 ; i<m_infoTotal ; i++ )
    {
        info = GetInfo(i);
        if ( info.name[0] == 0 )  break;

        line->AddParam("info"+boost::lexical_cast<std::string>(i+1), new CLevelParserParam(std::string(info.name)+"="+boost::lexical_cast<std::string>(info.value)));
    }

    // Sets the parameters of the command line.
    std::vector<CLevelParserParam*> cmdline;
    for ( i=0 ; i<OBJECTMAXCMDLINE ; i++ )
    {
        value = GetCmdLine(i);
        if ( std::isnan(value) )  break;

        cmdline.push_back(new CLevelParserParam(value));
    }
    if(cmdline.size() > 0)
        line->AddParam("cmdline", new CLevelParserParam(cmdline));

    if ( m_motion != nullptr )
    {
        m_motion->Write(line);
    }

    if ( m_brain != nullptr )
    {
        m_brain->Write(line);
    }

    if ( m_physics != nullptr )
    {
        m_physics->Write(line);
    }

    if ( m_auto != nullptr )
    {
        m_auto->Write(line);
    }

    return true;
}

// Returns all parameters of the object.

bool CObject::Read(CLevelParserLine* line)
{
    Math::Vector    pos, dir;
    Gfx::CameraType cType;
    int             i;

    cType = line->GetParam("camera")->AsCameraType(Gfx::CAM_TYPE_NULL);
    if ( cType != Gfx::CAM_TYPE_NULL )
    {
        SetCameraType(cType);
    }

    SetCameraLock(line->GetParam("cameraLock")->AsBool(false));
    SetEnergy(line->GetParam("energy")->AsFloat(0.0f));
    SetCapacity(line->GetParam("capacity")->AsFloat(1.0f));
    SetShield(line->GetParam("shield")->AsFloat(1.0f));
    SetRange(line->GetParam("range")->AsFloat(1.0f));
    SetSelectable(line->GetParam("selectable")->AsBool(true));
    SetEnable(line->GetParam("enable")->AsBool(true));
    SetFixed(line->GetParam("fixed")->AsBool(false));
    SetClip(line->GetParam("clip")->AsBool(true));
    SetLock(line->GetParam("lock")->AsBool(false));
    SetProxyActivate(line->GetParam("proxyActivate")->AsBool(false));
    SetProxyDistance(line->GetParam("proxyDistance")->AsFloat(15.0f)*g_unit);
    SetRange(line->GetParam("range")->AsFloat(30.0f));
    SetMagnifyDamage(line->GetParam("magnifyDamage")->AsFloat(1.0f));
    SetGunGoalV(line->GetParam("aimV")->AsFloat(0.0f));
    SetGunGoalH(line->GetParam("aimH")->AsFloat(0.0f));
    SetParam(line->GetParam("param")->AsFloat(0.0f));
    SetResetCap(static_cast<ResetCap>(line->GetParam("resetCap")->AsInt(0)));
    SetResetPosition(line->GetParam("resetPos")->AsPoint(Math::Vector())*g_unit);
    SetResetAngle(line->GetParam("resetAngle")->AsPoint(Math::Vector())*(Math::PI/180.0f));
    SetResetRun(m_brain->GetProgram(line->GetParam("resetRun")->AsInt(-1)));
    m_bBurn = line->GetParam("burnMode")->AsBool(false);
    m_bVirusMode = line->GetParam("virusMode")->AsBool(false);
    m_virusTime = line->GetParam("virusTime")->AsFloat(0.0f);

    // Puts information in terminal (OBJECT_INFO).
    for ( i=0 ; i<OBJECTMAXINFO ; i++ )
    {
        std::string op = std::string("info")+boost::lexical_cast<std::string>(i+1);
        if(!line->GetParam(op)->IsDefined()) break;
        std::string text = line->GetParam(op)->AsString();
        
        std::size_t p = text.find_first_of("=");
        if(p == std::string::npos) 
            throw CLevelParserExceptionBadParam(line->GetParam(op), "info");
        Info info;
        strcpy(info.name, text.substr(0, p).c_str());
        try {
            info.value = boost::lexical_cast<float>(text.substr(p+1).c_str());
        }
        catch(...)
        {
            throw CLevelParserExceptionBadParam(line->GetParam(op), "info.value (float)");
        }
        
        SetInfo(i, info);
    }

    // Sets the parameters of the command line.
    i = 0;
    if(line->GetParam("cmdline")->IsDefined()) {
        for(auto& p : line->GetParam("cmdline")->AsArray()) {
            if(i >= OBJECTMAXCMDLINE) break;
            SetCmdLine(i, p->AsFloat());
            i++;
        }
    }

    if ( m_motion != nullptr )
    {
        m_motion->Read(line);
    }

    if ( m_brain != nullptr )
    {
        m_brain->Read(line);
    }

    if ( m_physics != nullptr )
    {
        m_physics->Read(line);
    }

    if ( m_auto != nullptr )
    {
        m_auto->Read(line);
    }

    return true;
}



// Seeking the nth son of a father.

int CObject::SearchDescendant(int parent, int n)
{
    int     i;

    for ( i=0 ; i<m_totalPart ; i++ )
    {
        if ( !m_objectPart[i].bUsed )  continue;

        if ( parent == m_objectPart[i].parentPart )
        {
            if ( n-- == 0 )  return i;
        }
    }
    return -1;
}


// Removes all spheres used for collisions.

void CObject::FlushCrashShere()
{
    m_crashSphereUsed = 0;
}

// Adds a new sphere.

int CObject::CreateCrashSphere(Math::Vector pos, float radius, Sound sound,
                               float hardness)
{
    float   zoom;

    if ( m_crashSphereUsed >= MAXCRASHSPHERE )  return -1;

    zoom = GetZoomX(0);
    m_crashSpherePos[m_crashSphereUsed] = pos;
    m_crashSphereRadius[m_crashSphereUsed] = radius*zoom;
    m_crashSphereHardness[m_crashSphereUsed] = hardness;
    m_crashSphereSound[m_crashSphereUsed] = sound;
    return m_crashSphereUsed++;
}

// Returns the number of spheres.

int CObject::GetCrashSphereTotal()
{
    return m_crashSphereUsed;
}

// Returns a sphere for collisions.
// The position is absolute in the world.

bool CObject::GetCrashSphere(int rank, Math::Vector &pos, float &radius)
{
    if ( rank < 0 || rank >= m_crashSphereUsed )
    {
        pos = m_objectPart[0].position;
        radius = 0.0f;
        return false;
    }

    // Returns to the sphere collisions,
    // which ignores the inclination of the vehicle.
    // This is necessary to collisions with vehicles,
    // so as not to reflect SetInclinaison, for example.
    // The sphere must necessarily have a center (0, y, 0).
    if ( rank == 0 && m_crashSphereUsed == 1 &&
         m_crashSpherePos[0].x == 0.0f &&
         m_crashSpherePos[0].z == 0.0f )
    {
        pos = m_objectPart[0].position + m_crashSpherePos[0];
        radius = m_crashSphereRadius[0];
        return true;
    }

    if ( m_objectPart[0].bTranslate ||
         m_objectPart[0].bRotate    )
    {
        UpdateTransformObject();
    }
    pos = Math::Transform(m_objectPart[0].matWorld, m_crashSpherePos[rank]);
    radius = m_crashSphereRadius[rank];
    return true;
}

// Returns the hardness of a sphere.

Sound CObject::GetCrashSphereSound(int rank)
{
    return m_crashSphereSound[rank];
}

// Returns the hardness of a sphere.

float CObject::GetCrashSphereHardness(int rank)
{
    return m_crashSphereHardness[rank];
}

// Deletes a sphere.

void CObject::DeleteCrashSphere(int rank)
{
    int     i;

    if ( rank < 0 || rank >= m_crashSphereUsed )  return;

    for ( i=rank+1 ; i<MAXCRASHSPHERE ; i++ )
    {
        m_crashSpherePos[i-1]    = m_crashSpherePos[i];
        m_crashSphereRadius[i-1] = m_crashSphereRadius[i];
    }
    m_crashSphereUsed --;
}

// Specifies the global sphere, relative to the object.

void CObject::SetGlobalSphere(Math::Vector pos, float radius)
{
    float   zoom;

    zoom = GetZoomX(0);
    m_globalSpherePos    = pos;
    m_globalSphereRadius = radius*zoom;
}

// Returns the global sphere, in the world.

void CObject::GetGlobalSphere(Math::Vector &pos, float &radius)
{
    pos = Math::Transform(m_objectPart[0].matWorld, m_globalSpherePos);
    radius = m_globalSphereRadius;
}


// Specifies the sphere of jostling, relative to the object.

void CObject::SetJotlerSphere(Math::Vector pos, float radius)
{
    m_jotlerSpherePos    = pos;
    m_jotlerSphereRadius = radius;
}

// Specifies the sphere of jostling, in the world.

void CObject::GetJotlerSphere(Math::Vector &pos, float &radius)
{
    pos = Math::Transform(m_objectPart[0].matWorld, m_jotlerSpherePos);
    radius = m_jotlerSphereRadius;
}


// Specifies the radius of the shield.

void CObject::SetShieldRadius(float radius)
{
    m_shieldRadius = radius;
}

// Returns the radius of the shield.

float CObject::GetShieldRadius()
{
    return m_shieldRadius;
}


// Positioning an object on a certain height, above the ground.

void CObject::SetFloorHeight(float height)
{
    Math::Vector    pos;

    pos = m_objectPart[0].position;
    m_terrain->AdjustToFloor(pos);

    if ( m_physics != 0 )
    {
        m_physics->SetLand(height == 0.0f);
        m_physics->SetMotor(height != 0.0f);
    }

    m_objectPart[0].position.y = pos.y+height+m_character.height;
    m_objectPart[0].bTranslate = true;  // it will recalculate the matrices
}

// Adjust the inclination of an object laying on the ground.

void CObject::FloorAdjust()
{
    Math::Vector        pos, n;
    Math::Point         nn;
    float           a;

    pos = GetPosition(0);
    if ( m_terrain->GetNormal(n, pos) )
    {
#if 0
        SetAngleX(0,  sinf(n.z));
        SetAngleZ(0, -sinf(n.x));
        SetAngleY(0, 0.0f);
#else
        a = GetAngleY(0);
        nn = Math::RotatePoint(-a, Math::Point(n.z, n.x));
        SetAngleX(0,  sinf(nn.x));
        SetAngleZ(0, -sinf(nn.y));
#endif
    }
}


// Getes the linear vibration.

void CObject::SetLinVibration(Math::Vector dir)
{
    if ( m_linVibration.x != dir.x ||
         m_linVibration.y != dir.y ||
         m_linVibration.z != dir.z )
    {
        m_linVibration = dir;
        m_objectPart[0].bTranslate = true;
    }
}

Math::Vector CObject::GetLinVibration()
{
    return m_linVibration;
}

// Getes the circular vibration.

void CObject::SetCirVibration(Math::Vector dir)
{
    if ( m_cirVibration.x != dir.x ||
         m_cirVibration.y != dir.y ||
         m_cirVibration.z != dir.z )
    {
        m_cirVibration = dir;
        m_objectPart[0].bRotate = true;
    }
}

Math::Vector CObject::GetCirVibration()
{
    return m_cirVibration;
}

// Getes the inclination.

void CObject::SetInclinaison(Math::Vector dir)
{
    if ( m_inclinaison.x != dir.x ||
         m_inclinaison.y != dir.y ||
         m_inclinaison.z != dir.z )
    {
        m_inclinaison = dir;
        m_objectPart[0].bRotate = true;
    }
}

Math::Vector CObject::GetInclinaison()
{
    return m_inclinaison;
}


// Getes the position of center of the object.

void CObject::SetPosition(int part, const Math::Vector &pos)
{
    Math::Vector    shPos, n[20], norm;
    float       height, radius;
    int         rank, i, j;

    m_objectPart[part].position = pos;
    m_objectPart[part].bTranslate = true;  // it will recalculate the matrices

    if ( part == 0 && !m_bFlat )  // main part?
    {
        rank = m_objectPart[0].object;

        shPos = pos;
        m_terrain->AdjustToFloor(shPos, true);
        m_engine->SetObjectShadowPos(rank, shPos);

        if ( m_physics != 0 && m_physics->GetType() == TYPE_FLYING )
        {
            height = pos.y-shPos.y;
        }
        else
        {
            height = 0.0f;
        }
        m_engine->SetObjectShadowHeight(rank, height);

        // Calculating the normal to the ground in nine strategic locations,
        // then perform a weighted average (the dots in the center are more important).
        radius = m_engine->GetObjectShadowRadius(rank);
        i = 0;

        m_terrain->GetNormal(norm, pos);
        n[i++] = norm;
        n[i++] = norm;
        n[i++] = norm;

        shPos = pos;
        shPos.x += radius*0.6f;
        shPos.z += radius*0.6f;
        m_terrain->GetNormal(norm, shPos);
        n[i++] = norm;
        n[i++] = norm;

        shPos = pos;
        shPos.x -= radius*0.6f;
        shPos.z += radius*0.6f;
        m_terrain->GetNormal(norm, shPos);
        n[i++] = norm;
        n[i++] = norm;

        shPos = pos;
        shPos.x += radius*0.6f;
        shPos.z -= radius*0.6f;
        m_terrain->GetNormal(norm, shPos);
        n[i++] = norm;
        n[i++] = norm;

        shPos = pos;
        shPos.x -= radius*0.6f;
        shPos.z -= radius*0.6f;
        m_terrain->GetNormal(norm, shPos);
        n[i++] = norm;
        n[i++] = norm;

        shPos = pos;
        shPos.x += radius;
        shPos.z += radius;
        m_terrain->GetNormal(norm, shPos);
        n[i++] = norm;

        shPos = pos;
        shPos.x -= radius;
        shPos.z += radius;
        m_terrain->GetNormal(norm, shPos);
        n[i++] = norm;

        shPos = pos;
        shPos.x += radius;
        shPos.z -= radius;
        m_terrain->GetNormal(norm, shPos);
        n[i++] = norm;

        shPos = pos;
        shPos.x -= radius;
        shPos.z -= radius;
        m_terrain->GetNormal(norm, shPos);
        n[i++] = norm;

        norm.LoadZero();
        for ( j=0 ; j<i ; j++ )
        {
            norm += n[j];
        }
        norm /= static_cast<float>(i);  // average vector

        m_engine->SetObjectShadowNormal(rank, norm);

        if ( m_shadowLight != -1 )
        {
            shPos = pos;
            shPos.y += m_shadowHeight;
            m_lightMan->SetLightPos(m_shadowLight, shPos);
        }

        if ( m_effectLight != -1 )
        {
            shPos = pos;
            shPos.y += m_effectHeight;
            m_lightMan->SetLightPos(m_effectLight, shPos);
        }

        if ( m_bShowLimit )
        {
            m_main->AdjustShowLimit(0, pos);
        }
    }
}

Math::Vector CObject::GetPosition(int part)
{
    return m_objectPart[part].position;
}

// Getes the rotation around three axis.

void CObject::SetAngle(int part, const Math::Vector &angle)
{
    m_objectPart[part].angle = angle;
    m_objectPart[part].bRotate = true;  // it will recalculate the matrices

    if ( part == 0 && !m_bFlat )  // main part?
    {
        m_engine->SetObjectShadowAngle(m_objectPart[0].object, m_objectPart[0].angle.y);
    }
}

Math::Vector CObject::GetAngle(int part)
{
    return m_objectPart[part].angle;
}

// Getes the rotation about the axis Y.

void CObject::SetAngleY(int part, float angle)
{
    m_objectPart[part].angle.y = angle;
    m_objectPart[part].bRotate = true;  // it will recalculate the matrices

    if ( part == 0 && !m_bFlat )  // main part?
    {
        m_engine->SetObjectShadowAngle(m_objectPart[0].object, m_objectPart[0].angle.y);
    }
}

// Getes the rotation about the axis X.

void CObject::SetAngleX(int part, float angle)
{
    m_objectPart[part].angle.x = angle;
    m_objectPart[part].bRotate = true;  // it will recalculate the matrices
}

// Getes the rotation about the axis Z.

void CObject::SetAngleZ(int part, float angle)
{
    m_objectPart[part].angle.z = angle;
    m_objectPart[part].bRotate = true;  //it will recalculate the matrices
}

float CObject::GetAngleY(int part)
{
    return m_objectPart[part].angle.y;
}

float CObject::GetAngleX(int part)
{
    return m_objectPart[part].angle.x;
}

float CObject::GetAngleZ(int part)
{
    return m_objectPart[part].angle.z;
}


// Getes the global zoom.

void CObject::SetZoom(int part, float zoom)
{
    m_objectPart[part].bTranslate = true;  // it will recalculate the matrices
    m_objectPart[part].zoom.x = zoom;
    m_objectPart[part].zoom.y = zoom;
    m_objectPart[part].zoom.z = zoom;

    m_objectPart[part].bZoom = ( m_objectPart[part].zoom.x != 1.0f ||
                                 m_objectPart[part].zoom.y != 1.0f ||
                                 m_objectPart[part].zoom.z != 1.0f );
}

void CObject::SetZoom(int part, Math::Vector zoom)
{
    m_objectPart[part].bTranslate = true;  // it will recalculate the matrices
    m_objectPart[part].zoom = zoom;

    m_objectPart[part].bZoom = ( m_objectPart[part].zoom.x != 1.0f ||
                                 m_objectPart[part].zoom.y != 1.0f ||
                                 m_objectPart[part].zoom.z != 1.0f );
}

Math::Vector CObject::GetZoom(int part)
{
    return m_objectPart[part].zoom;
}

void CObject::SetZoomX(int part, float zoom)
{
    m_objectPart[part].bTranslate = true;  // it will recalculate the matrices
    m_objectPart[part].zoom.x = zoom;

    m_objectPart[part].bZoom = ( m_objectPart[part].zoom.x != 1.0f ||
                                 m_objectPart[part].zoom.y != 1.0f ||
                                 m_objectPart[part].zoom.z != 1.0f );
}

void CObject::SetZoomY(int part, float zoom)
{
    m_objectPart[part].bTranslate = true;  // it will recalculate the matrices
    m_objectPart[part].zoom.y = zoom;

    m_objectPart[part].bZoom = ( m_objectPart[part].zoom.x != 1.0f ||
                                 m_objectPart[part].zoom.y != 1.0f ||
                                 m_objectPart[part].zoom.z != 1.0f );
}

void CObject::SetZoomZ(int part, float zoom)
{
    m_objectPart[part].bTranslate = true;  // it will recalculate the matrices
    m_objectPart[part].zoom.z = zoom;

    m_objectPart[part].bZoom = ( m_objectPart[part].zoom.x != 1.0f ||
                                 m_objectPart[part].zoom.y != 1.0f ||
                                 m_objectPart[part].zoom.z != 1.0f );
}

float CObject::GetZoomX(int part)
{
    return m_objectPart[part].zoom.x;
}

float CObject::GetZoomY(int part)
{
    return m_objectPart[part].zoom.y;
}

float CObject::GetZoomZ(int part)
{
    return m_objectPart[part].zoom.z;
}


// Returns the water level.

float CObject::GetWaterLevel()
{
    return m_water->GetLevel();
}


void CObject::SetTrainer(bool bEnable)
{
    m_bTrainer = bEnable;

    if ( m_bTrainer )  // training?
    {
        m_cameraType = Gfx::CAM_TYPE_FIX;
    }
}

bool CObject::GetTrainer()
{
    return m_bTrainer;
}

void CObject::SetToy(bool bEnable)
{
    m_bToy = bEnable;
}

bool CObject::GetToy()
{
    return m_bToy;
}

void CObject::SetManual(bool bManual)
{
    m_bManual = bManual;
}

bool CObject::GetManual()
{
    return m_bManual;
}

void CObject::SetResetCap(ResetCap cap)
{
    m_resetCap = cap;
}

ResetCap CObject::GetResetCap()
{
    return m_resetCap;
}

void CObject::SetResetBusy(bool bBusy)
{
    m_bResetBusy = bBusy;
}

bool CObject::GetResetBusy()
{
    return m_bResetBusy;
}

void CObject::SetResetPosition(const Math::Vector &pos)
{
    m_resetPosition = pos;
}

Math::Vector CObject::GetResetPosition()
{
    return m_resetPosition;
}

void CObject::SetResetAngle(const Math::Vector &angle)
{
    m_resetAngle = angle;
}

Math::Vector CObject::GetResetAngle()
{
    return m_resetAngle;
}

Program* CObject::GetResetRun()
{
    return m_resetRun;
}

void CObject::SetResetRun(Program* run)
{
    m_resetRun = run;
}


// Management of the particle master.

void CObject::SetMasterParticle(int part, int parti)
{
    m_objectPart[part].masterParti = parti;
}

int CObject::GetMasterParticle(int part)
{
    return m_objectPart[part].masterParti;
}


// Management of the stack transport.

void CObject::SetPower(CObject* power)
{
    m_power = power;
}

CObject* CObject::GetPower()
{
    return m_power;
}

// Management of the object transport.

void CObject::SetFret(CObject* fret)
{
    m_fret = fret;
}

CObject* CObject::GetFret()
{
    return m_fret;
}

// Management of the object "truck" that transports it.

void CObject::SetTruck(CObject* truck)
{
    m_truck = truck;

    // Invisible shadow if the object is transported.
    m_engine->SetObjectShadowHide(m_objectPart[0].object, (m_truck != 0));
}

CObject* CObject::GetTruck()
{
    return m_truck;
}

// Management of the conveying portion.

void CObject::SetTruckPart(int part)
{
    m_truckLink = part;
}

int CObject::GetTruckPart()
{
    return m_truckLink;
}


// Management of user information.

void CObject::InfoFlush()
{
    m_infoTotal = 0;
    m_bInfoUpdate = true;
}

void CObject::DeleteInfo(int rank)
{
    int     i;

    if ( rank < 0 || rank >= m_infoTotal )  return;

    for ( i=rank ; i<m_infoTotal-1 ; i++ )
    {
        m_info[i] = m_info[i+1];
    }
    m_infoTotal --;
    m_bInfoUpdate = true;
}

void CObject::SetInfo(int rank, Info info)
{
    if ( rank < 0 || rank >= OBJECTMAXINFO )  return;
    m_info[rank] = info;

    if ( rank+1 > m_infoTotal )  m_infoTotal = rank+1;
    m_bInfoUpdate = true;
}

Info CObject::GetInfo(int rank)
{
    if ( rank < 0 || rank >= OBJECTMAXINFO )  rank = 0;
    return m_info[rank];
}

int CObject::GetInfoTotal()
{
    return m_infoTotal;
}

void CObject::SetInfoReturn(float value)
{
    m_infoReturn = value;
}

float CObject::GetInfoReturn()
{
    return m_infoReturn;
}

void CObject::SetInfoUpdate(bool bUpdate)
{
    m_bInfoUpdate = bUpdate;
}

bool CObject::GetInfoUpdate()
{
    return m_bInfoUpdate;
}


bool CObject::SetCmdLine(int rank, float value)
{
    if ( rank < 0 || rank >= OBJECTMAXCMDLINE )  return false;
    m_cmdLine[rank] = value;
    return true;
}

float CObject::GetCmdLine(int rank)
{
    if ( rank < 0 || rank >= OBJECTMAXCMDLINE )  return 0.0f;
    return m_cmdLine[rank];
}


// Returns matrices of an object portion.

Math::Matrix* CObject::GetRotateMatrix(int part)
{
    return &m_objectPart[part].matRotate;
}

Math::Matrix* CObject::GetTranslateMatrix(int part)
{
    return &m_objectPart[part].matTranslate;
}

Math::Matrix* CObject::GetTransformMatrix(int part)
{
    return &m_objectPart[part].matTransform;
}

Math::Matrix* CObject::GetWorldMatrix(int part)
{
    if ( m_objectPart[0].bTranslate ||
         m_objectPart[0].bRotate    )
    {
        UpdateTransformObject();
    }

    return &m_objectPart[part].matWorld;
}


// Indicates whether the object should be drawn below the interface.

void CObject::SetDrawWorld(bool bDraw)
{
    int     i;

    for ( i=0 ; i<OBJECTMAXPART ; i++ )
    {
        if ( m_objectPart[i].bUsed )
        {
            m_engine->SetObjectDrawWorld(m_objectPart[i].object, bDraw);
        }
    }
}

// Indicates whether the object should be drawn over the interface.

void CObject::SetDrawFront(bool bDraw)
{
    int     i;

    for ( i=0 ; i<OBJECTMAXPART ; i++ )
    {
        if ( m_objectPart[i].bUsed )
        {
            m_engine->SetObjectDrawFront(m_objectPart[i].object, bDraw);
        }
    }
}

// Creates shade under a vehicle as a negative light.

bool CObject::CreateShadowLight(float height, Gfx::Color color)
{
    if ( !m_engine->GetLightMode() )  return true;

    Math::Vector pos = GetPosition(0);
    m_shadowHeight = height;

    Gfx::Light light;
    light.type          = Gfx::LIGHT_SPOT;
    light.diffuse       = color;
    light.ambient       = color * 0.1f;
    light.position      = Math::Vector(pos.x, pos.y+height, pos.z);
    light.direction     = Math::Vector(0.0f, -1.0f, 0.0f); // against the bottom
    light.spotIntensity = 128;
    light.attenuation0  = 1.0f;
    light.attenuation1  = 0.0f;
    light.attenuation2  = 0.0f;
    light.spotAngle = 90.0f*Math::PI/180.0f;

    m_shadowLight = m_lightMan->CreateLight();
    if ( m_shadowLight == -1 )  return false;

    m_lightMan->SetLight(m_shadowLight, light);

    // Only illuminates the objects on the ground.
    m_lightMan->SetLightIncludeType(m_shadowLight, Gfx::ENG_OBJTYPE_TERRAIN);

    return true;
}

// Returns the number of negative light shade.

int CObject::GetShadowLight()
{
    return m_shadowLight;
}

// Creates light for the effects of a vehicle.

bool CObject::CreateEffectLight(float height, Gfx::Color color)
{
    if ( !m_engine->GetLightMode() )  return true;

    m_effectHeight = height;

    Gfx::Light light;
    light.type       = Gfx::LIGHT_SPOT;
    light.diffuse    = color;
    light.position   = Math::Vector(0.0f, height, 0.0f);
    light.direction  = Math::Vector(0.0f, -1.0f, 0.0f); // against the bottom
    light.spotIntensity = 0.0f;
    light.attenuation0 = 1.0f;
    light.attenuation1 = 0.0f;
    light.attenuation2 = 0.0f;
    light.spotAngle = 90.0f*Math::PI/180.0f;

    m_effectLight = m_lightMan->CreateLight();
    if ( m_effectLight == -1 )  return false;

    m_lightMan->SetLight(m_effectLight, light);
    m_lightMan->SetLightIntensity(m_effectLight, 0.0f);

    return true;
}

// Returns the number of light effects.

int CObject::GetEffectLight()
{
    return m_effectLight;
}

// Creates the circular shadow underneath a vehicle.

bool CObject::CreateShadowCircle(float radius, float intensity,
                                 Gfx::EngineShadowType type)
{
    float   zoom;

    if ( intensity == 0.0f )  return true;

    zoom = GetZoomX(0);

    m_engine->CreateShadow(m_objectPart[0].object);

    m_engine->SetObjectShadowRadius(m_objectPart[0].object, radius*zoom);
    m_engine->SetObjectShadowIntensity(m_objectPart[0].object, intensity);
    m_engine->SetObjectShadowHeight(m_objectPart[0].object, 0.0f);
    m_engine->SetObjectShadowAngle(m_objectPart[0].object, m_objectPart[0].angle.y);
    m_engine->SetObjectShadowType(m_objectPart[0].object, type);

    return true;
}

// Reads a program.

bool CObject::ReadProgram(Program* program, const char* filename)
{
    if ( m_brain != 0 )
    {
        return m_brain->ReadProgram(program, filename);
    }
    return false;
}

// Writes a program.

bool CObject::WriteProgram(Program* program, char* filename)
{
    if ( m_brain != 0 )
    {
        return m_brain->WriteProgram(program, filename);
    }
    return false;
}



// Calculates the matrix for transforming the object.
// Returns true if the matrix has changed.
// The rotations occur in the order Y, Z and X.

bool CObject::UpdateTransformObject(int part, bool bForceUpdate)
{
    Math::Vector    position, angle, eye;
    bool        bModif = false;
    int         parent;

    if ( m_truck != 0 )  // transported by truck?
    {
        m_objectPart[part].bTranslate = true;
        m_objectPart[part].bRotate = true;
    }

    if ( !bForceUpdate                  &&
         !m_objectPart[part].bTranslate &&
         !m_objectPart[part].bRotate    )  return false;

    position = m_objectPart[part].position;
    angle    = m_objectPart[part].angle;

    if ( part == 0 )  // main part?
    {
        position += m_linVibration;
        angle    += m_cirVibration+m_inclinaison;
    }

    if ( m_objectPart[part].bTranslate ||
         m_objectPart[part].bRotate    )
    {
        if ( m_objectPart[part].bTranslate )
        {
            m_objectPart[part].matTranslate.LoadIdentity();
            m_objectPart[part].matTranslate.Set(1, 4, position.x);
            m_objectPart[part].matTranslate.Set(2, 4, position.y);
            m_objectPart[part].matTranslate.Set(3, 4, position.z);
        }

        if ( m_objectPart[part].bRotate )
        {
            Math::LoadRotationZXYMatrix(m_objectPart[part].matRotate, angle);
        }

        if ( m_objectPart[part].bZoom )
        {
            Math::Matrix    mz;
            mz.LoadIdentity();
            mz.Set(1, 1, m_objectPart[part].zoom.x);
            mz.Set(2, 2, m_objectPart[part].zoom.y);
            mz.Set(3, 3, m_objectPart[part].zoom.z);
            m_objectPart[part].matTransform = Math::MultiplyMatrices(m_objectPart[part].matTranslate,
                                                Math::MultiplyMatrices(m_objectPart[part].matRotate, mz));
        }
        else
        {
            m_objectPart[part].matTransform = Math::MultiplyMatrices(m_objectPart[part].matTranslate,
                                                                     m_objectPart[part].matRotate);
        }
        bModif = true;
    }

    if ( bForceUpdate                  ||
         m_objectPart[part].bTranslate ||
         m_objectPart[part].bRotate    )
    {
        parent = m_objectPart[part].parentPart;

        if ( part == 0 && m_truck != 0 )  // transported by a truck?
        {
            Math::Matrix*   matWorldTruck;
            matWorldTruck = m_truck->GetWorldMatrix(m_truckLink);
            m_objectPart[part].matWorld = Math::MultiplyMatrices(*matWorldTruck,
                                                                 m_objectPart[part].matTransform);
        }
        else
        {
            if ( parent == -1 )  // no parent?
            {
                m_objectPart[part].matWorld = m_objectPart[part].matTransform;
            }
            else
            {
                m_objectPart[part].matWorld = Math::MultiplyMatrices(m_objectPart[parent].matWorld,
                                                                     m_objectPart[part].matTransform);
            }
        }
        bModif = true;
    }

    if ( bModif )
    {
        m_engine->SetObjectTransform(m_objectPart[part].object,
                                     m_objectPart[part].matWorld);
    }

    m_objectPart[part].bTranslate = false;
    m_objectPart[part].bRotate    = false;

    return bModif;
}

// Updates all matrices to transform the object father and all his sons.
// Assume a maximum of 4 degrees of freedom.
// Appropriate, for example, to a body, an arm, forearm, hand and fingers.

bool CObject::UpdateTransformObject()
{
    bool    bUpdate1, bUpdate2, bUpdate3, bUpdate4;
    int     level1, level2, level3, level4, rank;
    int     parent1, parent2, parent3, parent4;

    if ( m_bFlat )
    {
        for ( level1=0 ; level1<m_totalPart ; level1++ )
        {
            if ( !m_objectPart[level1].bUsed )  continue;
            UpdateTransformObject(level1, false);
        }
    }
    else
    {
        parent1 = 0;
        bUpdate1 = UpdateTransformObject(parent1, false);

        for ( level1=0 ; level1<m_totalPart ; level1++ )
        {
            rank = SearchDescendant(parent1, level1);
            if ( rank == -1 )  break;

            parent2 = rank;
            bUpdate2 = UpdateTransformObject(rank, bUpdate1);

            for ( level2=0 ; level2<m_totalPart ; level2++ )
            {
                rank = SearchDescendant(parent2, level2);
                if ( rank == -1 )  break;

                parent3 = rank;
                bUpdate3 = UpdateTransformObject(rank, bUpdate2);

                for ( level3=0 ; level3<m_totalPart ; level3++ )
                {
                    rank = SearchDescendant(parent3, level3);
                    if ( rank == -1 )  break;

                    parent4 = rank;
                    bUpdate4 = UpdateTransformObject(rank, bUpdate3);

                    for ( level4=0 ; level4<m_totalPart ; level4++ )
                    {
                        rank = SearchDescendant(parent4, level4);
                        if ( rank == -1 )  break;

                        UpdateTransformObject(rank, bUpdate4);
                    }
                }
            }
        }
    }

    return true;
}


// Puts all the progeny flat (there is more than fathers).
// This allows for debris independently from each other in all directions.

void CObject::FlatParent()
{
    int     i;

    for ( i=0 ; i<m_totalPart ; i++ )
    {
        m_objectPart[i].position.x = m_objectPart[i].matWorld.Get(1, 4);
        m_objectPart[i].position.y = m_objectPart[i].matWorld.Get(2, 4);
        m_objectPart[i].position.z = m_objectPart[i].matWorld.Get(3, 4);

        m_objectPart[i].matWorld.Set(1, 4, 0.0f);
        m_objectPart[i].matWorld.Set(2, 4, 0.0f);
        m_objectPart[i].matWorld.Set(3, 4, 0.0f);

        m_objectPart[i].matTranslate.Set(1, 4, 0.0f);
        m_objectPart[i].matTranslate.Set(2, 4, 0.0f);
        m_objectPart[i].matTranslate.Set(3, 4, 0.0f);

        m_objectPart[i].parentPart = -1;  // more parents
    }

    m_bFlat = true;
}



// Updates the mapping of the texture of the pile.

void CObject::UpdateEnergyMapping()
{
    if (Math::IsEqual(m_energy, m_lastEnergy, 0.01f))
        return;

    m_lastEnergy = m_energy;

    Gfx::Material mat;
    mat.diffuse = Gfx::Color(1.0f, 1.0f, 1.0f);  // white
    mat.ambient = Gfx::Color(0.5f, 0.5f, 0.5f);

    float a = 0.0f, b = 0.0f;

    if ( m_type == OBJECT_POWER  ||
         m_type == OBJECT_ATOMIC )
    {
        a = 2.0f;
        b = 0.0f;  // dimensions of the battery (according to y)
    }
    else if ( m_type == OBJECT_STATION )
    {
        a = 10.0f;
        b =  4.0f;  // dimensions of the battery (according to y)
    }
    else if ( m_type == OBJECT_ENERGY )
    {
        a = 9.0f;
        b = 3.0f;  // dimensions of the battery (according to y)
    }

    float i = 0.50f+0.25f*m_energy;  // origin
    float s = i+0.25f;  // width

    float au = (s-i)/(b-a);
    float bu = s-b*(s-i)/(b-a);

    Gfx::LODLevel lodLevels[3] = { Gfx::LOD_High, Gfx::LOD_Medium, Gfx::LOD_Low };

    for (int j = 0; j < 3; j++)
    {
        m_engine->ChangeTextureMapping(m_objectPart[0].object,
                                       mat, Gfx::ENG_RSTATE_PART3, "objects/lemt.png", "",
                                       lodLevels[j], Gfx::ENG_TEX_MAPPING_1Y,
                                       au, bu, 1.0f, 0.0f);
    }
}


// Manual action.

bool CObject::EventProcess(const Event &event)
{
    if ( event.type == EVENT_KEY_DOWN )
    {
#if ADJUST_ONBOARD
        if ( m_bSelect )
        {
            if ( event.param == 'E' )  debug_x += 0.1f;
            if ( event.param == 'D' )  debug_x -= 0.1f;
            if ( event.param == 'R' )  debug_y += 0.1f;
            if ( event.param == 'F' )  debug_y -= 0.1f;
            if ( event.param == 'T' )  debug_z += 0.1f;
            if ( event.param == 'G' )  debug_z -= 0.1f;
        }
#endif
#if ADJUST_ARM
        if ( m_bSelect )
        {
            if ( event.param == 'X' )  debug_arm1 += 5.0f*Math::PI/180.0f;
            if ( event.param == 'C' )  debug_arm1 -= 5.0f*Math::PI/180.0f;
            if ( event.param == 'V' )  debug_arm2 += 5.0f*Math::PI/180.0f;
            if ( event.param == 'B' )  debug_arm2 -= 5.0f*Math::PI/180.0f;
            if ( event.param == 'N' )  debug_arm3 += 5.0f*Math::PI/180.0f;
            if ( event.param == 'M' )  debug_arm3 -= 5.0f*Math::PI/180.0f;
            if ( event.param == 'X' ||
                 event.param == 'C' ||
                 event.param == 'V' ||
                 event.param == 'B' ||
                 event.param == 'N' ||
                 event.param == 'M' )
            {
                SetAngleZ(1, debug_arm1);
                SetAngleZ(2, debug_arm2);
                SetAngleZ(3, debug_arm3);
                char s[100];
                sprintf(s, "a=%.2f b=%.2f c=%.2f", debug_arm1*180.0f/Math::PI, debug_arm2*180.0f/Math::PI, debug_arm3*180.0f/Math::PI);
                m_engine->SetInfoText(5, s);
            }
        }
#endif
    }

    if ( m_physics != 0 )
    {
        if ( !m_physics->EventProcess(event) )  // object destroyed?
        {
            if ( GetSelect()             &&
                 m_type != OBJECT_ANT    &&
                 m_type != OBJECT_SPIDER &&
                 m_type != OBJECT_BEE    )
            {
                if ( !m_bDead )  m_camera->SetType(Gfx::CAM_TYPE_EXPLO);
                m_main->DeselectAll();
            }
            return false;
        }
    }

    if ( m_auto != 0 )
    {
        m_auto->EventProcess(event);

        if ( event.type == EVENT_FRAME &&
             m_auto->IsEnded() != ERR_CONTINUE )
        {
            m_auto->DeleteObject();
            delete m_auto;
            m_auto = 0;
        }
    }

    if ( m_motion != 0 )
    {
        m_motion->EventProcess(event);
    }

    if ( event.type == EVENT_FRAME )
    {
        return EventFrame(event);
    }

    return true;
}


// Animates the object.

bool CObject::EventFrame(const Event &event)
{
    if ( m_type == OBJECT_HUMAN && m_main->GetMainMovie() == MM_SATCOMopen )
    {
        UpdateTransformObject();
        return true;
    }

    if ( m_type != OBJECT_SHOW && m_engine->GetPause() )  return true;

    m_aTime += event.rTime;
    m_shotTime += event.rTime;

    VirusFrame(event.rTime);
    PartiFrame(event.rTime);

    UpdateMapping();
    UpdateTransformObject();
    UpdateSelectParticle();

    if ( m_bProxyActivate )  // active if it is near?
    {
        Gfx::CPyro*      pyro;
        Math::Vector    eye;
        float       dist;

        eye = m_engine->GetLookatPt();
        dist = Math::Distance(eye, GetPosition(0));
        if ( dist < m_proxyDistance )
        {
            m_bProxyActivate = false;
            m_main->CreateShortcuts();
            m_sound->Play(SOUND_FINDING);
            pyro = new Gfx::CPyro();
            pyro->Create(Gfx::PT_FINDING, this, 0.0f);
            m_main->DisplayError(INFO_FINDING, this);
        }
    }

    return true;
}

// Updates the mapping of the object.

void CObject::UpdateMapping()
{
    if ( m_type == OBJECT_POWER   ||
         m_type == OBJECT_ATOMIC  ||
         m_type == OBJECT_STATION ||
         m_type == OBJECT_ENERGY  )
    {
        UpdateEnergyMapping();
    }
}


// Management of viruses.

void CObject::VirusFrame(float rTime)
{
    Gfx::ParticleType   type;
    Math::Vector        pos, speed;
    Math::Point         dim;
    int                 r;

    if ( !m_bVirusMode )  return;  // healthy object?

    m_virusTime += rTime;
    if ( m_virusTime >= VIRUS_DELAY )
    {
        m_bVirusMode = false;  // the virus is no longer active
    }

    if ( m_lastVirusParticle+m_engine->ParticleAdapt(0.2f) <= m_aTime )
    {
        m_lastVirusParticle = m_aTime;

        r = rand()%10;
        if ( r == 0 )  type = Gfx::PARTIVIRUS1;
        if ( r == 1 )  type = Gfx::PARTIVIRUS2;
        if ( r == 2 )  type = Gfx::PARTIVIRUS3;
        if ( r == 3 )  type = Gfx::PARTIVIRUS4;
        if ( r == 4 )  type = Gfx::PARTIVIRUS5;
        if ( r == 5 )  type = Gfx::PARTIVIRUS6;
        if ( r == 6 )  type = Gfx::PARTIVIRUS7;
        if ( r == 7 )  type = Gfx::PARTIVIRUS8;
        if ( r == 8 )  type = Gfx::PARTIVIRUS9;
        if ( r == 9 )  type = Gfx::PARTIVIRUS10;

        pos = GetPosition(0);
        pos.x += (Math::Rand()-0.5f)*10.0f;
        pos.z += (Math::Rand()-0.5f)*10.0f;
        speed.x = (Math::Rand()-0.5f)*2.0f;
        speed.z = (Math::Rand()-0.5f)*2.0f;
        speed.y = Math::Rand()*4.0f+4.0f;
        dim.x = Math::Rand()*0.3f+0.3f;
        dim.y = dim.x;

        m_particle->CreateParticle(pos, speed, dim, type, 3.0f);
    }
}

// Management particles mistresses.

void CObject::PartiFrame(float rTime)
{
    Math::Vector    pos, angle, factor;
    int         i, channel;

    for ( i=0 ; i<OBJECTMAXPART ; i++ )
    {
        if ( !m_objectPart[i].bUsed )  continue;

        channel = m_objectPart[i].masterParti;
        if ( channel == -1 )  continue;

        if ( !m_particle->GetPosition(channel, pos) )
        {
            m_objectPart[i].masterParti = -1;  // particle no longer exists!
            continue;
        }

        SetPosition(i, pos);

        // Each song spins differently.
        switch( i%5 )
        {
            case 0:  factor = Math::Vector( 0.5f, 0.3f, 0.6f); break;
            case 1:  factor = Math::Vector(-0.3f, 0.4f,-0.2f); break;
            case 2:  factor = Math::Vector( 0.4f,-0.6f,-0.3f); break;
            case 3:  factor = Math::Vector(-0.6f,-0.2f, 0.0f); break;
            case 4:  factor = Math::Vector( 0.4f, 0.1f,-0.7f); break;
        }

        angle = GetAngle(i);
        angle += rTime*Math::PI*factor;
        SetAngle(i, angle);
    }
}


// Changes the perspective to view if it was like in the vehicle,
// or behind the vehicle.

void CObject::SetViewFromHere(Math::Vector &eye, float &dirH, float &dirV,
                              Math::Vector  &lookat, Math::Vector &upVec,
                              Gfx::CameraType type)
{
    float   speed;
    int     part;

    UpdateTransformObject();

    part = 0;
    if ( m_type == OBJECT_HUMAN ||
         m_type == OBJECT_TECH  )
    {
        eye.x = -0.2f;
        eye.y =  3.3f;
        eye.z =  0.0f;
//?     eye.x =  1.0f;
//?     eye.y =  3.3f;
//?     eye.z =  0.0f;
    }
    else if ( m_type == OBJECT_MOBILErt ||
              m_type == OBJECT_MOBILErr ||
              m_type == OBJECT_MOBILErs )
    {
        eye.x = -1.1f;  // on the cap
        eye.y =  7.9f;
        eye.z =  0.0f;
    }
    else if ( m_type == OBJECT_MOBILEwc ||
              m_type == OBJECT_MOBILEtc ||
              m_type == OBJECT_MOBILEfc ||
              m_type == OBJECT_MOBILEic )  // fireball?
    {
//?     eye.x = -0.9f;  // on the cannon
//?     eye.y =  3.0f;
//?     eye.z =  0.0f;
//?     part = 1;
        eye.x = -0.9f;  // on the cannon
        eye.y =  8.3f;
        eye.z =  0.0f;
    }
    else if ( m_type == OBJECT_MOBILEwi ||
              m_type == OBJECT_MOBILEti ||
              m_type == OBJECT_MOBILEfi ||
              m_type == OBJECT_MOBILEii )  // orgaball ?
    {
//?     eye.x = -3.5f;  // on the cannon
//?     eye.y =  5.1f;
//?     eye.z =  0.0f;
//?     part = 1;
        eye.x = -2.5f;  // on the cannon
        eye.y = 10.4f;
        eye.z =  0.0f;
    }
    else if ( m_type == OBJECT_MOBILErc )
    {
//?     eye.x =  2.0f;  // in the cannon
//?     eye.y =  0.0f;
//?     eye.z =  0.0f;
//?     part = 2;
        eye.x =  4.0f;  // on the cannon
        eye.y = 11.0f;
        eye.z =  0.0f;
    }
    else if ( m_type == OBJECT_MOBILEsa )
    {
        eye.x =  3.0f;
        eye.y =  4.5f;
        eye.z =  0.0f;
    }
    else if ( m_type == OBJECT_MOBILEdr )
    {
        eye.x =  1.0f;
        eye.y =  6.5f;
        eye.z =  0.0f;
    }
    else if ( m_type == OBJECT_APOLLO2 )
    {
        eye.x = -3.0f;
        eye.y =  6.0f;
        eye.z = -2.0f;
    }
    else
    {
        eye.x = 0.7f;  // between the brackets
        eye.y = 4.8f;
        eye.z = 0.0f;
    }
#if ADJUST_ONBOARD
    eye.x += debug_x;
    eye.y += debug_y;
    eye.z += debug_z;
    char s[100];
    sprintf(s, "x=%.2f y=%.2f z=%.2f", eye.x, eye.y, eye.z);
    m_engine->SetInfoText(4, s);
#endif

    if ( type == Gfx::CAM_TYPE_BACK )
    {
        eye.x -= 20.0f;
        eye.y +=  1.0f;
    }

    lookat.x = eye.x+1.0f;
    lookat.y = eye.y+0.0f;
    lookat.z = eye.z+0.0f;

    eye    = Math::Transform(m_objectPart[part].matWorld, eye);
    lookat = Math::Transform(m_objectPart[part].matWorld, lookat);

    // Camera tilts when turning.
    upVec = Math::Vector(0.0f, 1.0f, 0.0f);
    if ( m_physics != 0 )
    {
        if ( m_physics->GetLand() )  // on ground?
        {
            speed = m_physics->GetLinMotionX(MO_REASPEED);
            lookat.y -= speed*0.002f;

            speed = m_physics->GetCirMotionY(MO_REASPEED);
            upVec.z -= speed*0.04f;
        }
        else    // in flight?
        {
            speed = m_physics->GetLinMotionX(MO_REASPEED);
            lookat.y += speed*0.002f;

            speed = m_physics->GetCirMotionY(MO_REASPEED);
            upVec.z += speed*0.08f;
        }
    }
    upVec = Math::Transform(m_objectPart[0].matRotate, upVec);

    dirH = -(m_objectPart[part].angle.y+Math::PI/2.0f);
    dirV = 0.0f;

}


// Management of features.

void CObject::SetCharacter(Character* character)
{
    memcpy(&m_character, character, sizeof(Character));
}

void CObject::GetCharacter(Character* character)
{
    memcpy(character, &m_character, sizeof(Character));
}

Character* CObject::GetCharacter()
{
    return &m_character;
}


// Returns the absolute time.

float CObject::GetAbsTime()
{
    return m_aTime;
}


// Management of energy contained in a battery.
// Single subject possesses the battery energy, but not the vehicle that carries the battery!

void CObject::SetEnergy(float level)
{
    if ( level < 0.0f )  level = 0.0f;
    if ( level > 1.0f )  level = 1.0f;
    m_energy = level;
}

float CObject::GetEnergy()
{
    if ( m_type != OBJECT_POWER   &&
         m_type != OBJECT_ATOMIC  &&
         m_type != OBJECT_STATION &&
         m_type != OBJECT_ENERGY  )  return 0.0f;
    return m_energy;
}


// Management of the capacity of a battery.
// Single subject possesses a battery capacity,
// but not the vehicle that carries the battery!

void CObject::SetCapacity(float capacity)
{
    m_capacity = capacity;
}

float CObject::GetCapacity()
{
    return m_capacity;
}


// Management of the shield.

void CObject::SetShield(float level)
{
    m_shield = level;
}

float CObject::GetShield()
{
    if ( m_type == OBJECT_FRET     ||
         m_type == OBJECT_STONE    ||
         m_type == OBJECT_URANIUM  ||
         m_type == OBJECT_BULLET   ||
         m_type == OBJECT_METAL    ||
         m_type == OBJECT_BBOX     ||
         m_type == OBJECT_KEYa     ||
         m_type == OBJECT_KEYb     ||
         m_type == OBJECT_KEYc     ||
         m_type == OBJECT_KEYd     ||
         m_type == OBJECT_TNT      ||
         m_type == OBJECT_TEEN31    ||  // basket?
         m_type == OBJECT_SCRAP1   ||
         m_type == OBJECT_SCRAP2   ||
         m_type == OBJECT_SCRAP3   ||
         m_type == OBJECT_SCRAP4   ||
         m_type == OBJECT_SCRAP5   ||
         m_type == OBJECT_BOMB     ||
         m_type == OBJECT_WAYPOINT ||
         m_type == OBJECT_FLAGb    ||
         m_type == OBJECT_FLAGr    ||
         m_type == OBJECT_FLAGg    ||
         m_type == OBJECT_FLAGy    ||
         m_type == OBJECT_FLAGv    ||
         m_type == OBJECT_POWER    ||
         m_type == OBJECT_ATOMIC   ||
         m_type == OBJECT_ANT      ||
         m_type == OBJECT_SPIDER   ||
         m_type == OBJECT_BEE      ||
         m_type == OBJECT_WORM     )  return 0.0f;
    return m_shield;
}


// Management of flight range (zero = infinity).

void CObject::SetRange(float delay)
{
    m_range = delay;
}

float CObject::GetRange()
{
    return m_range;
}


// Management of transparency of the object.

void CObject::SetTransparency(float value)
{
    int     i;

    m_transparency = value;

    for ( i=0 ; i<m_totalPart ; i++ )
    {
        if ( m_objectPart[i].bUsed )
        {
            if ( m_type == OBJECT_BASE )
            {
                if ( i != 9 )  continue;  // no central pillar?
            }

            m_engine->SetObjectTransparency(m_objectPart[i].object, value);
        }
    }
}

float CObject::GetTransparency()
{
    return m_transparency;
}


// Management of the object matter.

ObjectMaterial CObject::GetMaterial()
{
    if ( m_type == OBJECT_HUMAN )
    {
        return OM_HUMAN;
    }

    if ( m_type == OBJECT_SCRAP4 ||
         m_type == OBJECT_SCRAP5 )
    {
        return OM_HUMAN;
    }

    return OM_METAL;
}


// Indicates whether the gadget is a nonessential.

void CObject::SetGadget(bool bMode)
{
    m_bGadget = bMode;
}

bool CObject::GetGadget()
{
    return m_bGadget;
}


// Indicates whether an object is stationary (ant on the back).

void CObject::SetFixed(bool bFixed)
{
    m_bFixed = bFixed;
}

bool CObject::GetFixed()
{
    return m_bFixed;
}


// Indicates whether an object is subjected to clipping (obstacles).

void CObject::SetClip(bool bClip)
{
    m_bClip = bClip;
}

bool CObject::GetClip()
{
    return m_bClip;
}



// Pushes an object.

bool CObject::JostleObject(float force)
{
    if ( m_type == OBJECT_FLAGb ||
         m_type == OBJECT_FLAGr ||
         m_type == OBJECT_FLAGg ||
         m_type == OBJECT_FLAGy ||
         m_type == OBJECT_FLAGv )  // flag?
    {
        if ( m_auto == 0 )  return false;

        m_auto->Start(1);
    }
    else
    {
        if ( m_auto != 0 )  return false;

        m_auto = new CAutoJostle(this);
        CAutoJostle* pa = static_cast<CAutoJostle*>(m_auto);
        pa->Start(0, force);
    }

    return true;
}


// Beginning of the effect when the instruction "detect" is used.

void CObject::StartDetectEffect(CObject *target, bool bFound)
{
    Math::Matrix*   mat;
    Math::Vector    pos, goal;
    Math::Point     dim;

    mat = GetWorldMatrix(0);
    pos = Math::Transform(*mat, Math::Vector(2.0f, 3.0f, 0.0f));

    if ( target == 0 )
    {
        goal = Math::Transform(*mat, Math::Vector(50.0f, 3.0f, 0.0f));
    }
    else
    {
        goal = target->GetPosition(0);
        goal.y += 3.0f;
        goal = Math::SegmentPoint(pos, goal, Math::Distance(pos, goal)-3.0f);
    }

    dim.x = 3.0f;
    dim.y = dim.x;
    m_particle->CreateRay(pos, goal, Gfx::PARTIRAY2, dim, 0.2f);

    if ( target != 0 )
    {
        goal = target->GetPosition(0);
        goal.y += 3.0f;
        goal = Math::SegmentPoint(pos, goal, Math::Distance(pos, goal)-1.0f);
        dim.x = 6.0f;
        dim.y = dim.x;
        m_particle->CreateParticle(goal, Math::Vector(0.0f, 0.0f, 0.0f), dim,
                                     bFound?Gfx::PARTIGLINT:Gfx::PARTIGLINTr, 0.5f);
    }

    m_sound->Play(bFound?SOUND_BUILD:SOUND_RECOVER);
}


// Management of time from which a virus is active.

void CObject::SetVirusMode(bool bEnable)
{
    m_bVirusMode = bEnable;
    m_virusTime = 0.0f;

    if ( m_bVirusMode && m_brain != 0 )
    {
        if ( !m_brain->IntroduceVirus() )  // tries to infect
        {
            m_bVirusMode = false;  // program was not contaminated!
        }
    }
}

bool CObject::GetVirusMode()
{
    return m_bVirusMode;
}

float CObject::GetVirusTime()
{
    return m_virusTime;
}


// Management mode of the camera.

void CObject::SetCameraType(Gfx::CameraType type)
{
    m_cameraType = type;
}

Gfx::CameraType CObject::GetCameraType()
{
    return m_cameraType;
}

void CObject::SetCameraDist(float dist)
{
    m_cameraDist = dist;
}

float CObject::GetCameraDist()
{
    return m_cameraDist;
}

void CObject::SetCameraLock(bool bLock)
{
    m_bCameraLock = bLock;
}

bool CObject::GetCameraLock()
{
    return m_bCameraLock;
}



// Management of the demonstration of the object.

void CObject::SetHilite(bool bMode)
{
    int     list[OBJECTMAXPART+1];
    int     i, j;

    m_bHilite = bMode;

    if ( m_bHilite )
    {
        j = 0;
        for ( i=0 ; i<m_totalPart ; i++ )
        {
            if ( m_objectPart[i].bUsed )
            {
                list[j++] = m_objectPart[i].object;
            }
        }
        list[j] = -1;  // terminate

        m_engine->SetHighlightRank(list);  // gives the list of selected parts
    }
}

bool CObject::GetHilite()
{
    return m_bHilite;
}


// Indicates whether the object is selected or not.

void CObject::SetSelect(bool bMode, bool bDisplayError)
{
    Error       err;

    m_bSelect = bMode;

    if ( m_physics != 0 )
    {
        m_physics->CreateInterface(m_bSelect);
    }

    if ( m_auto != 0 )
    {
        m_auto->CreateInterface(m_bSelect);
    }

    CreateSelectParticle();  // creates / removes particles

    if ( !m_bSelect )
    {
        //SetGunGoalH(0.0f);  // puts the cannon right
        return;  // selects if not finished
    }

    err = ERR_OK;
    if ( m_physics != 0 )
    {
        err = m_physics->GetError();
    }
    if ( m_auto != 0 )
    {
        err = m_auto->GetError();
    }
    if ( err != ERR_OK && bDisplayError )
    {
        m_main->DisplayError(err, this);
    }
}

// Indicates whether the object is selected or not.

bool CObject::GetSelect(bool bReal)
{
    if ( !bReal && m_main->GetFixScene() )  return false;
    return m_bSelect;
}


// Indicates whether the object is selectable or not.

void CObject::SetSelectable(bool bMode)
{
    m_bSelectable = bMode;
}

// Indicates whether the object is selecionnable or not.

bool CObject::GetSelectable()
{
    return m_bSelectable;
}


// Management of the activities of an object.

void CObject::SetActivity(bool bMode)
{
    if ( m_brain != 0 )
    {
        m_brain->SetActivity(bMode);
    }
}

bool CObject::GetActivity()
{
    if ( m_brain != 0 )
    {
        return m_brain->GetActivity();
    }
    return false;
}


// Indicates if necessary to check the tokens of the object.

void CObject::SetCheckToken(bool bMode)
{
    m_bCheckToken = bMode;
}

// Indicates if necessary to check the tokens of the object.

bool CObject::GetCheckToken()
{
    return m_bCheckToken;
}


// Management of the visibility of an object.
// The object is not hidden or visually disabled, but ignores detections!
// For example: underground worm.

void CObject::SetVisible(bool bVisible)
{
    m_bVisible = bVisible;
}

bool CObject::GetVisible()
{
    return m_bVisible;
}


// Management mode of operation of an object.
// An inactive object is an object destroyed, nonexistent.
// This mode is used for objects "resetables"
// during training to simulate destruction.

void CObject::SetEnable(bool bEnable)
{
    m_bEnable = bEnable;
}

bool CObject::GetEnable()
{
    return m_bEnable;
}


// Management mode or an object is only active when you're close.

void CObject::SetProxyActivate(bool bActivate)
{
    m_bProxyActivate = bActivate;
}

bool CObject::GetProxyActivate()
{
    return m_bProxyActivate;
}

void CObject::SetProxyDistance(float distance)
{
    m_proxyDistance = distance;
}

float CObject::GetProxyDistance()
{
    return m_proxyDistance;
}


// Management of the method of increasing damage.

void CObject::SetMagnifyDamage(float factor)
{
    m_magnifyDamage = factor;
}

float CObject::GetMagnifyDamage()
{
    return m_magnifyDamage;
}


// Management of free parameter.

void CObject::SetParam(float value)
{
    m_param = value;
}

float CObject::GetParam()
{
    return m_param;
}


// Management of the mode "blocked" of an object.
// For example, a cube of titanium is blocked while it is used to make something,
// or a vehicle is blocked as its construction is not finished.

void CObject::SetLock(bool bLock)
{
    m_bLock = bLock;
}

bool CObject::GetLock()
{
    return m_bLock;
}

// Ignore checks in build() function

void CObject::SetIgnoreBuildCheck(bool bIgnoreBuildCheck)
{
    m_bIgnoreBuildCheck = bIgnoreBuildCheck;
}

bool CObject::GetIgnoreBuildCheck()
{
    return m_bIgnoreBuildCheck;
}

// Management of the mode "current explosion" of an object.
// An object in this mode is not saving.

void CObject::SetExplo(bool bExplo)
{
    m_bExplo = bExplo;
}

bool CObject::GetExplo()
{
    return m_bExplo;
}


// Mode management "cargo ship" during movies.

void CObject::SetCargo(bool bCargo)
{
    m_bCargo = bCargo;
}

bool CObject::GetCargo()
{
    return m_bCargo;
}


// Management of the HS mode of an object.

void CObject::SetBurn(bool bBurn)
{
    m_bBurn = bBurn;

//? if ( m_botVar != 0 )
//? {
//?     if ( m_bBurn )  m_botVar->SetUserPtr(OBJECTDELETED);
//?     else            m_botVar->SetUserPtr(this);
//? }
}

bool CObject::GetBurn()
{
    return m_bBurn;
}

void CObject::SetDead(bool bDead)
{
    m_bDead = bDead;

    if ( bDead && m_brain != 0 )
    {
        m_brain->StopProgram();  // stops the current task
    }

//? if ( m_botVar != 0 )
//? {
//?     if ( m_bDead )  m_botVar->SetUserPtr(OBJECTDELETED);
//?     else            m_botVar->SetUserPtr(this);
//? }
}

bool CObject::GetDead()
{
    return m_bDead;
}

bool CObject::GetRuin()
{
    return m_bBurn|m_bFlat;
}

bool CObject::GetActif()
{
    return !m_bLock && !m_bBurn && !m_bFlat && m_bVisible && m_bEnable;
}


// Management of the point of aim.

void CObject::SetGunGoalV(float gunGoal)
{
    if ( m_type == OBJECT_MOBILEfc ||
         m_type == OBJECT_MOBILEtc ||
         m_type == OBJECT_MOBILEwc ||
         m_type == OBJECT_MOBILEic )  // fireball?
    {
        if ( gunGoal >  10.0f*Math::PI/180.0f )  gunGoal =  10.0f*Math::PI/180.0f;
        if ( gunGoal < -20.0f*Math::PI/180.0f )  gunGoal = -20.0f*Math::PI/180.0f;
        SetAngleZ(1, gunGoal);
    }
    else if ( m_type == OBJECT_MOBILEfi ||
              m_type == OBJECT_MOBILEti ||
              m_type == OBJECT_MOBILEwi ||
              m_type == OBJECT_MOBILEii )  // orgaball?
    {
        if ( gunGoal >  20.0f*Math::PI/180.0f )  gunGoal =  20.0f*Math::PI/180.0f;
        if ( gunGoal < -20.0f*Math::PI/180.0f )  gunGoal = -20.0f*Math::PI/180.0f;
        SetAngleZ(1, gunGoal);
    }
    else if ( m_type == OBJECT_MOBILErc )  // phazer?
    {
        if ( gunGoal >  45.0f*Math::PI/180.0f )  gunGoal =  45.0f*Math::PI/180.0f;
        if ( gunGoal < -20.0f*Math::PI/180.0f )  gunGoal = -20.0f*Math::PI/180.0f;
        SetAngleZ(2, gunGoal);
    }
    else
    {
        gunGoal = 0.0f;
    }

    m_gunGoalV = gunGoal;
}

void CObject::SetGunGoalH(float gunGoal)
{
    if ( m_type == OBJECT_MOBILEfc ||
         m_type == OBJECT_MOBILEtc ||
         m_type == OBJECT_MOBILEwc ||
         m_type == OBJECT_MOBILEic )  // fireball?
    {
        if ( gunGoal >  40.0f*Math::PI/180.0f )  gunGoal =  40.0f*Math::PI/180.0f;
        if ( gunGoal < -40.0f*Math::PI/180.0f )  gunGoal = -40.0f*Math::PI/180.0f;
        SetAngleY(1, gunGoal);
    }
    else if ( m_type == OBJECT_MOBILEfi ||
              m_type == OBJECT_MOBILEti ||
              m_type == OBJECT_MOBILEwi ||
              m_type == OBJECT_MOBILEii )  // orgaball?
    {
        if ( gunGoal >  40.0f*Math::PI/180.0f )  gunGoal =  40.0f*Math::PI/180.0f;
        if ( gunGoal < -40.0f*Math::PI/180.0f )  gunGoal = -40.0f*Math::PI/180.0f;
        SetAngleY(1, gunGoal);
    }
    else if ( m_type == OBJECT_MOBILErc )  // phazer?
    {
        if ( gunGoal >  40.0f*Math::PI/180.0f )  gunGoal =  40.0f*Math::PI/180.0f;
        if ( gunGoal < -40.0f*Math::PI/180.0f )  gunGoal = -40.0f*Math::PI/180.0f;
        SetAngleY(2, gunGoal);
    }
    else
    {
        gunGoal = 0.0f;
    }

    m_gunGoalH = gunGoal;
}

float CObject::GetGunGoalV()
{
    return m_gunGoalV;
}

float CObject::GetGunGoalH()
{
    return m_gunGoalH;
}



// Shows the limits of the object.

bool CObject::StartShowLimit()
{
    if ( m_showLimitRadius == 0.0f )  return false;

    m_main->SetShowLimit(0, Gfx::PARTILIMIT1, this, GetPosition(0), m_showLimitRadius);
    m_bShowLimit = true;
    return true;
}

void CObject::StopShowLimit()
{
    m_bShowLimit = false;
}

void CObject::SetShowLimitRadius(float radius)
{
    m_showLimitRadius = radius;
}

// Indicates whether a program is under execution.

bool CObject::IsProgram()
{
    if ( m_brain == 0 )  return false;
    return m_brain->IsProgram();
}


// Creates or removes particles associated to the object.

void CObject::CreateSelectParticle()
{
    Math::Vector    pos, speed;
    Math::Point     dim;
    int         i;

    // Removes particles preceding.
    for ( i=0 ; i<4 ; i++ )
    {
        if ( m_partiSel[i] != -1 )
        {
            m_particle->DeleteParticle(m_partiSel[i]);
            m_partiSel[i] = -1;
        }
    }

    if ( m_bSelect || IsProgram() || m_main->GetRetroMode() )
    {
        // Creates particles lens for the headlights.
        if ( m_type == OBJECT_MOBILEfa ||
             m_type == OBJECT_MOBILEta ||
             m_type == OBJECT_MOBILEwa ||
             m_type == OBJECT_MOBILEia ||
             m_type == OBJECT_MOBILEfc ||
             m_type == OBJECT_MOBILEtc ||
             m_type == OBJECT_MOBILEwc ||
             m_type == OBJECT_MOBILEic ||
             m_type == OBJECT_MOBILEfi ||
             m_type == OBJECT_MOBILEti ||
             m_type == OBJECT_MOBILEwi ||
             m_type == OBJECT_MOBILEii ||
             m_type == OBJECT_MOBILEfs ||
             m_type == OBJECT_MOBILEts ||
             m_type == OBJECT_MOBILEws ||
             m_type == OBJECT_MOBILEis ||
             m_type == OBJECT_MOBILErt ||
             m_type == OBJECT_MOBILErc ||
             m_type == OBJECT_MOBILErr ||
             m_type == OBJECT_MOBILErs ||
             m_type == OBJECT_MOBILEsa ||
             m_type == OBJECT_MOBILEtg ||
             m_type == OBJECT_MOBILEft ||
             m_type == OBJECT_MOBILEtt ||
             m_type == OBJECT_MOBILEwt ||
             m_type == OBJECT_MOBILEit ||
             m_type == OBJECT_MOBILEdr )  // vehicle?
        {
            pos = Math::Vector(0.0f, 0.0f, 0.0f);
            speed = Math::Vector(0.0f, 0.0f, 0.0f);
            dim.x = 0.0f;
            dim.y = 0.0f;
            m_partiSel[0] = m_particle->CreateParticle(pos, speed, dim, Gfx::PARTISELY, 1.0f, 0.0f, 0.0f);
            m_partiSel[1] = m_particle->CreateParticle(pos, speed, dim, Gfx::PARTISELY, 1.0f, 0.0f, 0.0f);
            m_partiSel[2] = m_particle->CreateParticle(pos, speed, dim, Gfx::PARTISELR, 1.0f, 0.0f, 0.0f);
            m_partiSel[3] = m_particle->CreateParticle(pos, speed, dim, Gfx::PARTISELR, 1.0f, 0.0f, 0.0f);
            UpdateSelectParticle();
        }
    }
}

// Updates the particles associated to the object.

void CObject::UpdateSelectParticle()
{
    Math::Vector    pos[4];
    Math::Point     dim[4];
    float       zoom[4];
    float       angle;
    int         i;

    if ( !m_bSelect && !IsProgram() && !m_main->GetRetroMode() )  return;

    dim[0].x = 1.0f;
    dim[1].x = 1.0f;
    dim[2].x = 1.2f;
    dim[3].x = 1.2f;

    // Lens front yellow.
    if ( m_type == OBJECT_MOBILErt ||
         m_type == OBJECT_MOBILErc ||
         m_type == OBJECT_MOBILErr ||
         m_type == OBJECT_MOBILErs )  // large caterpillars?
    {
        pos[0] = Math::Vector(4.2f, 2.8f,  1.5f);
        pos[1] = Math::Vector(4.2f, 2.8f, -1.5f);
        dim[0].x = 1.5f;
        dim[1].x = 1.5f;
    }
    else if ( m_type == OBJECT_MOBILEwt ||
              m_type == OBJECT_MOBILEtt ||
              m_type == OBJECT_MOBILEft ||
              m_type == OBJECT_MOBILEit )  // trainer ?
    {
        pos[0] = Math::Vector(4.2f, 2.5f,  1.2f);
        pos[1] = Math::Vector(4.2f, 2.5f, -1.2f);
        dim[0].x = 1.5f;
        dim[1].x = 1.5f;
    }
    else if ( m_type == OBJECT_MOBILEsa )  // submarine?
    {
        pos[0] = Math::Vector(3.6f, 4.0f,  2.0f);
        pos[1] = Math::Vector(3.6f, 4.0f, -2.0f);
    }
    else if ( m_type == OBJECT_MOBILEtg )  // target?
    {
        pos[0] = Math::Vector(3.4f, 6.5f,  2.0f);
        pos[1] = Math::Vector(3.4f, 6.5f, -2.0f);
    }
    else if ( m_type == OBJECT_MOBILEdr )  // designer?
    {
        pos[0] = Math::Vector(4.9f, 3.5f,  2.5f);
        pos[1] = Math::Vector(4.9f, 3.5f, -2.5f);
    }
    else
    {
        pos[0] = Math::Vector(4.2f, 2.5f,  1.5f);
        pos[1] = Math::Vector(4.2f, 2.5f, -1.5f);
    }

    // Red back lens
    if ( m_type == OBJECT_MOBILEfa ||
         m_type == OBJECT_MOBILEfc ||
         m_type == OBJECT_MOBILEfi ||
         m_type == OBJECT_MOBILEfs ||
         m_type == OBJECT_MOBILEft )  // flying?
    {
        pos[2] = Math::Vector(-4.0f, 3.1f,  4.5f);
        pos[3] = Math::Vector(-4.0f, 3.1f, -4.5f);
        dim[2].x = 0.6f;
        dim[3].x = 0.6f;
    }
    if ( m_type == OBJECT_MOBILEwa ||
         m_type == OBJECT_MOBILEwc ||
         m_type == OBJECT_MOBILEwi ||
         m_type == OBJECT_MOBILEws )  // wheels?
    {
        pos[2] = Math::Vector(-4.5f, 2.7f,  2.8f);
        pos[3] = Math::Vector(-4.5f, 2.7f, -2.8f);
    }
    if ( m_type == OBJECT_MOBILEwt )  // wheels?
    {
        pos[2] = Math::Vector(-4.0f, 2.5f,  2.2f);
        pos[3] = Math::Vector(-4.0f, 2.5f, -2.2f);
    }
    if ( m_type == OBJECT_MOBILEia ||
         m_type == OBJECT_MOBILEic ||
         m_type == OBJECT_MOBILEii ||
         m_type == OBJECT_MOBILEis ||
         m_type == OBJECT_MOBILEit )  // legs?
    {
        pos[2] = Math::Vector(-4.5f, 2.7f,  2.8f);
        pos[3] = Math::Vector(-4.5f, 2.7f, -2.8f);
    }
    if ( m_type == OBJECT_MOBILEta ||
         m_type == OBJECT_MOBILEtc ||
         m_type == OBJECT_MOBILEti ||
         m_type == OBJECT_MOBILEts ||
         m_type == OBJECT_MOBILEtt )  // caterpillars?
    {
        pos[2] = Math::Vector(-3.6f, 4.2f,  3.0f);
        pos[3] = Math::Vector(-3.6f, 4.2f, -3.0f);
    }
    if ( m_type == OBJECT_MOBILErt ||
         m_type == OBJECT_MOBILErc ||
         m_type == OBJECT_MOBILErr ||
         m_type == OBJECT_MOBILErs )  // large caterpillars?
    {
        pos[2] = Math::Vector(-5.0f, 5.2f,  2.5f);
        pos[3] = Math::Vector(-5.0f, 5.2f, -2.5f);
    }
    if ( m_type == OBJECT_MOBILEsa )  // submarine?
    {
        pos[2] = Math::Vector(-3.6f, 4.0f,  2.0f);
        pos[3] = Math::Vector(-3.6f, 4.0f, -2.0f);
    }
    if ( m_type == OBJECT_MOBILEtg )  // target?
    {
        pos[2] = Math::Vector(-2.4f, 6.5f,  2.0f);
        pos[3] = Math::Vector(-2.4f, 6.5f, -2.0f);
    }
    if ( m_type == OBJECT_MOBILEdr )  // designer?
    {
        pos[2] = Math::Vector(-5.3f, 2.7f,  1.8f);
        pos[3] = Math::Vector(-5.3f, 2.7f, -1.8f);
    }

    angle = GetAngleY(0)/Math::PI;

    zoom[0] = 1.0f;
    zoom[1] = 1.0f;
    zoom[2] = 1.0f;
    zoom[3] = 1.0f;

    if ( ( IsProgram() ||  // current program?
         m_main->GetRetroMode() ) && // Retro mode?
         Math::Mod(m_aTime, 0.7f) < 0.3f )
    {
        zoom[0] = 0.0f;  // blinks
        zoom[1] = 0.0f;
        zoom[2] = 0.0f;
        zoom[3] = 0.0f;
    }

    // Updates lens.
    for ( i=0 ; i<4 ; i++ )
    {
        pos[i] = Math::Transform(m_objectPart[0].matWorld, pos[i]);
        dim[i].y = dim[i].x;
        m_particle->SetParam(m_partiSel[i], pos[i], dim[i], zoom[i], angle, 1.0f);
    }
}


// Getes the pointer to the current script execution.

void CObject::SetRunScript(CScript* script)
{
    m_runScript = script;
}

CScript* CObject::GetRunScript()
{
    return m_runScript;
}

// Returns the variables of "this" for CBOT.

CBotVar* CObject::GetBotVar()
{
    return m_botVar;
}

// Returns the physics associated to the object.

CPhysics* CObject::GetPhysics()
{
    return m_physics;
}

void CObject::SetPhysics(CPhysics* physics)
{
    m_physics = physics;
}

// Returns the brain associated to the object.

CBrain* CObject::GetBrain()
{
    return m_brain;
}

void CObject::SetBrain(CBrain* brain)
{
    m_brain = brain;
}

// Returns the movement associated to the object.

CMotion* CObject::GetMotion()
{
    return m_motion;
}

void CObject::SetMotion(CMotion* motion)
{
    m_motion = motion;
}

// Returns the controller associated to the object.

CAuto* CObject::GetAuto()
{
    return m_auto;
}

void CObject::SetAuto(CAuto* automat)
{
    m_auto = automat;
}



// Management of the position in the file definition.

void CObject::SetDefRank(int rank)
{
    m_defRank = rank;
}

int  CObject::GetDefRank()
{
    return m_defRank;
}


// Getes the object name for the tooltip.

bool CObject::GetTooltipName(std::string& name)
{
    GetResource(RES_OBJECT, m_type, name);
    return !name.empty();
}


// Adds the object previously selected in the list.

void CObject::AddDeselList(CObject* pObj)
{
    int     i;

    if ( m_totalDesectList >= OBJECTMAXDESELLIST )
    {
        for ( i=0 ; i<OBJECTMAXDESELLIST-1 ; i++ )
        {
            m_objectDeselectList[i] = m_objectDeselectList[i+1];
        }
        m_totalDesectList --;
    }

    m_objectDeselectList[m_totalDesectList++] = pObj;
}

// Removes the previously selected object in the list.

CObject* CObject::SubDeselList()
{
    if ( m_totalDesectList == 0 )  return 0;

    return m_objectDeselectList[--m_totalDesectList];
}

// Removes an object reference if it is in the list.

void CObject::DeleteDeselList(CObject* pObj)
{
    int     i, j;

    j = 0;
    for ( i=0 ; i<m_totalDesectList ; i++ )
    {
        if ( m_objectDeselectList[i] != pObj )
        {
            m_objectDeselectList[j++] = m_objectDeselectList[i];
        }
    }
    m_totalDesectList = j;
}



// Management of the state of the pencil drawing robot.

bool CObject::GetTraceDown()
{
    if (m_motion == nullptr) return false;
    CMotionVehicle* mv = dynamic_cast<CMotionVehicle*>(m_motion);
    if (mv == nullptr)
    {
        GetLogger()->Trace("GetTraceDown() invalid m_motion class!\n");
        return false;
    }
    return mv->GetTraceDown();
}

void CObject::SetTraceDown(bool bDown)
{
    if (m_motion == nullptr) return;
    CMotionVehicle* mv = dynamic_cast<CMotionVehicle*>(m_motion);
    if (mv == nullptr)
    {
        GetLogger()->Trace("SetTraceDown() invalid m_motion class!\n");
        return;
    }
    mv->SetTraceDown(bDown);
}

int CObject::GetTraceColor()
{
    if (m_motion == nullptr) return 0;
    CMotionVehicle* mv = dynamic_cast<CMotionVehicle*>(m_motion);
    if (mv == nullptr)
    {
        GetLogger()->Trace("GetTraceColor() invalid m_motion class!\n");
        return 0;
    }
    return mv->GetTraceColor();
}

void CObject::SetTraceColor(int color)
{
    if (m_motion == nullptr) return;
    CMotionVehicle* mv = dynamic_cast<CMotionVehicle*>(m_motion);
    if (mv == nullptr)
    {
        GetLogger()->Trace("SetTraceColor() invalid m_motion class!\n");
        return;
    }
    mv->SetTraceColor(color);
}

float CObject::GetTraceWidth()
{
    if (m_motion == nullptr) return 0.0f;
    CMotionVehicle* mv = dynamic_cast<CMotionVehicle*>(m_motion);
    if (mv == nullptr)
    {
        GetLogger()->Trace("GetTraceWidth() invalid m_motion class!\n");
        return 0.0f;
    }
    return mv->GetTraceWidth();
}

void CObject::SetTraceWidth(float width)
{
    if (m_motion == nullptr) return;
    CMotionVehicle* mv = dynamic_cast<CMotionVehicle*>(m_motion);
    if (mv == nullptr)
    {
        GetLogger()->Trace("SetTraceWidth() invalid m_motion class!\n");
        return;
    }
    mv->SetTraceWidth(width);
}

DriveType CObject::GetDriveFromObject(ObjectType type)
{
    switch(type) {
        case OBJECT_MOBILEwt:
        case OBJECT_MOBILEwa:
        case OBJECT_MOBILEwc:
        case OBJECT_MOBILEwi:
        case OBJECT_MOBILEws:
            return DRIVE_WHEELED;
            
        case OBJECT_MOBILEtt:
        case OBJECT_MOBILEta:
        case OBJECT_MOBILEtc:
        case OBJECT_MOBILEti:
        case OBJECT_MOBILEts:
            return DRIVE_TRACKED;
            
        case OBJECT_MOBILEft:
        case OBJECT_MOBILEfa:
        case OBJECT_MOBILEfc:
        case OBJECT_MOBILEfi:
        case OBJECT_MOBILEfs:
            return DRIVE_WINGED;
            
        case OBJECT_MOBILEit:
        case OBJECT_MOBILEia:
        case OBJECT_MOBILEic:
        case OBJECT_MOBILEii:
        case OBJECT_MOBILEis:
            return DRIVE_LEGGED;
            
        default:
            return DRIVE_OTHER;
    }
}

ToolType CObject::GetToolFromObject(ObjectType type)
{
    switch(type) {
        case OBJECT_MOBILEwa:
        case OBJECT_MOBILEta:
        case OBJECT_MOBILEfa:
        case OBJECT_MOBILEia:
            return TOOL_GRABBER;
            
        case OBJECT_MOBILEws:
        case OBJECT_MOBILEts:
        case OBJECT_MOBILEfs:
        case OBJECT_MOBILEis:
            return TOOL_SNIFFER;
            
        case OBJECT_MOBILEwc:
        case OBJECT_MOBILEtc:
        case OBJECT_MOBILEfc:
        case OBJECT_MOBILEic:
            return TOOL_SHOOTER;
            
        case OBJECT_MOBILEwi:
        case OBJECT_MOBILEti:
        case OBJECT_MOBILEfi:
        case OBJECT_MOBILEii:
            return TOOL_ORGASHOOTER;
            
        default:
            return TOOL_OTHER;
    }
}
