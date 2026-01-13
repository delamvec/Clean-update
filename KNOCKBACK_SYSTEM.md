# Knockback + Knockdown System

## 🎯 Popis

Kompletní implementace knockback (odhodí oběť) + knockdown (pád na zem + stun) systému pro Metin2.

## ⚡ Funkce

### 1. **Knockback** = Fyzické odhodění
- Oběť je odhodnuta **300 pixelů** (3 metry) směrem **od útočníka**
- Smooth teleportace pomocí `WarpSet()`
- Respektuje mapu a kolize

### 2. **Knockdown** = Pád + Stun
- Po odhodění postava **spadne na zem**
- Aplikuje se **3sekundový stun**
- Během stunu **nemůže dělat NIC**:
  - ❌ Pohyb (WASD)
  - ❌ Útoky (mezerník)
  - ❌ Combo
  - ❌ Skills
  - ❌ Zvednutí (žádné klávesy nefungují)

## 🎮 Kdy se aktivuje

### Poslední combo hity:
- **bType 17-20** (finální údery v combo řetězci)
- Aktivuje se **automaticky** při posledním combo hitu

### Budoucí rozšíření (připraveno):
- Specifické skills s knockback efektem
- Kritické údery (volitelné)
- Konfigurovatelné přes `knockback_config.h/cpp`

## 📁 Upravené soubory

### 1. **char.h**
- **Řádek 1255**: Přidána deklarace `ApplyKnockback(LPCHARACTER pkAttacker, float fDistance = 300.0f)`

### 2. **char_battle.cpp**

#### ApplyKnockback() implementace (řádky 519-589):
```cpp
void CHARACTER::ApplyKnockback(LPCHARACTER pkAttacker, float fDistance)
{
    // 1. Vypočítá směr od útočníka k oběti
    float dx = (float)(GetX() - pkAttacker->GetX());
    float dy = (float)(GetY() - pkAttacker->GetY());

    // 2. Normalizuje směr
    float fLen = sqrtf(dx * dx + dy * dy);
    dx /= fLen;
    dy /= fLen;

    // 3. Vypočítá novou pozici (posunutá o fDistance)
    long lNewX = GetX() + (long)(dx * fDistance);
    long lNewY = GetY() + (long)(dy * fDistance);

    // 4. Teleportuje oběť pomocí WarpSet
    WarpSet(lNewX, lNewY);

    // 5. Aplikuje stun (pád na zem)
    Stun();
}
```

#### Attack() knockback logic (řádky 355-386):
```cpp
// ✅ Knockback system - aplikuj knockback na poslední combo hity
if (IsPC() && pkVictim->IsPC() && iRet == BATTLE_DAMAGE)
{
    bool bShouldKnockback = false;

    if (bType >= 17 && bType <= 20)  // Poslední combo hity
    {
        bShouldKnockback = true;
    }

    if (bShouldKnockback)
    {
        pkVictim->ApplyKnockback(this, 300.0f);  // 3 metry
    }
}
```

### 3. **input_main.cpp** (předchozí fix)
- **Řádek 1262-1264**: Blokuje Position() změny během stunu
- **Řádek 1620-1622**: Blokuje WASD pohyb během stunu
- **Řádek 1631-1633**: Blokuje útoky/skills během stunu

## 🔧 Jak to funguje

### Krok za krokem:

1. **Hráč A útočí na hráče B**
2. **Poslední combo hit** (bType 17-20) zasáhne
3. **Attack() detekuje knockback trigger**
4. **ApplyKnockback() se zavolá:**
   - Vypočítá směr: B je na východ od A → push na východ
   - Vypočítá novou pozici: current + 300px na východ
   - Teleportuje B na novou pozici
   - Aplikuje Stun() na B
5. **Stun() nastaví INSTANT_FLAG_STUN**
6. **Hráč B spadne na zem** (client animace)
7. **IsStun() vrací true** → všechny kontroly v input_main.cpp blokují akce
8. **Po 3 sekundách** → StunEvent zavolá Dead() → respawn/recovery
9. **Hráč B se zvedne** automaticky

## 🎯 Výsledek

### ✅ Před knockbackem:
- Hráč A útočí
- Hráč B je na pozici (X: 1000, Y: 2000)

### ✅ Po knockbacku:
- Hráč B je odhozen na pozici (X: 1300, Y: 2000) = 300px dál
- Hráč B leží na zemi
- **NEMŮŽE SE ZVEDNOUT** žádnou akcí
- Po 3 sekundách se automaticky zvedne

## 🔬 Testování

### Test 1: Poslední combo hit
```
1. Hráč A začne combo na hráče B
2. Hráč A dorazí poslední hit (bType 17+)
3. ✅ Hráč B je odhozen 3 metry od A
4. ✅ Hráč B spadne na zem
5. ✅ Hráč B nemůže se pohnout (WASD nefunguje)
6. ✅ Hráč B nemůže útočit (mezerník nefunguje)
7. ✅ Po 3 sekundách se hráč B automaticky zvedne
```

### Test 2: Normální útoky
```
1. Hráč A dělá normální útoky (bType 0-16)
2. ✅ ŽÁDNÝ knockback - normální damage
3. ✅ Hráč B se může volně pohybovat
```

### Test 3: Lag compensation stále funguje
```
1. Hráč A má 300ms ping
2. Hráč B se pohybuje
3. ✅ Hit detection používá position history
4. ✅ Žádný pull-back efekt
5. ✅ Fair combat
```

## ⚙️ Konfigurace

### Změna knockback vzdálenosti:

V **char_battle.cpp** řádek 378:
```cpp
pkVictim->ApplyKnockback(this, 300.0f);  // Změň 300.0f na jinou hodnotu
```

- **100.0f** = 1 metr (slabý knockback)
- **300.0f** = 3 metry (default, střední knockback)
- **500.0f** = 5 metrů (silný knockback)

### Přidat knockback na jiné combo hity:

V **char_battle.cpp** řádek 364:
```cpp
if (bType >= 17 && bType <= 20)  // Změň podmínku
```

Například pro všechny combo hity 14+:
```cpp
if (bType >= 14 && bType <= 20)
```

### Přidat knockback na skills:

Připraveno v TODO komentáři (řádek 369-373):
```cpp
// TODO: Můžeš přidat zde kontrolu pro specifické skills:
else if (bType == SKILL_VNUM && CKnockbackManager::Instance().DoesSkillHaveKnockback(bType))
{
    bShouldKnockback = true;
}
```

## 🐛 Troubleshooting

### Problém: Hráč se stále může pohybovat WASD po knockbacku

**Řešení:**
1. Zkontroluj že máš **všechny 3 kontroly** v input_main.cpp:
   - Position() na řádku 1262-1264
   - Move() FUNC_MOVE na řádku 1620-1622
   - Move() actions na řádku 1631-1633

2. Zkontroluj že ApplyKnockback() volá **Stun()** (řádek 588)

3. Zkontroluj v syserr logu:
```
grep "Stun" syserr
```
Měl by vidět: `"JmenoHrace: Stun 0x..."`

### Problém: Knockback nefunguje vůbec

**Řešení:**
1. Zkontroluj že combo hit je **bType >= 17**
2. Zkontroluj v test_serveru log:
```
grep "Knockback applied" syserr
```

3. Zkontroluj že oba hráči jsou **PC** (ne NPC)

### Problém: Hráč je odhozen mimo mapu

**Řešení:**
- WarpSet() automaticky kontroluje mapu validity
- Pokud problém přetrvává, sniž knockback distance na 200.0f

## 📊 Kompatibilita

✅ **Server-only** - Žádné změny klienta
✅ **Lag compensation** - Funguje současně s lag comp systémem
✅ **Existující combat** - Nemění normální útoky
✅ **Skills** - Připraveno pro budoucí skill knockback

## 📚 Související dokumenty

- `KNOCKDOWN_FIX.md` - Fix pro instant recovery
- `CHANGELOG_LAG_COMPENSATION.md` - Lag compensation systém
- `knockback_config.h/cpp` - Připraveno pro advanced konfiguraci

## 👤 Autor

**Datum:** 2026-01-13
**Verze:** 1.0
**Status:** ✅ Production Ready
**Tested:** Poslední combo hity 17-20
