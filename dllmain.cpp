#pragma comment(lib, "AsaApi.lib")

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"

#include "VyHub.h"
#include "Timer.h"
#include "Api.h"
#include "User.h"
#include "Reward.h"
#include "Statistics.h"

#include <API/ARK/Ark.h>

#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <any>
#include <json.hpp>


// DECLARE_HOOK(AShooterGameMode_PostLogin, void, AShooterGameMode*, APlayerController*);
DECLARE_HOOK(AShooterCharacter_Die, bool, AShooterCharacter*, float, FDamageEvent*, AController*, AActor*); // Die
DECLARE_HOOK(AShooterGameMode_Logout, void, AShooterGameMode*, AController*); // Logout
DECLARE_HOOK(AShooterGameMode_HandleNewPlayer, bool, AShooterGameMode*, AShooterPlayerController*, UPrimalPlayerData*, AShooterCharacter*, bool);
DECLARE_HOOK(AShooterCharacter_AuthPostSpawnInit, void, AShooterCharacter*);

void getDelayedPlayerName(std::string eosId)
{
	AShooterPlayerController* player = AsaApi::GetApiUtils().FindPlayerFromEOSID(eosId.c_str());
	if (!player)
	{
		// User probably offline again
		return;
	}
	std::string player_name = TCHAR_TO_UTF8(*AsaApi::GetApiUtils().GetCharacterName(player));
	if (player_name == "")
	{
		API::Timer::Get().DelayExecute([=]() {
			getDelayedPlayerName(eosId);
			}, 5);
		return;
	};

	if (!VyHub::config.contains("api_key") || !VyHub::config.contains("api_url") || !VyHub::config.contains("server_id") || !VyHub::config.contains("serverbundle_id"))
	{
		return;
	}

	std::unique_ptr<Entity::VyHubUser> vyhub_user = User::getUser(eosId, player_name);

	if (!vyhub_user)
	{
		// User creation likely failed
		return;
	}
	std::string playerId = TCHAR_TO_UTF8(*AsaApi::GetApiUtils().GetEOSIDFromController(player));

	VyHub::users.insert({ playerId, *vyhub_user });

	// TODO this does not work, as the users rewards are not loaded
	std::vector<std::string> events = { "SPAWN", "CONNECT" };
	Reward::executeReward(events, playerId);
}

bool Hook_AShooterGameMode_HandleNewPlayer(AShooterGameMode* _this, AShooterPlayerController* new_player, UPrimalPlayerData* player_data, AShooterCharacter* player_character, bool is_from_login)
{
	const FString eos_id = AsaApi::IApiUtils::GetEOSIDFromController(new_player);
	std::string player_name = TCHAR_TO_UTF8(*AsaApi::GetApiUtils().GetCharacterName(new_player));

	// std::unique_ptr<Entity::VyHubUser> vyhub_user = Api::getUser(TCHAR_TO_UTF8(*eos_id));

	// if (!vyhub_user)
	// {
		// User probably not registered yet. Registering is done through the collect server function as there is no username available in this hook :(
	// }

	API::Timer::Get().DelayExecute([=]() {
		getDelayedPlayerName(TCHAR_TO_UTF8(*eos_id));
		}, 5);


	if (!VyHub::config.contains("api_key") || !VyHub::config.contains("api_url") || !VyHub::config.contains("server_id") || !VyHub::config.contains("serverbundle_id"))
	{
		Log::GetLog()->info("Looks like this is a fresh setup. Get started by creating a server and generating the correct command in your VyHub server settings");
		return AShooterGameMode_HandleNewPlayer_original(_this, new_player, player_data, player_character, is_from_login);
	}



	return AShooterGameMode_HandleNewPlayer_original(_this, new_player, player_data, player_character, is_from_login);
}


void Hook_AShooterCharacter_AuthPostSpawnInit(AShooterCharacter* _this)
{
	std::string player_name = TCHAR_TO_UTF8(*AsaApi::GetApiUtils().GetCharacterName(AsaApi::GetApiUtils().FindControllerFromCharacter(_this)));
	
	if (!AsaApi::GetApiUtils().FindControllerFromCharacter(_this))
	{
		return AShooterCharacter_AuthPostSpawnInit_original(_this);
	}

	std::vector<std::string> events = { "SPAWN" };
	Reward::executeReward(events, TCHAR_TO_UTF8(*AsaApi::GetApiUtils().GetEOSIDFromController(AsaApi::GetApiUtils().FindControllerFromCharacter(_this))));

	return AShooterCharacter_AuthPostSpawnInit_original(_this);
}

bool Hook_AShooterCharacter_Die(AShooterCharacter* _this, float KillingDamage,FDamageEvent* DamageEvent, AController* Killer, AActor* DamageCauser)
{
	std::vector<std::string> events = { "DEATH" };

	if (!AsaApi::GetApiUtils().FindControllerFromCharacter(_this))
	{
		return AShooterCharacter_Die_original(_this, KillingDamage, DamageEvent, Killer, DamageCauser);
	}


	Reward::executeReward(events, TCHAR_TO_UTF8(*AsaApi::GetApiUtils().GetEOSIDFromController(AsaApi::GetApiUtils().FindControllerFromCharacter(_this))));


	return AShooterCharacter_Die_original(_this, KillingDamage, DamageEvent, Killer, DamageCauser);
}

void Hook_AShooterGameMode_Logout(AShooterGameMode* _this, AController* Exiting)
{
	std::string playerId = TCHAR_TO_UTF8(*AsaApi::GetApiUtils().GetEOSIDFromController(Exiting));
	VyHub::users.erase(playerId);

	AShooterGameMode_Logout_original(_this, Exiting);
}


std::vector<std::string> getOnlinePlayerIds()
{
	std::vector<std::string> online_player_ids = {};
	TArray<TWeakObjectPtr<APlayerController>, FDefaultAllocator> PlayerControllers = AsaApi::GetApiUtils().GetWorld()->PlayerControllerListField();
	for (TWeakObjectPtr<APlayerController>& PlayerController : PlayerControllers)
	{
		std::string playerId = TCHAR_TO_UTF8(*AsaApi::GetApiUtils().GetEOSIDFromController(PlayerController.Get()));
		online_player_ids.push_back(playerId);
	}

	return online_player_ids;
}

void collectServerStatistics()
{

	std::vector<nlohmann::json> user_activities;

	std::vector<std::string> online_player_ids = getOnlinePlayerIds();
	for (const auto& player_id : online_player_ids) {
		std::unique_ptr<Entity::VyHubUser> vyhub_user = User::getUser(player_id);

		if (!vyhub_user) {
			Log::GetLog()->warn("VyHub User not found for online player " + player_id + ".This should not happen. Report this!");
			continue;
		}

		nlohmann::json map;
		map["user_id"] = vyhub_user.get()->id;
		map["extra"] = {};
		user_activities.push_back(map);
	}

	nlohmann::json j;
	j["user_activities"] = user_activities;
	j["users_current"] = online_player_ids.size();
	j["is_alive"] = true;

	Api::patchServer(j);
}

void getRewards()
{
	std::vector<std::string> online_player_ids = getOnlinePlayerIds();
	VyHub::rewards = Api::fetchRewards(online_player_ids);
	if (VyHub::rewards == NULL)
	{
		return; // Fetch not successful
	}
	else {
		// reset sentAndExecuted Rewards
		VyHub::executedAndSentRewards = {};
	}

	std::vector<std::string> events = { "DIRECT", "DISABLE" };
	Reward::executeReward(events, std::nullopt);
}

void collectPlayerTime()
{
	const std::string config_path = AsaApi::Tools::GetCurrentDir() + "/ArkApi/Plugins/VyHub/playtime.json";
	std::ifstream file{ config_path };
	if (!file.is_open())
	{
		Log::GetLog()->error("Can't open playtime.json");
		throw std::runtime_error("Can't open playtime.json");
	}

	nlohmann::json j;
	file >> j;

	std::vector<std::string> onlinePlayerIds = getOnlinePlayerIds();

	for (const std::string& playerId : onlinePlayerIds) {
		if (j.contains(playerId)) {
			double oldTime = j[playerId];
			j[playerId] = oldTime + 1;
		}
		else {
			j[playerId] = 1.0;
		}
	}


	std::ofstream o(config_path);
	o << std::setw(4) << j << std::endl;
}

void sendPlayerTime()
{
	Log::GetLog()->info("Sending Player Time to VyHub API");
	Statistic::sendPlayerTime();
}

void ReadConfig()
{
	const std::string config_path = AsaApi::Tools::GetCurrentDir() + "/ArkApi/Plugins/VyHub/config.json";
	std::ifstream file{ config_path };
	if (!file.is_open())
	{
		Log::GetLog()->error("Can't open config.json");
		throw std::runtime_error("Can't open config.json");
	}

	file >> VyHub::config;

	file.close();

	Log::GetLog()->info("VyHub config loaded");
}


void CheckReady()
{
	if (VyHub::config["api_key"].empty() || VyHub::config["api_url"].empty() || VyHub::config["server_id"].empty())
	{
		Log::GetLog()->error("Looks like this is a fresh setup. Get started by creating a server and generating the correct command in your VyHub server settings.");
		return;
	}
	Log::GetLog()->info("Validating API Credentials...");

	if (Api::init())
	{
		// Registering timers
		Log::GetLog()->info("Registering recurring tasks..");
		API::Timer::Get().RecurringExecute(&collectServerStatistics, 60, -1, false);
		API::Timer::Get().RecurringExecute(&getRewards, 60, -1, false);
		API::Timer::Get().RecurringExecute(&collectPlayerTime, 60, -1, false);
		API::Timer::Get().RecurringExecute(&sendPlayerTime, 1800, -1, true); // 30min
	}
}


void vylogin(AShooterPlayerController* player_controller, FString* message, int, int)
{
	TArray<FString> parsed;
	message->ParseIntoArray(parsed, L" ", true);

	if (parsed.Num() != 2)
	{
		AsaApi::GetApiUtils().SendServerMessage(player_controller, FLinearColor(255, 255, 0), "Invalid syntax /vylogin <UUID>");
		return;
	}

	std::string uuid = TCHAR_TO_UTF8(*parsed[1]);
	std::string identifier = TCHAR_TO_UTF8(*AsaApi::GetApiUtils().GetEOSIDFromController(player_controller));

	std::string username = TCHAR_TO_UTF8(*AsaApi::GetApiUtils().GetCharacterName(player_controller));

	std::string return_msg = Api::confirmAuth(identifier, uuid, username);

	AsaApi::GetApiUtils().SendServerMessage(player_controller, FLinearColor(255, 255, 0), return_msg.c_str());

}

void vh_setup(AShooterPlayerController* shooter_controller, FString* message, int, int)
{
	if (shooter_controller->bIsAdmin().Get() == false)
	{
		AsaApi::GetApiUtils().SendChatMessage(shooter_controller, "Server", "You need to be an admin to execute this command!");
		return;
	}

	// The message has the form /vh_setup <api_key> <api_url> <server_id> <serverbundle_id>
	const std::string config_path = AsaApi::Tools::GetCurrentDir() + "/ArkApi/Plugins/VyHub/config.json";
	std::ifstream file{ config_path };
	nlohmann::json j;
	file >> j;

	TArray<FString> parsed;
	message->ParseIntoArray(parsed, L" ", true);

	if (parsed.Num() != 5)
	{
		AsaApi::GetApiUtils().SendServerMessage(shooter_controller, FLinearColor(255, 255, 0), "Invalid number of arguments: /vh_setup <api_key> <api_url> <server_id> <serverbundle_id>");
		return;
	}

	j["api_key"] = TCHAR_TO_UTF8(*parsed[1]);

	// Split the API Url into two parts as the library only allows for base ulr and no suffix
	std::string url = TCHAR_TO_UTF8(*parsed[2]);
	size_t slashPos = url.find("/", 8);
	j["api_url"] = url.substr(0, slashPos); // eg. https://vyhub.app
	j["api_suffix"] = url.substr(slashPos); // eg. /api/v1

	Log::GetLog()->info("Config edited: Api Url: %s", TCHAR_TO_UTF8(*parsed[2]));
	j["server_id"] = TCHAR_TO_UTF8(*parsed[3]);
	Log::GetLog()->info("Config edited: Server Id: %s", TCHAR_TO_UTF8(*parsed[3]));
	j["serverbundle_id"] = TCHAR_TO_UTF8(*parsed[4]);
	Log::GetLog()->info("Config edited: Serverbundle Id: %s", TCHAR_TO_UTF8(*parsed[4]));

	std::ofstream o(config_path);
	o << std::setw(4) << j << std::endl;

	AsaApi::GetApiUtils().SendServerMessage(shooter_controller, FLinearColor(255, 255, 0), "Config Set");
	ReadConfig();
	CheckReady();
}


void vh_setup_console(APlayerController* player, FString* message, bool)
{
	auto* shooter_controller = static_cast<AShooterPlayerController*>(player);

	// The message has the form /vh_setup <api_key> <api_url> <server_id> <serverbundle_id>
	const std::string config_path = AsaApi::Tools::GetCurrentDir() + "/ArkApi/Plugins/VyHub/config.json";
	std::ifstream file{ config_path };
	nlohmann::json j;
	file >> j;

	TArray<FString> parsed;
	message->ParseIntoArray(parsed, L" ", true);
	
	if (parsed.Num() != 5)
	{
		AsaApi::GetApiUtils().SendServerMessage(shooter_controller, FLinearColor(255, 255, 0), "Invalid number of arguments: /vh_setup <api_key> <api_url> <server_id> <serverbundle_id>");
		return;
	}


	j["api_key"] = TCHAR_TO_UTF8(*parsed[1]);

	// Split the API Url into two parts as the library only allows for base ulr and no suffix
	std::string url = TCHAR_TO_UTF8(*parsed[2]);
	size_t slashPos = url.find("/", 8);
	j["api_url"] = url.substr(0, slashPos); // eg. https://vyhub.app
	j["api_suffix"] = url.substr(slashPos); // eg. /api/v1

	Log::GetLog()->info("Config edited: Api Url: %s", TCHAR_TO_UTF8(*parsed[2]));
	j["server_id"] = TCHAR_TO_UTF8(*parsed[3]);
	Log::GetLog()->info("Config edited: Server Id: %s", TCHAR_TO_UTF8(*parsed[3]));
	j["serverbundle_id"] = TCHAR_TO_UTF8(*parsed[4]);
	Log::GetLog()->info("Config edited: Serverbundle Id: %s", TCHAR_TO_UTF8(*parsed[4]));

	std::ofstream o(config_path);
	o << std::setw(4) << j << std::endl;

	AsaApi::GetApiUtils().SendServerMessage(shooter_controller, FLinearColor(255, 255, 0), "Config Set");
	ReadConfig();
	CheckReady();
}

BOOL Load()
{
	try
	{
		Log::Get().Init("VyHub");

		AsaApi::GetCommands().AddChatCommand("/vh_setup", &vh_setup);
		AsaApi::GetCommands().AddConsoleCommand("/vh_setup", &vh_setup_console);
		AsaApi::GetCommands().AddChatCommand("/vylogin", &vylogin);
		AsaApi::GetHooks().SetHook("AShooterGameMode.HandleNewPlayer_Implementation(AShooterPlayerController*,UPrimalPlayerData*,AShooterCharacter*,bool)", &Hook_AShooterGameMode_HandleNewPlayer, &AShooterGameMode_HandleNewPlayer_original);
		AsaApi::GetHooks().SetHook("AShooterCharacter.Die(float,FDamageEvent&,AController*,AActor*)", &Hook_AShooterCharacter_Die, &AShooterCharacter_Die_original);
		AsaApi::GetHooks().SetHook("AShooterGameMode.Logout(AController*)", &Hook_AShooterGameMode_Logout, &AShooterGameMode_Logout_original);
		AsaApi::GetHooks().SetHook("AShooterCharacter.AuthPostSpawnInit()", &Hook_AShooterCharacter_AuthPostSpawnInit, &AShooterCharacter_AuthPostSpawnInit_original);

		ReadConfig();
		CheckReady();
	}
	catch (const std::exception& error)
	{
		Log::GetLog()->error(error.what());
		throw;
	}


	return TRUE;
}
BOOL Unload()
{
	AsaApi::GetCommands().RemoveConsoleCommand("/vh_setup");
	AsaApi::GetCommands().RemoveChatCommand("/vylogin");
	AsaApi::GetCommands().RemoveChatCommand("/vh_setup");

	AsaApi::GetHooks().DisableHook("AShooterCharacter.Die(float,FDamageEvent&,AController*,AActor*)", &Hook_AShooterCharacter_Die);
	AsaApi::GetHooks().DisableHook("AShooterGameMode.Logout(AController*)", &Hook_AShooterGameMode_Logout);
	AsaApi::GetHooks().DisableHook("AShooterCharacter.AuthPostSpawnInit()", &Hook_AShooterCharacter_AuthPostSpawnInit);
	AsaApi::GetHooks().DisableHook("AShooterGameMode.HandleNewPlayer(AShooterPlayerController*,UPrimalPlayerData*,AShooterCharacter*,bool)", &Hook_AShooterGameMode_HandleNewPlayer);

	API::Timer::Get().UnloadAllTimers();

	return TRUE;
}


BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD ul_reason_for_call, LPVOID /*lpReserved*/)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		Load();
		break;
	case DLL_PROCESS_DETACH:
		Unload();
		break;
	}
	return TRUE;
}
