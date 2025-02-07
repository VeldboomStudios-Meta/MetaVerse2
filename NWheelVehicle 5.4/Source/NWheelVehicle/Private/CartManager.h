#pragma once

#include "ShopConfigLoader.h"
#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"

class CartManager
{

public:
	CartManager();
	~CartManager();

	static CartManager& Get();

	FString CalculateTotalPrice() const;
	void CreateShopifyCart(TFunction<void()> OnCartCreated);
	void AddProductToCart(const FString& CartId, const FString& VariantId);
	void RemoveFromCart(const FString& CartId, const FString& LineItemId);
	void CreateCheckout(const FString& CartId);
	void FetchCartDetails(const FString& CartId, TFunction<void(TArray<FString>)> OnFetchCompleted);

	// Function to get the stored cart ID
	UFUNCTION(BlueprintCallable, Category = "Cart")
	FString GetStoredCartId() const;

private:
	CartManager(const CartManager&) = delete;
	CartManager& operator=(const CartManager&) = delete;

	TMap<FString, int32> CartItems; // Stores cart items: Key - Product ID, Value - Quantity
	FString StoredCartId;            // Variable to store the cart ID
	ShopConfigLoader& ConfigLoader;  // Instance of ShopConfigLoader to load config data
};
