// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
//#include "CartManager.h"
#include "Delegates/Delegate.h"
#include "UCartManagerWrapper.generated.h"

DECLARE_DYNAMIC_DELEGATE(FOnCartCreatedDelegate);
UCLASS(Blueprintable)

class NWHEELVEHICLE_API UCartManagerWrapper : public UObject
{
	GENERATED_BODY()
	
public:
	UCartManagerWrapper();

	UPROPERTY(BlueprintReadWrite, Category = "Cart")
	UCartManagerWrapper* CartManagerWrapper;

	UFUNCTION(BlueprintCallable, Category = "Cart")
	void AddProductToCart(const FString& CartId, const FString& VariantId);

	UFUNCTION(BlueprintCallable, Category = "Cart")
	void RemoveFromCart(const FString& CartId, const FString& LineItemId);

	UFUNCTION(BlueprintCallable, Category = "Cart")
	void CreateCheckout(const FString& CartId); 

	//UFUNCTION(BlueprintCallable, Category = "Cart")
	//FString GetStoredCartId();

	//UFUNCTION(BlueprintCallable, Category = "Cart")
	//void CreateCart();

	//UFUNCTION(BlueprintCallable, Category = "Cart")
	//void FetchCartDetails(const FString& CartId);

	UPROPERTY(BlueprintReadWrite, Category = "Cart")
	TArray<FString> LineItems;

private:
};
