#include "precompiled.h"

#ifndef RECOIL_MAX
#define RECOIL_MAX 100.0f
#endif

void RG_CBasePlayer_PostThink(IReGameHook_CBasePlayer_PostThink *chain, CBasePlayer *player)
{
    chain->callNext(player);
    g_recoil_mngr.think_post(player);
}

Vector &RG_CBaseEntity_FireBullets3(
    IReGameHook_CBaseEntity_FireBullets3 *chain,
    CBaseEntity *pEntity,
    Vector &vecSrc,
    Vector &vecDirShooting,
    float flSpread,
    float flDistance,
    int iPenetration,
    int iBulletType,
    int iDamage,
    float flRangeModifier,
    entvars_t *pevAttacker,
    bool bPistol,
    int shared_rand)
{
    if (pevAttacker && (pevAttacker->flags & FL_CLIENT))
    {
        CBasePlayer *player = UTIL_PlayerByIndexSafe(ENTINDEX(pevAttacker));
        if (player && player->m_pActiveItem && player->m_pActiveItem->IsWeapon())
        {
            float recoil = g_recoil_mngr.get_weapon_recoil(static_cast<CBasePlayerWeapon *>(player->m_pActiveItem));
            if (recoil >= 0.0f && recoil != 1.0f)
                flSpread *= recoil;
        }
    }

    return chain->callNext(
        pEntity, vecSrc, vecDirShooting, flSpread, flDistance,
        iPenetration, iBulletType, iDamage, flRangeModifier,
        pevAttacker, bPistol, shared_rand);
}

// Резолв игрока-стрелка. Возвращает nullptr, если стреляет не игрок.
static CBasePlayer *recoil_resolve_shooter(entvars_t *pevAttacker, CBaseEntity *pEntity)
{
    if (pevAttacker && (pevAttacker->flags & FL_CLIENT))
    {
        CBasePlayer *player = UTIL_PlayerByIndexSafe(ENTINDEX(pevAttacker));
        if (player != nullptr)
            return player;
    }
    // Фолбэк: вызов вида m_pPlayer->FireBullets(...) — сущность сама игрок.
    if (pEntity != nullptr && pEntity->IsPlayer())
        return static_cast<CBasePlayer *>(pEntity);
    return nullptr;
}

// Масштабирование конуса. recoil == 1.0f — не вмешиваться.
static void recoil_scale_spread(Vector &vecSpread, float recoil)
{
    if (!(recoil >= 0.0f) || recoil == 1.0f)
        return;
    // Дублирование клампа, на всякий случай
    if (recoil > RECOIL_MAX)
        recoil = RECOIL_MAX;
    vecSpread.x *= recoil;
    vecSpread.y *= recoil;
    vecSpread.z *= recoil;
}

// ─── Хук 1: FireBuckshots (REGAMEDLL_FIXES) ────────────────────────────────
void RG_CBaseEntity_FireBuckshots(
    IReGameHook_CBaseEntity_FireBuckshots *chain,
    CBaseEntity *pEntity,
    ULONG cShots,
    Vector &vecSrc,
    Vector &vecDirShooting,
    Vector &vecSpread,
    float flDistance,
    int iTracerFreq,
    int iDamage,
    entvars_t *pevAttacker)
{
    CBasePlayer *player = recoil_resolve_shooter(pevAttacker, pEntity);
    if (player != nullptr && player->m_pActiveItem != nullptr && player->m_pActiveItem->IsWeapon())
    {
        recoil_scale_spread(
            vecSpread,
            g_recoil_mngr.get_weapon_recoil(
                static_cast<CBasePlayerWeapon *>(player->m_pActiveItem)));
    }

    chain->callNext(pEntity, cShots, vecSrc, vecDirShooting, vecSpread,
                    flDistance, iTracerFreq, iDamage, pevAttacker);
}

// ─── Хук 2: FireBullets (сборка без REGAMEDLL_FIXES + танк) ────────────────
void RG_CBaseEntity_FireBullets(
    IReGameHook_CBaseEntity_FireBullets *chain,
    CBaseEntity *pEntity,
    ULONG cShots,
    Vector &vecSrc,
    Vector &vecDirShooting,
    Vector &vecSpread,
    float flDistance,
    int iBulletType,
    int iTracerFreq,
    int iDamage,
    entvars_t *pevAttacker)
{
    CBasePlayer *player = recoil_resolve_shooter(pevAttacker, pEntity);
    if (player != nullptr && player->m_pActiveItem != nullptr && player->m_pActiveItem->IsWeapon())
    {
        int wid = player->m_pActiveItem->m_iId;
        // Только дробовики: FireBullets использует и танк (BULLET_MONSTER_*).
        if (wid == WEAPON_M3 || wid == WEAPON_XM1014)
        {
            recoil_scale_spread(
                vecSpread,
                g_recoil_mngr.get_weapon_recoil(
                    static_cast<CBasePlayerWeapon *>(player->m_pActiveItem)));
        }
    }

    chain->callNext(pEntity, cShots, vecSrc, vecDirShooting, vecSpread,
                    flDistance, iBulletType, iTracerFreq, iDamage, pevAttacker);
}