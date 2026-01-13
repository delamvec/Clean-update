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