#include "hns_gameplay.h"
#include "hns_player.h"
#include "hns_config.h"
#include "utils/utils.h"
#include "sdk/gamerules.h"
#include "sdk/services.h"
#include "sdk/entity/cbaseplayerweapon.h"
#include "igameevents.h"

#include "tier0/memdbgon.h"

using hns::config::settings;

namespace hns::gameplay
{
	// Server time when the CT freeze ends. 0 = no freeze active.
	static_global f32 freezeEndTime {};
	static_global i32 lastCountdownShown = -1;

	static_global bool shouldSwitchTeams {};
	static_global i32 terroristWinStreak {};

	static_function f32 Now()
	{
		return g_pKZUtils->GetServerGlobals()->curtime;
	}

	static_function bool IsWarmup()
	{
		CCSGameRules *rules = g_pKZUtils->GetGameRules();
		return rules && rules->m_bWarmupPeriod();
	}

	static_function void StartCTFreeze()
	{
		if (settings.ctFreezeTime <= 0.0f || IsWarmup())
		{
			freezeEndTime = 0.0f;
			return;
		}
		freezeEndTime = Now() + settings.ctFreezeTime;
		lastCountdownShown = -1;
	}

	static_function void SwitchTeams()
	{
		for (i32 i = 0; i < MAXPLAYERS; i++)
		{
			HNSPlayer *player = g_pHNSPlayerManager->ToPlayer(CPlayerSlot(i));
			if (!player || !player->IsInGame())
			{
				continue;
			}
			CCSPlayerController *controller = player->GetController();
			if (!controller)
			{
				continue;
			}
			i32 team = controller->m_iTeamNum();
			if (team == CS_TEAM_CT)
			{
				controller->SwitchTeam(CS_TEAM_T);
			}
			else if (team == CS_TEAM_T)
			{
				controller->SwitchTeam(CS_TEAM_CT);
			}
		}
	}

	void OnFireEvent(IGameEvent *event)
	{
		const char *name = event->GetName();
		if (KZ_STREQI(name, "round_end"))
		{
			if (!settings.teamSwitch || IsWarmup())
			{
				return;
			}
			i32 winner = event->GetInt("winner");
			shouldSwitchTeams = false;
			if (winner == CS_TEAM_CT)
			{
				terroristWinStreak = 0;
				shouldSwitchTeams = true;
				utils::CPrintChatAll(" {blue}CT{grey} win - teams are being switched.");
			}
			else if (winner == CS_TEAM_T)
			{
				terroristWinStreak++;
				if (settings.tWinStreakSwitch > 0 && terroristWinStreak >= settings.tWinStreakSwitch)
				{
					utils::CPrintChatAll(" {gold}T{grey} win - teams are being switched ({default}%i{grey} wins in a row).", terroristWinStreak);
					terroristWinStreak = 0;
					shouldSwitchTeams = true;
				}
				else
				{
					utils::CPrintChatAll(" {gold}T{grey} win.");
				}
			}
		}
		else if (KZ_STREQI(name, "round_prestart"))
		{
			freezeEndTime = 0.0f;
			if (shouldSwitchTeams)
			{
				shouldSwitchTeams = false;
				SwitchTeams();
			}
		}
		else if (KZ_STREQI(name, "round_freeze_end"))
		{
			// With mp_freezetime 0 this fires right as the round starts.
			StartCTFreeze();
		}
	}

	void OnMapStart()
	{
		freezeEndTime = 0.0f;
		shouldSwitchTeams = false;
		terroristWinStreak = 0;
	}

	void OnGameFrame()
	{
		if (freezeEndTime <= 0.0f)
		{
			return;
		}
		f32 left = freezeEndTime - Now();
		if (left <= 0.0f)
		{
			freezeEndTime = 0.0f;
			utils::PrintCentreAll("Round start!");
			return;
		}
		i32 shown = (i32)ceilf(left);
		if (shown != lastCountdownShown)
		{
			lastCountdownShown = shown;
			utils::PrintCentreAll("Seekers released in %i...", shown);
		}
	}

	bool IsFrozen(HNSPlayer *player)
	{
		if (freezeEndTime <= 0.0f || Now() >= freezeEndTime)
		{
			return false;
		}
		CCSPlayerController *controller = player->GetController();
		return controller && controller->m_iTeamNum() == CS_TEAM_CT && player->IsAlive();
	}

	void ApplyFreezeToMovement(HNSPlayer *player)
	{
		if (!IsFrozen(player) || !player->currentMoveData)
		{
			return;
		}
		CMoveData *mv = player->currentMoveData;
		mv->m_flForwardMove = 0.0f;
		mv->m_flSideMove = 0.0f;
		mv->m_flUpMove = 0.0f;
		mv->m_vecVelocity.x = 0.0f;
		mv->m_vecVelocity.y = 0.0f;
		CCSPlayer_MovementServices *ms = player->GetMoveServices();
		if (ms)
		{
			// Held and newly-pressed states: no jumping while frozen.
			ms->m_nButtons().m_pButtonStates[0] &= ~IN_JUMP;
			ms->m_nButtons().m_pButtonStates[1] &= ~IN_JUMP;
		}
	}

	void ApplyFreezeToWeapons(HNSPlayer *player)
	{
		if (!IsFrozen(player))
		{
			return;
		}
		CCSPlayerPawn *pawn = player->GetPlayerPawn();
		if (!pawn || !pawn->m_pWeaponServices())
		{
			return;
		}
		i32 unfreezeTick = g_pKZUtils->GetServerGlobals()->tickcount + (i32)((freezeEndTime - Now()) * ENGINE_FIXED_TICK_RATE) + 1;
		CUtlVector<CHandle<CBasePlayerWeapon>> *weapons = pawn->m_pWeaponServices()->m_hMyWeapons();
		FOR_EACH_VEC(*weapons, i)
		{
			CBasePlayerWeapon *weapon = (*weapons)[i].Get();
			if (!weapon)
			{
				continue;
			}
			if (weapon->m_nNextPrimaryAttackTick() < unfreezeTick)
			{
				weapon->m_nNextPrimaryAttackTick(unfreezeTick);
			}
			if (weapon->m_nNextSecondaryAttackTick() < unfreezeTick)
			{
				weapon->m_nNextSecondaryAttackTick(unfreezeTick);
			}
		}
	}
} // namespace hns::gameplay
