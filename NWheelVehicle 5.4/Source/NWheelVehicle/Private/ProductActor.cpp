#include "ProductActor.h"
#include "ProductManager.h"
#include "CartManager.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"

// Sets default values
AProductActor::AProductActor()
{
    PrimaryActorTick.bCanEverTick = true;

    // Initialize the ProductMesh component
    ProductMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProductMesh"));

    // Set ProductMesh as the RootComponent using SetRootComponent
    SetRootComponent(ProductMesh);
}

void AProductActor::BeginPlay()
{
    Super::BeginPlay();

    // Ensure products are fetched before setting details
    if (ProductManagerInstance.IsProductsFetched())
    {
        SetProductDetails();
    }
    else
    {
        // Set a callback or delay to set the product details after fetching
        ProductManagerInstance.GetAllProducts([this]()
            {
                SetProductDetails();
            });
    }
   
}

void AProductActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

}

void AProductActor::SetProductDetails()
{
    // Get product details
    TSharedPtr<FJsonObject> ProductDetails = ProductManagerInstance.GetProductDetailsById(ProductId);

    if (!ProductDetails.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Product details are invalid or null for Product ID: %s"), *ProductId);
        return;
    }

    // Set the product title
    ProductTitle = ProductDetails->GetStringField(TEXT("title"));

    // Check for 'variants' field
    if (!ProductDetails->HasTypedField<EJson::Array>(TEXT("variants")))
    {
        UE_LOG(LogTemp, Error, TEXT("No 'variants' field found or it's null for Product ID: %s"), *ProductId);
        return;
    }

    // Process variants
    TArray<TSharedPtr<FJsonValue>> VariantsArray = ProductDetails->GetArrayField(TEXT("variants"));
    Variants.Empty(); // Clear existing variants

    for (const auto& VariantValue : VariantsArray)
    {
        ProcessVariant(VariantValue);
    }
}

void AProductActor::ProcessVariant(const TSharedPtr<FJsonValue>& VariantValue)
{
    if (!VariantValue.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid VariantObject for Product ID: %s"), *ProductId);
        return;
    }

    TSharedPtr<FJsonObject> VariantObject = VariantValue->AsObject();
    if (!VariantObject.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid VariantObject for Product ID: %s"), *ProductId);
        return;
    }

    FString VariantId = VariantObject->GetStringField(TEXT("id"));
    FString VariantName = VariantObject->GetStringField(TEXT("title"));

    // Validate and normalize VariantId
    if (VariantId.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Variant ID is empty for Product ID: %s"), *ProductId);
        return;
    }

    if (!VariantId.StartsWith(TEXT("gid://shopify/ProductVariant/")))
    {
        VariantId = FString(TEXT("gid://shopify/ProductVariant/")) + VariantId;
    }

    // Validate and set price
    if (VariantObject->HasField(TEXT("price")))
    {
        ProductPrice = VariantObject->GetNumberField(TEXT("price"));
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Missing 'price' field for Variant in Product ID: %s"), *ProductId);
        return;
    }

    // Add the variant to the map
    Variants.Add(VariantId, VariantName);
}
