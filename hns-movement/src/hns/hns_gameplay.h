#pragma once
#include "common.h"

class HNSPlayer;
class IGameEvent;

// HNS round flow, native replacement for ChaseMod's round-start freeze and team switch (no CounterStrikeSharp needed).
//   - CT start freeze: for hns_ct_freeze_time seconds after the round starts, CTs cannot move, jump or attack; they are tinted blue and fall damage is off (hns_freeze_*).
//     Ts move freely. The engine freezetime (mp_freezetime) is forced to 0 by the cvar table so it never freezes Ts.
//   - Team switch: CTs and Ts swap on a CT win, and on a T win streak of hns_t_winstreak_switch (0 = never).
namespace hns::gameplay
{
	void OnFireEvent(IGameEvent *event);
	void OnGameFrame();

	// True while this player is held by the CT start freeze.
	bool IsFrozen(HNSPlayer *player);
	// Called from movement processing: removes all movement input and horizontal velocity of a frozen player.
	void ApplyFreezeToMovement(HNSPlayer *player);
	// Called once per tick per player: keeps a frozen player's weapons from attacking until the freeze ends.
	void ApplyFreezeToWeapons(HNSPlayer *player);

	void OnMapStart();
	// Server cvars the plugin sets once (map start, load, hns_reload) rather than enforcing every tick.
	void ApplyServerSettings();
} // namespace hns::gameplay
