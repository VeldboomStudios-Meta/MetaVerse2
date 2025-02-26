#include "ShopConfigLoader.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "JsonUtilities.h"
#include "Engine/Engine.h"

ShopConfigLoader& ShopConfigLoader::Get()
{
    static ShopConfigLoader Instance;
    return Instance;
}

ShopConfigLoader::ShopConfigLoader()
    : bIsConfigLoaded(false)  // Initialize config flag
{
    LoadShopConfig();
}

ShopConfigLoader::~ShopConfigLoader()
{
}

void ShopConfigLoader::LoadShopConfig()
{
    // Prevent loading more than once
    if (bIsConfigLoaded)
    {
        UE_LOG(LogTemp, Warning, TEXT("ShopConfigLoader: Config already loaded, skipping."));
        return;
    }

    // Construct absolute path using FPaths::Combine
    FString FilePath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Intermediate/ProjectFiles/ShopConfig.json"));
    UE_LOG(LogTemp, Warning, TEXT("ShopConfigLoader: Attempting to load JSON file from: %s"), *FilePath);

    FString JsonString;

    // Attempt to load the JSON file
    if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
    {
        UE_LOG(LogTemp, Error, TEXT("ShopConfigLoader: Failed to load JSON file from path: %s"), *FilePath);
        return;
    }

    // Parse the JSON file
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("ShopConfigLoader: Failed to parse JSON file."));
        return;
    }

    // Extract data from JSON and store in member variables
    if (JsonObject->HasTypedField<EJson::String>(TEXT("api_key")))
    {
        ApiKey = JsonObject->GetStringField(TEXT("api_key"));
    }

    if (JsonObject->HasTypedField<EJson::String>(TEXT("admin_access_token")))
    {
        AdminAccessToken = JsonObject->GetStringField(TEXT("admin_access_token"));
    }

    if (JsonObject->HasTypedField<EJson::String>(TEXT("storefront_access_token")))
    {
        StorefrontAccessToken = JsonObject->GetStringField(TEXT("storefront_access_token"));
    }

    if (JsonObject->HasTypedField<EJson::String>(TEXT("storefront_api_link")))
    {
        StorefrontApiLink = JsonObject->GetStringField(TEXT("storefront_api_link"));
    }

    if (JsonObject->HasTypedField<EJson::String>(TEXT("admin_api_link")))
    {
        AdminApiLink = JsonObject->GetStringField(TEXT("admin_api_link"));
    }

    // Mark config as loaded
    bIsConfigLoaded = true;

    // Debug log
    UE_LOG(LogTemp, Warning, TEXT("ShopConfigLoader: Config loaded successfully."));

    // Display message on screen if in an Unreal Engine game/editor
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("ShopConfig Loaded Successfully"));
    }
}
