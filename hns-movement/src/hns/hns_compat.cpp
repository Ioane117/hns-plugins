#include "hns_compat.h"
#include "hns_config.h"

#include <string>
#include <vector>
#include <cstdlib>

#include "tier0/memdbgon.h"

using hns::config::settings;

#define CHASEMOD_CONFIG_PATH "addons/counterstrikesharp/configs/plugins/ChaseMod/ChaseMod.json"

namespace hns::compat
{
	struct ChaseModConfig
	{
		bool found = false;
		f32 ctStartFreezeTime = 15.0f;
		bool teamSwitch = true;
		i32 maxTerroristWinStreak = 5;
		bool knifeHook = true;
		bool knifeDamageModify = true;
		f32 knifeDamage = 50.0f;
		bool knifeDisabling = true;
		f32 stunFreezeTime = 15.0f;
	};

	// ChaseMod's built-in defaults are used for keys the file does not contain.
	static_global ChaseModConfig chasemod;

	static_function bool ReadFile(std::string &out)
	{
		char absPath[1024];
		V_snprintf(absPath, sizeof(absPath), "%s/csgo/%s", Plat_GetGameDirectory(), CHASEMOD_CONFIG_PATH);
		FILE *fp = fopen(absPath, "rb");
		if (!fp)
		{
			return false;
		}
		char buffer[4096];
		size_t n;
		while ((n = fread(buffer, 1, sizeof(buffer), fp)) > 0)
		{
			out.append(buffer, n);
		}
		fclose(fp);
		return true;
	}

	// Minimal lookup for a flat "key": value pair (ChaseMod's config is flat apart from freezeRingParticle).
	static_function bool FindValue(const std::string &json, const char *key, std::string &value)
	{
		std::string needle = std::string("\"") + key + "\"";
		size_t pos = json.find(needle);
		if (pos == std::string::npos)
		{
			return false;
		}
		pos = json.find(':', pos + needle.size());
		if (pos == std::string::npos)
		{
			return false;
		}
		pos++;
		while (pos < json.size() && isspace((unsigned char)json[pos]))
		{
			pos++;
		}
		size_t end = pos;
		while (end < json.size() && json[end] != ',' && json[end] != '}' && json[end] != '\n' && json[end] != '\r')
		{
			end++;
		}
		value = json.substr(pos, end - pos);
		while (!value.empty() && isspace((unsigned char)value.back()))
		{
			value.pop_back();
		}
		return !value.empty();
	}

	static_function void ReadBool(const std::string &json, const char *key, bool &out)
	{
		std::string v;
		if (FindValue(json, key, v))
		{
			out = (v == "true");
		}
	}

	static_function void ReadFloat(const std::string &json, const char *key, f32 &out)
	{
		std::string v;
		if (FindValue(json, key, v))
		{
			out = (f32)atof(v.c_str());
		}
	}

	static_function void ReadInt(const std::string &json, const char *key, i32 &out)
	{
		std::string v;
		if (FindValue(json, key, v))
		{
			out = atoi(v.c_str());
		}
	}

	static_function void Load()
	{
		chasemod = ChaseModConfig();
		std::string json;
		if (!ReadFile(json))
		{
			return;
		}
		chasemod.found = true;
		ReadFloat(json, "ctStartFreezeTime", chasemod.ctStartFreezeTime);
		ReadBool(json, "enableTeamSwitchingConditions", chasemod.teamSwitch);
		ReadInt(json, "maxTerroristWinStreak", chasemod.maxTerroristWinStreak);
		ReadBool(json, "enableKnifeHook", chasemod.knifeHook);
		ReadBool(json, "knifeDamageModify", chasemod.knifeDamageModify);
		ReadFloat(json, "knifeDamage", chasemod.knifeDamage);
		ReadBool(json, "enableKnifeDisabling", chasemod.knifeDisabling);
		ReadFloat(json, "stunFreezeTime", chasemod.stunFreezeTime);
	}

	// Calls report(message) for every conflict; returns the number found.
	template<typename F>
	static_function i32 ForEachConflict(F report)
	{
		i32 count = 0;
		char msg[256];
		bool hnsTeamSwitch = settings.teamSwitch || settings.tWinStreakSwitch > 0;
		if (!chasemod.found)
		{
			if (!hnsTeamSwitch)
			{
				report("ChaseMod config not found and hns_team_switch / hns_t_winstreak_switch are off: nobody switches teams.");
				count++;
			}
			return count;
		}
		if (settings.ctFreezeTime > 0.0f && chasemod.ctStartFreezeTime > 0.0f)
		{
			V_snprintf(msg, sizeof(msg), "CT start freeze is on in both (hns_ct_freeze_time %g, ChaseMod ctStartFreezeTime %g). Set ctStartFreezeTime 0.",
					   settings.ctFreezeTime, chasemod.ctStartFreezeTime);
			report(msg);
			count++;
		}
		if (hnsTeamSwitch && chasemod.teamSwitch)
		{
			report("Team switch is on in both: teams would be swapped twice (back to where they were). Set hns_team_switch 0 and "
				   "hns_t_winstreak_switch 0.");
			count++;
		}
		if (!hnsTeamSwitch && !chasemod.teamSwitch)
		{
			report("Team switch is off in both: nobody switches teams. Set enableTeamSwitchingConditions true in ChaseMod.json.");
			count++;
		}
		if (settings.knifeDamage > 0.0f && chasemod.knifeHook && chasemod.knifeDamageModify)
		{
			V_snprintf(msg, sizeof(msg),
					   "Knife damage is set by both (hns_knife_damage %g, ChaseMod knifeDamage %g); whichever runs last wins. Set "
					   "enableKnifeHook false in ChaseMod.json, or hns_knife_damage 0.",
					   settings.knifeDamage, chasemod.knifeDamage);
			report(msg);
			count++;
		}
		if (settings.knifeCTOnly && chasemod.knifeDisabling)
		{
			report("T knife lockout is on in both (hns_knife_ct_only, ChaseMod enableKnifeDisabling). Set enableKnifeDisabling false.");
			count++;
		}
		return count;
	}

	void Check()
	{
		Load();
		i32 conflicts = ForEachConflict([](const char *message) { HNS_LOG_WARN(LogChannel::General, "ChaseMod overlap: %s\n", message); });
		if (chasemod.found && conflicts == 0)
		{
			HNS_LOG_INFO(LogChannel::General, "ChaseMod config found, no overlapping features.\n");
		}
	}

	void PrintStatus()
	{
		Load();
		META_CONPRINTF("---- ChaseMod compatibility ----\n");
		if (!chasemod.found)
		{
			META_CONPRINTF("ChaseMod config not found (%s).\n", CHASEMOD_CONFIG_PATH);
		}
		else
		{
			META_CONPRINTF("ctStartFreezeTime %g, team switch %d (T streak %d), knife hook %d (modify %d, damage %g), knife disabling %d, "
						   "stun freeze %g\n",
						   chasemod.ctStartFreezeTime, chasemod.teamSwitch, chasemod.maxTerroristWinStreak, chasemod.knifeHook,
						   chasemod.knifeDamageModify, chasemod.knifeDamage, chasemod.knifeDisabling, chasemod.stunFreezeTime);
		}
		i32 conflicts = ForEachConflict([](const char *message) { META_CONPRINTF("OVERLAP: %s\n", message); });
		if (conflicts == 0)
		{
			META_CONPRINTF("No overlapping features.\n");
		}
	}
} // namespace hns::compat
