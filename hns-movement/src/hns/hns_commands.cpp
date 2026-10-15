#include "hns_commands.h"
#include "hns_config.h"
#include "hns_cvars.h"
#include "hns_player.h"
#include "utils/utils.h"

#include "tier0/memdbgon.h"

// All commands are server console / rcon only.
static_function bool FromServer(const CCommandContext &context)
{
	if (context.GetPlayerSlot().Get() != -1)
	{
		return false;
	}
	return true;
}

CON_COMMAND_F(hns_reload, "Reload cfg/hns-movement/hns-movement.cfg and push the movement cvars to every client.", FCVAR_NONE)
{
	if (!FromServer(context))
	{
		return;
	}
	hns::config::Load();
	hns::cvars::ApplyAll();
	hns::cvars::SendToAllClients();
	HNS_LOG_INFO(LogChannel::General, "Reloaded. Run hns_write_fallback if you also want the fallback cfg updated.\n");
}

CON_COMMAND_F(hns_status, "Print the enforced movement cvars and the plugin settings.", FCVAR_NONE)
{
	if (!FromServer(context))
	{
		return;
	}
	META_CONPRINTF("---- hns-movement cvars (enforced every tick) ----\n");
	hns::cvars::PrintTable();
	META_CONPRINTF("---- hns-movement settings ----\n");
	hns::config::PrintSettings();
}

CON_COMMAND_F(hns_write_fallback, "Write cfg/hns-movement/hns_movement_fallback.cfg with the enforced values.", FCVAR_NONE)
{
	if (!FromServer(context))
	{
		return;
	}
	hns::config::WriteFallback();
}

// Read-only stats, also the simplest bridge for ChaseMod or an admin plugin (plan section 6, option b).
CON_COMMAND_F(hns_perf_stats, "Print per-player jump/perf counts since connect.", FCVAR_NONE)
{
	if (!FromServer(context))
	{
		return;
	}
	META_CONPRINTF("%-4s %-24s %8s %8s %7s %10s %10s\n", "slot", "name", "jumps", "perfs", "perf%", "lastspeed", "lastgnd");
	for (i32 i = 0; i < MAXPLAYERS; i++)
	{
		HNSPlayer *player = g_pHNSPlayerManager->ToPlayer(CPlayerSlot(i));
		if (!player || !player->IsInGame())
		{
			continue;
		}
		const HNSPlayer::Stats &s = player->stats;
		f32 ratio = s.jumps ? 100.0f * (f32)s.perfs / (f32)s.jumps : 0.0f;
		META_CONPRINTF("%-4d %-24.24s %8llu %8llu %6.1f%% %10.2f %9.4fs\n", i, player->GetName(), s.jumps, s.perfs, ratio, s.lastTakeoffSpeed,
					   s.lastTimeOnGround);
	}
}

void hns::commands::Init()
{
	// The commands above register themselves through META_CONVAR_REGISTER. Nothing else to do yet; the sweep test and the
	// per-tick CSV logger (plan section 7, testing) will be added here.
}
