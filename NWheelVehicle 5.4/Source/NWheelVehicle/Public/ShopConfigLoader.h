#pragma once

#include "CoreMinimal.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Json.h"

class NWHEELVEHICLE_API ShopConfigLoader  
{
public:
    
	// Singleton access method
	static ShopConfigLoader& Get();

	// Delete the copy constructor and assignment operator to enforce singleton
	ShopConfigLoader(ShopConfigLoader const&) = delete;
	void operator=(ShopConfigLoader const&) = delete;

	// Function to load the shop configuration
	void LoadShopConfig();

	// Getter functions for specific fields
	FString GetApiKey() const { return ApiKey; }
	FString GetAdminAccessToken() const { return AdminAccessToken; }
	FString GetStorefrontAccessToken() const { return StorefrontAccessToken; }
	FString GetStorefrontApiLink() const { return StorefrontApiLink; }
	FString GetAdminApiLink() const { return AdminApiLink; }

private:
	ShopConfigLoader();  // Private constructor
	~ShopConfigLoader();
   
	// Variables to store the data from the JSON file
	FString ApiKey;
	FString AdminAccessToken;
	FString StorefrontAccessToken;
	FString StorefrontApiLink;
	FString AdminApiLink;

	bool bIsConfigLoaded = false;
};
