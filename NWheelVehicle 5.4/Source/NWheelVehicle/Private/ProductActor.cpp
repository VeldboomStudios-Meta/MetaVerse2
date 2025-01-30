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
    TSharedPtr<FJsonObject> ProductDetails = ProductManagerInstance.GetProductDetailsById(ProductId);

    if (ProductDetails.IsValid())
    {
        // Set the product title
        ProductTitle = ProductDetails->GetStringField(TEXT("title"));

        // Check if the 'variants' array exists and is valid
        if (ProductDetails->HasTypedField<EJson::Array>(TEXT("variants")))
        {
            TArray<TSharedPtr<FJsonValue>> VariantsArray = ProductDetails->GetArrayField(TEXT("variants"));

            // Clear current variants
            Variants.Empty();

            // Populate the variants map and get the price
            for (TSharedPtr<FJsonValue> VariantValue : VariantsArray)
            {
                TSharedPtr<FJsonObject> VariantObject = VariantValue->AsObject();

                if (VariantObject.IsValid())
                {
                    FString VariantId = VariantObject->GetStringField(TEXT("id"));
                    FString VariantName = VariantObject->GetStringField(TEXT("title"));

                    // Ensure VariantId is in the proper format
                    if (VariantId.IsEmpty())
                    {
                        UE_LOG(LogTemp, Error, TEXT("Variant ID is empty for Product ID: %s"), *ProductId);
                        continue;
                    }

                    if (!VariantId.StartsWith(TEXT("gid://shopify/ProductVariant/")))
                    {
                        VariantId = FString(TEXT("gid://shopify/ProductVariant/")) + VariantId;
                    }

                    // Get the price as a float directly, with validation
                    if (VariantObject->HasField(TEXT("price")))
                    {
                        ProductPrice = VariantObject->GetNumberField(TEXT("price"));
                    }
                    else
                    {
                        UE_LOG(LogTemp, Error, TEXT("Missing 'price' field for Variant in Product ID: %s"), *ProductId);
                        continue;
                    }

                    // Add the variant to the map
                    Variants.Add(VariantId, VariantName);
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("Invalid VariantObject for Product ID: %s"), *ProductId);
                }
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("No 'variants' field found or it's null for Product ID: %s"), *ProductId);
        }
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("Product details are invalid or null for Product ID: %s"), *ProductId);
    }
}

