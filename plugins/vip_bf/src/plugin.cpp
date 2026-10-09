// vip_bf — Plugify port of the Pisex VIP module "VIP_BaseFunctions" / "[VIP] Base Functions"
// (ref/src/cs2-vip-modules/VIP_BaseFunctions/source/vip_bf.cpp, 201 lines).
//
// Behaviour kept identical:
//   * config <configs>/vip_bf/vip_bf.ini (the original's addons/configs/vip/vip_bf.ini), the
//     "VIP" KeyValues file with "round_min".
//   * features "health", "defuser", "money" (VIP_STRING, TOGGLABLE), then "helmet" (VIP_BOOL,
//     TOGGLABLE) and "defuser" again (VIP_BOOL, TOGGLABLE) - the original registered "defuser"
//     twice, and the later (BOOL) registration is the one the menu shows. Kept in that order.
//   * on VIP_OnPlayerSpawn (the original's OnPlayerSpawn callback), for a VIP on a round the
//     round_min condition allows:
//       - "health": the group value is either "+N" (add N) or an absolute number; it is written
//         to m_iMaxHealth and then to m_iHealth (the original's order), with the original's
//         "+N" quirk: str.erase(0, 2) drops the '+' *and* the first digit (see bf_logic.hpp);
//       - "armor": the same on m_ArmorValue;
//       - "money": the same on the in-game money account of the controller;
//       - "helmet": m_bHasHelmet = true; - "defuser": m_bHasDefuser = true on CT (team 3) only.
//   * the original's early-outs: money services missing -> stop (helmet/defuser are skipped),
//     item services missing -> stop; both are kept (the original's pointer checks).
//
// Port notes:
//   * m_iMaxHealth/m_iHealth/m_ArmorValue/m_iAccount -> s2sdk Get/SetClientMaxHealth,
//     Get/SetClientHealth, Get/SetClientArmor, Get/SetClientMoney (the engine's own accessors,
//     the same ones the sibling vip_regen_armor/vip_speed ports use).
//   * m_bHasHelmet/m_bHasDefuser have no s2sdk helper: they are written through the schema on the
//     raw item-services pointer (CBasePlayerPawn::m_pItemServices -> CCSPlayer_ItemServices).
//     The original wrote them through the field reference without a network state change; the
//     port does the same (changeState = false).
//   * the original's `pPlayerController->m_hPlayerPawn() + IsAlive` becomes pisex::IsAlive.
//   * a missing config no longer aborts the plugin (the original's Load() returned false): the
//     module logs it and stays disabled, which is what the original did in effect.

#include <pisex/pisex.hpp>
#include <plg/plugin.hpp>
#include <plugin_export.h>
#include <s2sdk/s2sdk.hpp>
#include <vip_api.hpp>

#include "bf_logic.hpp"

#include <chrono>
#include <cstdint>
#include <exception>
#include <string>
#include <string_view>

namespace {

// The original code's unqualified spellings (VIP_STRING, VIP_BOOL, TOGGLABLE, ...).
using namespace vip;

constexpr const char* kLogTag = "VIP-BF"; // GetLogTag() of the original ("[VIP-BF]")
constexpr const char* kConfigFile = "vip_bf.ini";
constexpr const char* kHealthFeature = "health";
constexpr const char* kArmorFeature = "armor";
constexpr const char* kMoneyFeature = "money";
constexpr const char* kHelmetFeature = "helmet";
constexpr const char* kDefuserFeature = "defuser";
constexpr int kCTTeam = 3;

IVIPApi* g_pVIPCore = &IVIPApi::Get();

void LogInfo(std::string_view message) {
	if (plg::Log) {
		plg::Log(message, plg::Severity::Info, plg::source_location::current());
	}
}

void LogError(std::string_view message) {
	if (plg::Log) {
		plg::Log(message, plg::Severity::Error, plg::source_location::current());
	}
}

// The original's ErrorLog("[%s] ...", GetLogTag()).
void LogTaggedError(const std::string& message) {
	LogError(std::string("[") + kLogTag + "] " + message);
}

// --- raw entity access -------------------------------------------------------
// The item services are a component object of the pawn (the original's m_pItemServices); the
// component pointers have no s2sdk helper, so they are read through the schema by class + field
// name on the raw pawn pointer (CBasePlayerPawn::m_pItemServices).
void* ItemServices(int iSlot) {
	// s2sdk's GetClientPawn warns "invalid player slot: N" (to the server console) for every call
	// on a slot without a controller, so an empty slot is filtered out here.
	if (!__s2sdk_GetClientPawn || iSlot < 0 || !pisex::IsConnected(iSlot)) {
		return nullptr;
	}
	try {
		void* pawn = s2sdk::GetClientPawn(iSlot);
		if (!pawn) {
			return nullptr;
		}
		const int offset = pisex::SchemaOffset("CBasePlayerPawn", "m_pItemServices");
		if (offset < 0) {
			return nullptr;
		}
		// m_pItemServices is a pointer field: its value is the component address.
		return *reinterpret_cast<void**>(static_cast<char*>(pawn) + offset);
	} catch (...) {
		return nullptr;
	}
}

// The controller's money services (the original's m_pInGameMoneyServices null check).
void* MoneyServices(int iSlot) {
	if (!__s2sdk_PlayerSlotToEntPointer) {
		return nullptr;
	}
	try {
		void* controller = s2sdk::PlayerSlotToEntPointer(iSlot);
		if (!controller) {
			return nullptr;
		}
		const int offset = pisex::SchemaOffset("CCSPlayerController", "m_pInGameMoneyServices");
		if (offset < 0) {
			return nullptr;
		}
		return *reinterpret_cast<void**>(static_cast<char*>(controller) + offset);
	} catch (...) {
		return nullptr;
	}
}

class BaseFunctionsPlugin final : public plg::Plugin {
public:
	plg::PluginResult OnPluginStart() override;
	plg::PluginResult OnPluginUpdate(std::chrono::milliseconds dt) override;
	plg::PluginResult OnPluginEnd() override;

private:
	void LoadConfig();
	// VIP_OnPlayerSpawn of the original.
	void OnPlayerSpawn(int iSlot, int iTeam, bool bIsVIP);

	bool m_loaded = false;
	int m_roundMin = 0;
};

BaseFunctionsPlugin g_plugin;

// --- spawn -------------------------------------------------------------------

void BaseFunctionsPlugin::OnPlayerSpawn(int iSlot, int /*iTeam*/, bool bIsVIP) {
	if (!m_loaded || !bIsVIP) {
		return;
	}

	// (iRoundMin == 1 && !pistol) || (iRoundMin <= totalRounds && !pistol) || iRoundMin == 0
	if (!vip_bf::ShouldApply(m_roundMin, g_pVIPCore->VIP_PistolRound(), g_pVIPCore->VIP_GetTotalRounds())) {
		return;
	}

	// CCSPlayerController + m_hPlayerPawn + IsAlive of the original.
	if (!pisex::IsAlive(iSlot)) {
		return;
	}

	// const char* sHealth = ...; if(strlen(sHealth) > 0)
	const std::string sHealth = g_pVIPCore->VIP_GetClientFeatureString(iSlot, kHealthFeature);
	if (!sHealth.empty() && __s2sdk_GetClientMaxHealth && __s2sdk_SetClientMaxHealth) {
		const int iValue = vip_bf::ParseValue(sHealth);
		const bool bBonus = vip_bf::IsBonus(sHealth);
		s2sdk::SetClientMaxHealth(iSlot, bBonus ? s2sdk::GetClientMaxHealth(iSlot) + iValue : iValue);
		if (__s2sdk_GetClientHealth && __s2sdk_SetClientHealth) {
			s2sdk::SetClientHealth(iSlot, bBonus ? s2sdk::GetClientHealth(iSlot) + iValue : iValue);
		}
	}

	// const char* sArmor = ...; if(strlen(sArmor) > 0)
	const std::string sArmor = g_pVIPCore->VIP_GetClientFeatureString(iSlot, kArmorFeature);
	if (!sArmor.empty() && __s2sdk_GetClientArmor && __s2sdk_SetClientArmor) {
		const int iValue = vip_bf::ParseValue(sArmor);
		s2sdk::SetClientArmor(iSlot, vip_bf::IsBonus(sArmor) ? s2sdk::GetClientArmor(iSlot) + iValue : iValue);
	}

	// CCSPlayerController_InGameMoneyServices* pMoneyServices = ...; if(!pMoneyServices) return;
	if (MoneyServices(iSlot) == nullptr) {
		return;
	}
	const std::string sMoney = g_pVIPCore->VIP_GetClientFeatureString(iSlot, kMoneyFeature);
	if (!sMoney.empty() && __s2sdk_GetClientMoney && __s2sdk_SetClientMoney) {
		const int iValue = vip_bf::ParseValue(sMoney);
		s2sdk::SetClientMoney(iSlot, vip_bf::IsBonus(sMoney) ? s2sdk::GetClientMoney(iSlot) + iValue : iValue);
	}

	// CCSPlayer_ItemServices* pItemServices = ...; if(!pItemServices) return;
	void* itemServices = ItemServices(iSlot);
	if (itemServices == nullptr || !__s2sdk_SetEntSchema2) {
		return;
	}

	// pItemServices->m_bHasHelmet() = true;
	if (g_pVIPCore->VIP_GetClientFeatureBool(iSlot, kHelmetFeature)) {
		s2sdk::SetEntSchema2(itemServices, "CCSPlayer_ItemServices", "m_bHasHelmet", 1, false, 0);
	}
	// if (bDefuser && pPlayerPawn->m_iTeamNum() == 3) pItemServices->m_bHasDefuser() = true;
	if (g_pVIPCore->VIP_GetClientFeatureBool(iSlot, kDefuserFeature) && pisex::GetPlayerTeam(iSlot) == kCTTeam) {
		s2sdk::SetEntSchema2(itemServices, "CCSPlayer_ItemServices", "m_bHasDefuser", 1, false, 0);
	}
}

// --- lifecycle ---------------------------------------------------------------

void BaseFunctionsPlugin::LoadConfig() {
	m_loaded = false;
	pisex::KeyValues kv("VIP");
	if (!kv.LoadFromFile(kConfigFile)) {
		LogTaggedError(std::string("failed to load ") + kConfigFile + " - the module stays disabled");
		return;
	}
	m_roundMin = kv.GetInt("round_min");
	m_loaded = true;
}

plg::PluginResult BaseFunctionsPlugin::OnPluginStart() {
	try {
		// Never fatal: only logs when the s2sdk bindings are missing.
		pisex::Init("vip_bf");

		LoadConfig();

		// g_pVIPCore->VIP_OnPlayerSpawn(VIP_OnPlayerSpawn) of AllPluginsLoaded.
		g_pVIPCore->VIP_OnPlayerSpawn([this](int iSlot, int iTeam, bool bIsVIP) {
			OnPlayerSpawn(iSlot, iTeam, bIsVIP);
		});

		// The five registrations of the original, in its order (a repeated name overwrites).
		g_pVIPCore->VIP_RegisterFeature(kHealthFeature, VIP_STRING, TOGGLABLE);
		g_pVIPCore->VIP_RegisterFeature(kDefuserFeature, VIP_STRING, TOGGLABLE);
		g_pVIPCore->VIP_RegisterFeature(kMoneyFeature, VIP_STRING, TOGGLABLE);
		g_pVIPCore->VIP_RegisterFeature(kHelmetFeature, VIP_BOOL, TOGGLABLE);
		g_pVIPCore->VIP_RegisterFeature(kDefuserFeature, VIP_BOOL, TOGGLABLE);

		LogInfo(std::string("[") + kLogTag + "] ready");
	} catch (const std::exception& ex) {
		LogTaggedError(std::string("OnPluginStart failed: ") + ex.what());
	} catch (...) {
		LogTaggedError("OnPluginStart failed with an unknown exception");
	}
	return {}; // never Failed: that stops every plugify plugin
}

plg::PluginResult BaseFunctionsPlugin::OnPluginUpdate(std::chrono::milliseconds /*dt*/) {
	try {
		pisex::Poll();
	} catch (const std::exception& ex) {
		LogTaggedError(std::string("OnPluginUpdate failed: ") + ex.what());
	} catch (...) {
		LogTaggedError("OnPluginUpdate failed with an unknown exception");
	}
	return {};
}

plg::PluginResult BaseFunctionsPlugin::OnPluginEnd() {
	try {
		g_pVIPCore->VIP_OnPlayerSpawnUnregister();
		pisex::Shutdown();
	} catch (...) {
		// nothing left to do at unload
	}
	return {};
}

} // namespace

PLUGIFY_PLUGIN(PLUGIN_API, &g_plugin)
