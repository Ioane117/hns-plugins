#pragma once
#include "common.h"
#include "convar.h"

// Movement cvars owned by hns-movement (plan section 4).
// Values are written straight into the cvar data every tick, the way CS2KZ's KZ::mode::ApplyModeSettings does, so map
// cfgs, mp_restartgame or an admin typo cannot change how movement feels. Edit hns-movement.cfg and run hns_reload instead.
//
// mp_solid_teammates is owned, set through hns_solid_teammates (default 0, same as CS2KZ). The surf/rampbug code
// predicts TryPlayerMove without player collisions, which is only correct when teammates are not solid.
//
// Deliberately NOT owned here:
//   mp_solid_enemies                       (HNS wants Ts and CTs to collide)
//   sv_falldamage_scale                    (ChaseMod toggles it during the CT freeze)
//   game_type, game_mode, mp_*, bot_*, sv_alltalk (server policy, stays in the server cfg)
enum HNSMovementCvar : int
{
	HNSCVAR_SV_ACCELERATE = 0,
	HNSCVAR_SV_ACCELERATE_USE_WEAPON_SPEED,
	HNSCVAR_SV_AIRACCELERATE,
	HNSCVAR_SV_AIR_MAX_WISHSPEED,
	HNSCVAR_SV_AUTOBUNNYHOPPING,
	HNSCVAR_SV_BOUNCE,
	HNSCVAR_SV_ENABLEBUNNYHOPPING,
	HNSCVAR_SV_FRICTION,
	HNSCVAR_SV_GRAVITY,
	HNSCVAR_SV_JUMP_IMPULSE,
	HNSCVAR_SV_JUMP_PRECISION_ENABLE,
	HNSCVAR_SV_JUMP_SPAM_PENALTY_TIME,
	HNSCVAR_SV_LADDER_ANGLE,
	HNSCVAR_SV_LADDER_DAMPEN,
	HNSCVAR_SV_LADDER_SCALE_SPEED,
	HNSCVAR_SV_MAXSPEED,
	HNSCVAR_SV_MAXVELOCITY,
	HNSCVAR_SV_STAMINAJUMPCOST,
	HNSCVAR_SV_STAMINALANDCOST,
	HNSCVAR_SV_STAMINAMAX,
	HNSCVAR_SV_STAMINARECOVERYRATE,
	HNSCVAR_SV_STANDABLE_NORMAL,
	HNSCVAR_SV_STEP_MOVE_VEL_MIN,
	HNSCVAR_SV_TIMEBETWEENDUCKS,
	HNSCVAR_SV_WALKABLE_NORMAL,
	HNSCVAR_SV_WATERACCELERATE,
	HNSCVAR_SV_WATERFRICTION,
	HNSCVAR_SV_WATER_SLOW_AMOUNT,
	HNSCVAR_SV_SUBTICK_MOVEMENT_VIEW_ANGLES,
	HNSCVAR_SV_LEGACY_JUMP,
	HNSCVAR_SV_BHOP_TIME_WINDOW,
	HNSCVAR_MP_SOLID_TEAMMATES,
	HNSCVAR_MP_FREEZETIME,
	HNSCVAR_COUNT,
};

namespace hns::cvars
{
	enum class Type
	{
		Float,
		Bool,
		Int,
	};

	struct Definition
	{
		const char *name;
		Type type;
		// HNS default. Used when hns-movement.cfg is missing or does not set this cvar.
		f32 defaultValue;
		// CS2KZ Classic value, for reference in the generated cfg.
		f32 kzValue;
		// Name used in hns-movement.cfg when it differs from the cvar name.
		const char *cfgName = nullptr;

		constexpr const char *ConfigName() const
		{
			return cfgName ? cfgName : name;
		}
	};

	// clang-format off
	inline constexpr Definition definitions[HNSCVAR_COUNT] = {
		{"sv_accelerate",                   Type::Float, 6.5f,     6.5f},
		{"sv_accelerate_use_weapon_speed",  Type::Bool,  0.0f,     0.0f},
		{"sv_airaccelerate",                Type::Float, 1000.0f,  100.0f},
		// HNS: 35 instead of 30 gives ~17% more speed per air strafe (surf and bhop air gain).
		{"sv_air_max_wishspeed",            Type::Float, 35.0f,    30.0f},
		{"sv_autobunnyhopping",             Type::Bool,  0.0f,     0.0f},
		{"sv_bounce",                       Type::Float, 0.0f,     0.0f},
		{"sv_enablebunnyhopping",           Type::Bool,  1.0f,     1.0f},
		{"sv_friction",                     Type::Float, 4.05f,    5.2f},
		{"sv_gravity",                      Type::Float, 800.0f,   800.0f},
		{"sv_jump_impulse",                 Type::Float, 302.0f,   302.0f},
		{"sv_jump_precision_enable",        Type::Bool,  0.0f,     0.0f},
		{"sv_jump_spam_penalty_time",       Type::Float, 0.0f,     0.0f},
		{"sv_ladder_angle",                 Type::Float, -0.707f,  -0.707f},
		{"sv_ladder_dampen",                Type::Float, 1.0f,     1.0f},
		{"sv_ladder_scale_speed",           Type::Float, 1.0f,     1.0f},
		{"sv_maxspeed",                     Type::Float, 380.0f,   320.0f},
		{"sv_maxvelocity",                  Type::Float, 3500.0f,  3500.0f},
		{"sv_staminajumpcost",              Type::Float, 0.0f,     0.0f},
		{"sv_staminalandcost",              Type::Float, 0.0f,     0.0f},
		{"sv_staminamax",                   Type::Float, 0.0f,     0.0f},
		{"sv_staminarecoveryrate",          Type::Float, 9999.0f,  9999.0f},
		{"sv_standable_normal",             Type::Float, 0.7f,     0.7f},
		{"sv_step_move_vel_min",            Type::Float, 64.0f,    64.0f},
		{"sv_timebetweenducks",             Type::Float, 0.0f,     0.0f},
		{"sv_walkable_normal",              Type::Float, 0.7f,     0.7f},
		{"sv_wateraccelerate",              Type::Float, 10.0f,    10.0f},
		{"sv_waterfriction",                Type::Float, 1.0f,     1.0f},
		{"sv_water_slow_amount",            Type::Float, 0.9f,     0.9f},
		{"sv_subtick_movement_view_angles", Type::Bool,  0.0f,     0.0f},
		{"sv_legacy_jump",                  Type::Bool,  1.0f,     1.0f},
		// Inert while sv_legacy_jump is on. Kept so the modern-jump perf check has the value if legacy jump is ever turned off.
		{"sv_bhop_time_window",             Type::Float, 0.05f,    0.02f},
		// 0 = teammates pass through each other, 1 = solid, 2 = solid but can't stand on each other's heads.
		{"mp_solid_teammates",              Type::Int,   0.0f,     0.0f, "hns_solid_teammates"},
		// The engine freezetime freezes both teams. HNS freezes only the CTs (hns_ct_freeze_time), so keep this at 0.
		{"mp_freezetime",                   Type::Int,   0.0f,     0.0f, "hns_engine_freezetime"},
	};
	// clang-format on

	// Resolve every cvar reference. Fails if any of them does not exist in the current game build.
	bool Init(char *error, size_t maxlen);
	void Cleanup();

	// Value the plugin enforces for this cvar.
	const CVValue_t &Get(HNSMovementCvar cvar);
	f32 GetFloat(HNSMovementCvar cvar);
	bool GetBool(HNSMovementCvar cvar);

	// Set the enforced value (from the cfg). Does not write to the game until the next ApplyAll.
	bool SetFromString(const char *name, const char *value);
	void ResetToDefaults();
	i32 FindByName(const char *name);

	// Write every enforced value into the cvar data. Cheap: called every tick per player, like CS2KZ.
	// Logs a warning (once per change) if something outside the plugin changed one of the cvars.
	void ApplyAll();

	// CS2KZ strips FCVAR_REPLICATED from these at load and sends the values to each client itself.
	// Values are written directly to cvar memory, which never triggers the engine's own replication, so clients only
	// learn the real values through SendToClient. Removing the flag also stops a later ordinary Set() from broadcasting
	// a value that the plugin will overwrite on the next tick anyway.
	void DisableReplication();
	void EnableReplication();

	// Send the enforced values to one client (or all), so client-side movement prediction matches the server.
	void SendToClient(CPlayerSlot slot);
	void SendToAllClients();

	// Print the table (enforced value, current game value, CS2KZ value) to the console.
	void PrintTable();
} // namespace hns::cvars
