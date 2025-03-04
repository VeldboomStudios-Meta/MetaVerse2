#include "CartManager.h"
#include "Http.h"
#include "HttpModule.h"
#include "ShopConfigLoader.h"
#include "Interfaces/IHttpResponse.h"
#include "Json.h"
#include "JsonUtilities.h"

// Revised Singleton Accessor (Meyers' Singleton)
UCartManager& UCartManager::Get()
{
    static UCartManager SingletonInstance;
    return SingletonInstance;
}

UCartManager::UCartManager()
    : ConfigLoader(ShopConfigLoader::Get())  // Initialize with reference from static method
{
}

UCartManager::~UCartManager()
{
}

// Helper Functions

FString UCartManager::BuildGraphQLPayload(const FString& Query, TSharedPtr<FJsonObject> Variables)
{
    TSharedPtr<FJsonObject> RequestJson = MakeShareable(new FJsonObject());
    RequestJson->SetStringField(TEXT("query"), Query);

    if (Variables.IsValid())
    {
        RequestJson->SetObjectField(TEXT("variables"), Variables);
    }

    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

    return RequestBody;
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> UCartManager::SetupHttpRequest(const FString& ApiLink, const FString& AccessToken, const FString& RequestBody)
{
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(ApiLink);
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetHeader(TEXT("X-Shopify-Storefront-Access-Token"), AccessToken);
    HttpRequest->SetContentAsString(RequestBody);
    return HttpRequest;
}

TSharedPtr<FJsonObject> UCartManager::ParseGraphQLResponse(const FString& ResponseStr)
{
    TSharedPtr<FJsonObject> JsonResponse;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseStr);

    if (FJsonSerializer::Deserialize(Reader, JsonResponse) && JsonResponse.IsValid())
    {
        if (JsonResponse->HasField(TEXT("errors")))
        {
            TArray<TSharedPtr<FJsonValue>> Errors = JsonResponse->GetArrayField(TEXT("errors"));
            for (auto& ErrorValue : Errors)
            {
                TSharedPtr<FJsonObject> ErrorObject = ErrorValue->AsObject();
                FString ErrorMessage = ErrorObject->GetStringField(TEXT("message"));
                UE_LOG(LogTemp, Error, TEXT("GraphQL Error: %s"), *ErrorMessage);
            }
            return nullptr;
        }

        if (JsonResponse->HasField(TEXT("data")))
        {
            return JsonResponse->GetObjectField(TEXT("data"));
        }
    }

    return nullptr;
}

void UCartManager::HandleHttpResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful,
    TFunction<void(TSharedPtr<FJsonObject> DataObject)> OnSuccess,
    TFunction<void()> OnFailure)
{
    if (!bWasSuccessful || !Response.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("HTTP request failed or response is invalid."));
        OnFailure();
        return;
    }

    FString ResponseStr = Response->GetContentAsString();
    UE_LOG(LogTemp, Log, TEXT("Response: %s"), *ResponseStr);

    TSharedPtr<FJsonObject> DataObject = ParseGraphQLResponse(ResponseStr);
    if (DataObject.IsValid())
    {
        OnSuccess(DataObject);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Failed to parse JSON response or GraphQL errors occurred."));
        OnFailure();
    }
}

// Main Function Implementations

void UCartManager::CreateShopifyCart(FOnCartCreated OnCartCreated)
{
    if (!StoredCartId.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("A cart already exists with ID: %s. Cannot create a new cart."), *StoredCartId);
        OnCartCreated.ExecuteIfBound();
        return;
    }

    FString ApiLink = ConfigLoader.GetStorefrontApiLink();
    FString AccessToken = ConfigLoader.GetStorefrontAccessToken();

    FString Mutation = TEXT(R"(
        mutation cartCreate($cartInput: CartCreateInput!) {
          cartCreate(input: $cartInput) {
            cart {
              id
              checkoutUrl
            }
            userErrors {
              field
              message
            }
          }
        }
    )");

    TSharedPtr<FJsonObject> VariablesJson = MakeShareable(new FJsonObject());
    TSharedPtr<FJsonObject> CartInputJson = MakeShareable(new FJsonObject());
    VariablesJson->SetObjectField(TEXT("cartInput"), CartInputJson);

    FString RequestBody = BuildGraphQLPayload(Mutation, VariablesJson);
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = SetupHttpRequest(ApiLink, AccessToken, RequestBody);

    HttpRequest->OnProcessRequestComplete().BindLambda([this, OnCartCreated](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            HandleHttpResponse(Request, Response, bWasSuccessful,
                [this, OnCartCreated](TSharedPtr<FJsonObject> DataObject)
                {
                    TSharedPtr<FJsonObject> CartCreateObj = DataObject->GetObjectField(TEXT("cartCreate"));

                    if (CartCreateObj->HasField(TEXT("userErrors")))
                    {
                        TArray<TSharedPtr<FJsonValue>> UserErrors = CartCreateObj->GetArrayField(TEXT("userErrors"));
                        if (UserErrors.Num() > 0)
                        {
                            for (auto& ErrorValue : UserErrors)
                            {
                                TSharedPtr<FJsonObject> ErrorObj = ErrorValue->AsObject();
                                FString ErrorMsg = ErrorObj->GetStringField(TEXT("message"));
                                UE_LOG(LogTemp, Error, TEXT("User Error: %s"), *ErrorMsg);
                            }
                            OnCartCreated.ExecuteIfBound();
                            return;
                        }
                    }

                    TSharedPtr<FJsonObject> CartObject = CartCreateObj->GetObjectField(TEXT("cart"));
                    if (CartObject.IsValid() && CartObject->HasField(TEXT("id")))
                    {
                        FString CartId = CartObject->GetStringField(TEXT("id"));
                        StoredCartId = CartId;
                        UE_LOG(LogTemp, Log, TEXT("Cart created successfully with ID: %s"), *CartId);
                    }
                    else
                    {
                        UE_LOG(LogTemp, Error, TEXT("Cart object not found in response."));
                    }
                    OnCartCreated.ExecuteIfBound();
                },
                [OnCartCreated]()
                {
                    OnCartCreated.ExecuteIfBound();
                }
            );
        });

    HttpRequest->ProcessRequest();
}

void UCartManager::AddItemToCart(FString VariantId, int32 Quantity, FOnItemAdded OnItemAdded)
{
    if (StoredCartId.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("No active cart found. Please create a cart first."));
        OnItemAdded.ExecuteIfBound(false);
        return;
    }

    FString ApiLink = ConfigLoader.GetStorefrontApiLink();
    FString AccessToken = ConfigLoader.GetStorefrontAccessToken();

    FString Mutation = TEXT(R"(
        mutation cartLinesAdd($cartId: ID!, $lines: [CartLineInput!]!) {
          cartLinesAdd(cartId: $cartId, lines: $lines) {
            cart {
              id
            }
            userErrors {
              field
              message
            }
          }
        }
    )");

    TSharedPtr<FJsonObject> VariablesJson = MakeShareable(new FJsonObject());
    VariablesJson->SetStringField(TEXT("cartId"), StoredCartId);

    TArray<TSharedPtr<FJsonValue>> LinesArray;
    TSharedPtr<FJsonObject> LineInput = MakeShareable(new FJsonObject());
    LineInput->SetStringField(TEXT("merchandiseId"), VariantId);
    LineInput->SetNumberField(TEXT("quantity"), Quantity);
    LinesArray.Add(MakeShareable(new FJsonValueObject(LineInput)));

    VariablesJson->SetArrayField(TEXT("lines"), LinesArray);

    FString RequestBody = BuildGraphQLPayload(Mutation, VariablesJson);
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = SetupHttpRequest(ApiLink, AccessToken, RequestBody);

    HttpRequest->OnProcessRequestComplete().BindLambda([OnItemAdded](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            HandleHttpResponse(Request, Response, bWasSuccessful,
                [OnItemAdded](TSharedPtr<FJsonObject> DataObject)
                {
                    TSharedPtr<FJsonObject> CartLinesAddObj = DataObject->GetObjectField(TEXT("cartLinesAdd"));

                    if (CartLinesAddObj->HasField(TEXT("userErrors")))
                    {
                        TArray<TSharedPtr<FJsonValue>> UserErrors = CartLinesAddObj->GetArrayField(TEXT("userErrors"));
                        if (UserErrors.Num() > 0)
                        {
                            for (auto& ErrorValue : UserErrors)
                            {
                                TSharedPtr<FJsonObject> ErrorObj = ErrorValue->AsObject();
                                FString ErrorMsg = ErrorObj->GetStringField(TEXT("message"));
                                UE_LOG(LogTemp, Error, TEXT("User Error: %s"), *ErrorMsg);
                            }
                            OnItemAdded.ExecuteIfBound(false);
                            return;
                        }
                    }

                    OnItemAdded.ExecuteIfBound(true);
                },
                [OnItemAdded]()
                {
                    OnItemAdded.ExecuteIfBound(false);
                }
            );
        });

    HttpRequest->ProcessRequest();
}

void UCartManager::UpdateCartItem(FString LineId, int32 NewQuantity, FOnItemUpdated OnItemUpdated)
{
    if (StoredCartId.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("No active cart found. Please create a cart first."));
        OnItemUpdated.ExecuteIfBound(false);
        return;
    }

    // Placeholder implementation: Update logic here
    OnItemUpdated.ExecuteIfBound(false);
}

void UCartManager::RemoveItemFromCart(FString LineId, FOnItemRemoved OnItemRemoved)
{
    if (StoredCartId.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("No active cart found. Please create a cart first."));
        OnItemRemoved.ExecuteIfBound(false);
        return;
    }

    // Placeholder implementation: Remove logic here
    OnItemRemoved.ExecuteIfBound(false);
}

void UCartManager::GetCartContents(FOnGetCartContents OnCartContentLoaded)
{
    if (StoredCartId.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("No active cart found. Please create a cart first."));
        OnCartContentLoaded.ExecuteIfBound(TArray<FString>());
        return;
    }

    // Placeholder implementation: Retrieve cart contents logic here
    OnCartContentLoaded.ExecuteIfBound(TArray<FString>());
}

void UCartManager::ProceedToCheckout(FOnProceedToCheckout OnCheckoutUrlReady)
{
    if (StoredCartId.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("No active cart found. Please create a cart first."));
        OnCheckoutUrlReady.ExecuteIfBound(FString());
        return;
    }

    // Placeholder implementation: Retrieve checkout URL logic here
    OnCheckoutUrlReady.ExecuteIfBound(FString());
}

void UCartManager::ClearCart(FOnCartCleared OnCartCleared)
{
    if (StoredCartId.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("No active cart found. Please create a cart first."));
        OnCartCleared.ExecuteIfBound(false);
        return;
    }

    // Placeholder implementation: Clear cart logic here
    OnCartCleared.ExecuteIfBound(false);
}

bool UCartManager::IsCartEmpty()
{
    // Placeholder implementation: Check if cart is empty
    return StoredCartId.IsEmpty();
}

FString UCartManager::GetStoredCartId()
{
    return StoredCartId;
}

void UCartManager::SetStoredCartId(FString CartId)
{
    StoredCartId = CartId;
}

void UCartManager::HandleErrors(FString ErrorMessage)
{
    UE_LOG(LogTemp, Error, TEXT("Error: %s"), *ErrorMessage);
}