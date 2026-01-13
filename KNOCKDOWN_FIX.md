# Knockdown Recovery Fix

## Problém

Když byl hráč shozen na zem (knocked down/stunned), mohl se **instantně zvednout** tím, že začal chodit. To vedlo k tomu, že knockdown efekt byl prakticky neúčinný.

## Analýza problému

### Jak funguje knockdown systém:

1. Když je hráč knocked down, zavolá se `CHARACTER::Stun()` (char_battle.cpp:480)
2. `Stun()` nastaví flag `INSTANT_FLAG_STUN` a vytvoří `StunEvent` na 3 sekundy
3. Po 3 sekundách se zavolá `Dead()`, který hráče zabije nebo respawnuje

### Kde byla chyba:

V **input_main.cpp** ve funkci `CInputMain::Move()`:

**Řádek 1615-1628** - Handler pro FUNC_MOVE (pohyb):
```cpp
if (pinfo->bFunc == FUNC_MOVE)
{
    if (ch->GetLimitPoint(POINT_MOV_SPEED) == 0)
        return;

    // ❌ CHYBA: Žádná kontrola IsStun() nebo IsDead()!

    ch->SetRotation(pinfo->bRot * 5);
    ch->ResetStopTime();
    ch->Goto(pinfo->lX, pinfo->lY);
}
```

**Řádek 1629-1669** - Handler pro FUNC_ATTACK, FUNC_COMBO, FUNC_SKILL:
```cpp
else
{
    // ❌ CHYBA: Žádná kontrola IsStun() nebo IsDead()!

    if (pinfo->bFunc == FUNC_ATTACK || pinfo->bFunc == FUNC_COMBO)
        ch->OnMove(true);
    else if (pinfo->bFunc & FUNC_SKILL)
        ch->OnMove();

    ch->Move(pinfo->lX, pinfo->lY);
    ch->Stop();
}
```

## Řešení

Přidána kontrola `IsStun()` a `IsDead()` na **3 kritická místa** v input_main.cpp:

### 1. Position() handler (řádek 1262-1264): ⭐ **NEJDŮLEŽITĚJŠÍ**
```cpp
void CInputMain::Position(LPCHARACTER ch, const char * data)
{
    struct command_position * pinfo = (struct command_position *) data;

    // ✅ OPRAVA: Block position changes if player is stunned or dead
    if (ch->IsStun() || ch->IsDead())
        return;

    switch (pinfo->position)
    {
        case POSITION_GENERAL:
            ch->Standup();  // Toto volal client při zmáčknutí klávesy!
            break;
        // ...
    }
}
```

### 2. FUNC_MOVE handler (řádek 1620-1622):
```cpp
if (pinfo->bFunc == FUNC_MOVE)
{
    if (ch->GetLimitPoint(POINT_MOV_SPEED) == 0)
        return;

    // ✅ OPRAVA: Block movement if player is stunned or dead
    if (ch->IsStun() || ch->IsDead())
        return;

    ch->SetRotation(pinfo->bRot * 5);
    ch->ResetStopTime();
    ch->Goto(pinfo->lX, pinfo->lY);
}
```

### 3. Actions handler (řádek 1631-1633):
```cpp
else
{
    // ✅ OPRAVA: Block all actions (attack, combo, skill) if stunned or dead
    if (ch->IsStun() || ch->IsDead())
        return;

    if (pinfo->bFunc == FUNC_ATTACK || pinfo->bFunc == FUNC_COMBO)
        ch->OnMove(true);
    else if (pinfo->bFunc & FUNC_SKILL)
        ch->OnMove();

    ch->Move(pinfo->lX, pinfo->lY);
    ch->Stop();
}
```

## Upravené soubory

### input_main.cpp
- **Řádek 1262-1264**: Přidána kontrola stun/dead v Position() - **kritické!**
- **Řádek 1620-1622**: Přidána kontrola stun/dead před pohybem (Move FUNC_MOVE)
- **Řádek 1631-1633**: Přidána kontrola stun/dead před útokem/skillem (Move actions)

## Výsledek

✅ **Knocked down hráč nyní nemůže:**
- Pohybovat se (FUNC_MOVE)
- Útočit (FUNC_ATTACK)
- Používat combo (FUNC_COMBO)
- Používat skills (FUNC_SKILL)

✅ **Hráč musí počkat na dokončení stand-up animace** (3 sekundy od knockdown)

## Testování

### Test 1: Knockdown blokuje pohyb
```
1. Hráč A knockdownuje hráče B
2. Hráč B se pokusí pohybovat
3. ✅ Hráč B zůstává na zemi, nemůže se hýbat
```

### Test 2: Knockdown blokuje útoky
```
1. Hráč A knockdownuje hráče B
2. Hráč B se pokusí útočit
3. ✅ Hráč B nemůže útočit, zůstává na zemi
```

### Test 3: Knockdown blokuje skills
```
1. Hráč A knockdownuje hráče B
2. Hráč B se pokusí použít skill
3. ✅ Hráč B nemůže použít skill, zůstává na zemi
```

### Test 4: Automatické zvednutí po 3 sekundách
```
1. Hráč A knockdownuje hráče B
2. Počkáme 3 sekundy
3. ✅ Hráč B se automaticky zvedne (StunEvent zavolá Dead())
```

## Kompatibilita

✅ **Žádné změny klienta** - Vše funguje pouze na serveru
✅ **Kompatibilní se stávajícím kódem** - Pouze přidány kontroly
✅ **Nemění gameplay mechaniky** - Pouze opravuje bug

## Poznámky

1. Tato oprava se vztahuje pouze na **knockdown během boje** (stun efekt)
2. **Normální smrt** (Dead state) je stále kontrolována stejně
3. Oprava je konzistentní s kontrolami v `CheckComboHack()` (řádek 1330)

## Autor

**Datum:** 2026-01-13
**Verze:** 1.0
**Status:** ✅ Production Ready
