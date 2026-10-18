#include "hns_hud.h"
#include "hns_player.h"
#include "hns_config.h"
#include "utils/utils.h"
#include "sdk/services.h"
#include "sdk/usercmd.h"

#include "tier0/memdbgon.h"

using hns::config::settings;

namespace hns::hud
{
	// A mouse wheel jump is a press and release inside one tick. Keep Jump lit this long so it can be seen.
	static constexpr i32 JUMP_SHOW_TICKS = 8;
	// The HUD is resent this often (in ticks). Each send lasts about a second on the client.
	static constexpr i32 REFRESH_TICKS = 2;

	struct State
	{
		u64 held {};          // buttons held at the end of the last processed tick
		i32 jumpShowUntil {}; // tick until which Jump stays lit
		f32 takeoffSpeed {};
		bool hasTakeoff {};
	};

	static State states[MAXPLAYERS + 1];

	static_function i32 Tick()
	{
		return g_pKZUtils->GetServerGlobals()->tickcount;
	}

	static_function State *GetState(HNSPlayer *player)
	{
		i32 slot = player ? player->GetPlayerSlot().Get() : -1;
		return (slot >= 0 && slot <= MAXPLAYERS) ? &states[slot] : nullptr;
	}

	void OnPlayerReset(HNSPlayer *player)
	{
		if (State *state = GetState(player))
		{
			*state = {};
		}
	}

	void OnProcessUsercmds(HNSPlayer *player, PlayerCommand *cmds, int numcmds)
	{
		State *state = GetState(player);
		if (!state || !cmds || numcmds <= 0)
		{
			return;
		}
		bool jumpInput = false;
		for (i32 i = 0; i < numcmds; i++)
		{
			PlayerCommand &cmd = cmds[i];
			// [0] held, [1] changed this tick, [2] scroll. A wheel jump sets jump in [1]/[2] and in a subtick step,
			// but is already released again by the end of the tick, so [0] alone misses it.
			for (i32 k = 0; k < 3; k++)
			{
				if (cmd.buttonstates.m_pButtonStates[k] & IN_JUMP)
				{
					jumpInput = true;
				}
			}
			const CBaseUserCmdPB &base = cmd.base();
			if (base.has_buttons_pb())
			{
				const CInButtonStatePB &pb = base.buttons_pb();
				if ((pb.buttonstate1() | pb.buttonstate2() | pb.buttonstate3()) & IN_JUMP)
				{
					jumpInput = true;
				}
			}
			for (i32 j = 0; j < base.subtick_moves_size(); j++)
			{
				if (base.subtick_moves(j).button() == IN_JUMP)
				{
					jumpInput = true;
				}
			}
			if (i == numcmds - 1)
			{
				state->held = cmd.buttonstates.m_pButtonStates[0];
				if (base.has_buttons_pb())
				{
					state->held |= base.buttons_pb().buttonstate1();
				}
			}
		}
		if (jumpInput)
		{
			state->jumpShowUntil = Tick() + JUMP_SHOW_TICKS;
		}
	}

	void OnTakeoff(HNSPlayer *player)
	{
		State *state = GetState(player);
		if (!state)
		{
			return;
		}
		state->takeoffSpeed = player->takeoffVelocity.Length2D();
		state->hasTakeoff = true;
		state->jumpShowUntil = MAX(state->jumpShowUntil, Tick() + JUMP_SHOW_TICKS);
	}

	// The player whose movement this player is looking at: themselves, or who they spectate in first/third person.
	static_function HNSPlayer *GetViewedPlayer(HNSPlayer *player)
	{
		CCSPlayerController *controller = player->GetController();
		if (!controller)
		{
			return nullptr;
		}
		CCSPlayerPawnBase *observer = controller->GetObserverPawn();
		if (observer && observer->m_pObserverServices())
		{
			CPlayer_ObserverServices *obs = observer->m_pObserverServices();
			ObserverMode_t mode = obs->m_iObserverMode();
			CBaseEntity *target = obs->m_hObserverTarget().Get();
			if ((mode == OBS_MODE_IN_EYE || mode == OBS_MODE_CHASE) && target && target->IsPawn())
			{
				HNSPlayer *viewed = g_pHNSPlayerManager->ToPlayer(static_cast<CBasePlayerPawn *>(target));
				if (viewed)
				{
					return viewed;
				}
			}
		}
		return player->GetPlayerPawn() ? player : nullptr;
	}

	static_function const char *Key(u64 held, u64 button, const char *label)
	{
		return (held & button) ? label : "_";
	}

	static_function void Show(HNSPlayer *viewer, i32 tick)
	{
		HNSPlayer *viewed = GetViewedPlayer(viewer);
		State *state = GetState(viewed);
		if (!viewed || !state || !viewed->GetPlayerPawn())
		{
			return;
		}
		Vector velocity;
		viewed->GetVelocity(&velocity);
		i32 speed = (i32)roundf(velocity.Length2D());

		u64 held = state->held;
		if (tick <= state->jumpShowUntil)
		{
			held |= IN_JUMP;
		}

		char takeoff[32] = "";
		if (state->hasTakeoff)
		{
			V_snprintf(takeoff, sizeof(takeoff), " (%i)", (i32)roundf(state->takeoffSpeed));
		}

		utils::PrintHTMLCentre(viewer->GetController(), "Speed: %i u/s%s<br>%s %s %s %s %s %s", speed, takeoff,
							   Key(held, IN_FORWARD, "W"), Key(held, IN_MOVELEFT, "A"), Key(held, IN_BACK, "S"),
							   Key(held, IN_MOVERIGHT, "D"), Key(held, IN_DUCK, "C"), Key(held, IN_JUMP, "J"));
	}

	void OnGameFrame()
	{
		if (!settings.speedHud)
		{
			return;
		}
		i32 tick = Tick();
		if (tick % REFRESH_TICKS != 0)
		{
			return;
		}
		for (i32 i = 0; i < MAXPLAYERS; i++)
		{
			HNSPlayer *player = g_pHNSPlayerManager->ToPlayer(CPlayerSlot(i));
			if (!player || !player->IsConnectedController() || player->IsFakeClient())
			{
				continue;
			}
			Show(player, tick);
		}
	}
} // namespace hns::hud
