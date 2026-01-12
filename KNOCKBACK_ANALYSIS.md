# Analýza Knockback/Pull-back mechanismu v Metin2

## Souhrn problému

Když útočník útočí a oběť se pokusí utéct, každý sekání útočníka "přitáhne" oběť zpět k útočníkovi. Tento efekt je způsoben problematickým mechanismem synchronizace pozic mezi klientem a serverem.

---

## Podrobná analýza - Jak mechanismus funguje

### 1. Klientská strana (Client branch)

#### A) Position Interpolation (PhysicsObject.cpp)
- Klient používá **interpolaci** pro plynulé přechody pozice
- `CEaseOutInterpolation` pro X a Y souřadnice
- Když přijde nová pozice, klient ji "blenduje" (interpoluje) po dobu ~2s (100 frames * 0.02s)

**Klíčový kód:**
```cpp
// PhysicsObject.cpp:110-117
void CPhysicsObject::SetLastPosition(const TPixelPosition & c_rPosition, float fBlendingTime)
{
    m_v3LastPosition.x = float(c_rPosition.x);
    m_v3LastPosition.y = float(c_rPosition.y);
    m_v3LastPosition.z = float(c_rPosition.z);
    m_xPushingPosition.Setup(0.0f, c_rPosition.x, fBlendingTime);  // Ease-out interpolace
    m_yPushingPosition.Setup(0.0f, c_rPosition.y, fBlendingTime);  // Ease-out interpolace
}
```

#### B) Position Sync Packet (PythonNetworkStreamPhaseGame.cpp)
- Klient posílá pozici oběti serveru pomocí `SendSyncPositionElementPacket()`
- Packet obsahuje: VID oběti, X, Y pozice

**Klíčový kód:**
```cpp
// PythonNetworkStreamPhaseGame.cpp:2731-2747
bool CPythonNetworkStream::SendSyncPositionElementPacket(DWORD dwVictimVID, DWORD dwVictimX, DWORD dwVictimY)
{
    TPacketCGSyncPositionElement kSyncPos;
    kSyncPos.dwVID=dwVictimVID;
    kSyncPos.lX=dwVictimX;
    kSyncPos.lY=dwVictimY;

    __LocalPositionToGlobalPosition(kSyncPos.lX, kSyncPos.lY);

    if (!Send(sizeof(kSyncPos), &kSyncPos))
    {
        Tracen("CPythonNetworkStream::SendSyncPositionElementPacket - ERROR");
        return false;
    }

    return true;
}
```

#### C) Network Actor Manager (NetworkActorManager.cpp)
- Když přijde sync pozice ze serveru, volá se `SyncActor()`
- Ta pak volá `NEW_SyncPixelPosition()` na oběti

**Klíčový kód:**
```cpp
// NetworkActorManager.cpp:539-559
void CNetworkActorManager::SyncActor(DWORD dwVID, LONG lPosX, LONG lPosY)
{
    // ... najdi oběť ...

    CInstanceBase* pkInstFind=__FindActor(rkNetActorData);
    if (pkInstFind)
    {
        pkInstFind->NEW_SyncPixelPosition(lPosX, lPosY);  // Interpoluje pozici!
    }

    rkNetActorData.SetPosition(lPosX, lPosY);
}
```

---

### 2. Serverová strana (Server branch)

#### A) SyncPosition Handler (input_main.cpp)
Zde je **jádro problému**! Server přijme pozici od útočníka a autoritativně ji nastaví oběti.

**Klíčový kód:**
```cpp
// input_main.cpp:1798-1953
int CInputMain::SyncPosition(LPCHARACTER ch, const char * c_pcData, size_t uiBytes)
{
    // ... parsování paketu ...

    for (int i = 0; i < iCount; ++i, ++e)
    {
        LPCHARACTER victim = CHARACTER_MANAGER::instance().Find(e->dwVID);

        // ... validace (vzdálenost, interval) ...

        // ⚠️ PROBLÉM: Server autoritativně nastaví pozici oběti!
        victim->Sync(e->lX, e->lY);  // Řádek 1939
        buffer_write(lpBuf, e, sizeof(TPacketCGSyncPositionElement));
    }

    // ⚠️ PROBLÉM: Pošle synchronizovanou pozici VŠEM okolo včetně oběti!
    if (buffer_size(lpBuf) != sizeof(TPacketGCSyncPosition))
    {
        pHeader->bHeader = HEADER_GC_SYNC_POSITION;
        pHeader->wSize = buffer_size(lpBuf);

        ch->PacketAround(buffer_read_peek(lpBuf), buffer_size(lpBuf), ch);  // Řádek 1949
    }

    return iExtraLen;
}
```

**Validace na serveru:**
1. Vzdálenost útočníka od oběti < 3500 (2500 skill range + 1000 buffer)
2. Rozdíl mezi aktuální a sync pozicí oběti < 25.0 jednotek
3. Časový interval mezi sync > 50ms

#### B) CHARACTER::Sync() (char.cpp)
Tato funkce prostě nastaví pozici bez dalších kontrol.

**Klíčový kód:**
```cpp
// char.cpp:2634-2658
bool CHARACTER::Sync(long x, long y)
{
    if (!GetSectree())
        return false;

    LPSECTREE new_tree = SECTREE_MANAGER::instance().Get(GetMapIndex(), x, y);

    if (!new_tree)
    {
        // ... error handling ...
        return false;
    }

    SetRotationToXY(x, y);
    SetXYZ(x, y, 0);  // ⚠️ Přímo nastaví pozici!

    // ... event handling ...
}
```

---

## Proč oběť je přitahována k útočníkovi?

### Flow událostí:

1. **Útočník útočí:**
   - Útočníkův klient vidí oběť na pozici A (kde byla při úderu)
   - Útočník posílá serveru: "Oběť je na pozici A" (`SendSyncPositionElementPacket`)

2. **Oběť se pokouší utéct:**
   - Klient oběti se pohybuje na pozici B, C, D...
   - Oběť posílá serveru své pohybové pakety

3. **Server přijme sync od útočníka:**
   - Server validuje, že pozice A je blízko aktuální pozice oběti (< 25 jednotek)
   - Server **autoritativně nastaví** pozici oběti zpět na A (`victim->Sync(A)`)
   - Server pošle všem klientům (včetně oběti): "Oběť je na pozici A"

4. **Klient oběti přijme sync:**
   - Klient oběti je už na pozici D
   - Přijme od serveru: "Ty jsi na pozici A"
   - Klient použije **ease-out interpolaci** (blending) k přechodu z D na A
   - Výsledek: Vizuální efekt "přitažení" zpět k útočníkovi

5. **Opakování:**
   - Každý útok útočníka → sync pozice → oběť je "přitažena"
   - Oběť se nemůže efektivně utéct

---

## Proč byl tento mechanismus implementován?

Tento mechanismus byl pravděpodobně navržen pro:
1. **Anti-cheat** - Prevence teleportů a speed hacků
2. **Lag compensation** - Kompenzace síťového zpoždění
3. **Hit validation** - Zajištění, že útoky zasáhnou správný cíl

**Problém:** Útočník má autoritu nad pozicí oběti během boje, což umožňuje "pull-back" efekt.

---

## Návrhy řešení

### Řešení 1: Server-side pozice jako autorita (Doporučeno)

**Princip:** Server by neměl slepě věřit pozici od útočníka. Místo toho by měl používat svou vlastní autoritu nad pozicemi.

**Implementace na serveru (input_main.cpp):**

```cpp
int CInputMain::SyncPosition(LPCHARACTER ch, const char * c_pcData, size_t uiBytes)
{
    // ... existující kód ...

    for (int i = 0; i < iCount; ++i, ++e)
    {
        LPCHARACTER victim = CHARACTER_MANAGER::instance().Find(e->dwVID);

        // ... existující validace ...

        // ✅ NOVÉ: Neposílejme sync pozici, pokud oběť NENÍ v útočníkově valid rangi
        //    NEBO pokud se oběť aktivně pohybuje

        // Kontrola, zda se oběť pohybuje
        if (victim->IsMoving())
        {
            // Oběť se aktivně pohybuje, IGNORUJ sync od útočníka
            // Server už má aktuální pozici od pohybových paketů oběti
            continue;
        }

        // Kontrola, zda je oběť stále v combat rangi
        const float fCurrentDist = DISTANCE_SQRT(
            (victim->GetX() - ch->GetX()) / 100,
            (victim->GetY() - ch->GetY()) / 100
        );

        const float fMaxCombatRange = 500.f;  // Melee range

        if (fCurrentDist > fMaxCombatRange)
        {
            // Oběť je moc daleko, útočník už by neměl ovládat její pozici
            continue;
        }

        // Pokud oběť NESTOJÍ a NENÍ v rangi, ignoruj sync

        // ✅ UPRAVENÉ: Používej server pozici jako autoritu
        // Místo victim->Sync(e->lX, e->lY) použij validaci:
        const float fSyncDist = DISTANCE_SQRT(
            (victim->GetX() - e->lX) / 100,
            (victim->GetY() - e->lY) / 100
        );

        // Pouze pro MALÉ korekce (lag compensation)
        const float fMaxSyncCorrection = 50.f;  // 50 pixel max korekce

        if (fSyncDist < fMaxSyncCorrection)
        {
            victim->SetLastSyncTime(tvCurTime);
            victim->Sync(e->lX, e->lY);
            buffer_write(lpBuf, e, sizeof(TPacketCGSyncPositionElement));
        }
        // Jinak ignoruj sync požadavek
    }

    // ... zbytek kódu ...
}
```

**Výhody:**
- ✅ Oběť může utéct, protože její pohybové pakety mají prioritu
- ✅ Stále funguje lag compensation pro malé korekce
- ✅ Anti-cheat stále funguje (validace vzdálenosti)
- ✅ Minimální změny v kódu

**Nevýhody:**
- ⚠️ Útočník může občas vidět "miss" animace kvůli desyncu
- ⚠️ Vyžaduje vyladění parametrů (max range, max correction)

---

### Řešení 2: Jednosměrný sync pouze pro stojící oběti

**Princip:** Sync pozice funguje POUZE když oběť stojí (není v pohybu).

**Implementace na serveru (input_main.cpp):**

```cpp
// Před řádkem 1939
if (victim->IsMoving() || victim->GetMotionMode() == MOTION_MODE_MOVE)
{
    // Oběť se pohybuje, NEPŘEPISUJ její pozici
    continue;
}

// Kontrola stavu
DWORD dwVictimState = victim->GetCharacterState();
if (dwVictimState & (CHARACTER_STATE_MOVE | CHARACTER_STATE_SWIMMING))
{
    // Oběť je v pohybu, IGNORUJ sync
    continue;
}

// Pouze pokud oběť STOJÍ
victim->SetLastSyncTime(tvCurTime);
victim->Sync(e->lX, e->lY);
buffer_write(lpBuf, e, sizeof(TPacketCGSyncPositionElement));
```

**Výhody:**
- ✅ Jednoduchá implementace
- ✅ Oběť může vždy utéct
- ✅ Žádný pull-back během útěku

**Nevýhody:**
- ⚠️ Možné problémy s lag compensation
- ⚠️ Oběť může "teleportovat" z pohledu útočníka

---

### Řešení 3: Časové okno pro sync autoritu

**Princip:** Útočník má autoritu nad pozicí oběti pouze krátce po úspěšném útoku (např. 200ms).

**Implementace na serveru (char.h + char_battle.cpp):**

```cpp
// char.h
class CHARACTER
{
    // ...
    DWORD m_dwLastHitTime;  // Čas posledního útoku na tuto postavu

    bool CanBeSyncedBy(LPCHARACTER attacker);
};

// char_battle.cpp
void CHARACTER::Damage(...)
{
    // ... existující damage kód ...

    // Po úspěšném útoku
    if (damage > 0)
    {
        m_dwLastHitTime = get_dword_time();
    }
}

bool CHARACTER::CanBeSyncedBy(LPCHARACTER attacker)
{
    if (!attacker)
        return false;

    // Sync je povolen pouze 200ms po útoku
    const DWORD SYNC_AUTHORITY_WINDOW = 200;  // ms

    DWORD dwCurrentTime = get_dword_time();
    DWORD dwTimeSinceHit = dwCurrentTime - m_dwLastHitTime;

    return (dwTimeSinceHit < SYNC_AUTHORITY_WINDOW);
}

// input_main.cpp
int CInputMain::SyncPosition(...)
{
    // ...

    for (int i = 0; i < iCount; ++i, ++e)
    {
        LPCHARACTER victim = CHARACTER_MANAGER::instance().Find(e->dwVID);

        // ✅ NOVÉ: Kontrola časového okna
        if (!victim->CanBeSyncedBy(ch))
        {
            // Útočník už nemá autoritu
            continue;
        }

        // ... zbytek kódu ...
        victim->Sync(e->lX, e->lY);
    }
}
```

**Výhody:**
- ✅ Balancovaný přístup
- ✅ Útočník má autoritu pouze krátce po hit
- ✅ Oběť může utéct po 200ms
- ✅ Zachovává původní účel sync (hit validation)

**Nevýhody:**
- ⚠️ Složitější implementace
- ⚠️ Vyžaduje trackování hit times

---

### Řešení 4: Weighted sync (Váhovaná synchronizace)

**Princip:** Místo absolutního přepsání pozice použij váhovaný průměr mezi server pozicí a sync pozicí.

**Implementace na serveru (char.cpp):**

```cpp
// Místo přímého Sync() použij váhovanou pozici
void CHARACTER::WeightedSync(long syncX, long syncY, float weight)
{
    long currentX = GetX();
    long currentY = GetY();

    // Interpolace mezi aktuální a sync pozicí
    long newX = (long)(currentX * (1.0f - weight) + syncX * weight);
    long newY = (long)(currentY * (1.0f - weight) + syncY * weight);

    Sync(newX, newY);
}

// input_main.cpp
// Místo victim->Sync(e->lX, e->lY);
float fSyncWeight = 0.3f;  // 30% sync pozice, 70% server pozice
victim->WeightedSync(e->lX, e->lY, fSyncWeight);
```

**Výhody:**
- ✅ Plynulý přechod
- ✅ Kompromis mezi útočníkem a obětí
- ✅ Snížený pull-back efekt

**Nevýhody:**
- ⚠️ Stále existuje pull-back, jen menší
- ⚠️ Vyžaduje tuning weight parametru

---

## Doporučení

**Nejlepší řešení:** Kombinace **Řešení 1** a **Řešení 3**

1. **Primárně:** Ignoruj sync pokud se oběť aktivně pohybuje
2. **Sekundárně:** Omezte sync autoritu časovým oknem po útoku
3. **Fallback:** Použij váženou synchronizaci pro malé korekce

**Implementační kroky:**

1. ✅ Přidej kontrolu `IsMoving()` v `SyncPosition()`
2. ✅ Přidaj časové okno `CanBeSyncedBy()`
3. ✅ Omezte maximální sync korekci (např. 50 pixelů)
4. ✅ Testuj balance mezi anti-cheat a gameplay

---

## Alternativní přístup: Client-side predikce

Pokud chcete zásadnější změnu, zvažte:

1. **Client-side prediction:** Klient předvídá pozice na základě pohybu
2. **Server reconciliation:** Server posílá korekce pouze při velkém rozdílu
3. **Lag compensation:** Server používá rewind techniku pro hit detection

To by však vyžadovalo větší refactoring síťového kódu.

---

## Soubory k úpravě

### Serverová strana:
- `input_main.cpp` (funkce `SyncPosition()`)
- `char.cpp` (funkce `Sync()`, možná nová `WeightedSync()`)
- `char.h` (nové členy jako `m_dwLastHitTime`, `CanBeSyncedBy()`)
- `char_battle.cpp` (trackování hit times)

### Klientská strana:
- Možná žádné změny nutné
- Nebo úprava interpolace v `PhysicsObject.cpp` (kratší blending time)

---

## Závěr

Pull-back problém je způsoben tím, že **útočník má autoritu nad pozicí oběti** během boje prostřednictvím `SyncPosition` mechanismu. Server slepě věří pozici od útočníka a přepisuje serverovou pozici oběti, což způsobuje, že klient oběti interpoluje zpět.

Řešení spočívá v omezení této autority - buď ignorováním sync když se oběť pohybuje, nebo omezením časovým oknem, nebo kombinací obou přístupů.

Doporučuji začít s **Řešením 1** (kontrola pohybu) jako nejjednodušší a nejefektivnější přístup.
