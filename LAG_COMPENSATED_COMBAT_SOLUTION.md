# Lag-Compensated Combat System - Implementační řešení

## 🎯 Požadavky

1. ✅ **Žádný pull-back** - Oběť není přitahována k útočníkovi
2. ✅ **Plynulý pohyb** - Smooth animace i při lagu
3. ✅ **Lag tolerance 300-500ms** - Funguje i při vysokém pingu
4. ✅ **Pohyb během boje** - Oběť se může pohybovat během útoku
5. ✅ **Selective knockback** - Pouze poslední combo hit nebo speciální skilly
6. ✅ **Fair hit detection** - Založená na průměru pozic klienta A a B

---

## 🏗️ Architektura řešení

### Klíčové principy:

1. **Server má autoritu** - Server rozhoduje o hit/miss, damage, knockback
2. **Klienti nepřepisují pozice** - SyncPosition mechanismus je VYPNUTÝ
3. **Position history** - Server ukládá historii pozic (500ms) pro lag compensation
4. **Client-side prediction** - Klient predikuje vlastní pohyb
5. **Server reconciliation** - Server posílá korekce pouze při velkém rozdílu
6. **Selective knockback** - Jen specifické útoky způsobují knockback

---

## 📦 Část 1: Serverová strana

### 1.1 Position History System

Server musí ukládat historii pozic každého hráče pro lag compensation.

#### Nový soubor: `char_position_history.h`

```cpp
#ifndef __INC_CHAR_POSITION_HISTORY_H__
#define __INC_CHAR_POSITION_HISTORY_H__

#include <deque>
#include <sys/time.h>

// Struktura pro uložení historické pozice
struct SPositionSnapshot
{
    long    lX;
    long    lY;
    DWORD   dwTimestamp;  // milisekundy od epoch

    SPositionSnapshot() : lX(0), lY(0), dwTimestamp(0) {}
    SPositionSnapshot(long x, long y, DWORD timestamp)
        : lX(x), lY(y), dwTimestamp(timestamp) {}
};

// Třída pro správu historie pozic
class CPositionHistory
{
public:
    CPositionHistory();
    ~CPositionHistory();

    // Přidá novou pozici do historie
    void AddSnapshot(long lX, long lY, DWORD dwTimestamp);

    // Získá pozici v daném čase (lag compensation)
    bool GetPositionAtTime(DWORD dwTimestamp, long& lX, long& lY);

    // Vyčistí staré záznamy (starší než HISTORY_DURATION_MS)
    void CleanOldSnapshots(DWORD dwCurrentTime);

    // Získá nejaktuálnější pozici
    bool GetLatestPosition(long& lX, long& lY, DWORD& dwTimestamp);

    // Vymaže všechny záznamy
    void Clear();

private:
    static const DWORD HISTORY_DURATION_MS = 1000;  // 1 sekunda historie
    static const DWORD MAX_SNAPSHOTS = 50;          // Max 50 snapshotů

    std::deque<SPositionSnapshot> m_PositionQueue;
};

#endif
```

#### Implementace: `char_position_history.cpp`

```cpp
#include "stdafx.h"
#include "char_position_history.h"
#include "utils.h"

CPositionHistory::CPositionHistory()
{
    m_PositionQueue.clear();
}

CPositionHistory::~CPositionHistory()
{
    Clear();
}

void CPositionHistory::AddSnapshot(long lX, long lY, DWORD dwTimestamp)
{
    // Přidej nový snapshot
    m_PositionQueue.push_back(SPositionSnapshot(lX, lY, dwTimestamp));

    // Omezte velikost fronty
    if (m_PositionQueue.size() > MAX_SNAPSHOTS)
    {
        m_PositionQueue.pop_front();
    }
}

bool CPositionHistory::GetPositionAtTime(DWORD dwTimestamp, long& lX, long& lY)
{
    if (m_PositionQueue.empty())
        return false;

    // Pokud je timestamp novější než nejnovější záznam, vrať nejnovější
    if (dwTimestamp >= m_PositionQueue.back().dwTimestamp)
    {
        lX = m_PositionQueue.back().lX;
        lY = m_PositionQueue.back().lY;
        return true;
    }

    // Pokud je timestamp starší než nejstarší záznam, vrať nejstarší
    if (dwTimestamp <= m_PositionQueue.front().dwTimestamp)
    {
        lX = m_PositionQueue.front().lX;
        lY = m_PositionQueue.front().lY;
        return true;
    }

    // Najdi dva sousední snapshoty a interpoluj
    for (size_t i = 0; i < m_PositionQueue.size() - 1; ++i)
    {
        const SPositionSnapshot& snapshot1 = m_PositionQueue[i];
        const SPositionSnapshot& snapshot2 = m_PositionQueue[i + 1];

        if (dwTimestamp >= snapshot1.dwTimestamp && dwTimestamp <= snapshot2.dwTimestamp)
        {
            // Lineární interpolace
            DWORD dwTimeDiff = snapshot2.dwTimestamp - snapshot1.dwTimestamp;
            if (dwTimeDiff == 0)
            {
                lX = snapshot1.lX;
                lY = snapshot1.lY;
            }
            else
            {
                float fRatio = (float)(dwTimestamp - snapshot1.dwTimestamp) / (float)dwTimeDiff;
                lX = (long)(snapshot1.lX + (snapshot2.lX - snapshot1.lX) * fRatio);
                lY = (long)(snapshot1.lY + (snapshot2.lY - snapshot1.lY) * fRatio);
            }
            return true;
        }
    }

    return false;
}

void CPositionHistory::CleanOldSnapshots(DWORD dwCurrentTime)
{
    while (!m_PositionQueue.empty())
    {
        const SPositionSnapshot& oldest = m_PositionQueue.front();

        // Pokud je snapshot starší než HISTORY_DURATION_MS, smaž ho
        if (dwCurrentTime - oldest.dwTimestamp > HISTORY_DURATION_MS)
        {
            m_PositionQueue.pop_front();
        }
        else
        {
            break;  // Zbytek je novější
        }
    }
}

bool CPositionHistory::GetLatestPosition(long& lX, long& lY, DWORD& dwTimestamp)
{
    if (m_PositionQueue.empty())
        return false;

    const SPositionSnapshot& latest = m_PositionQueue.back();
    lX = latest.lX;
    lY = latest.lY;
    dwTimestamp = latest.dwTimestamp;
    return true;
}

void CPositionHistory::Clear()
{
    m_PositionQueue.clear();
}
```

### 1.2 Úprava CHARACTER class

Přidej position history do `char.h`:

```cpp
// char.h

#include "char_position_history.h"

class CHARACTER
{
    // ... existující kód ...

private:
    // ✅ NOVÉ: Position history pro lag compensation
    CPositionHistory    m_kPositionHistory;

public:
    // ✅ NOVÉ: Přidání pozice do historie
    void UpdatePositionHistory();

    // ✅ NOVÉ: Získání pozice v čase (pro lag compensation)
    bool GetHistoricalPosition(DWORD dwTimestamp, long& lX, long& lY);

    // ✅ NOVÉ: Vyčištění staré historie
    void CleanPositionHistory();
};
```

Implementace v `char.cpp`:

```cpp
// char.cpp

void CHARACTER::UpdatePositionHistory()
{
    DWORD dwCurrentTime = get_dword_time();
    m_kPositionHistory.AddSnapshot(GetX(), GetY(), dwCurrentTime);

    // Vyčisti staré záznamy
    m_kPositionHistory.CleanOldSnapshots(dwCurrentTime);
}

bool CHARACTER::GetHistoricalPosition(DWORD dwTimestamp, long& lX, long& lY)
{
    return m_kPositionHistory.GetPositionAtTime(dwTimestamp, lX, lY);
}

void CHARACTER::CleanPositionHistory()
{
    m_kPositionHistory.Clear();
}
```

### 1.3 Aktualizace historie při pohybu

Každý frame když se hráč pohybuje, aktualizuj historii:

```cpp
// char.cpp - v Update() funkci nebo Move() funkci

void CHARACTER::Move(long x, long y)
{
    // ... existující move kód ...

    // ✅ NOVÉ: Aktualizuj position history
    UpdatePositionHistory();
}

void CHARACTER::UpdatePosition()
{
    // ... existující update kód ...

    // ✅ NOVÉ: Aktualizuj position history každý frame
    UpdatePositionHistory();
}
```

### 1.4 Lag-Compensated Hit Detection

Upravíme hit detection, aby používal lag compensation:

```cpp
// char_battle.cpp

bool CHARACTER::ComputeAttackDistance(LPCHARACTER pkVictim, float& fDistance)
{
    if (!pkVictim)
        return false;

    // ✅ NOVÉ: Lag compensation
    // Získej ping útočníka
    DWORD dwAttackerPing = 0;
    if (GetDesc())
        dwAttackerPing = GetDesc()->GetPing();

    // Získej ping oběti
    DWORD dwVictimPing = 0;
    if (pkVictim->GetDesc())
        dwVictimPing = pkVictim->GetDesc()->GetPing();

    // Průměrný lag mezi klienty
    DWORD dwAverageLag = (dwAttackerPing + dwVictimPing) / 2;

    // Omezte max lag compensation na 500ms
    if (dwAverageLag > 500)
        dwAverageLag = 500;

    // Získej historickou pozici oběti (před "dwAverageLag" ms)
    DWORD dwCurrentTime = get_dword_time();
    DWORD dwHistoricalTime = dwCurrentTime - dwAverageLag;

    long lVictimX, lVictimY;
    bool bHasHistory = pkVictim->GetHistoricalPosition(dwHistoricalTime, lVictimX, lVictimY);

    if (!bHasHistory)
    {
        // Fallback na aktuální pozici
        lVictimX = pkVictim->GetX();
        lVictimY = pkVictim->GetY();
    }

    // Spočítej vzdálenost s lag-compensated pozicí
    fDistance = DISTANCE_SQRT(
        (GetX() - lVictimX) / 100,
        (GetY() - lVictimY) / 100
    );

    return true;
}

// Použití v Attack() funkci
void CHARACTER::Attack(LPCHARACTER pkVictim, BYTE bType)
{
    // ... existující kód ...

    // ✅ UPRAVENO: Použij lag-compensated distance
    float fDistance = 0.0f;
    if (!ComputeAttackDistance(pkVictim, fDistance))
        return;

    // Kontrola dosahu
    float fAttackRange = GetAttackRange();

    if (fDistance > fAttackRange)
    {
        // ❌ Oběť není v dosahu, útok NEPROBĚHNE
        if (GetDesc())
        {
            // Pošli klientovi info o miss (out of range)
            TPacketGCAttack pack;
            pack.bHeader = HEADER_GC_ATTACK;
            pack.dwVID = GetVID();
            pack.dwVictimVID = pkVictim->GetVID();
            pack.bType = ATTACK_TYPE_MISS;
            GetDesc()->Packet(&pack, sizeof(pack));
        }
        return;
    }

    // ✅ Oběť je v dosahu, pokračuj s útokem
    // ... zbytek attack kódu ...
}
```

### 1.5 Selective Knockback System

Přidáme systém pro kontrolu, které útoky způsobují knockback:

#### Nový soubor: `knockback_config.h`

```cpp
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
```

#### Implementace: `knockback_config.cpp`

```cpp
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
```

### 1.6 Použití Selective Knockback v Attack

```cpp
// char_battle.cpp

void CHARACTER::Attack(LPCHARACTER pkVictim, BYTE bType)
{
    // ... lag-compensated distance check ...

    // ✅ Spočítej damage
    int iDamage = CalcMeleeDamage(this, pkVictim);

    // ✅ Aplikuj damage
    pkVictim->Damage(this, iDamage, DAMAGE_TYPE_NORMAL);

    // ✅ NOVÉ: Selective knockback
    bool bShouldKnockback = false;
    float fKnockbackDistance = 0.0f;
    float fKnockbackDuration = 0.0f;

    // Kontrola combo final hit
    if (bType == ATTACK_TYPE_COMBO)
    {
        BYTE bComboIndex = GetComboIndex();
        if (CKnockbackManager::Instance().IsComboFinalHit(bComboIndex, GetRaceNum()))
        {
            bShouldKnockback = true;
            fKnockbackDistance = 150.0f;  // Default combo knockback
            fKnockbackDuration = 0.3f;
        }
    }

    // Pošli attack packet
    TPacketGCAttack pack;
    pack.bHeader = HEADER_GC_ATTACK;
    pack.dwVID = GetVID();
    pack.dwVictimVID = pkVictim->GetVID();
    pack.bType = bType;

    // ✅ NOVÉ: Přidej knockback info do packetu
    if (bShouldKnockback)
    {
        pack.bType |= 0x80;  // Flag pro knockback

        // Spočítej knockback směr
        float fAngle = GetDegreeFromPosition(pkVictim->GetX(), pkVictim->GetY());
        long lKnockbackX = (long)(fKnockbackDistance * cos(fAngle * M_PI / 180.0f));
        long lKnockbackY = (long)(fKnockbackDistance * sin(fAngle * M_PI / 180.0f));

        // Aplikuj knockback na oběť
        pkVictim->ApplyKnockback(lKnockbackX, lKnockbackY, fKnockbackDuration);
    }

    PacketAround(&pack, sizeof(pack));
}

void CHARACTER::UseSkill(DWORD dwVnum, LPCHARACTER pkVictim)
{
    // ... existující skill kód ...

    // ✅ NOVÉ: Kontrola skill knockback
    float fKnockbackDistance = 0.0f;
    float fKnockbackDuration = 0.0f;

    if (CKnockbackManager::Instance().GetSkillKnockbackInfo(dwVnum, fKnockbackDistance, fKnockbackDuration))
    {
        // Tento skill má knockback
        if (pkVictim && !pkVictim->IsPC())  // Můžeš přidat další podmínky
        {
            float fAngle = GetDegreeFromPosition(pkVictim->GetX(), pkVictim->GetY());
            long lKnockbackX = (long)(fKnockbackDistance * cos(fAngle * M_PI / 180.0f));
            long lKnockbackY = (long)(fKnockbackDistance * sin(fAngle * M_PI / 180.0f));

            pkVictim->ApplyKnockback(lKnockbackX, lKnockbackY, fKnockbackDuration);
        }
    }

    // ... zbytek skill kódu ...
}
```

### 1.7 ApplyKnockback funkce

```cpp
// char.h
class CHARACTER
{
public:
    void ApplyKnockback(long lDeltaX, long lDeltaY, float fDuration);

private:
    bool m_bIsKnockedBack;
    DWORD m_dwKnockbackEndTime;
};

// char.cpp
void CHARACTER::ApplyKnockback(long lDeltaX, long lDeltaY, float fDuration)
{
    if (IsPC() && IsImmune(IMMUNE_STUN))
        return;  // Imunní proti knockbacku

    // Vypočítej novou pozici po knockbacku
    long lNewX = GetX() + lDeltaX;
    long lNewY = GetY() + lDeltaY;

    // Kontrola kolizí s terénem
    if (!SECTREE_MANAGER::instance().IsMovablePosition(GetMapIndex(), lNewX, lNewY))
    {
        // Pokud nelze tam jít, zkrať knockback
        // TODO: Implementuj ray-cast pro nalezení nejbližší validní pozice
        return;
    }

    // Nastav knockback stav
    m_bIsKnockedBack = true;
    m_dwKnockbackEndTime = get_dword_time() + (DWORD)(fDuration * 1000);

    // Pošli knockback packet klientům
    TPacketGCKnockback pack;
    pack.bHeader = HEADER_GC_KNOCKBACK;
    pack.dwVID = GetVID();
    pack.lX = lNewX;
    pack.lY = lNewY;
    pack.fDuration = fDuration;

    PacketAround(&pack, sizeof(pack), this);

    // Okamžitě přesuň postavu
    Show(GetMapIndex(), lNewX, lNewY, GetZ());

    // Aktualizuj position history
    UpdatePositionHistory();
}

void CHARACTER::UpdateKnockbackState()
{
    if (m_bIsKnockedBack)
    {
        DWORD dwCurrentTime = get_dword_time();
        if (dwCurrentTime >= m_dwKnockbackEndTime)
        {
            m_bIsKnockedBack = false;
        }
    }
}

bool CHARACTER::IsKnockedBack() const
{
    return m_bIsKnockedBack;
}
```

### 1.8 VYPNUTÍ/OMEZENÍ SyncPosition

Toto je **klíčové** - musíme zakázat nebo drasticky omezit SyncPosition:

```cpp
// input_main.cpp

int CInputMain::SyncPosition(LPCHARACTER ch, const char * c_pcData, size_t uiBytes)
{
    // ⛔ MOŽNOST 1: Úplně vypnout SyncPosition
    #ifdef DISABLE_SYNC_POSITION
        sys_log(0, "SyncPosition is disabled in new combat system");
        return pinfo->wSize - sizeof(TPacketCGSyncPosition);
    #endif

    // ⛔ MOŽNOST 2: Povolit jen v speciálních případech
    const TPacketCGSyncPosition* pinfo = reinterpret_cast<const TPacketCGSyncPosition*>(c_pcData);

    if (uiBytes < pinfo->wSize)
        return -1;

    int iExtraLen = pinfo->wSize - sizeof(TPacketCGSyncPosition);

    // ... existující validace ...

    for (int i = 0; i < iCount; ++i, ++e)
    {
        LPCHARACTER victim = CHARACTER_MANAGER::instance().Find(e->dwVID);

        if (!victim)
            continue;

        // ✅ NOVÉ: Ignoruj sync pokud se oběť pohybuje
        if (victim->IsMoving())
        {
            sys_log(0, "Ignoring SyncPosition for moving victim %s", victim->GetName());
            continue;
        }

        // ✅ NOVÉ: Ignoruj sync pokud je oběť knockback
        if (victim->IsKnockedBack())
        {
            sys_log(0, "Ignoring SyncPosition for knocked back victim %s", victim->GetName());
            continue;
        }

        // ✅ NOVÉ: Ignoruj sync pokud je difference moc velký (>100 pixels)
        const float fDist = DISTANCE_SQRT((victim->GetX() - e->lX) / 100, (victim->GetY() - e->lY) / 100);

        if (fDist > 100.0f)  // Původně bylo 25.0f
        {
            sys_log(0, "Ignoring SyncPosition: distance too large (%f) for victim %s", fDist, victim->GetName());
            continue;
        }

        // Pokud projde všemi kontrolami, povol MALOU korekci
        // Ale NIKDY nepřepisuj pozici autoritativně

        // ✅ MOŽNOST: Použij váhovanou synchronizaci
        long lCurrentX = victim->GetX();
        long lCurrentY = victim->GetY();

        const float fSyncWeight = 0.1f;  // Pouze 10% váha sync pozice
        long lNewX = (long)(lCurrentX * (1.0f - fSyncWeight) + e->lX * fSyncWeight);
        long lNewY = (long)(lCurrentY * (1.0f - fSyncWeight) + e->lY * fSyncWeight);

        victim->SetLastSyncTime(tvCurTime);
        victim->Sync(lNewX, lNewY);  // Jemná korekce

        buffer_write(lpBuf, e, sizeof(TPacketCGSyncPositionElement));
    }

    // ... zbytek kódu ...

    return iExtraLen;
}
```

---

## 📦 Část 2: Klientská strana

### 2.1 Nový Knockback Packet Handler

Přidej nový packet type do `Packet.h`:

```cpp
// Packet.h (client)

enum
{
    // ... existující headers ...
    HEADER_GC_KNOCKBACK = 99,  // Nový header pro knockback
};

typedef struct packet_knockback
{
    BYTE    bHeader;
    DWORD   dwVID;
    long    lX;
    long    lY;
    float   fDuration;
} TPacketGCKnockback;
```

### 2.2 Knockback Handler v klientu

```cpp
// PythonNetworkStreamPhaseGame.cpp

bool CPythonNetworkStream::RecvKnockbackPacket()
{
    TPacketGCKnockback kPacket;

    if (!Recv(sizeof(kPacket), &kPacket))
        return false;

    // Najdi instanci
    CInstanceBase* pkInst = CPythonCharacterManager::Instance().GetInstancePtr(kPacket.dwVID);

    if (!pkInst)
        return true;

    // ✅ Aplikuj knockback
    TPixelPosition kPosTarget;
    kPosTarget.x = (float)kPacket.lX;
    kPosTarget.y = (float)kPacket.lY;
    kPosTarget.z = 0.0f;

    // Nastav knockback animaci
    pkInst->SetKnockback(kPosTarget, kPacket.fDuration);

    return true;
}

// Registrace v packet handleru
bool CPythonNetworkStream::RecvPhaseGame()
{
    // ... existující kód ...

    switch (header)
    {
        // ... existující cases ...

        case HEADER_GC_KNOCKBACK:
            ret = RecvKnockbackPacket();
            break;
    }

    // ...
}
```

### 2.3 Knockback implementace na instanci

```cpp
// InstanceBase.h

class CInstanceBase
{
public:
    void SetKnockback(const TPixelPosition& c_rkPosTarget, float fDuration);
    bool IsInKnockback() const;
    void UpdateKnockback();

private:
    bool m_bIsInKnockback;
    TPixelPosition m_kKnockbackTarget;
    DWORD m_dwKnockbackStartTime;
    float m_fKnockbackDuration;
};

// InstanceBase.cpp

void CInstanceBase::SetKnockback(const TPixelPosition& c_rkPosTarget, float fDuration)
{
    m_bIsInKnockback = true;
    m_kKnockbackTarget = c_rkPosTarget;
    m_dwKnockbackStartTime = ELTimer_GetMSec();
    m_fKnockbackDuration = fDuration;

    // Nastav knockback animaci
    m_GraphicThingInstance.InterceptOnceMotion(CRaceMotionData::NAME_DAMAGE_FLYING);

    // ✅ Použij kratší blending time pro okamžitou reakci
    m_GraphicThingInstance.SetBlendingPosition(c_rkPosTarget, fDuration);
}

bool CInstanceBase::IsInKnockback() const
{
    return m_bIsInKnockback;
}

void CInstanceBase::UpdateKnockback()
{
    if (!m_bIsInKnockback)
        return;

    DWORD dwCurrentTime = ELTimer_GetMSec();
    DWORD dwElapsedTime = dwCurrentTime - m_dwKnockbackStartTime;

    if (dwElapsedTime >= (DWORD)(m_fKnockbackDuration * 1000))
    {
        // Knockback skončil
        m_bIsInKnockback = false;

        // Nastav pozici na cílovou
        NEW_SetPixelPosition(m_kKnockbackTarget);
    }
}

// V Update() funkci
void CInstanceBase::Update()
{
    // ... existující kód ...

    UpdateKnockback();

    // ...
}
```

### 2.4 Client-side Prediction (Volitelné, ale doporučené)

Pro ještě plynulejší pohyb implementuj client-side prediction:

```cpp
// InstanceBase.cpp

void CInstanceBase::NEW_MoveToDestPixelPositionDirection(const TPixelPosition& c_rkPPosDst)
{
    // ✅ Client-side prediction: Okamžitě začni pohyb bez čekání na server

    // Ulož původní pozici pro reconciliation
    m_kPredictedPosition = GetPosition();

    // Spočítej směr
    TPixelPosition kPPosCur;
    NEW_GetPixelPosition(&kPPosCur);

    float fDirRot = GetDegreeFromPosition(kPPosCur.x, kPPosCur.y, c_rkPPosDst.x, c_rkPPosDst.y);

    // Okamžitě začni walk animaci
    SetAdvancingRotation(fDirRot);
    SetAdvancingDestinationPixelPosition(c_rkPPosDst);

    // Pošli serveru pohybový příkaz
    CPythonNetworkStream::Instance().SendCharacterStatePacket(c_rkPPosDst, fDirRot, CInstanceBase::FUNC_MOVE, 0);
}
```

### 2.5 Server Reconciliation

Když klient přijme pozici ze serveru, musí ji "reconcilovat" s lokální predikcí:

```cpp
// InstanceBase.cpp

void CInstanceBase::NEW_SyncPixelPosition(long& nPPosX, long& nPPosY)
{
    // ✅ NOVÉ: Kratší blending time a reconciliation

    TPixelPosition kPPosCur;
    NEW_GetPixelPosition(&kPPosCur);

    TPixelPosition kPPosServer;
    kPPosServer.x = (float)nPPosX;
    kPPosServer.y = (float)nPPosY;
    kPPosServer.z = 0.0f;

    // Spočítej rozdíl mezi klient a server pozicí
    float fDist = sqrtf(
        (kPPosCur.x - kPPosServer.x) * (kPPosCur.x - kPPosServer.x) +
        (kPPosCur.y - kPPosServer.y) * (kPPosCur.y - kPPosServer.y)
    );

    // ✅ Pokud je rozdíl malý (< 100 pixels), použij kratší blending
    float fBlendTime = 0.1f;  // 100ms místo 2000ms!

    if (fDist > 100.0f)
    {
        // Větší rozdíl, použij delší blending ale stále kratší než původně
        fBlendTime = 0.3f;  // 300ms
    }

    if (fDist > 500.0f)
    {
        // Obrovský rozdíl, pravděpodobně teleport nebo lag spike
        // Okamžitě nastav pozici
        fBlendTime = 0.0f;
    }

    // Aplikuj blending
    if (fBlendTime > 0.0f)
    {
        m_GraphicThingInstance.SetBlendingPosition(kPPosServer, fBlendTime);
    }
    else
    {
        NEW_SetPixelPosition(kPPosServer);
    }
}
```

### 2.6 Redukce interpolačního času

Uprav interpolační časy globálně:

```cpp
// PhysicsObject.cpp

void CPhysicsObject::SetLastPosition(const TPixelPosition & c_rPosition, float fBlendingTime)
{
    m_v3LastPosition.x = float(c_rPosition.x);
    m_v3LastPosition.y = float(c_rPosition.y);
    m_v3LastPosition.z = float(c_rPosition.z);

    // ✅ UPRAVENO: Kratší blending time
    // Původně bylo ~2 sekundy, nyní 0.1-0.3s
    float fAdjustedBlendTime = fBlendingTime * 0.05f;  // 5% původního času

    if (fAdjustedBlendTime < 0.05f)
        fAdjustedBlendTime = 0.05f;  // Min 50ms

    if (fAdjustedBlendTime > 0.3f)
        fAdjustedBlendTime = 0.3f;   // Max 300ms

    m_xPushingPosition.Setup(0.0f, c_rPosition.x, fAdjustedBlendTime);
    m_yPushingPosition.Setup(0.0f, c_rPosition.y, fAdjustedBlendTime);
}
```

---

## 📊 Část 3: Packet Definitions

### 3.1 Server packet.h

```cpp
// packet.h (server)

enum
{
    // ... existující ...
    HEADER_GC_KNOCKBACK = 99,
};

typedef struct packet_knockback
{
    BYTE    bHeader;
    DWORD   dwVID;
    long    lX;
    long    lY;
    float   fDuration;
} TPacketGCKnockback;
```

### 3.2 Client Packet.h

Stejný jako server (už je výše).

---

## 🎮 Část 4: Jak to funguje v praxi

### Scénář: Hráč A útočí na Hráče B

1. **Hráč A klikne na útok:**
   - Klient A okamžitě spustí útočnou animaci
   - Klient A pošle attack packet serveru

2. **Hráč B se pokouší utéct:**
   - Klient B používá client-side prediction
   - Okamžitě začne pohybovou animaci
   - Posílá pohybové pakety serveru

3. **Server přijme attack packet od A:**
   - Server zjistí ping A (např. 150ms) a ping B (např. 200ms)
   - Průměrný lag = 175ms
   - Server použije lag compensation: Získá pozici B z historie před 175ms
   - Server spočítá vzdálenost mezi A a historickou pozicí B

4. **Hit detection:**
   - **Pokud B NENÍ v dosahu:** Server pošle A miss packet, útok NEPROBĚHNE
   - **Pokud B JE v dosahu:** Server spočítá damage a aplikuje ho

5. **Knockback detection:**
   - Server zkontroluje, zda tento útok má knockback (combo final hit?)
   - **Pokud NÁ:** Server pošle knockback packet všem klientům
   - **Pokud NEMÁ:** B se může dál pohybovat

6. **Klient B přijme:**
   - **Hit packet:** Spustí damage animaci, NECITELNÉ pohyb (pokud není knockback)
   - **Knockback packet:** Spustí knockback animaci a interpoluje na novou pozici

7. **Výsledek:**
   - ✅ Žádný pull-back (B není přitahován k A)
   - ✅ B se může pohybovat během boje
   - ✅ Knockback pouze při finálních hitech
   - ✅ Fair hit detection díky lag compensation

---

## ⚙️ Část 5: Konfigurace a Tuning

### 5.1 Konfigurační soubor: `combat_config.lua` nebo `CONFIG`

```lua
-- combat_config.lua

-- Lag compensation
MAX_LAG_COMPENSATION = 500  -- ms, maximální lag compensation
POSITION_HISTORY_DURATION = 1000  -- ms, jak dlouho uchovávat historii

-- Knockback
COMBO_FINAL_KNOCKBACK_DISTANCE = 150  -- pixels
COMBO_FINAL_KNOCKBACK_DURATION = 0.3  -- seconds
SKILL_KNOCKBACK_ENABLED = true

-- Movement interpolation
CLIENT_POSITION_BLEND_TIME = 0.1  -- seconds, jak rychle blendovat pozice
MAX_POSITION_BLEND_TIME = 0.3  -- seconds, maximum
MIN_POSITION_BLEND_TIME = 0.05  -- seconds, minimum

-- SyncPosition (legacy system)
SYNC_POSITION_ENABLED = false  -- Vypnout úplně
SYNC_POSITION_WEIGHT = 0.1  -- Pokud povoleno, váha sync pozice

-- Hit detection
MELEE_ATTACK_RANGE = 170  -- pixels, základní melee dosah
BOW_ATTACK_RANGE = 1000  -- pixels, základní bow dosah
```

### 5.2 Načítání konfigurace

```cpp
// config.cpp

bool LoadCombatConfig()
{
    // TODO: Načíst z combat_config.lua nebo CONFIG file

    // Příklad s pevnými hodnotami:
    g_iMaxLagCompensation = 500;
    g_iPositionHistoryDuration = 1000;
    g_fComboKnockbackDistance = 150.0f;
    g_fComboKnockbackDuration = 0.3f;
    g_bSyncPositionEnabled = false;  // VYPNOUT!
    g_fSyncPositionWeight = 0.1f;

    return true;
}
```

---

## 🧪 Část 6: Testování

### 6.1 Testovací checklist:

- [ ] **Test 1:** Oběť se může pohybovat během normálního útoku (bez knockback)
- [ ] **Test 2:** Žádný pull-back efekt při útěku
- [ ] **Test 3:** Knockback funguje pouze na poslední combo hit
- [ ] **Test 4:** Skill s knockback správně odhazuje oběť
- [ ] **Test 5:** Hit detection funguje při 0ms lagu
- [ ] **Test 6:** Hit detection funguje při 100ms lagu
- [ ] **Test 7:** Hit detection funguje při 300ms lagu
- [ ] **Test 8:** Hit detection funguje při 500ms lagu
- [ ] **Test 9:** Útok mimo dosah vrací miss
- [ ] **Test 10:** Plynulý pohyb i při vysokém lagu

### 6.2 Debug logging

Přidej debug logging pro analýzu:

```cpp
// char_battle.cpp

void CHARACTER::Attack(LPCHARACTER pkVictim, BYTE bType)
{
    // ... lag compensation ...

    #ifdef ENABLE_COMBAT_DEBUG
    sys_log(0, "[COMBAT DEBUG] Attacker=%s Victim=%s AttackerPing=%d VictimPing=%d AvgLag=%d Distance=%f Range=%f Hit=%s",
        GetName(),
        pkVictim->GetName(),
        dwAttackerPing,
        dwVictimPing,
        dwAverageLag,
        fDistance,
        fAttackRange,
        (fDistance <= fAttackRange) ? "YES" : "NO"
    );
    #endif

    // ...
}
```

---

## 📚 Část 7: Shrnutí změn

### Serverová strana:
1. ✅ Přidán `CPositionHistory` - ukládání historie pozic
2. ✅ Přidán `CKnockbackManager` - konfigurace knockback skillů
3. ✅ Upravena `ComputeAttackDistance()` - lag compensation
4. ✅ Upravena `Attack()` - selective knockback
5. ✅ Přidána `ApplyKnockback()` - aplikace knockback
6. ✅ Upravena/vypnuta `SyncPosition()` - odstranění pull-back
7. ✅ Aktualizace historie v `Move()` a `Update()`

### Klientská strana:
1. ✅ Přidán knockback packet handler
2. ✅ Přidána `SetKnockback()` - knockback animace
3. ✅ Upravena `NEW_SyncPixelPosition()` - kratší blending time
4. ✅ Upravena `SetLastPosition()` - redukce interpolačního času
5. ✅ Volitelně: Client-side prediction

### Nové packety:
1. ✅ `HEADER_GC_KNOCKBACK` - knockback notifikace
2. ✅ `TPacketGCKnockback` - knockback data

---

## 🎯 Výhody tohoto řešení

1. ✅ **Žádný pull-back** - Oběť není přitahována zpět
2. ✅ **Plynulý pohyb** - Kratší interpolační časy (100-300ms vs. 2000ms)
3. ✅ **Lag tolerance** - Funguje i při 300-500ms lagu díky lag compensation
4. ✅ **Fair gameplay** - Hit detection založená na průměru pingů
5. ✅ **Selective knockback** - Pouze poslední combo hit a speciální skilly
6. ✅ **Server authority** - Server má finální autoritu
7. ✅ **Anti-cheat** - Position history umožňuje detekovat teleporty
8. ✅ **Backwards compatible** - Můžeš zachovat SyncPosition pro legacy módy

---

## 🔧 Další vylepšení (volitelné)

### 1. Position prediction na serveru
Server může predikovat pozici na základě velocity:
```cpp
void CHARACTER::PredictPosition(float fDeltaTime, long& lPredictedX, long& lPredictedY)
{
    // Pokud se hráč pohybuje, predikuj kam dorazí
    if (IsMoving())
    {
        float fSpeed = GetMoveSpeed();
        float fDist = fSpeed * fDeltaTime;

        // Směr pohybu
        float fAngle = GetRotation();
        lPredictedX = GetX() + (long)(fDist * cos(fAngle));
        lPredictedY = GetY() + (long)(fDist * sin(fAngle));
    }
    else
    {
        lPredictedX = GetX();
        lPredictedY = GetY();
    }
}
```

### 2. Ray-cast pro knockback kolize
```cpp
bool FindValidKnockbackPosition(long lStartX, long lStartY, long lTargetX, long lTargetY, long& lFinalX, long& lFinalY)
{
    // Implementuj ray-casting pro nalezení nejbližší validní pozice
    // pokud je přímá cesta blokovaná
}
```

### 3. Adaptivní blending time
```cpp
float CalculateAdaptiveBlendTime(float fDistance, DWORD dwPing)
{
    // Kratší blend pro malé vzdálenosti a nízký ping
    // Delší blend pro velké vzdálenosti a vysoký ping

    float fBaseTime = 0.1f;
    float fPingFactor = (float)dwPing / 1000.0f;  // ping v sekundách
    float fDistFactor = fDistance / 100.0f;

    float fBlendTime = fBaseTime + fPingFactor + fDistFactor * 0.001f;

    if (fBlendTime > 0.5f)
        fBlendTime = 0.5f;

    return fBlendTime;
}
```

---

## 📖 Závěr

Toto řešení poskytuje **moderní combat systém** který:

- Eliminuje pull-back problém úplně
- Funguje plynule i při vysokém lagu
- Poskytuje fair gameplay díky lag compensation
- Zachovává server autoritu pro bezpečnost
- Umožňuje plynulý pohyb během boje
- Implementuje selektivní knockback pouze pro specifické útoky

Implementace vyžaduje změny jak na serveru, tak na klientu, ale výsledek je mnohem lepší herní zážitek pro všechny hráče, zejména ty s vyšším pingem.
