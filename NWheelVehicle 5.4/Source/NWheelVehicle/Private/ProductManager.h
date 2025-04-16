#pragma once

#include "CoreMinimal.h"
#include "Http.h"
#include "ShopConfigLoader.h"

#include "ProductManager.generated.h"

USTRUCT(BlueprintType)
struct FProductVariant
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadOnly)
    FString VariantId;
    
    UPROPERTY(BlueprintReadOnly)
    FString Title;
    
    UPROPERTY(BlueprintReadOnly)
    float Price = 0.f;
};

USTRUCT(BlueprintType)
struct FProductDetails
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Product Details")
    FString ProductId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Product Details")
    FString Title;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Product Details")
    float Price;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Product Details")
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Product Details")
    TArray<FProductVariant> Variants;

    // Constructor for easy initialization
    FProductDetails()
        : Price(0.0f)
    {}
};

// Declare delegate types
DECLARE_DYNAMIC_DELEGATE(FOnProductsFetched);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnProductDetailsFetched, FProductDetails, ProductDetails);

UCLASS(BlueprintType)
class NWHEELVEHICLE_API UProductManager : public UObject
{
    GENERATED_BODY()

public:
    UProductManager();

    UFUNCTION(BlueprintCallable, Category = "ProductManager")
    static UProductManager* GetProductManagerInstance();

    UFUNCTION(BlueprintCallable, Category = "ProductManager")
    void GetAllProducts(FOnProductsFetched OnProductsFetched);

    UFUNCTION(BlueprintCallable, Category = "ProductManager")
    void GetProductDetailsById(const FString& ProductId, FOnProductDetailsFetched OnProductDetailsFetched);

   // UFUNCTION(BlueprintCallable, Category = "ProductManager")
    //void SetProductDetailsById(AProductActor* ProductActor, const FString& ProductId, FOnProductDetailsFetched OnProductDetailsFetched);

    UFUNCTION(BlueprintCallable, Category = "ProductManager")
    bool IsProductsFetched() const;

private:
    static UProductManager* Instance;

    void ProcessProductsResponse(const FHttpResponsePtr& Response, const FOnProductsFetched& OnProductsFetched);
    //static void ApplyProductDetailsToActor(AProductActor* ProductActor, const FProductDetails& ProductDetails, const FOnProductDetailsFetched& OnProductDetailsFetched);

    TMap<FString, TSharedPtr<FJsonObject>> AllProducts;
    ShopConfigLoader* ConfigLoader;
    bool bProductsFetched = false;
};
