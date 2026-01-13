#ifndef __INC_KNOCKBACK_CONFIG_H__
#define __INC_KNOCKBACK_CONFIG_H__

// Typy útoků
enum EAttackKnockbackType
{
    KNOCKBACK_TYPE_NONE = 0,        // Žádný knockback
    KNOCKBACK_TYPE_COMBO_FINAL,     // Poslední hit komba
    KNOCKBACK_TYPE_SKILL,           // Skill s knockback
    KNOCKBACK_TYPE_CRITICAL,        // Critical hit (volitelné)
};

// Konfigurace knockback pro skill
struct SKillKnockbackInfo
{
    DWORD   dwSkillVnum;
    bool    bHasKnockback;
    float   fKnockbackDistance;  // v pixelech
    float   fKnockbackDuration;  // v sekundách
};

// Manager pro knockback konfiguraci
class CKnockbackManager
{
public:
    static CKnockbackManager& Instance();

    void Initialize();

    // Kontrola, zda skill má knockback
    bool DoesSkillHaveKnockback(DWORD dwSkillVnum);

    // Získání knockback parametrů pro skill
    bool GetSkillKnockbackInfo(DWORD dwSkillVnum, float& fDistance, float& fDuration);

    // Kontrola, zda je to poslední combo hit
    bool IsComboFinalHit(BYTE bComboIndex, DWORD dwRaceVnum);

private:
    CKnockbackManager();
    ~CKnockbackManager();

    std::map<DWORD, SKillKnockbackInfo> m_mapSkillKnockback;
    std::map<DWORD, BYTE> m_mapComboFinalHits;  // Race -> poslední combo index
};

#endif