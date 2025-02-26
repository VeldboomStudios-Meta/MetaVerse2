// Copyright VeldboomStudios 2025

#pragma once

#include "CoreMinimal.h"

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h" // For Blueprint compatibility
#include "Delegates/Delegate.h" // Include delegates for callbacks
#include "CartManager.generated.h"

// Custom delegate declarations for Blueprint compatibility
// These must be declared outside the class scope
DECLARE_DYNAMIC_DELEGATE(FOnCartCreated);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnItemAdded, bool, bSuccess);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnItemUpdated, bool, bSuccess);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnItemRemoved, bool, bSuccess);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnCartCleared, bool, bSuccess);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnGetCartContents, TArray<FString>, CartContents);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnProceedToCheckout, FString, CheckoutUrl);

/**
 * @class CartManager
 * @brief Manages interactions with Shopify's Storefront API for cart operations.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class NWHEELVEHICLE_API UCartManager : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Constructor
    UCartManager();
    ~UCartManager();

    static UCartManager& Get();

    /**
     * Creates a new cart in Shopify and stores its ID locally.
     * @param OnCartCreated Delegate executed after the cart is created.
     */
    UFUNCTION(BlueprintCallable, Category = "CartManager")
    static void CreateShopifyCart(FOnCartCreated OnCartCreated)
    {
        if (!StoredCartId.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("A cart already exists with ID: %s. Cannot create a new cart."), *StoredCartId);
            return;
        }

        if (!FHttpModule::Get().IsHttpEnabled())
        {
            UE_LOG(LogTemp, Error, TEXT("HTTP module is not enabled. Cannot create request."));
            return;
        }

        FString GraphQLQuery = R"({"query": "mutation cartCreate($cartInput: CartCreateInput!) { cartCreate(input: $cartInput) { cart { id checkoutUrl } userErrors { field message } } }", "variables": { "cartInput": { "lines": [ { "quantity": 1, "merchandiseId": "gid://shopify/ProductVariant/42567741931576" } ] } } })";

        TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
        Request->SetURL(ConfigLoader.GetStorefrontApiLink());
        Request->SetVerb("POST");
        Request->SetHeader("Content-Type", "application/json");
        Request->SetHeader("X-Shopify-Storefront-Access-Token", ConfigLoader.GetStorefrontAccessToken());
        Request->SetContentAsString(GraphQLQuery);

        Request->OnProcessRequestComplete().BindLambda([OnCartCreated](FHttpRequestPtr, FHttpResponsePtr Response, bool bWasSuccessful)
            {
                if (!bWasSuccessful || !Response.IsValid() || Response->GetResponseCode() != 200)
                {
                    UE_LOG(LogTemp, Error, TEXT("HTTP request failed with response code: %d"), Response.IsValid() ? Response->GetResponseCode() : 0);
                    return;
                }

                FString ResponseBody = Response->GetContentAsString();
                TSharedPtr<FJsonObject> JsonObject;
                if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ResponseBody), JsonObject) || !JsonObject.IsValid())
                {
                    UE_LOG(LogTemp, Error, TEXT("Failed to parse JSON response: %s"), *ResponseBody);
                    return;
                }

                TSharedPtr<FJsonObject> CartCreate = JsonObject->GetObjectField(TEXT("data"))->GetObjectField(TEXT("cartCreate"));
                if (!CartCreate.IsValid())
                {
                    UE_LOG(LogTemp, Error, TEXT("Malformed JSON response: Missing 'cartCreate' field."));
                    return;
                }

                TArray<TSharedPtr<FJsonValue>> UserErrors = CartCreate->GetArrayField(TEXT("userErrors"));
                for (const auto& ErrorValue : UserErrors)
                {
                    TSharedPtr<FJsonObject> ErrorObject = ErrorValue->AsObject();
                    UE_LOG(LogTemp, Error, TEXT("Cart API Error - Field: %s, Message: %s"), *ErrorObject->GetStringField(TEXT("field")), *ErrorObject->GetStringField(TEXT("message")));
                }
                if (UserErrors.Num() > 0) return;

                FString NewCartId;
                if (CartCreate->GetObjectField(TEXT("cart"))->TryGetStringField(TEXT("id"), NewCartId))
                {
                    UE_LOG(LogTemp, Log, TEXT("Successfully created cart with ID: %s"), *NewCartId);
                    if (OnCartCreated.IsBound())
                    {
                        OnCartCreated.Execute(NewCartId);
                    }
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("Malformed JSON response: Missing 'id' field."));
                }
            });

        UE_LOG(LogTemp, Log, TEXT("Sending HTTP request to %s"), *ConfigLoader.GetStorefrontApiLink());
        Request->ProcessRequest();
    }

    /**
     * Adds a product variant to the existing cart.
     * @param VariantId The ID of the product variant to add.
     * @param Quantity The quantity of the product variant to add.
     * @param OnItemAdded Delegate executed after the item is added.
     */
    UFUNCTION(BlueprintCallable, Category = "CartManager")
    static void AddItemToCart(FString VariantId, int32 Quantity, FOnItemAdded OnItemAdded);

    /**
     * Updates the quantity of an existing item in the cart.
     * @param LineId The ID of the cart line representing the item.
     * @param NewQuantity The updated quantity for the item.
     * @param OnItemUpdated Delegate executed after the item is updated.
     */
    UFUNCTION(BlueprintCallable, Category = "CartManager")
    static void UpdateCartItem(FString LineId, int32 NewQuantity, FOnItemUpdated OnItemUpdated);

    /**
     * Removes an item from the cart.
     * @param LineId The ID of the cart line to remove.
     * @param OnItemRemoved Delegate executed after the item is removed.
     */
    UFUNCTION(BlueprintCallable, Category = "CartManager")
    static void RemoveItemFromCart(FString LineId, FOnItemRemoved OnItemRemoved);

    /**
     * Retrieves the current contents of the cart.
     * @param OnCartContentLoaded Delegate to return the cart's contents as an array of strings.
     */
    UFUNCTION(BlueprintCallable, Category = "CartManager")
    static void GetCartContents(FOnGetCartContents OnCartContentLoaded);

    /**
     * Retrieves the checkout URL for the cart and redirects the user to Shopify's checkout page.
     * @param OnCheckoutUrlReady Delegate to return the checkout URL as a string.
     */
    UFUNCTION(BlueprintCallable, Category = "CartManager")
    static void ProceedToCheckout(FOnProceedToCheckout OnCheckoutUrlReady);

    /**
     * Clears all items from the cart.
     * @param OnCartCleared Delegate executed after the cart is cleared.
     */
    UFUNCTION(BlueprintCallable, Category = "CartManager")
    static void ClearCart(FOnCartCleared OnCartCleared);

    /**
     * Checks if the cart is empty.
     * @return True if the cart is empty, false otherwise.
     */
    UFUNCTION(BlueprintCallable, Category = "CartManager")
    static bool IsCartEmpty();

    /**
     * Retrieves the stored cart ID.
     * @return The cart ID as a string.
     */
    UFUNCTION(BlueprintCallable, Category = "CartManager")
    static FString GetStoredCartId();

    /**
     * Sets the stored cart ID.
     * @param CartId The new cart ID to set.
     */
    UFUNCTION(BlueprintCallable, Category = "CartManager")
    static void SetStoredCartId(FString CartId);

    /**
     * Handles errors that occur during API requests.
     * @param ErrorMessage The error message to log or display.
     */
    UFUNCTION(BlueprintCallable, Category = "CartManager")
    static void HandleErrors(FString ErrorMessage);

private:
    ShopConfigLoader& ConfigLoader;
    FString StoredCartId;
    static UCartManager* Instance;
};