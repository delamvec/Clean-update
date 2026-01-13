#include "stdafx.h"
#include "knockback_config.h"

CKnockbackManager::CKnockbackManager()
{
}

CKnockbackManager::~CKnockbackManager()
{
}

CKnockbackManager& CKnockbackManager::Instance()
{
    static CKnockbackManager instance;
    return instance;
}

void CKnockbackManager::Initialize()
{
    // ✅ Konfigurace knockback skillů
    // Příklad: Skill ID 1 (Warrior's Bash) má knockback
    SKillKnockbackInfo bashInfo;
    bashInfo.dwSkillVnum = 1;
    bashInfo.bHasKnockback = true;
    bashInfo.fKnockbackDistance = 200.0f;  // 200 pixelů
    bashInfo.fKnockbackDuration = 0.5f;    // 0.5 sekundy
    m_mapSkillKnockback[1] = bashInfo;

    // Příklad: Skill ID 16 (Sura's Enchanted Blade) má knockback
    SKillKnockbackInfo enchantedInfo;
    enchantedInfo.dwSkillVnum = 16;
    enchantedInfo.bHasKnockback = true;
    enchantedInfo.fKnockbackDistance = 300.0f;
    enchantedInfo.fKnockbackDuration = 0.7f;
    m_mapSkillKnockback[16] = enchantedInfo;

    // TODO: Načíst z konfiguračního souboru nebo databáze

    // ✅ Konfigurace combo final hits
    // Warrior (race 0, 4): poslední hit je index 2 (3. hit)
    m_mapComboFinalHits[0] = 2;  // Male Warrior
    m_mapComboFinalHits[4] = 2;  // Female Warrior

    // Assassin (race 1, 5): poslední hit je index 3 (4. hit)
    m_mapComboFinalHits[1] = 3;  // Male Assassin
    m_mapComboFinalHits[5] = 3;  // Female Assassin

    // Sura (race 2, 6): poslední hit je index 2
    m_mapComboFinalHits[2] = 2;  // Male Sura
    m_mapComboFinalHits[6] = 2;  // Female Sura

    // Shaman (race 3, 7): poslední hit je index 1
    m_mapComboFinalHits[3] = 1;  // Male Shaman
    m_mapComboFinalHits[7] = 1;  // Female Shaman
}

bool CKnockbackManager::DoesSkillHaveKnockback(DWORD dwSkillVnum)
{
    auto it = m_mapSkillKnockback.find(dwSkillVnum);
    if (it != m_mapSkillKnockback.end())
        return it->second.bHasKnockback;

    return false;
}

bool CKnockbackManager::GetSkillKnockbackInfo(DWORD dwSkillVnum, float& fDistance, float& fDuration)
{
    auto it = m_mapSkillKnockback.find(dwSkillVnum);
    if (it != m_mapSkillKnockback.end() && it->second.bHasKnockback)
    {
        fDistance = it->second.fKnockbackDistance;
        fDuration = it->second.fKnockbackDuration;
        return true;
    }

    return false;
}

bool CKnockbackManager::IsComboFinalHit(BYTE bComboIndex, DWORD dwRaceVnum)
{
    auto it = m_mapComboFinalHits.find(dwRaceVnum);
    if (it != m_mapComboFinalHits.end())
        return (bComboIndex >= it->second);

    return false;
}