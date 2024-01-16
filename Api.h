#pragma once

#include <json.hpp>
#include <map>
#include "httplib.h"
#include "Entity.h"
#include "VyHub.h"


#include <API/ARK/Ark.h>
#pragma comment(lib, "AsaApi.lib")


namespace User
{
	inline std::unique_ptr<Entity::VyHubUser> getUser(std::string eosId, std::optional<std::string> username);
	inline std::unique_ptr<Entity::VyHubUser> getUser(std::string eosId);
	inline std::unique_ptr<Entity::VyHubUser> getUser(FString eosId);
}


namespace Api
{
	inline std::string suffix;
	inline httplib::Client getClient()
	{
		std::string api_key = VyHub::config["api_key"];
		std::string api_url = VyHub::config["api_url"];
		std::string server_id = VyHub::config["server_id"];
		suffix = VyHub::config["api_suffix"];

		httplib::Client cli(api_url);
		cli.set_default_headers({ { "Authorization", "Bearer " + api_key}, {"Content-Type", "application/json"} });
		cli.set_follow_location(true);
		return cli;
	}


	inline boolean init()
	{
		httplib::Client cli = getClient();
		Log::GetLog()->info("Client created...");

		auto res = cli.Get(suffix + "/server/" + static_cast<std::string>(VyHub::config["server_id"]));
		Log::GetLog()->info("Init status: {}", httplib::to_string(res.error()));

		if (res.error() != httplib::Error::Success)
		{
			Log::GetLog()->warn("Connection not successful.");
			Log::GetLog()->info("Error: {}", httplib::to_string(res.error()));
			return false;
		}

		if (res->status != 200)
		{
			Log::GetLog()->warn("Connection to VyHub API not successful! Set the credentials again.");
			Log::GetLog()->info(res->body);
			return false;
		}
		Log::GetLog()->info("Connection to VyHub API successful!");
		return true;

	}

	inline std::string confirmAuth(std::string identifier, std::string validation_uuid, std::string username)
	{
		httplib::Client cli = getClient();

		nlohmann::json j;
		j["user_type"] = "ASA";
		j["identifier"] = identifier;
		j["user_data"] = { {"username", username} };

		auto res = cli.Post(suffix + "/auth/request/" + validation_uuid, j.dump(), "application/json");

		if (res.error() != httplib::Error::Success)
		{
			Log::GetLog()->info("API Error: {}", httplib::to_string(res.error()));
			return "VyHub API not available.";
		}


		if (res->status == 400)
		{
			return "Invalid login UUID!";
		}
		else if (res->status != 200)
		{
			return "VyHub API not available!";
			Log::GetLog()->warn("API not available! Error: \n" + res->body);
		}
		else if (res->status == 200)
		{
			return "Login successful!";
		}
		else
		{
			return "VyHub API not available!";
			Log::GetLog()->warn("API not available! Error: \n" + res->body);
		}
	}

	inline std::unique_ptr<Entity::VyHubUser> createUser(std::string identifier, std::optional<std::string> username)
	{
		httplib::Client cli = getClient();

		Log::GetLog()->info("Creating user"); // TODO Remve

		nlohmann::json params;
		params["type"] = "ASA";
		params["identifier"] = identifier;

		if (username.has_value())
		{
			params["username"] = username.value();
		}

		auto res = cli.Post(suffix + "/user/", params.dump(), "application/json");

		if (res.error() != httplib::Error::Success)
		{
			Log::GetLog()->info("API Error: {}", httplib::to_string(res.error()));
			return NULL;
		}


		if (res->status != 200)
		{
			Log::GetLog()->warn("User could not be registered! Error: \n" + res->body);
			return NULL;
		}

		std::unique_ptr<Entity::VyHubUser> vyhub_user = NULL;
		try {
			vyhub_user = std::make_unique<Entity::VyHubUser>(nlohmann::json::parse(res->body));
		}
		catch (const nlohmann::detail::exception& e) {
			Log::GetLog()->warn("Error while parsing user json.");
			Log::GetLog()->warn(e.what());
			return NULL;
		}

		return vyhub_user;
	}

	inline std::unique_ptr<Entity::VyHubUser> getUser(std::string userId)
	{
		httplib::Client cli = getClient();
		auto res = cli.Get(suffix + "/user/" + userId + "?type=ASA");

		if (res.error() != httplib::Error::Success)
		{
			Log::GetLog()->info("API Error: {}", httplib::to_string(res.error()));
			return NULL;
		}


		if (res->status != 200)
		{
			Log::GetLog()->info("User not registered / found!");
			return NULL;
		}

		std::unique_ptr<Entity::VyHubUser> vyhub_user = NULL;
		try {
			vyhub_user = std::make_unique<Entity::VyHubUser>(nlohmann::json::parse(res->body));
		}
		catch (const nlohmann::detail::exception& e) {
			Log::GetLog()->warn("Error while parsing user json.");
			Log::GetLog()->warn(e.what());
			return NULL;
		}

		return vyhub_user;
	}

	inline std::unique_ptr<std::map<std::string, std::vector<Entity::AppliedReward>>> fetchRewards(std::vector<std::string> onlinePlayerIds)
	{
		std::string serverbundle_id = static_cast<std::string>(VyHub::config["serverbundle_id"]);
		std::string server_id = static_cast<std::string>(VyHub::config["server_id"]);
		// Find users that are online
		std::ostringstream stringBuilder;
		for (const std::string& playerId : onlinePlayerIds)
		{
			std::unique_ptr<Entity::VyHubUser> user = User::getUser(playerId);

			if (user)
			{
				stringBuilder << "user_id=" << user->id << "&";
			}
		}
		std::string user_ids = stringBuilder.str();
		if (!user_ids.empty())
		{
			user_ids.pop_back();
		}



		httplib::Client cli = getClient();
		auto res = cli.Get(suffix + "/packet/reward/applied/user?active=true&foreign_ids=true&status=OPEN&serverbundle_id=" + serverbundle_id + "&for_server_id=" + server_id + "&user_ids=" + user_ids);

		if (res.error() != httplib::Error::Success)
		{
			Log::GetLog()->info("API Error: {}", httplib::to_string(res.error()));
			return NULL;
		}


		if (res->status != 200)
		{
			Log::GetLog()->warn("Failed to fetch rewards from API! Error: \n" + res->body);
			return NULL;
		}

		auto rewardsMap = std::make_unique<std::map<std::string, std::vector<Entity::AppliedReward>>>();

		// Parse the JSON response into the map
		try {
			auto jsonMap = nlohmann::json::parse(res->body);
			for (auto it = jsonMap.begin(); it != jsonMap.end(); ++it)
			{
				std::string userId = it.key();
				std::vector<Entity::AppliedReward> appliedRewards = {};

				// Parse the list of AppliedRewards for each user
				for (const auto& rewardJson : it.value())
				{
					Entity::AppliedReward appliedReward = Entity::AppliedReward(rewardJson);
					appliedRewards.push_back(appliedReward);
				}

				// Insert into the map
				(*rewardsMap)[userId] = appliedRewards;
			}
		}
		catch (const nlohmann::detail::exception& e) {
			Log::GetLog()->warn("Error in fetch Rewards (while parsing json).");
			Log::GetLog()->warn(e.what());
			return NULL;
		}

		return rewardsMap;
	}

	inline boolean sendExecutedReward(std::string reward_id)
	{
		httplib::Client cli = getClient();

		nlohmann::json params;
		params["executed_on"] = { static_cast<std::string>(VyHub::config["server_id"]) };

		auto res = cli.Post(suffix + "/packet/reward/applied/" + reward_id, params.dump(), "application/json");

		if (res.error() != httplib::Error::Success)
		{
			Log::GetLog()->info("API Error: {}", httplib::to_string(res.error()));
			return false;
		}

		if (res->status != 200)
		{
			Log::GetLog()->warn("Failed to send executed rewards to API! Error: \n" + res->body);
			return false;
		}
		return true;
	}

	inline boolean patchServer(nlohmann::json params)
	{
		httplib::Client cli = getClient();

		auto res = cli.Post(suffix + "/server/" + static_cast<std::string>(VyHub::config["server_id"]), params.dump(), "application/json");

		if (res.error() != httplib::Error::Success)
		{
			Log::GetLog()->info("API Error: {}", httplib::to_string(res.error()));
			return false;
		}


		if (res->status != 200)
		{
			Log::GetLog()->warn("Failed to update Server Status API! Error: \n" + res->body);
			return false;
		}
		return true;
	}

	inline std::unique_ptr<Entity::Definition> getPlaytimeDefinition()
	{
		httplib::Client cli = getClient();

		auto res = cli.Get(suffix + "/user/attribute/definition/playtime");

		if (res.error() != httplib::Error::Success)
		{
			Log::GetLog()->info("API Error: {}", httplib::to_string(res.error()));
			return NULL;
		}

		if (res->status != 200)
		{
			Log::GetLog()->warn("Failed to get Playtime definition: \n" + res->body);
			return NULL;
		}
		if (res->status == 404)
		{
			return NULL;
		}

		std::unique_ptr<Entity::Definition> definition = NULL;
		try {
			definition = std::make_unique<Entity::Definition>(nlohmann::json::parse(res->body));
		}
		catch (const nlohmann::detail::exception& e) {
			Log::GetLog()->warn("Error while parsing definition json.");
			Log::GetLog()->warn(e.what());
			return NULL;
		}
		return definition;
	}

	inline std::string createPlaytimeDefinition()
	{
		httplib::Client cli = getClient();

		nlohmann::json params;
		params["name"] = "playtime";
		params["title"] = "Play Time";
		params["unit"] = "HOURS";
		params["type"] = "ACCUMULATED";
		params["accumulation_interval"] = "day";
		params["unspecific"] = true;


		auto res = cli.Post(suffix + "/user/attribute/definition", params.dump(), "application/json");

		if (res.error() != httplib::Error::Success)
		{
			Log::GetLog()->info("API Error: {}", httplib::to_string(res.error()));
			return "";
		}

		if (res->status != 200)
		{
			Log::GetLog()->warn("Failed to create Playtime definition: \n" + res->body);
			return "";
		}

		nlohmann::json body;
		try {
			body = nlohmann::json::parse(res->body);
		}
		catch (const nlohmann::detail::exception& e) {
			Log::GetLog()->warn("Error while parsing definition json.");
			Log::GetLog()->warn(e.what());
			return "";
		}
		return body["id"];
	}

	inline boolean updatePlaytimeDefinition(nlohmann::json params)
	{
		httplib::Client cli = getClient();

		auto res = cli.Post(suffix + "/user/attribute/", params.dump(), "application/json");

		if (res.error() != httplib::Error::Success)
		{
			Log::GetLog()->info("API Error: {}", httplib::to_string(res.error()));
			return false;
		}

		if (res->status != 200)
		{
			Log::GetLog()->warn("Failed send playtime: \n" + res->body);
			return false;
		}
		return true;
	}
}