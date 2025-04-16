// Copyright VeldboomStudios 2025

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h" // For Blueprint compatibility
#include "Delegates/Delegate.h" // Include delegates for callbacks
#include "Templates/Function.h" // For TFunction
#include "ShopConfigLoader.h" // For ShopConfigLoader
#include "CartManager.generated.h"

USTRUCT(BlueprintType)
struct FCartItem
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString ProductName;

    UPROPERTY(BlueprintReadOnly)
    FString VariantName;

    UPROPERTY(BlueprintReadOnly)
    FString VariantId; // VariantID

    UPROPERTY(BlueprintReadOnly)
    FString LineId; // Line Item ID

    UPROPERTY(BlueprintReadOnly)
    int32 Quantity;

    UPROPERTY(BlueprintReadOnly)
    float Price;

    UPROPERTY(BlueprintReadOnly)
    FString CurrencyCode;
};


// Custom delegate declarations for Blueprint compatibility
// These must be declared outside the class scope
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnCartCreated, const FString&, CartId, const FString&, CheckoutUrl);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnItemAdded, bool, bSuccess);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnItemUpdated, bool, bSuccess);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnItemRemoved, bool, bSuccess);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnGetCartContents, const TArray<FCartItem>&, CartItems);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnProceedToCheckout, FString, CheckoutUrl);


/**
 * @brief Manages interactions with Shopify Storefront API for cart operations.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class NWHEELVEHICLE_API UCartManager : public UObject // Changed from UBlueprintFunctionLibrary to UObject
{
    GENERATED_BODY()

public:
    // Constructor
    UCartManager();

    // Singleton instance accessor
    static UCartManager* Get();
    
    UFUNCTION(BlueprintCallable, Category = "CartManager")
    void CreateShopifyCart(FOnCartCreated OnCartCreated);
    
    UFUNCTION(BlueprintCallable, Category = "CartManager")
    void AddItemToCart(FString CartId, FString VariantId, int32 Quantity, FOnItemAdded OnItemAdded);
    
    UFUNCTION(BlueprintCallable, Category = "CartManager")
    void UpdateCartItem(FString CartId, FString LineId, int32 NewQuantity, FOnItemUpdated OnItemUpdated);
    
    UFUNCTION(BlueprintCallable, Category = "CartManager")
    void RemoveItemFromCart(FString CartId, FString LineId, FOnItemRemoved OnItemRemoved);
    
    UFUNCTION(BlueprintCallable, Category = "CartManager")
    void GetCartContents(FString CartId, FOnGetCartContents OnCartContentLoaded);

    UFUNCTION(BlueprintCallable, Category = "CartManager")
    static UCartManager* GetCartManagerInstance();

    UPROPERTY(BlueprintReadWrite, Category = "CartManager|Stored Data")
    FString StoredCartId;
    
    UPROPERTY(BlueprintReadWrite, Category = "CartManager|Stored Data")
    FString StoredCheckoutUrl;
private:
    FOnCartCreated CartCreatedDelegate;
    FOnItemAdded ItemAddedDelegate;
    FOnItemUpdated ItemUpdatedDelegate;
    FOnItemRemoved ItemRemovedDelegate;
    FOnGetCartContents CartContentsLoadedDelegate;
    FOnProceedToCheckout CheckoutUrlReadyDelegate;
    
    ShopConfigLoader* ConfigLoader;
  
  
    
    static UCartManager* Instance; // Singleton instance
};