#pragma once

#include "precompiled.h"

#define RECOIL_ALL 0

#ifndef RECOIL_MAX
#define RECOIL_MAX 100.0f
#endif

#ifndef RECOIL_LAST_FIRED_NONE
#define RECOIL_LAST_FIRED_NONE (-15.0f)
#endif

// Forward declarations
void RG_CBasePlayer_PostThink(IReGameHook_CBasePlayer_PostThink *chain, CBasePlayer *player);
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
    int shared_rand);

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
    entvars_t *pevAttacker);

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
    entvars_t *pevAttacker);

class recoil_mngr
{
    int IMPULSE_OFFSET;
    std::array<m_cvar_t *, MAX_WEAPONS + 1> weapon_recoil;
    std::array<float, MAX_PLAYERS + 1> last_fired;
    bool is_enabled;

private:
    void add(size_t weapon_id, const char *weapon_name)
    {
        // TODO: префикс "wcs_recoil_" конфликтует с плагином WCS.
        // Рассмотреть переименование на "rf_recoil_" с миграцией конфигов.
        weapon_recoil[weapon_id] = g_cvar_mngr.get(wfmt(L"wcs_recoil_%s", weapon_name).c_str());
    }

public:
    recoil_mngr() : IMPULSE_OFFSET(0),
                    is_enabled(false)
    {
        weapon_recoil.fill(nullptr);
        last_fired.fill(RECOIL_LAST_FIRED_NONE);
    }

    recoil_mngr(const recoil_mngr &) = delete;
    recoil_mngr &operator=(const recoil_mngr &) = delete;

    void init()
    {
        add(RECOIL_ALL, "weapon_all");
        for (int weapon_id = WEAPON_P228; weapon_id <= WEAPON_P90; weapon_id++)
        {
            auto slot_info = g_ReGameApi->GetWeaponSlot((WeaponIdType)weapon_id);
            if (slot_info != nullptr &&
                (slot_info->slot == PRIMARY_WEAPON_SLOT || slot_info->slot == PISTOL_SLOT) &&
                slot_info->weaponName[0])
                add(weapon_id, slot_info->weaponName);
        }
    }

    void set(size_t weapon_id, float recoil)
    {
        if (weapon_id < weapon_recoil.size() &&
            weapon_recoil[weapon_id] &&
            weapon_recoil[weapon_id]->cvar)
        {
            CVAR_SET_FLOAT(weapon_recoil[weapon_id]->cvar->name, recoil);
        }
    }

    void enable(int _offset = 0)
    {
        if (is_enabled)
            disable();
        init();
        is_enabled = true;
        IMPULSE_OFFSET = _offset;
        last_fired.fill(RECOIL_LAST_FIRED_NONE);
        g_ReGameHookchains->CBaseEntity_FireBullets3()->registerHook(RG_CBaseEntity_FireBullets3);
        g_ReGameHookchains->CBaseEntity_FireBuckshots()->registerHook(RG_CBaseEntity_FireBuckshots);
        g_ReGameHookchains->CBaseEntity_FireBullets()->registerHook(RG_CBaseEntity_FireBullets);
        g_ReGameHookchains->CBasePlayer_PostThink()->registerHook(RG_CBasePlayer_PostThink);
    }

    void disable()
    {
        if (!is_enabled)
            return;
        is_enabled = false;
        g_ReGameHookchains->CBaseEntity_FireBullets3()->unregisterHook(RG_CBaseEntity_FireBullets3);
        g_ReGameHookchains->CBaseEntity_FireBuckshots()->unregisterHook(RG_CBaseEntity_FireBuckshots);
        g_ReGameHookchains->CBaseEntity_FireBullets()->unregisterHook(RG_CBaseEntity_FireBullets);
        g_ReGameHookchains->CBasePlayer_PostThink()->unregisterHook(RG_CBasePlayer_PostThink);
    }

    // Сброс состояния при disconnect — слоты переиспользуются
    void on_client_disconnected(edict_t *p)
    {
        int id = ENTINDEX(p);
        if (is_valid_index(id))
            last_fired[id] = RECOIL_LAST_FIRED_NONE;
    }

    // Сброс при спавне — покрывает reconnect и смену карты
    void reset_player(int id)
    {
        if (is_valid_index(id))
            last_fired[id] = RECOIL_LAST_FIRED_NONE;
    }

    // Очистка - разброс = 1.0f
    void clear()
    {
        for (auto rc : weapon_recoil)
            if (rc && rc->cvar)
                CVAR_SET_FLOAT(rc->cvar->name, 1.0f);
    }

    // Возвращает множитель отдачи для оружия.
    // 1.0f — не вмешиваться (значение по умолчанию / выключено).
    // Значения 0.0..1.0 — снижение, >1.0 — усиление (для admin-режимов).
    float get_weapon_recoil(CBasePlayerWeapon *weapon)
    {
        if (!weapon || !is_enabled)
            return 1.0f;

        int wid = weapon->m_iId;
        // Защита от OOB и UB в BIT(): щит = 99, невалидные id.
        if (wid <= WEAPON_NONE || wid > WEAPON_P90)
            return 1.0f;

        float r = 1.0f;
        bool decided = false;

        // Отдача: если wed->v.impulse == iuser4 - IMPULSE_OFFSET,
        // то fuser1 несёт индивидуальный recoil для этого выстрела.
        auto wed = weapon->edict();
        if (IMPULSE_OFFSET > 0 &&
            (wed->v.iuser4 - IMPULSE_OFFSET) >= 0 &&
            wed->v.impulse == wed->v.iuser4)
        {
            // NaN отсекается: сравнение с ним всегда ложно.
            // Если значение мусорное — не решаем здесь, идём на cvar.
            if (wed->v.fuser1 >= 0.0f)
            {
                r = wed->v.fuser1;
                decided = true;
            }
        }

        // Индивидуальный cvar оружия.
        if (!decided)
        {
            auto m_cvar = weapon_recoil[wid];
            if (m_cvar && m_cvar->cvar)
            {
                float v = m_cvar->cvar->value;
                if (v >= 0.0f && v != 1.0f)
                {
                    r = v;
                    decided = true;
                }
            }
        }

        // Общий cvar "weapon_all".
        if (!decided)
        {
            auto m_cvar_all = weapon_recoil[RECOIL_ALL];
            if (m_cvar_all && m_cvar_all->cvar)
            {
                float v = m_cvar_all->cvar->value;
                if (v >= 0.0f && v != 1.0f)
                {
                    r = v;
                    decided = true;
                }
            }
        }

        // ЕДИНАЯ точка ограничения: сюда сходятся все три источника
        // (fuser1, cvar оружия, cvar weapon_all), поэтому FireBullets3
        // и think_post получают уже безопасное значение.
        // Выше предела — прижимаем, а не отбрасываем: иначе рост настройки
        // отменял бы эффект (обрыв монотонности).
        if (r > RECOIL_MAX)
            r = RECOIL_MAX;

        return r;
    }

    // Только визуальная отдача (punchangle).
    // Реальный разброс снижается в RG_CBaseEntity_FireBullets3.
    void think_post(CBasePlayer *player)
    {
        if (!is_enabled || !player || player->IsBot() || !player->IsAlive())
            return;

        int index = player->entindex();
        // Детект выстрела: m_flLastFired ставится в SetAnimation при ACT_RANGE_ATTACK1/2
        if (!is_valid_index(index) || player->m_flLastFired == last_fired[index])
            return;

        last_fired[index] = player->m_flLastFired;

        if (!player->m_pActiveItem || !player->m_pActiveItem->IsWeapon())
            return;

        int wid = player->m_pActiveItem->m_iId;
        if (wid <= WEAPON_NONE || wid > WEAPON_P90)
            return;

        // Гранаты, нож, C4 — визуальная отдача не имеет смысла.
        if (BIT(wid) & (BIT(WEAPON_HEGRENADE) | BIT(WEAPON_FLASHBANG) |
                        BIT(WEAPON_SMOKEGRENADE) | BIT(WEAPON_C4) |
                        BIT(WEAPON_KNIFE)))
            return;

        float recoil = get_weapon_recoil(
            static_cast<CBasePlayerWeapon *>(player->m_pActiveItem));

        if (recoil >= 0.0f && recoil != 1.0f)
        {
            auto ped = player->edict();
            ped->v.punchangle = ped->v.punchangle * recoil;
        }
    }
};

extern recoil_mngr g_recoil_mngr;