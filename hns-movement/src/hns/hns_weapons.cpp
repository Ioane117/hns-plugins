#include "hns_weapons.h"
#include "hns_player.h"
#include "hns_movement.h"
#include "hns_config.h"
#include "utils/utils.h"
#include "utils/gameconfig.h"
#include "utils/interfaces.h"
#include "utils/virtual.h"
#include "sdk/services.h"
#include "sdk/takedamageinfo.h"
#include "sdk/usercmd.h"
#include "sdk/entity/cbaseplayerweapon.h"

#include "tier0/memdbgon.h"

using hns::config::settings;

namespace hns::weapons
{
	typedef void RemovePlayerItem_t(CBasePlayerPawn *pawn, CBasePlayerWeapon *weapon);
	static_global RemovePlayerItem_t *RemovePlayerItem {};
	static_global bool damageHooked {};
	static_global u32 removedCount {};
	static_global u32 damageOverrides {};
	// Last tick a "use weapon_knife" was sent to each slot, so an empty-handed player is not spammed.
	static_global i32 lastEquipTick[MAXPLAYERS + 1] {};
	// Tick at which a spawned player's grenades should be checked. 0 = nothing pending.
	static_global i32 pendingSpawnItems[MAXPLAYERS + 1] {};
	static_global i32 giveNamedItemOffset = -1;

	enum class WeaponKind
	{
		Knife,
		Grenade,
		Gun,
		Other, // C4, health shot, ...
	};

	// Item definition indexes (items_game.txt).
	static_function WeaponKind Classify(i32 itemDef)
	{
		switch (itemDef)
		{
			case 41: // knifegg
			case 42: // knife (CT)
			case 59: // knife_t
			case 74: // knife_ghost
			case 80: // knife_ghost (alt)
				return WeaponKind::Knife;
			case 43: // flashbang
			case 44: // hegrenade
			case 45: // smokegrenade
			case 46: // molotov
			case 47: // decoy
			case 48: // incgrenade
			case 68: // tagrenade
				return WeaponKind::Grenade;
			case 49: // c4
			case 57: // healthshot
				return WeaponKind::Other;
			default:
				break;
		}
		if (itemDef >= 500 && itemDef < 600) // knife skins (bayonet, karambit, ...)
		{
			return WeaponKind::Knife;
		}
		if (itemDef >= 1 && itemDef <= 64) // pistols, rifles, SMGs, shotguns, snipers, machine guns, taser (31)
		{
			return WeaponKind::Gun;
		}
		return WeaponKind::Other;
	}

	static_function WeaponKind Classify(CBasePlayerWeapon *weapon)
	{
		return Classify(weapon->m_AttributeManager().m_Item().m_iItemDefinitionIndex());
	}

	void SetDamageHooked(bool hooked)
	{
		damageHooked = hooked;
	}

	void Init()
	{
		RemovePlayerItem = (RemovePlayerItem_t *)g_pGameConfig->ResolveSignature("CBasePlayerPawn_RemovePlayerItem");
		if (!RemovePlayerItem || !g_pKZUtils->RemoveEntity)
		{
			HNS_LOG_WARN(LogChannel::General,
						 "CBasePlayerPawn_RemovePlayerItem/RemoveEntity did not resolve: disallowed weapons are disabled instead of removed.\n");
		}
		giveNamedItemOffset = g_pGameConfig->GetOffset("CCSPlayer_ItemServices_GiveNamedItem");
		HNS_LOG_INFO(LogChannel::General, "Weapon rules: RemovePlayerItem %s, RemoveEntity %s.\n", RemovePlayerItem ? "ok" : "MISSING",
					 g_pKZUtils->RemoveEntity ? "ok" : "MISSING");
	}

	static_function bool ShouldRemove(WeaponKind kind, i32 team)
	{
		if (kind == WeaponKind::Gun)
		{
			return settings.noGuns;
		}
		return false;
	}

	static_function void ApplyToPlayer(HNSPlayer *player, i32 tick)
	{
		CCSPlayerPawn *pawn = player->GetPlayerPawn();
		CCSPlayerController *controller = player->GetController();
		if (!pawn || !controller || !pawn->m_pWeaponServices())
		{
			return;
		}
		i32 team = controller->m_iTeamNum();
		CUtlVector<CHandle<CBasePlayerWeapon>> *weapons = pawn->m_pWeaponServices()->m_hMyWeapons();

		// Collect first: removing a weapon changes m_hMyWeapons.
		CBasePlayerWeapon *toRemove[64];
		i32 removeCount = 0;
		FOR_EACH_VEC(*weapons, i)
		{
			CBasePlayerWeapon *weapon = (*weapons)[i].Get();
			if (!weapon)
			{
				continue;
			}
			WeaponKind kind = Classify(weapon);
			if (ShouldRemove(kind, team))
			{
				if (RemovePlayerItem && g_pKZUtils->RemoveEntity && removeCount < 64)
				{
					toRemove[removeCount++] = weapon;
				}
				else
				{
					// Fallback: keep it but make it useless.
					weapon->m_nNextPrimaryAttackTick(tick + 64);
					weapon->m_nNextSecondaryAttackTick(tick + 64);
				}
				continue;
			}
			if (kind != WeaponKind::Knife)
			{
				continue;
			}
			// The client predicts from these values too, so a blocked attack plays no swing animation.
			bool blockSlash = settings.knifeStabOnly || (settings.knifeCTOnly && team != CS_TEAM_CT);
			bool blockStab = settings.knifeCTOnly && team != CS_TEAM_CT;
			if (blockSlash && weapon->m_nNextPrimaryAttackTick() < tick + 32)
			{
				weapon->m_nNextPrimaryAttackTick(tick + 64);
			}
			if (blockStab && weapon->m_nNextSecondaryAttackTick() < tick + 32)
			{
				weapon->m_nNextSecondaryAttackTick(tick + 64);
			}
		}
		bool hasKnife = false;
		FOR_EACH_VEC(*weapons, i)
		{
			CBasePlayerWeapon *weapon = (*weapons)[i].Get();
			if (weapon && Classify(weapon) == WeaponKind::Knife)
			{
				hasKnife = true;
				break;
			}
		}
		CBasePlayerWeapon *active = pawn->m_pWeaponServices()->m_hActiveWeapon().Get();
		bool activeRemoved = false;
		for (i32 i = 0; i < removeCount; i++)
		{
			activeRemoved |= (toRemove[i] == active);
			RemovePlayerItem(pawn, toRemove[i]);
			g_pKZUtils->RemoveEntity(toRemove[i]);
			removedCount++;
		}
		// Spawning with a pistol in hand and having it removed leaves the player holding nothing; the game does not switch on
		// its own. Pull the knife out, the same as the player pressing their knife key.
		if (settings.equipKnife && hasKnife && (activeRemoved || !active))
		{
			i32 slot = player->GetPlayerSlot().Get();
			if (tick - lastEquipTick[slot] >= 16 || tick < lastEquipTick[slot])
			{
				lastEquipTick[slot] = tick;
				interfaces::pEngine->ClientCommand(player->GetPlayerSlot(), "use weapon_knife");
			}
		}
	}

	static_function u64 AttackToStab(u64 buttons)
	{
		if (buttons & IN_ATTACK)
		{
			buttons = (buttons & ~(u64)IN_ATTACK) | (u64)IN_ATTACK2;
		}
		return buttons;
	}

	void RemapKnifeAttack(HNSPlayer *player, PlayerCommand *cmds, int numcmds)
	{
		if (!settings.knifeAttackIsStab || !cmds || numcmds <= 0)
		{
			return;
		}
		CCSPlayerController *controller = player->GetController();
		CCSPlayerPawn *pawn = player->GetPlayerPawn();
		if (!controller || controller->m_iTeamNum() != CS_TEAM_CT || !pawn || !pawn->m_pWeaponServices())
		{
			return;
		}
		CBasePlayerWeapon *active = pawn->m_pWeaponServices()->m_hActiveWeapon().Get();
		if (!active || Classify(active) != WeaponKind::Knife)
		{
			return;
		}
		for (i32 i = 0; i < numcmds; i++)
		{
			PlayerCommand &cmd = cmds[i];
			CBaseUserCmdPB *base = cmd.mutable_base();
			if (base->has_buttons_pb())
			{
				CInButtonStatePB *pb = base->mutable_buttons_pb();
				pb->set_buttonstate1(AttackToStab(pb->buttonstate1()));
				pb->set_buttonstate2(AttackToStab(pb->buttonstate2()));
				pb->set_buttonstate3(AttackToStab(pb->buttonstate3()));
			}
			for (i32 k = 0; k < 3; k++)
			{
				cmd.buttonstates.m_pButtonStates[k] = AttackToStab(cmd.buttonstates.m_pButtonStates[k]);
			}
			for (i32 j = 0; j < base->subtick_moves_size(); j++)
			{
				CSubtickMoveStep *move = base->mutable_subtick_moves(j);
				if (move->button() == IN_ATTACK)
				{
					move->set_button(IN_ATTACK2);
				}
			}
			if (cmd.attack1_start_history_index() >= 0)
			{
				cmd.set_attack2_start_history_index(cmd.attack1_start_history_index());
				cmd.set_attack1_start_history_index(-1);
			}
		}
	}

	void OnPlayerSpawn(i32 slot)
	{
		if (slot >= 0 && slot <= MAXPLAYERS)
		{
			// 4 ticks: the game hands out the default loadout right after the spawn event.
			pendingSpawnItems[slot] = g_pKZUtils->GetServerGlobals()->tickcount + 4;
		}
	}

	static_function void GiveSpawnItems(HNSPlayer *player)
	{
		CCSPlayerController *controller = player->GetController();
		CCSPlayerPawn *pawn = player->GetPlayerPawn();
		if (!controller || !pawn || !pawn->m_pItemServices() || !pawn->m_pWeaponServices() || giveNamedItemOffset < 0)
		{
			return;
		}
		i32 team = controller->m_iTeamNum();
		bool wantTGrenades = settings.tSpawnGrenades && team == CS_TEAM_T;
		bool wantCTDecoy = settings.ctSpawnDecoy && team == CS_TEAM_CT;
		if (!wantTGrenades && !wantCTDecoy)
		{
			return;
		}
		bool hasSmoke = false;
		bool hasFlash = false;
		bool hasDecoy = false;
		CUtlVector<CHandle<CBasePlayerWeapon>> *weapons = pawn->m_pWeaponServices()->m_hMyWeapons();
		FOR_EACH_VEC(*weapons, i)
		{
			CBasePlayerWeapon *weapon = (*weapons)[i].Get();
			if (!weapon)
			{
				continue;
			}
			i32 def = weapon->m_AttributeManager().m_Item().m_iItemDefinitionIndex();
			hasSmoke |= def == 45;
			hasFlash |= def == 43;
			hasDecoy |= def == 47;
		}
		CCSPlayer_ItemServices *items = pawn->m_pItemServices();
		if (wantTGrenades && !hasSmoke)
		{
			CALL_VIRTUAL(CBasePlayerWeapon *, giveNamedItemOffset, items, "weapon_smokegrenade");
		}
		if (wantTGrenades && !hasFlash)
		{
			CALL_VIRTUAL(CBasePlayerWeapon *, giveNamedItemOffset, items, "weapon_flashbang");
		}
		if (wantCTDecoy && !hasDecoy)
		{
			// Something to hold besides the knife, so a CT whose pistol was removed is never left empty-handed.
			CALL_VIRTUAL(CBasePlayerWeapon *, giveNamedItemOffset, items, "weapon_decoy");
		}
	}

	void OnGameFrame()
	{
		{
			i32 now = g_pKZUtils->GetServerGlobals()->tickcount;
			for (i32 i = 0; i < MAXPLAYERS; i++)
			{
				if (pendingSpawnItems[i] == 0 || now < pendingSpawnItems[i])
				{
					continue;
				}
				pendingSpawnItems[i] = 0;
				HNSPlayer *player = g_pHNSPlayerManager->ToPlayer(CPlayerSlot(i));
				if (player && player->IsConnectedController() && player->IsAlive())
				{
					GiveSpawnItems(player);
				}
			}
		}
		if (!settings.noGuns && !settings.knifeCTOnly && !settings.knifeStabOnly && !settings.equipKnife)
		{
			return;
		}
		i32 tick = g_pKZUtils->GetServerGlobals()->tickcount;
		for (i32 i = 0; i < MAXPLAYERS; i++)
		{
			HNSPlayer *player = g_pHNSPlayerManager->ToPlayer(CPlayerSlot(i));
			if (!player || !player->IsConnectedController() || !player->IsAlive())
			{
				continue;
			}
			ApplyToPlayer(player, tick);
		}
	}

	bool OnTakeDamage(CBaseEntity *victim, CTakeDamageInfo *info)
	{
		if (settings.knifeDamage <= 0.0f || !victim || !info)
		{
			return false;
		}
		CBaseEntity *attacker = info->m_hAttacker().Get();
		CBaseEntity *inflictor = info->m_hInflictor().Get();
		// Melee: victim, attacker and inflictor are all player pawns (same check ChaseMod uses).
		if (!attacker || !inflictor || !KZ_STREQ(victim->GetClassname(), "player") || !KZ_STREQ(attacker->GetClassname(), "player")
			|| !KZ_STREQ(inflictor->GetClassname(), "player"))
		{
			return false;
		}
		CCSPlayerPawn *attackerPawn = static_cast<CCSPlayerPawn *>(attacker);
		if (!attackerPawn->m_pWeaponServices())
		{
			return false;
		}
		CBasePlayerWeapon *weapon = attackerPawn->m_pWeaponServices()->m_hActiveWeapon().Get();
		if (!weapon || Classify(weapon) != WeaponKind::Knife)
		{
			return false;
		}
		if (attacker->m_iTeamNum() == victim->m_iTeamNum())
		{
			return false;
		}
		info->m_flDamage() = settings.knifeDamage;
		damageOverrides++;
		return false;
	}
	static_function const char *KindName(WeaponKind kind)
	{
		switch (kind)
		{
			case WeaponKind::Knife:
				return "knife";
			case WeaponKind::Grenade:
				return "grenade";
			case WeaponKind::Gun:
				return "gun";
			default:
				return "other";
		}
	}

	void PrintDebug()
	{
		META_CONPRINTF("---- hns-movement weapon rules ----\n");
		META_CONPRINTF("no_guns %d, knife_ct_only %d, knife_stab_only %d, knife_damage %g\n", settings.noGuns, settings.knifeCTOnly,
					   settings.knifeStabOnly, settings.knifeDamage);
		META_CONPRINTF("RemovePlayerItem %s, RemoveEntity %s, damage hook %s\n", RemovePlayerItem ? "ok" : "MISSING",
					   g_pKZUtils->RemoveEntity ? "ok" : "MISSING", damageHooked ? "ok" : "MISSING");
		META_CONPRINTF("weapons removed so far %u, knife damage overrides so far %u\n", removedCount, damageOverrides);
		for (i32 i = 0; i < MAXPLAYERS; i++)
		{
			HNSPlayer *player = g_pHNSPlayerManager->ToPlayer(CPlayerSlot(i));
			if (!player || !player->IsConnectedController())
			{
				continue;
			}
			CCSPlayerController *controller = player->GetController();
			CCSPlayerPawn *pawn = player->GetPlayerPawn();
			META_CONPRINTF("[%d] team %d connected %d engine-ingame %d alive %d pawn %p\n", i, controller ? controller->m_iTeamNum() : -1,
						   player->IsConnectedController(), player->IsInGame(), player->IsAlive(), pawn);
			if (!pawn || !pawn->m_pWeaponServices())
			{
				continue;
			}
			{
				CBasePlayerWeapon *active = pawn->m_pWeaponServices()->m_hActiveWeapon().Get();
				Vector velocity;
				player->GetVelocity(&velocity);
				META_CONPRINTF("    holding %s: game max speed %.1f, velocity modifier %.3f, prestrafe gain %.1f, speed %.1f\n",
							   active ? active->GetClassname() : "nothing", player->movement ? player->movement->GetOriginalMaxSpeed() : -1.0f,
							   pawn->m_flVelocityModifier(), player->movement ? player->movement->GetPrestrafeGain() : -1.0f, velocity.Length2D());
			}
			CUtlVector<CHandle<CBasePlayerWeapon>> *weapons = pawn->m_pWeaponServices()->m_hMyWeapons();
			FOR_EACH_VEC(*weapons, w)
			{
				CBasePlayerWeapon *weapon = (*weapons)[w].Get();
				if (!weapon)
				{
					continue;
				}
				i32 def = weapon->m_AttributeManager().m_Item().m_iItemDefinitionIndex();
				META_CONPRINTF("    %s def %d -> %s\n", weapon->GetClassname(), def, KindName(Classify(def)));
			}
		}
	}
} // namespace hns::weapons
