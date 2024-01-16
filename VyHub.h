#pragma once

#include <json.hpp>
#include "Entity.h"
#include <map>
#include <vector>

namespace VyHub
{
	inline nlohmann::json config;
	inline std::unique_ptr<std::map<std::string, std::vector<Entity::AppliedReward>>> rewards = std::make_unique<std::map<std::string, std::vector<Entity::AppliedReward>>>();
	inline std::vector<std::string> executedRewards = std::vector<std::string>();
	inline std::vector<std::string> executedAndSentRewards = std::vector<std::string>();
	inline std::map<std::string, Entity::VyHubUser> users = {};
	inline std::string playtimeDefinitionId = "";
}