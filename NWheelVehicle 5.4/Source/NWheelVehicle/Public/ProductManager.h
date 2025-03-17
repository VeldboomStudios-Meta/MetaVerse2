#pragma once

#include "CoreMinimal.h"
#include "Http.h"
#include "ShopConfigLoader.h"
#include "ProductActor.h"

#include "ProductManager.generated.h"


DECLARE_DYNAMIC_DELEGATE(FOnProductsFetched);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnProductDetailsFetched, AProductActor*, ProductActor);


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
    FString GetProductDetailsById(const FString& ProductId);

    UFUNCTION(BlueprintCallable, Category = "ProductManager")
    void SetProductDetailsById(AProductActor* ProductActor, const FString& ProductId, FOnProductDetailsFetched OnProductDetailsFetched);

    UFUNCTION(BlueprintCallable, Category = "ProductManager")
    bool IsProductsFetched() const;

private:
    static UProductManager* Instance;

    void ProcessProductsResponse(FHttpResponsePtr Response, const FOnProductsFetched& OnProductsFetched);
    static void ApplyProductDetailsToActor(AProductActor* ProductActor, const TSharedPtr<FJsonObject>& ProductData, const FOnProductDetailsFetched& OnProductDetailsFetched);

    TMap<FString, TSharedPtr<FJsonObject>> AllProducts;
    ShopConfigLoader* ConfigLoader;
    bool bProductsFetched = false;
};
