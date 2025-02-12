#pragma once

#include "CoreMinimal.h"
#include "HttpModule.h"  // For HTTP requests
#include "Interfaces/IHttpResponse.h"  // For handling HTTP responses
#include "Json.h"  // For JSON parsing
#include "JsonUtilities.h"  // For JSON utilities
#include "ShopConfigLoader.h"  // Include the ShopConfigLoader

class AProductActor;
// Regular C++ class without Unreal-specific macros
class ProductManager
{
public:
	// Constructor
	ProductManager();

	// Function to get all products from the Shopify store using Admin API
	void GetAllProducts(TFunction<void()> OnProductsFetched);

	// Function to get the product details by ProductId
	TSharedPtr<FJsonObject> GetProductDetailsById(const FString& ProductId);
	// Function to set product details in the ProductActor by the ProductId
	void SetProductDetailsById(AProductActor* ProductActor);

	bool IsProductsFetched() const;
private:


	// Helper function to process product data after fetching it
	void ProcessProductsResponse(FHttpResponsePtr Response, TFunction<void()> OnProductsFetched);


	// Data structure to hold all the products fetched from the Shopify store
	TMap<FString, TSharedPtr<FJsonObject>> AllProducts;

	// Instance of ShopConfigLoader to load config data
	ShopConfigLoader& ConfigLoader;

	// Flag to check if the products have been fetched
	bool bProductsFetched = false;
};