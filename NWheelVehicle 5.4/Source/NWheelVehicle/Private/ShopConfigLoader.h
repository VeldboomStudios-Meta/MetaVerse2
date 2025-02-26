#pragma once

#include "CoreMinimal.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Json.h"

/**
 * Singleton class to load and manage shop configuration from a JSON file.
 */
class NWHEELVEHICLE_API ShopConfigLoader
{
public:
    /** Returns the singleton instance of the ShopConfigLoader. */
    static ShopConfigLoader& Get();

    // Delete copy constructor and assignment operator to enforce singleton pattern
    ShopConfigLoader(const ShopConfigLoader&) = delete;
    void operator=(const ShopConfigLoader&) = delete;

    /** Loads the shop configuration from the JSON file. */
    void LoadShopConfig();

    /** Getters for configuration values */
    FString GetApiKey() const { return ApiKey; }
    FString GetAdminAccessToken() const { return AdminAccessToken; }
    FString GetStorefrontAccessToken() const { return StorefrontAccessToken; }
    FString GetStorefrontApiLink() const { return StorefrontApiLink; }
    FString GetAdminApiLink() const { return AdminApiLink; }

private:
    /** Private constructor for singleton pattern. */
    ShopConfigLoader();

    /** Destructor */
    ~ShopConfigLoader();

    /** Member variables to store configuration data */
    FString ApiKey;
    FString AdminAccessToken;
    FString StorefrontAccessToken;
    FString StorefrontApiLink;
    FString AdminApiLink;

    /** Flag to ensure configuration is loaded only once */
    bool bIsConfigLoaded = false;
};
