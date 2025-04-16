#include "ProductManager.h"
#include "Http.h"
#include "Interfaces/IHttpResponse.h"

UProductManager* UProductManager::Instance = nullptr;

UProductManager::UProductManager()
    : ConfigLoader(&ShopConfigLoader::Get())  
{
    if (!ConfigLoader)
    {
        UE_LOG(LogTemp, Error, TEXT("CRITICAL: ShopConfigLoader::Get() returned null!"));
    }
}

UProductManager* UProductManager::GetProductManagerInstance()
{
    if (!Instance || !Instance->IsValidLowLevelFast()) // Ensure instance is valid
    {
        Instance = NewObject<UProductManager>();
        Instance->AddToRoot(); // Prevent garbage collection
    }
    return Instance;
}

void UProductManager::GetAllProducts(FOnProductsFetched OnProductsFetched)
{
    if (!UProductManager::GetProductManagerInstance()) 
    {
        UE_LOG(LogTemp, Error, TEXT("ProductManager not initialized!"));
        return;
    }
    if (!ConfigLoader)
    {
        UE_LOG(LogTemp, Error, TEXT("ConfigLoader is null! Cannot fetch API credentials."));
        return;
    }

    FString ApiLink = ConfigLoader->GetAdminApiLink();
    FString AdminToken = ConfigLoader->GetAdminAccessToken();

    if (ApiLink.IsEmpty() || AdminToken.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("API Link or Admin Token is missing. Please check configuration."));
        return;
    }

    const FString Endpoint = ApiLink + TEXT("/products.json");

    UE_LOG(LogTemp, Log, TEXT("Fetching products from Shopify: %s"), *Endpoint);

    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Endpoint);
    Request->SetVerb("GET");
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("X-Shopify-Access-Token"), AdminToken);

    TWeakObjectPtr<UProductManager> WeakThis = this;

    Request->OnProcessRequestComplete().BindLambda(
        [WeakThis, OnProductsFetched](FHttpRequestPtr, const FHttpResponsePtr& Response, bool bWasSuccessful)
        {
            if (!WeakThis.IsValid()) return; 

            if (bWasSuccessful && Response.IsValid())
            {
                UE_LOG(LogTemp, Log, TEXT("Successfully fetched products from Shopify."));
                WeakThis->ProcessProductsResponse(Response, OnProductsFetched);
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("Failed to fetch products from Shopify."));
            }
        });

    Request->ProcessRequest();
}

void UProductManager::GetProductDetailsById(const FString& ProductId, FOnProductDetailsFetched OnProductDetailsFetched)
{
    // Validate dependencies
    if (!Instance) {
        UE_LOG(LogTemp, Error, TEXT("ProductManager instance is null!"));
        return;
    }
    
    if (!ConfigLoader) {
        UE_LOG(LogTemp, Error, TEXT("ConfigLoader is null! Cannot fetch API credentials."));
        return;
    }

    // Get API configuration
    FString ApiLink = ConfigLoader->GetAdminApiLink();
    FString AdminToken = ConfigLoader->GetAdminAccessToken();
    
    if (ApiLink.IsEmpty() || AdminToken.IsEmpty()) {
        UE_LOG(LogTemp, Error, TEXT("API Link or Admin Token is missing. Please check configuration."));
        return;
    }

    // Setup HTTP request
    FString Endpoint = FString::Printf(TEXT("%s/products/%s.json"), *ApiLink, *ProductId);
    UE_LOG(LogTemp, Display, TEXT("Fetching product details from: %s"), *Endpoint);

    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Endpoint);
    Request->SetVerb("GET");
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("X-Shopify-Access-Token"), AdminToken);

    Request->OnProcessRequestComplete().BindLambda(
        [OnProductDetailsFetched](FHttpRequestPtr, const FHttpResponsePtr& Response, bool bWasSuccessful)
        {
            // Handle request failure
            if (!bWasSuccessful || !Response.IsValid()) {
                UE_LOG(LogTemp, Error, TEXT("HTTP request failed with code %d"), 
                    Response.IsValid() ? Response->GetResponseCode() : 0);
                return;
            }

            // Handle non-200 responses
            if (Response->GetResponseCode() != 200) {
                UE_LOG(LogTemp, Error, TEXT("HTTP error: %d - %s"), 
                    Response->GetResponseCode(), *Response->GetContentAsString());
                return;
            }

            // Parse JSON response
            TSharedPtr<FJsonObject> JsonObject;
            TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(Response->GetContentAsString());
            
            if (!FJsonSerializer::Deserialize(JsonReader, JsonObject) || !JsonObject.IsValid()) {
                UE_LOG(LogTemp, Error, TEXT("Failed to parse JSON response"));
                return;
            }

            // Extract product details
            FProductDetails ProductDetails;
            if (JsonObject->HasField(TEXT("product"))) {
                TSharedPtr<FJsonObject> ProductObj = JsonObject->GetObjectField(TEXT("product"));
                
                ProductDetails.ProductId = ProductObj->GetStringField(TEXT("id"));
                ProductDetails.Title = ProductObj->GetStringField(TEXT("title"));

                // Process variants
                if (const TArray<TSharedPtr<FJsonValue>>* VariantsArray = nullptr; ProductObj->TryGetArrayField(TEXT("variants"), VariantsArray)) {
                    for (const auto& Variant : *VariantsArray) {
                        if (Variant.IsValid() && Variant->Type == EJson::Object) {
                            TSharedPtr<FJsonObject> VariantObj = Variant->AsObject();
                            FProductVariant NewVariant;
                            
                            if (VariantObj->HasField(TEXT("id"))) 
                                NewVariant.VariantId = VariantObj->GetStringField(TEXT("id"));
                            if (VariantObj->HasField(TEXT("title")))
                                NewVariant.Title = VariantObj->GetStringField(TEXT("title"));
                            if (VariantObj->HasField(TEXT("price")))
                                NewVariant.Price = FCString::Atof(*VariantObj->GetStringField(TEXT("price")));
                            
                            ProductDetails.Variants.Add(NewVariant);
                        }
                    }
                    
                    if (ProductDetails.Variants.Num() > 0) {
                        ProductDetails.Price = ProductDetails.Variants[0].Price;
                    }
                }
            }

            // Log results
      //      UE_LOG(LogTemp, Display, TEXT("Product Details Output:\nID: %s\nTitle: %s\nPrice: %f\nVariants (%d):"), 
      //          *ProductDetails.ProductId, *ProductDetails.Title, ProductDetails.Price, ProductDetails.Variants.Num());
            
          // for (const FProductVariant& Variant : ProductDetails.Variants) {
         //       UE_LOG(LogTemp, Display, TEXT("  - %s (ID: %s, Price: %f)"), *Variant.Title, *Variant.VariantId, Variant.Price);
          //  }

            // Execute callback
            if (OnProductDetailsFetched.IsBound()) {
                if (IsInGameThread()) {
                    OnProductDetailsFetched.Execute(ProductDetails);
                } else {
                    AsyncTask(ENamedThreads::GameThread, [OnProductDetailsFetched, ProductDetails]() {
                        (void)OnProductDetailsFetched.ExecuteIfBound(ProductDetails);
                    });
                }
            }
        }
    );

    Request->ProcessRequest();
}

bool UProductManager::IsProductsFetched() const
{
    return bProductsFetched; // Returns whether the products have been fetched or not
}

void UProductManager::ProcessProductsResponse(const FHttpResponsePtr& Response, const FOnProductsFetched& OnProductsFetched)
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
         (void)OnProductsFetched.ExecuteIfBound();  // The callback is now passed by reference
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to deserialize the product response."));
    }
}

