#include "object/auto/autodelegate.h"

CAutoDelegate::CAutoDelegate(COldObject* oldObject, std::unique_ptr<CAuto> object)
    : CAuto(oldObject), m_object(std::move(object))
{}

CAutoDelegate::~CAutoDelegate() {}

void CAutoDelegate::DeleteObject(bool bAll)
{
    m_object->DeleteObject(bAll);
}

void CAutoDelegate::Init()
{
    m_object->Init();
}

void CAutoDelegate::Start(int param)
{
    m_object->Start(param);
}

bool CAutoDelegate::EventProcess(const Event &event)
{
    return m_object->EventProcess(event);
}

Error CAutoDelegate::IsEnded()
{
    return m_object->IsEnded();
}

bool CAutoDelegate::Abort()
{
    return m_object->Abort();
}

Error CAutoDelegate::StartAction(int param)
{
    return m_object->StartAction(param);
}

bool CAutoDelegate::SetType(ObjectType type)
{
    return m_object->SetType(type);
}

bool CAutoDelegate::SetValue(int rank, float value)
{
    return m_object->SetValue(rank, value);
}

bool CAutoDelegate::SetString(char *string)
{
    return m_object->SetString(string);
}

bool CAutoDelegate::CreateInterface(bool bSelect)
{
    return m_object->CreateInterface(bSelect);
}

Error CAutoDelegate::GetError()
{
    return m_object->GetError();
}

bool CAutoDelegate::GetBusy()
{
    return m_object->GetBusy();
}

void CAutoDelegate::SetBusy(bool bBuse)
{
    m_object->SetBusy(bBuse);
}

void CAutoDelegate::InitProgressTotal(float total)
{
    m_object->InitProgressTotal(total);
}

void CAutoDelegate::EventProgress(float rTime)
{
    m_object->EventProgress(rTime);
}

bool CAutoDelegate::GetMotor()
{
    return m_object->GetMotor();
}

void CAutoDelegate::SetMotor(bool bMotor)
{
    m_object->SetMotor(bMotor);
}

bool CAutoDelegate::Write(CLevelParserLine *line)
{
    return m_object->Write(line);
}

bool CAutoDelegate::Read(CLevelParserLine *line)
{
    return m_object->Read(line);
}
