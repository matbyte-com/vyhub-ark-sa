#pragma once

#include "VyHub.h"
#include "Api.h"

#include <API/ARK/Ark.h>

#include <fstream>
#include <Timer.h>
#include <string>
#include <map>
#include <vector>
#include <json.hpp>
#include <mutex>

#pragma comment(lib, "AsaApi.lib")

namespace Reward
{
	std::mutex executeRewardMutex;
	inline void sendExecuted()
	{
		for (auto it = VyHub::executedRewards.begin(); it != VyHub::executedRewards.end();) {
			const std::string& reward_id = *it;

			if (reward_id == "")
			{
				Log::GetLog()->info("Empty Reward ID");
				VyHub::executedRewards.erase(it);
				continue;
			}

			// Call the sendExecutedReward function
			if (Api::sendExecutedReward(reward_id)) {
				// If the function returns true, erase the element from the vector
				VyHub::executedAndSentRewards.push_back(reward_id);
				VyHub::executedRewards.erase(it);
			}
			else {
				++it;
			}
		}
	}

	inline std::string replaceVariables(std::string command, AShooterPlayerController* player, Entity::AppliedReward* appliedReward)
	{
		std::string vyhubUserId = VyHub::users.at(TCHAR_TO_UTF8(*AsaApi::GetApiUtils().GetEOSIDFromController(player))).id;
		std::string newString = command;
		newString = std::regex_replace(newString, std::regex("%nick%"), TCHAR_TO_UTF8(*AsaApi::GetApiUtils().GetCharacterName(player)));
		newString = std::regex_replace(newString, std::regex("%user_id%"), vyhubUserId);
		newString = std::regex_replace(newString, std::regex("%applied_packet_id%"), appliedReward->applied_packet_id);
		newString = std::regex_replace(newString, std::regex("%player_id%"), TCHAR_TO_UTF8(*AsaApi::GetApiUtils().GetEOSIDFromController(player)));

		if (appliedReward->applied_packet.purchase)
		{
			newString = std::regex_replace(newString, std::regex("%purchase_amount%"), appliedReward->applied_packet.purchase->amount_text);
		}

		newString = std::regex_replace(newString, std::regex("%packet_title%"), appliedReward->applied_packet.packet.title);
		return newString;
	}

	inline bool dispatchCommand(std::string command)
	{
		APlayerController* firstPlayer = AsaApi::GetApiUtils().GetWorld()->GetFirstPlayerController();
		if (!firstPlayer)
		{
			Log::GetLog()->info("Reward No Player");
			return false;
		}
		else
		{
			FString result;
			const FString commandString = command.c_str();
			firstPlayer->ConsoleCommand(&result, &commandString, true);
		}
		return true;
	}

	inline void executeReward(std::vector<std::string> events, std::optional<std::string> playerID)
	{
		std::lock_guard<std::mutex>lock(executeRewardMutex); // is automatically released when out of scope

		if (VyHub::rewards == NULL)
		{
			return;
		}

		std::unique_ptr<std::map<std::string, std::vector<Entity::AppliedReward>>> rewardsByPlayer = std::make_unique<std::map<std::string, std::vector<Entity::AppliedReward>>>(*VyHub::rewards);

		if (!playerID.has_value())
		{
			for (const std::string& event : events) {
				if (event != "DIRECT" && event != "DISABLE") {
					throw std::runtime_error("Invalid event");
				}
			}
		}
		else
		{
			rewardsByPlayer = std::make_unique<std::map<std::string, std::vector<Entity::AppliedReward>>>();

			if (VyHub::rewards->find(playerID.value()) != VyHub::rewards->end())
			{
				rewardsByPlayer->insert({ playerID.value(), VyHub::rewards->at(playerID.value()) });
			}
			else
			{
				return;
			}
		}

		for (auto it = rewardsByPlayer->begin(); it != rewardsByPlayer->end(); ++it)
		{
			std::string playerId = it->first;
			std::vector<Entity::AppliedReward> appliedRewards = it->second;

			AShooterPlayerController* player = nullptr;
			player = AsaApi::GetApiUtils().FindPlayerFromEOSID(playerId.c_str());
			if (!player)
			{
				// The player is not online to execute
				continue;
			}

			for (Entity::AppliedReward& appliedReward : appliedRewards)
			{
				Log::GetLog()->info(appliedReward.id);
				if (std::find(VyHub::executedRewards.begin(), VyHub::executedRewards.end(), appliedReward.id) != VyHub::executedRewards.end())
				{
					// executed Rewards already contains the reward id
					continue;
				}
				if (std::find(VyHub::executedAndSentRewards.begin(), VyHub::executedAndSentRewards.end(), appliedReward.id) != VyHub::executedAndSentRewards.end())
				{
					// executedAndSentRewards already contains the reward id
					continue;
				}

				const Entity::Reward& reward = appliedReward.reward;
				if (std::find(events.begin(), events.end(), reward.on_event) != events.end())
				{
					bool success = true;
					if (reward.type == "COMMAND")
					{
						// Search for the key "command" in the map
						auto it = appliedReward.reward.data.find("command");
						if (it != appliedReward.reward.data.end()) {
							try
							{
								std::string command = replaceVariables(it->second, player, &appliedReward);
								success = dispatchCommand(command);
							}
							catch (const std::exception& ex)
							{
								Log::GetLog()->warn("Error while executing command {}", ex.what());
							}
						}
						else {
							// Key "command" does not exist in the map
							Log::GetLog()->warn("Key 'command' not found in reward data");
							success = false;
						}
					}
					else {
						success = false;
						Log::GetLog()->warn("No implementation for Reward Type: " + reward.type);
						return;
					}

					if (reward.once && success)
					{
						VyHub::executedRewards.push_back(appliedReward.id);
						// Remove reward from the Rewards map to prevent double execution (also when the reward is already sent back to the API)
					}

					if (success) {
						const FString& player_name = AsaApi::GetApiUtils().GetCharacterName(player);
						Log::GetLog()->info("Reward executed: " + reward.name + " for player: " + TCHAR_TO_UTF8(*player_name));
					}
				}

			}
		}
		sendExecuted();
	}
}
