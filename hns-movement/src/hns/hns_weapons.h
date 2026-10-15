#pragma once
#include "common.h"

class HNSPlayer;
class CBaseEntity;
class CTakeDamageInfo;

// HNS weapon rules (hns_no_guns, hns_knife_ct_only, hns_knife_stab_only, hns_knife_damage):
//   - No guns for anyone: any firearm or taser a player holds (bought, picked up, given by the map) is removed.
//   - Only CTs have a knife: Ts' knives are removed, Ts keep their grenades.
//   - Knife: primary attack (slash) is disabled, only the right-click stab works.
//   - Every CT knife hit on a T does a fixed amount of damage (default 55), backstabs included.
namespace hns::weapons
{
	// Resolves the optional signatures. Missing ones only disable the matching rule.
	void Init();
	void SetDamageHooked(bool hooked);
	// hns_status: rule state, signature state and every player's weapons.
	void PrintDebug();
	// Once per server frame, before players are simulated.
	void OnGameFrame();
	// TakeDamageOld pre-hook. Returns true to block the damage entirely.
	bool OnTakeDamage(CBaseEntity *victim, CTakeDamageInfo *info);
} // namespace hns::weapons
