#pragma once

#include <map>
#include "Entity.h"
#include "VyHub.h"
#include "Api.h"


#include <API/ARK/Ark.h>
#pragma comment(lib, "AsaApi.lib")


namespace User
{
	inline std::unique_ptr<Entity::VyHubUser> getUser(std::string eosId, std::optional<std::string> username)
	{
		// Find the VyHubUser in the map based on the user ID
		auto userIterator = VyHub::users.find(eosId);

		if (userIterator != VyHub::users.end())
		{
			// return found user
			const Entity::VyHubUser& user = userIterator->second;
			return std::make_unique<Entity::VyHubUser>(user);
		}
		else {
			// user not found, query user from API
			std::unique_ptr<Entity::VyHubUser> vyhub_user = Api::getUser(eosId);

			if (!vyhub_user)
			{
				// user not found, create user
				vyhub_user = Api::createUser(eosId, username);

				if (!vyhub_user)
				{
					// User creation failed
					return NULL;
				}
			}
			return vyhub_user;
		}
	}


	inline std::unique_ptr<Entity::VyHubUser> getUser(std::string eosId)
	{
		return getUser(eosId, std::nullopt);
	}

	inline std::unique_ptr<Entity::VyHubUser> getUser(FString eosId)
	{
		return getUser(std::string(TCHAR_TO_UTF8(*eosId)), std::nullopt);
	}
}