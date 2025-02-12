
#pragma once

#include "ProductManager.h"
#include "CoreMinimal.h"
#include "ShopConfigLoader.h"
#include "GameFramework/Actor.h"
#include "CartManager.h"
#include "ProductActor.generated.h"

UCLASS()
class NWHEELVEHICLE_API AProductActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AProductActor();


protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Product ID field
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Product")
	FString ProductId;

	// Product Title
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Product")
	FString ProductTitle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Product")
	float ProductPrice;



	// Variants map: Variant ID to Variant Name
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Product")
	TMap<FString, FString> Variants;

	// Static Mesh for visualization (for testing purposes)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Product")
	UStaticMeshComponent* ProductMesh;
        
	// Function to set product details from ProductManager
	UFUNCTION(BlueprintCallable, Category = "Cart")
	void SetProductDetails();


private:
	FTimerHandle CheckoutTimerHandle;
	CartManager CartManagerInstance;
	ProductManager ProductManagerInstance;
	void ProcessVariant(const TSharedPtr<FJsonValue>& VariantValue);

};