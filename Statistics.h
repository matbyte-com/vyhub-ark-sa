#pragma once

#include "VyHub.h"
#include "Api.h"
#include "User.h"

#include <API/ARK/Ark.h>

#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <json.hpp>

#pragma comment(lib, "AsaApi.lib")

namespace Statistic
{
	inline std::string checkDefintion()
	{
		if (VyHub::playtimeDefinitionId != "")
		{
			return VyHub::playtimeDefinitionId;
		}
		else
		{
			std::unique_ptr<Entity::Definition> definition = NULL;
			definition = Api::getPlaytimeDefinition();
			if (definition)
			{
				// Definition available
				VyHub::playtimeDefinitionId = definition->id;
				return VyHub::playtimeDefinitionId;
			}
			else
			{
				// No definition available, needs to be created
				Log::GetLog()->error("Could not find playtime definition, creating...");
				std::string new_id = Api::createPlaytimeDefinition();
				if (new_id != "")
				{
					VyHub::playtimeDefinitionId = new_id;
					return new_id;
				}
				else
				{
					Log::GetLog()->error("Could not create playtime definition, please check your configuration");
					return "";
				}
			}
		}
	}

	inline void sendPlayerTime()
	{
		std::string definitionId = checkDefintion();
		if (definitionId != "")
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
			file.close();

			for (auto& entry : j.items())
			{
				std::string playerID = entry.key();
				std::unique_ptr<Entity::VyHubUser> user = User::getUser(playerID);
				if (!user)
				{
					continue;
				}

				double hours = std::round((entry.value().get<double>() / 60) * 100.0) / 100.0;

				if (hours < 0.1)
				{
					continue;
				}

				std::string serverbundleID = static_cast<std::string>(VyHub::config["serverbundle_id"]);

				nlohmann::json values = {
					{"definition_id", definitionId},
					{"user_id", user->id},
					{"serverbundle_id", serverbundleID},
					{"value", hours}
				};

				boolean r = Api::updatePlaytimeDefinition(values);

				if (!r)
				{
					// Something went wrong.
					continue;
				}

				// Reset player time
				j[playerID] = 0.0;
			}

			// Save the modified JSON back to the file
			std::ofstream o(config_path);
			o << std::setw(4) << j << std::endl;
		}
	}
}