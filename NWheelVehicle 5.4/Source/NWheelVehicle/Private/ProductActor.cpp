#include "ProductActor.h"

// Sets default values
// ProductActor.cpp

AProductActor::AProductActor()
	: ProductId(TEXT("No Id Set")),
	  Title(TEXT("Unknown Product")),
	  ProductDescription(TEXT("No description available.")),
	  ProductPrice(0.0f)
{
	// Disable ticking unless necessary
	PrimaryActorTick.bCanEverTick = false;

	// Create and initialize the StaticMeshComponent
	ProductMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProductMesh"));

	// Set the static mesh component as the root component
	RootComponent = ProductMeshComponent;

	// Create and initialize the WidgetComponent
	ProductWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("ProductWidget"));

	// Attach the WidgetComponent to the root component
	ProductWidgetComponent->SetupAttachment(RootComponent);
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

