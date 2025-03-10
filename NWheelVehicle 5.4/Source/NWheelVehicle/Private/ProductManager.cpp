#include "ProductManager.h"
#include "Http.h"
#include "Interfaces/IHttpResponse.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "ProductActor.h" 

UProductManager* UProductManager::Instance = nullptr;

UProductManager::UProductManager()
    : ConfigLoader(&ShopConfigLoader::Get())  // Get a pointer by using the address-of operator
{
    // If ConfigLoader is null, handle it (Optional for safety)
    if (!ConfigLoader)
    {
        UE_LOG(LogTemp, Error, TEXT("ConfigLoader is not initialized correctly."));
    }
}

UProductManager::~UProductManager()
{
}

UProductManager* UProductManager::GetProductManagerInstance()
{
    if (!Instance)
    {
        Instance = NewObject<UProductManager>();
    }
    return Instance;
}

void UProductManager::GetAllProducts(FOnProductsFetched OnProductsFetched)
{
    // Get the API link and Access token from the configuration loader
    FString ApiLink = ConfigLoader->GetStorefrontApiLink();
    FString AccessToken = ConfigLoader->GetStorefrontAccessToken();

    // Define the Shopify endpoint
    FString Endpoint = ApiLink + TEXT("/products.json");

    // Create the HTTP request to Shopify Admin API to fetch all products
    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Endpoint);
    Request->SetVerb("GET");
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("X-Shopify-Access-Token"), AccessToken);

    // Bind the callback when the request is completed
    Request->OnProcessRequestComplete().BindLambda([this, OnProductsFetched](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            if (bWasSuccessful && Response.IsValid())
            {
                // Process the response and call the callback function
                ProcessProductsResponse(Response, OnProductsFetched);
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("Failed to fetch products from Shopify."));
            }
        });

    // Send the request
    Request->ProcessRequest();
}

FString UProductManager::GetProductDetailsById(const FString& ProductId)
{
    FString ApiLink = ConfigLoader->GetStorefrontApiLink();
    FString AccessToken = ConfigLoader->GetStorefrontAccessToken();

    FString Endpoint = ApiLink + TEXT("/products/") + ProductId + TEXT(".json");

    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Endpoint);
    Request->SetVerb("GET");
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("X-Shopify-Access-Token"), AccessToken);

    FString ProductDetails;

    Request->OnProcessRequestComplete().BindLambda([this, &ProductDetails](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            if (bWasSuccessful && Response.IsValid())
            {
                // Parse response data
                TSharedPtr<FJsonObject> JsonObject;
                TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(Response->GetContentAsString());

                if (FJsonSerializer::Deserialize(JsonReader, JsonObject))
                {
                    ProductDetails = JsonObject->GetStringField(TEXT("product_name")); // Example field, adjust according to your API response
                }
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("Failed to fetch product details from Shopify."));
            }
        });

    // Send the request
    Request->ProcessRequest();

    return ProductDetails;
}

void UProductManager::SetProductDetailsById(AProductActor* ProductActor, const FString& ProductId, FOnProductDetailsFetched /*bool*/ OnProductDetailsFetched) // ❌

{
    // Retrieve product details from AllProducts using ProductId
    TSharedPtr<FJsonObject> ProductData = AllProducts.FindRef(ProductId);

    if (ProductData.IsValid() && ProductActor != nullptr) // Explicit nullptr check
    {
        // Apply the product details to the actor
        ApplyProductDetailsToActor(ProductActor, ProductData, OnProductDetailsFetched);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Product with ID: %s not found or invalid ProductActor."), *ProductId);
    }
}


bool UProductManager::IsProductsFetched() const
{
    return bProductsFetched; // Returns whether the products have been fetched or not
}

void UProductManager::ProcessProductsResponse(FHttpResponsePtr Response, FOnProductsFetched OnProductsFetched)
{
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(Response->GetContentAsString());

    if (FJsonSerializer::Deserialize(JsonReader, JsonObject))
    {
        // Process all the products in the response
        const TArray<TSharedPtr<FJsonValue>> ProductsArray = JsonObject->GetArrayField(TEXT("products"));

        for (const TSharedPtr<FJsonValue>& Value : ProductsArray)
        {
            TSharedPtr<FJsonObject> ProductData = Value->AsObject();

            // Assuming each product has a unique 'id' field
            FString ProductId = ProductData->GetStringField(TEXT("id"));

            // Store product data in AllProducts using ProductId as the key
            AllProducts.Add(ProductId, ProductData);
        }

        // Mark products as fetched and invoke the callback
        bProductsFetched = true;
        OnProductsFetched.ExecuteIfBound(); // Call the callback
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to deserialize the product response."));
    }
}

void UProductManager::ApplyProductDetailsToActor(AProductActor* ProductActor, TSharedPtr<FJsonObject> ProductData, FOnProductDetailsFetched OnProductDetailsFetched)
{
    // Explicitly check for nullptr for ProductActor and ensure ProductData is valid
    if (ProductActor != nullptr && ProductData.IsValid())
    {
        // Example: Apply the product details to the actor
        FString ProductName = ProductData->GetStringField(TEXT("name"));
        float ProductPrice = ProductData->GetNumberField(TEXT("price"));
        FString ProductDescription = ProductData->GetStringField(TEXT("description"));

        // Set properties on the ProductActor using the new generalized Set function
        ProductActor->SetProductDetailString(TEXT("ProductName"), ProductName);
        ProductActor->SetProductDetailFloat(TEXT("ProductPrice"), ProductPrice);
        ProductActor->SetProductDetailString(TEXT("ProductDescription"), ProductDescription);

        // Execute the callback once details are applied
        OnProductDetailsFetched.ExecuteIfBound(ProductActor);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid ProductActor or ProductData."));
    }
}
