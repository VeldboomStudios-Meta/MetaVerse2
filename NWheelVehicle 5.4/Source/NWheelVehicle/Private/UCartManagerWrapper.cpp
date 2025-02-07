// Fill out your copyright notice in the Description page of Project Settings.

#include "UCartManagerWrapper.h"
#include "CartManager.h"

UCartManagerWrapper::UCartManagerWrapper()
{
}

void UCartManagerWrapper::AddProductToCart(const FString& CartId, const FString& VariantId)
{
    CartManager::Get().AddProductToCart(CartId, VariantId);
}

void UCartManagerWrapper::CreateCheckout(const FString& CartId)
{
    CartManager::Get().CreateCheckout(CartId);
}

FString UCartManagerWrapper::GetStoredCartId()
{
    // Check the cart ID through the singleton instance
    if (CartManager::Get().GetStoredCartId().IsEmpty())
    {
     
    }

    return CartManager::Get().GetStoredCartId();
}

void UCartManagerWrapper::CreateCart()
{
    // Avoid creating a new cart if one already exists
    if (!CartManager::Get().GetStoredCartId().IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("Cart already created. Cart ID: %s"), *CartManager::Get().GetStoredCartId());
        return;
    }

    // Create a new cart via the singleton instance
    CartManager::Get().CreateShopifyCart(
        [this]()
        {
            // Double-check if the cart is still empty in case the cart creation logic changed the state
            if (CartManager::Get().GetStoredCartId().IsEmpty())
            {
                FString CartId = CartManager::Get().GetStoredCartId();
                UE_LOG(LogTemp, Warning, TEXT("Cart created. Cart ID: %s"), *CartId);

                // Additional logic here if needed after cart creation
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("Cart creation failed or cart already created. Cart ID: %s"), *CartManager::Get().GetStoredCartId());
            }
        }
    );
}


void UCartManagerWrapper::FetchCartDetails(const FString& CartId)
{
    CartManager::Get().FetchCartDetails(CartId, [this](TArray<FString> ProductList)
        {
            // Log the product list directly before processing
            UE_LOG(LogTemp, Warning, TEXT("Fetched Cart Items Count: %d"), ProductList.Num());

            // Clear existing items in case you're fetching updated details
            LineItems.Empty();

            // Directly process the ProductList without parsing
            for (const FString& ProductDetails : ProductList)
            {
                // Log the product details for debugging
                UE_LOG(LogTemp, Warning, TEXT("Product Details: %s"), *ProductDetails);

                // Add the product details directly to the LineItems array
                LineItems.Add(ProductDetails);
            }
        });
}

void UCartManagerWrapper::RemoveFromCart(const FString& CartId, const FString& LineItemId)
{
    CartManager::Get().RemoveFromCart(CartId, LineItemId);
}
