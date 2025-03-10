#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
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

	// Product properties
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Product Details")
	FString ProductName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Product Details")
	float ProductPrice;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Product Details")
	FString ProductDescription;

	// General function to set any product property
	UFUNCTION(BlueprintCallable, Category = "Product Details")
	void SetProductDetailString(const FString& DetailType, const FString& DetailValue);

	UFUNCTION(BlueprintCallable, Category = "Product Details")
	void SetProductDetailFloat(const FString& DetailType, float DetailValue);

	// General function to get any product property
	UFUNCTION(BlueprintCallable, Category = "Product Details")
	FString GetProductDetail(const FString& DetailType) const;

	// Function to get product price as float
	UFUNCTION(BlueprintCallable, Category = "Product Details")
	float GetProductDetailAsFloat(const FString& DetailType) const;
};
