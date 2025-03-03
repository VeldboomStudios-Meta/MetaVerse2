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
    void CreateShopifyCart(FOnCartCreated OnCartCreated);

    /**
     * Adds a product variant to the existing cart.
     * @param VariantId The ID of the product variant to add.
     * @param Quantity The quantity of the product variant to add.
     * @param OnItemAdded Delegate executed after the item is added.
     */
    UFUNCTION(BlueprintCallable, Category = "CartManager")
    void AddItemToCart(FString VariantId, int32 Quantity, FOnItemAdded OnItemAdded);

    /**
     * Updates the quantity of an existing item in the cart.
     * @param LineId The ID of the cart line representing the item.
     * @param NewQuantity The updated quantity for the item.
     * @param OnItemUpdated Delegate executed after the item is updated.
     */
    UFUNCTION(BlueprintCallable, Category = "CartManager")
    void UpdateCartItem(FString LineId, int32 NewQuantity, FOnItemUpdated OnItemUpdated);

    /**
     * Removes an item from the cart.
     * @param LineId The ID of the cart line to remove.
     * @param OnItemRemoved Delegate executed after the item is removed.
     */
    UFUNCTION(BlueprintCallable, Category = "CartManager")
    void RemoveItemFromCart(FString LineId, FOnItemRemoved OnItemRemoved);

    /**
     * Retrieves the current contents of the cart.
     * @param OnCartContentLoaded Delegate to return the cart's contents as an array of strings.
     */
    UFUNCTION(BlueprintCallable, Category = "CartManager")
    void GetCartContents(FOnGetCartContents OnCartContentLoaded);

    /**
     * Retrieves the checkout URL for the cart and redirects the user to Shopify's checkout page.
     * @param OnCheckoutUrlReady Delegate to return the checkout URL as a string.
     */
    UFUNCTION(BlueprintCallable, Category = "CartManager")
    void ProceedToCheckout(FOnProceedToCheckout OnCheckoutUrlReady);

    /**
     * Clears all items from the cart.
     * @param OnCartCleared Delegate executed after the cart is cleared.
     */
    UFUNCTION(BlueprintCallable, Category = "CartManager")
    void ClearCart(FOnCartCleared OnCartCleared);

    /**
     * Checks if the cart is empty.
     * @return True if the cart is empty, false otherwise.
     */
    UFUNCTION(BlueprintCallable, Category = "CartManager")
    bool IsCartEmpty();

    /**
     * Retrieves the stored cart ID.
     * @return The cart ID as a string.
     */
    UFUNCTION(BlueprintCallable, Category = "CartManager")
    FString GetStoredCartId();

    /**
     * Sets the stored cart ID.
     * @param CartId The new cart ID to set.
     */
    UFUNCTION(BlueprintCallable, Category = "CartManager")
    void SetStoredCartId(FString CartId);

    /**
     * Handles errors that occur during API requests.
     * @param ErrorMessage The error message to log or display.
     */
    UFUNCTION(BlueprintCallable, Category = "CartManager")
    void HandleErrors(FString ErrorMessage);


private:
    ShopConfigLoader& ConfigLoader;
    FString StoredCartId;
    static UCartManager* Instance;
};