#include "ProductActor.h"

// Sets default values
AProductActor::AProductActor()
{
	// Set this actor to call Tick() every frame
	PrimaryActorTick.bCanEverTick = true;

	// Initialize default values
	ProductName = TEXT("Unknown Product");
	ProductDescription = TEXT("No description available.");
	ProductPrice = 0.0f;
}

// Called when the game starts or when spawned
void AProductActor::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AProductActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Function to set product detail (FString)
void AProductActor::SetProductDetailString(const FString& DetailType, const FString& DetailValue)
{
	if (DetailType == TEXT("ProductName"))
	{
		ProductName = DetailValue;
	}
	else if (DetailType == TEXT("ProductDescription"))
	{
		ProductDescription = DetailValue;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid string detail type: %s"), *DetailType);
	}
}

// Function to set product detail (float)
void AProductActor::SetProductDetailFloat(const FString& DetailType, float DetailValue)
{
	if (DetailType == TEXT("ProductPrice"))
	{
		ProductPrice = DetailValue;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid float detail type: %s"), *DetailType);
	}
}

// Function to get product detail as FString
FString AProductActor::GetProductDetail(const FString& DetailType) const
{
	if (DetailType == TEXT("ProductName"))
	{
		return ProductName;
	}
	else if (DetailType == TEXT("ProductDescription"))
	{
		return ProductDescription;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid string detail type: %s"), *DetailType);
		return TEXT("");
	}
}

// Function to get product price as float
float AProductActor::GetProductDetailAsFloat(const FString& DetailType) const
{
	if (DetailType == TEXT("ProductPrice"))
	{
		return ProductPrice;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid float detail type: %s"), *DetailType);
		return 0.0f;
	}
}
