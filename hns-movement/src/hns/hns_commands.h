#pragma once

// Server console commands:
//   hns_reload          reload hns-movement.cfg and resend cvars to clients
//   hns_status          print enforced cvars (enforced / live game value / CS2KZ value) and settings
//   hns_write_fallback  write the plain cvar fallback cfg
//   hns_perf_stats      per-player jump and perf counts
namespace hns::commands
{
	void Init();
} // namespace hns::commands
