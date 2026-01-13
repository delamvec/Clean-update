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