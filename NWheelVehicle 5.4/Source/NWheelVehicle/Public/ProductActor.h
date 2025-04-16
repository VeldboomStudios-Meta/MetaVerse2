#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"

#include "ProductManager.h" 

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
	
    //Mesh
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
	UStaticMeshComponent* ProductMeshComponent;

	// Widget Component
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
	UWidgetComponent* ProductWidgetComponent;

	
	// Product properties

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Product Details")
	FString ProductId;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Product Details")
	FString Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Product Details")
	float ProductPrice;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Product Details")
	FString ProductDescription;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Product Details")
	TArray<FProductVariant> ProductVariants; // Now matches ProductManager's struct!
	
};
