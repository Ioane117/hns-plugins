#pragma once
#include "common.h"

// Running alongside ChaseMod (CounterStrikeSharp). hns-movement is the movement unlocker and behaviour enforcer; ChaseMod
// owns the gameplay features it already does well. Each feature must be switched on in exactly one of the two:
//
//   feature             owner          hns-movement setting                 ChaseMod.json key
//   CT start freeze     hns-movement   hns_ct_freeze_time                   ctStartFreezeTime 0
//   team switch         ChaseMod       hns_team_switch 0                    enableTeamSwitchingConditions true
//   knife damage        hns-movement   hns_knife_damage                     enableKnifeHook false (or knifeDamageModify false)
//   T knife lockout     hns-movement   hns_knife_ct_only                    enableKnifeDisabling false
//   freeze grenade      ChaseMod       -                                    stunFreezeTime / stunFreezeRadius
//   team antiflash      ChaseMod       -                                    -
//
// The check reads ChaseMod's config file and warns when a feature is on in both (or, for the team switch, in neither).
namespace hns::compat
{
	// Reads ChaseMod.json and logs a warning per conflict. Called on load and by hns_reload.
	void Check();
	// hns_status section.
	void PrintStatus();
} // namespace hns::compat
