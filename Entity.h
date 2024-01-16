#pragma once

#include <json.hpp>
#include <map>

// The order of the definitions is relevant for compiling (Classes have to be defined before they are used)

namespace Entity
{
	class Reward {
	public:
		Reward(const nlohmann::json& json)
		{
			name = json["name"];
			type = json["type"];
			id = json["id"];
			on_event = json["on_event"];
			once = json["once"];
			
			if (json.find("data") != json.end()) {
				nlohmann::json jsonData = json["data"]; // CHeck if data exists

				for (auto it = jsonData.begin(); it != jsonData.end(); ++it) {
					data[it.key()] = it.value().get<std::string>();
				}
			}
		}
		std::string name, type, id, on_event;
		std::map<std::string, std::string> data;
		bool once;
	};
	class Packet {
	public:
		Packet(const nlohmann::json& json)
		{
			id = json["id"];
			title = json["title"];
		}
		std::string id, title;
	};
	class Purchase {
	public:
		Purchase(const nlohmann::json& json)
		{
			id = json["id"];
			amount_text = json["amount_text"];
		}
		std::string id, amount_text;
	};
	class AppliedPacket {
	public:
		AppliedPacket(const nlohmann::json& json) : id(json["id"]), packet(json["packet"]) {
			auto purchaseField = json.find("purchase");
			if (purchaseField != json.end() && !purchaseField->is_null()) {
				purchase = std::make_unique<Purchase>(*purchaseField); // Purchase exists
			}
			else {
				purchase = nullptr; // Assuming Purchase has a default constructor
			}
		}
		std::string id;
		Packet packet;
		std::shared_ptr<Purchase> purchase;
	};
	class AppliedReward {
	public:
		AppliedReward(const nlohmann::json& json): id(json["id"]), applied_packet_id(json["applied_packet_id"]), reward(json["reward"]), applied_packet(json["applied_packet"]) {}
		std::string id, applied_packet_id;
		Reward reward;
		AppliedPacket applied_packet;
	};
	class Server {
	public:
		Server(const nlohmann::json& json)
		{
			id = json["id"];
			name = json["name"];
			serverbundle_id = json["serverbundle_id"];
			extra = json["extra"];
		}
		std::string id, name, serverbundle_id, extra;
	};
	class VyHubUser {
	public:
		VyHubUser(const nlohmann::json& json)
		{
			id = json["id"];
			type = json["type"];
			identifier = json["identifier"];
			registered_on = json["registered_on"];
			username = json["username"];
			avatar = json["avatar"];
		}
		std::string id, type, identifier, registered_on, username, avatar;
	};
	class Definition {
	public:
		Definition(const nlohmann::json& json)
		{
			id = json["id"];
		}
		std::string id;
	};
}
