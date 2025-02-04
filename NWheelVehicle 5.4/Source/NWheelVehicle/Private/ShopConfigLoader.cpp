#include "ShopConfigLoader.h"

ShopConfigLoader& ShopConfigLoader::Get()
{
    static ShopConfigLoader Instance;  // This ensures only one instance exists
    return Instance;
}

ShopConfigLoader::ShopConfigLoader()

{
   
    ShopConfigLoader::LoadShopConfig();
}

ShopConfigLoader::~ShopConfigLoader()
{
}

void ShopConfigLoader::LoadShopConfig()
{
    // Check if the config has already been loaded
    if (bIsConfigLoaded)
    {
        return;  // Do nothing if the config has already been loaded
    }

    // Use FPaths::ProjectDir() to dynamically construct the absolute path to the JSON file
    FString FilePath = FPaths::ProjectDir() + TEXT("Intermediate/ProjectFiles/ShopConfig.json");
    UE_LOG(LogTemp, Warning, TEXT("Attempting to load JSON file from: %s"), *FilePath);

    FString JsonString;

    // Attempt to load the JSON file
    if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
    {
        // If loading the file fails, log the error and exit the function
        UE_LOG(LogTemp, Error, TEXT("Failed to load JSON file from path: %s"), *FilePath);
        return;
    }

    // Parse the JSON file
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        // Extract data from the JSON and store it in the member variables
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

        // Log the successful loading of the API key as an example
        //UE_LOG(LogTemp, Warning, TEXT("API Key loaded successfully: %s"), *ApiKey);

        // Mark the config as loaded to prevent reloading
        bIsConfigLoaded = true;
    }
    else
    {
        // Log an error if JSON parsing fails
        UE_LOG(LogTemp, Error, TEXT("Failed to parse JSON file."));
    }
}
