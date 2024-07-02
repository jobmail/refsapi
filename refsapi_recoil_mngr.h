#include "precompiled.h"

#define RECOIL_ALL              0

void CmdEnd_Post(const edict_t* pEdict);
void RG_CBasePlayer_PostThink(IReGameHook_CBasePlayer_PostThink* chain, CBasePlayer* player);

class recoil_mngr
{
    int IMPULSE_OFFSET;
    std::array<cvar_t*, MAX_WEAPONS + 1> weapon_recoil;
    std::array<float, MAX_PLAYERS + 1> last_fired;
    bool is_enabled;

private:
    void add(size_t weapon_id, const char* weapon_name)
    {
        auto cvar_list = g_cvar_mngr.get(wfmt(L"wcs_recoil_%s", weapon_name).c_str());
        weapon_recoil[weapon_id] = check_it_empty(cvar_list) ? nullptr : cvar_list->second.cvar;
    }

public:
    void init()
    {
        add(RECOIL_ALL, "weapon_all");
        for (int weapon_id = WEAPON_P228; weapon_id <= WEAPON_P90; weapon_id++)
        {
            auto slot_info = g_ReGameApi->GetWeaponSlot((WeaponIdType)weapon_id);
            if (slot_info != nullptr && (slot_info->slot == PRIMARY_WEAPON_SLOT || slot_info->slot == PISTOL_SLOT) && slot_info->weaponName[0])
                add(weapon_id, slot_info->weaponName);
        }
    }
    void set(size_t weapon_id, float recoil)
    {
        weapon_recoil[weapon_id]->value = recoil;
    }
    void enable(int _offset = 0)
    {   
        disable();
        init();
        is_enabled = true;
        IMPULSE_OFFSET = _offset;
        g_ReGameHookchains->CBasePlayer_PostThink()->registerHook(RG_CBasePlayer_PostThink);
    }
    void disable()
    {
        if (!is_enabled)
            return;
        is_enabled = false;
        g_ReGameHookchains->CBasePlayer_PostThink()->unregisterHook(RG_CBasePlayer_PostThink);        
    }
    void clear()
    {
        for (auto rc : weapon_recoil)
            CVAR_SET_FLOAT(rc->name, 0.0f);
    }
    void cmd_end(const edict_t* pEdict)
    {
        if (!is_enabled)
            return;
	    auto player = UTIL_PlayerByIndexSafe(ENTINDEX(pEdict));
        if (player != nullptr && player->IsAlive())
            last_fired[player->entindex()] = player->m_flLastFired;
    }
    void think_post(CBasePlayer* player)
    {
        if (!is_enabled || !player->IsAlive() || player->IsBot())
            return;
        auto ped = player->edict();
        auto index = player->entindex();
        if (is_valid_entity(ped) && (ped->v.button & IN_ATTACK) && (player->m_flLastFired != last_fired[index]) && player->m_pActiveItem)
        {
            last_fired[index] = player->m_flLastFired;
            CBasePlayerWeapon* weapon = static_cast<CBasePlayerWeapon*>(player->m_pActiveItem);
            if (!((BIT(WEAPON_NONE) | BIT(WEAPON_HEGRENADE) | BIT(WEAPON_C4) | BIT(WEAPON_SMOKEGRENADE) | BIT(WEAPON_FLASHBANG) | BIT(WEAPON_KNIFE)) & BIT(player->m_pActiveItem->m_iId)))
            {
                auto wed = weapon->edict();
                auto pcvar = weapon_recoil[player->m_pActiveItem->m_iId];
                auto pcvar_all = weapon_recoil[RECOIL_ALL];
                auto recoil = IMPULSE_OFFSET > 0 && (wed->v.iuser4 - IMPULSE_OFFSET) >= 0 && wed->v.impulse == wed->v.iuser4 ? wed->v.fuser1 : pcvar != nullptr ? pcvar->value : 0.0f;
                UTIL_ServerPrint("[DEBUG] think_post(): offset = %d, pcvar = %d, pcvar_recoil = %f, weapon_id = %d, recoil = %f\n", IMPULSE_OFFSET, pcvar, pcvar->value, player->m_pActiveItem->m_iId, recoil);
                bool is_recoil_set = recoil > 0.0f && recoil < 1.0f;
                if (!is_recoil_set && pcvar_all != nullptr && (is_recoil_set = pcvar_all->value > 0.0f && pcvar_all->value < 1.0f))
                    recoil = weapon_recoil[RECOIL_ALL]->value;
                UTIL_ServerPrint("[DEBUG] think_post(): is_recoil_set = %d, pcvar = %d, recoil = %f\n", is_recoil_set, pcvar_all, recoil);
                if (is_recoil_set)
                {
                    ped->v.punchangle = ped->v.punchangle * recoil;
                }
            }
        }
    }
};

extern recoil_mngr g_recoil_mngr;