// Fill out your copyright notice in the Description page of Project Settings.

#include "ProductManager.h"
#include "Http.h"
#include "Interfaces/IHttpResponse.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "ProductActor.h"

ProductManager::ProductManager()
    : ConfigLoader(ShopConfigLoader::Get())
{
    
}

void ProductManager::GetAllProducts(TFunction<void()> OnProductsFetched)
{
    FString  ApiLink = ConfigLoader.GetAdminApiLink();
    FString  AccessToken = ConfigLoader.GetAdminAccessToken();
   
   // UE_LOG(LogTemp, Warning, TEXT("API Link: %s"), *ApiLink);

    // Create the HTTP request to Shopify Admin API to fetch all products
    FString Endpoint = ApiLink + TEXT("/products.json");

    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(Endpoint);
    Request->SetVerb("GET");
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("X-Shopify-Access-Token"), AccessToken);

    Request->OnProcessRequestComplete().BindLambda([this, OnProductsFetched](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            if (bWasSuccessful && Response.IsValid())
            {
                //UE_LOG(LogTemp, Warning, TEXT("Products Fetched: %s"), *Response->GetContentAsString());

                // Process the response and store the products
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

void ProductManager::ProcessProductsResponse(FHttpResponsePtr Response, TFunction<void()> OnProductsFetched)
{
    FString ResponseBody = Response->GetContentAsString();
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);

    if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
    {
        // Check if "products" exists and is an array
         if (JsonObject->HasTypedField<EJson::Array>(TEXT("products")))
        {
            TArray<TSharedPtr<FJsonValue>> ProductsArray = JsonObject->GetArrayField(TEXT("products"));

            for (TSharedPtr<FJsonValue> ProductValue : ProductsArray)
            {
                TSharedPtr<FJsonObject> ProductObject = ProductValue->AsObject();
                FString ProductId = ProductObject->GetStringField(TEXT("id"));

                AllProducts.Add(ProductId, ProductObject);
            }

            bProductsFetched = true;
            OnProductsFetched();
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("No 'products' field found or it's null in the response."));
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to parse products response."));
    }
}

TSharedPtr<FJsonObject> ProductManager::GetProductDetailsById(const FString& ProductId)
{
    if (AllProducts.Contains(ProductId))
    {
        return AllProducts[ProductId];
    }
    return nullptr;
}

bool ProductManager::IsProductsFetched() const
{
    return bProductsFetched;
}