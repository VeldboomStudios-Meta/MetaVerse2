// Copyright VeldboomStudios 2025

#include "CartManager.h"
#include "Http.h"
#include "ShopConfigLoader.h"
#include "Interfaces/IHttpResponse.h"
#include "Json.h"
#include "JsonUtilities.h"


// Constructor
UCartManager::UCartManager()
    : ConfigLoader(ShopConfigLoader::Get())
{

}

UCartManager& UCartManager::Get()
{
    static UCartManager Instance;
    return Instance;
}

UCartManager::~UCartManager()
{
}

// Helper Functions

FString UCartManager::BuildGraphQLPayload(const FString& Query, TSharedPtr<FJsonObject> Variables){
    TSharedPtr<FJsonObject> RequestJson = MakeShareable(new FJsonObject());
    RequestJson->SetStringField(TEXT("query"), Query);
    RequestJson->SetObjectField(TEXT("variables"), Variables);

    FString RequestBody;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    FJsonSerializer::Serialize(RequestJson.ToSharedRef(), Writer);

    return RequestBody;
}

TSharedRef<IHttpRequest> UCartManager::SetupHttpRequest(const FString& ApiLink, const FString& AccessToken, const FString& RequestBody){
    TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetURL(ApiLink);
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetHeader(TEXT("X-Shopify-Storefront-Access-Token"), AccessToken);
    HttpRequest->SetContentAsString(RequestBody);
    return HttpRequest;
}

TSharedPtr<FJsonObject> UCartManager::ParseGraphQLResponse(const FString& ResponseStr){
    TSharedPtr<FJsonObject> JsonResponse;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseStr);
    if (FJsonSerializer::Deserialize(Reader, JsonResponse) && JsonResponse.IsValid())
    {
        // Log any top-level errors
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

        // Return the "data" object if it exists
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

    // Define the GraphQL mutation string
    FString Mutation = TEXT(R"(
        mutation cartCreate($cartInput: CartCreateInput!) {
          cartCreate(input: $cartInput) {
            cart {
              id
              checkoutUrl
              lines(first: 5) {
                edges {
                  node {
                    merchandise {
                      ... on ProductVariant {
                        title
                      }
                    }
                    quantity
                  }
                }
              }
            }
            checkoutUserErrors {
              code
              field
              message
            }
          }
        }
    )");

    // Prepare variables for the mutation
    TSharedPtr<FJsonObject> VariablesJson = MakeShareable(new FJsonObject());
    TSharedPtr<FJsonObject> CartInputJson = MakeShareable(new FJsonObject());
    // Populate CartInputJson as needed, e.g. initial empty cart state, etc.
    VariablesJson->SetObjectField(TEXT("cartInput"), CartInputJson);

    // Build the request body
    FString RequestBody = BuildGraphQLPayload(Mutation, VariablesJson);

    // Set up the HTTP request
    TSharedRef<IHttpRequest> HttpRequest = SetupHttpRequest(ApiLink, AccessToken, RequestBody);

    // Bind the HTTP response handling lambda
    HttpRequest->OnProcessRequestComplete().BindLambda([this, OnCartCreated](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            HandleHttpResponse(Request, Response, bWasSuccessful,
            // OnSuccess lambda
                [this, OnCartCreated](TSharedPtr<FJsonObject> DataObject)
                {
                    TSharedPtr<FJsonObject> CartCreateObj = DataObject->GetObjectField(TEXT("cartCreate"));
                    if (CartCreateObj->HasField(TEXT("checkoutUserErrors")))
                    {
                        TArray<TSharedPtr<FJsonValue>> CheckoutErrors = CartCreateObj->GetArrayField(TEXT("checkoutUserErrors"));
                        if (CheckoutErrors.Num() > 0)
                        {
                            for (auto& ErrorValue : CheckoutErrors)
                            {
                                TSharedPtr<FJsonObject> ErrorObj = ErrorValue->AsObject();
                                FString ErrorMsg = ErrorObj->GetStringField(TEXT("message"));
                                UE_LOG(LogTemp, Error, TEXT("Checkout Error: %s"), *ErrorMsg);
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
                // OnFailure lambda
                [OnCartCreated]()
                {
                    OnCartCreated.ExecuteIfBound();
                }
    );
        });

    // Execute the HTTP request
    HttpRequest->ProcessRequest();
}

void UCartManager::AddItemToCart(FString VariantId, int32 Quantity, FOnItemAdded OnItemAdded)
{
    // Function definition: Add a product variant to the cart
}

void UCartManager::UpdateCartItem(FString LineId, int32 NewQuantity, FOnItemUpdated OnItemUpdated)
{
    // Function definition: Update the quantity of an item in the cart
}

void UCartManager::RemoveItemFromCart(FString LineId, FOnItemRemoved OnItemRemoved)
{
    // Function definition: Remove an item from the cart
}

void UCartManager::GetCartContents(FOnGetCartContents OnCartContentLoaded)
{
    // Function definition: Retrieve the current contents of the cart
}

void UCartManager::ProceedToCheckout(FOnProceedToCheckout OnCheckoutUrlReady)
{
    // Function definition: Retrieve the checkout URL and proceed to checkout
}

void UCartManager::ClearCart(FOnCartCleared OnCartCleared)
{
    // Function definition: Clear all items from the cart
}

bool UCartManager::IsCartEmpty()
{
    // Function definition: Check if the cart is empty
    return false; // Placeholder return value
}

FString UCartManager::GetStoredCartId()
{
    // Function definition: Retrieve the stored cart ID
    return FString(); // Placeholder return value
}

void UCartManager::SetStoredCartId(FString CartId)
{
    // Function definition: Set the stored cart ID
}

void UCartManager::HandleErrors(FString ErrorMessage)
{
    // Function definition: Handle errors during API requests
}