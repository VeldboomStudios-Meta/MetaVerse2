
#include "CartManager.h"
#include "ShopConfigLoader.h"
#include "HttpModule.h"
#include "UCartManagerWrapper.h"
#include "Http.h"  
    
CartManager& CartManager::Get()
{
    static CartManager Instance;
    return Instance;
}
CartManager::CartManager()
    : ConfigLoader(ShopConfigLoader::Get())
{
}
CartManager::~CartManager()
{
}

void CartManager::CreateShopifyCart(TFunction<void()> OnCartCreated)
{
    if (!StoredCartId.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("A cart already exists with ID: %s. Cannot create a new cart."), *StoredCartId);
        return;
    }

    FString ApiLink = ConfigLoader.GetStorefrontApiLink();
    FString AccessToken = ConfigLoader.GetStorefrontAccessToken();

    if (!FHttpModule::Get().IsHttpEnabled())
    {
        UE_LOG(LogTemp, Error, TEXT("HTTP module is not enabled. Cannot create request."));
        return;
    }

    FString GraphQLQuery = R"({"query": "mutation { cartCreate { cart { id } } }"})";

    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(ApiLink);
    Request->SetVerb("POST");
    Request->SetHeader("Content-Type", "application/json");
    Request->SetHeader("X-Shopify-Storefront-Access-Token", AccessToken);
    Request->SetContentAsString(GraphQLQuery);

    Request->OnProcessRequestComplete().BindLambda([this, OnCartCreated](FHttpRequestPtr, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            if (!bWasSuccessful || !Response.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("HTTP request failed or response is invalid."));
                return;
            }

            if (Response->GetResponseCode() != 200)
            {
                UE_LOG(LogTemp, Error, TEXT("Unexpected response code: %d"), Response->GetResponseCode());
                return;
            }

            FString ResponseBody = Response->GetContentAsString();
            TSharedPtr<FJsonObject> JsonObject;
            if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ResponseBody), JsonObject) && JsonObject.IsValid())
            {
                TSharedPtr<FJsonObject> DataObject = JsonObject->GetObjectField(TEXT("data"));
                TSharedPtr<FJsonObject> CartCreate = DataObject->GetObjectField(TEXT("cartCreate"));
                TSharedPtr<FJsonObject> CartObject = CartCreate->GetObjectField(TEXT("cart"));

                if (CartObject->HasTypedField<EJson::String>(TEXT("id")))
                {
                    StoredCartId = CartObject->GetStringField(TEXT("id"));
                    UE_LOG(LogTemp, Log, TEXT("Successfully created cart with ID: %s"), *StoredCartId);

                    if (OnCartCreated)
                    {
                        OnCartCreated();
                    }
                }
                else
                {
                    UE_LOG(LogTemp, Error, TEXT("Malformed JSON response: Missing 'id' field."));
                }
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("Failed to parse JSON response: %s"), *ResponseBody);
            }
        });

    UE_LOG(LogTemp, Log, TEXT("Sending HTTP request to %s"), *ApiLink);
    Request->ProcessRequest();
}

void CartManager::FetchCartDetails(const FString& CartId, TFunction<void(TArray<FString>)> OnFetchCompleted)
{
    FString ApiLink = ConfigLoader.GetStorefrontApiLink();
    FString AccessToken = ConfigLoader.GetStorefrontAccessToken();

    FString FetchCartQuery = FString::Printf(TEXT(R"(
    {
        cart(id: "%s") {
            id
            lines(first: 10) {
                edges {
                    node {
                        id
                        merchandise {
                            ... on ProductVariant {
                                id
                                title
                                product {
                                    id
                                    title
                                }
                                priceV2 {
                                    amount
                                    currencyCode
                                }   
                            }
                        }
                        quantity
                    }
                }
            }
        }
    }
    )"), *CartId);

    TSharedRef<IHttpRequest> FetchCartRequest = FHttpModule::Get().CreateRequest();
    FetchCartRequest->SetURL(ApiLink);
    FetchCartRequest->SetVerb("POST");
    FetchCartRequest->SetHeader("Content-Type", "application/graphql");
    FetchCartRequest->SetHeader("X-Shopify-Storefront-Access-Token", AccessToken);
    FetchCartRequest->SetContentAsString(FetchCartQuery);

    FetchCartRequest->OnProcessRequestComplete().BindLambda([this, OnFetchCompleted](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            TArray<FString> LineItems;
            TMap<FString, int32> LineItemMap;

            auto CallCallback = [&]() { OnFetchCompleted(LineItems); };

            if (!bWasSuccessful || !Response.IsValid())
            {
                FString ErrorResponse = Response.IsValid() ? Response->GetContentAsString() : TEXT("No response received.");
                UE_LOG(LogTemp, Error, TEXT("Failed to fetch cart details. Response: %s"), *ErrorResponse);
                CallCallback();
                return;
            }

            FString ResponseBody = Response->GetContentAsString();
            TSharedPtr<FJsonObject> JsonObject;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);

            if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("Failed to deserialize JSON response."));
                CallCallback();
                return;
            }

            const TSharedPtr<FJsonObject> DataObject = JsonObject->GetObjectField(TEXT("data"));
            if (!DataObject.IsValid() || !DataObject->HasField(TEXT("cart")))
            {
                UE_LOG(LogTemp, Error, TEXT("Cart field is missing in the response."));
                CallCallback();
                return;
            }

            const TSharedPtr<FJsonObject> CartObject = DataObject->GetObjectField(TEXT("cart"));
            const TArray<TSharedPtr<FJsonValue>> LineEdges = CartObject->GetObjectField(TEXT("lines"))->GetArrayField(TEXT("edges"));

            if (LineEdges.Num() == 0)
            {
                CallCallback();
                return;
            }

            for (const TSharedPtr<FJsonValue>& Edge : LineEdges)
            {
                if (!Edge.IsValid())
                {
                    continue;
                }

                const TSharedPtr<FJsonObject> NodeObject = Edge->AsObject()->GetObjectField(TEXT("node"));
                if (!NodeObject.IsValid())
                {
                    continue;
                }

                FString LineItemId, VariantTitle, ProductTitle, Amount, CurrencyCode;
                int32 Quantity = 0;

                if (!NodeObject->TryGetStringField(TEXT("id"), LineItemId) || !NodeObject->TryGetNumberField(TEXT("quantity"), Quantity))
                {
                    continue;
                }
                
                if (LineItemMap.Contains(LineItemId))
                {
                    LineItemMap[LineItemId] += Quantity;
                    continue;
                }

                const TSharedPtr<FJsonObject> MerchandiseObject = NodeObject->GetObjectField(TEXT("merchandise"));
                if (!MerchandiseObject.IsValid())
                {
                    continue;
                }

                if (!MerchandiseObject->TryGetStringField(TEXT("title"), VariantTitle))
                {
                    VariantTitle = TEXT("Unknown Variant");
                }

                const TSharedPtr<FJsonObject> ProductObject = MerchandiseObject->GetObjectField(TEXT("product"));
                if (ProductObject.IsValid() && ProductObject->TryGetStringField(TEXT("title"), ProductTitle))
                {
                    // Product title successfully retrieved
                }
                else
                {
                    ProductTitle = TEXT("Unknown Product");
                }

                const TSharedPtr<FJsonObject> PriceObject = MerchandiseObject->GetObjectField(TEXT("priceV2"));
                if (!PriceObject.IsValid() || !PriceObject->TryGetStringField(TEXT("amount"), Amount) || !PriceObject->TryGetStringField(TEXT("currencyCode"), CurrencyCode))
                {
                    continue;
                }

                LineItems.Add(FString::Printf(TEXT("%s (Variant: %s) | ID: %s | Quantity: %d | Price: %s %s"),
                    *ProductTitle,   // Product Name
                    *VariantTitle,   // Variant Name
                    *LineItemId,
                    Quantity,
                    *Amount,
                    *CurrencyCode));

                LineItemMap.Add(LineItemId, Quantity);
            }

            CallCallback();
        });

    FetchCartRequest->ProcessRequest();
}

void CartManager::AddProductToCart(const FString& CartId, const FString& VariantId)
{
    UE_LOG(LogTemp, Log, TEXT("AddProductToCart called with CartId: %s, VariantId: %s"), *CartId, *VariantId);

    if (CartId.IsEmpty() || VariantId.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid Cart ID or Variant ID. Cannot add product to cart."));
        return;
    }

    FString ApiLink = ConfigLoader.GetStorefrontApiLink();
    FString AccessToken = ConfigLoader.GetStorefrontAccessToken();

    FString GraphQLQuery = FString::Printf(TEXT(R"(
        {
            "query": "mutation { cartLinesAdd(cartId: \"%s\", lines: [{ quantity: 1, merchandiseId: \"%s\" }]) { cart { id } } }"
        }
    )"), *CartId, *VariantId);

    UE_LOG(LogTemp, Log, TEXT("GraphQL Query: %s"), *GraphQLQuery);

    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(ApiLink);
    Request->SetVerb("POST");
    Request->SetHeader("Content-Type", "application/json");
    Request->SetHeader("X-Shopify-Storefront-Access-Token", AccessToken);
    Request->SetContentAsString(GraphQLQuery);

    UE_LOG(LogTemp, Log, TEXT("Sending HTTP request to %s"), *ApiLink);

    Request->OnProcessRequestComplete().BindLambda([this, CartId](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            if (!bWasSuccessful || !Response.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("HTTP request failed or response is invalid."));
                return;
            }

            int32 ResponseCode = Response->GetResponseCode();
            FString ResponseBody = Response->GetContentAsString();

            UE_LOG(LogTemp, Log, TEXT("HTTP Response Code: %d"), ResponseCode);
            UE_LOG(LogTemp, Log, TEXT("HTTP Response Body: %s"), *ResponseBody);

            if (ResponseCode != 200)
            {
                UE_LOG(LogTemp, Error, TEXT("Unexpected response code: %d"), ResponseCode);
                return;
            }

            TSharedPtr<FJsonObject> JsonObject;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);

            if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("Failed to parse JSON response: %s"), *ResponseBody);
                return;
            }

            if (JsonObject->HasField(TEXT("errors")))
            {
                UE_LOG(LogTemp, Error, TEXT("Error in response: %s"), *JsonObject->GetStringField(TEXT("errors")));
                return;
            }

            const TSharedPtr<FJsonObject> DataObject = JsonObject->GetObjectField(TEXT("data"));
            if (!DataObject.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("Missing 'data' field in response."));
                return;
            }

            const TSharedPtr<FJsonObject> CartLinesAdd = DataObject->GetObjectField(TEXT("cartLinesAdd"));
            if (!CartLinesAdd.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("Missing 'cartLinesAdd' field in response."));
                return;
            }

            const TSharedPtr<FJsonObject> CartObject = CartLinesAdd->GetObjectField(TEXT("cart"));
            if (!CartObject.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("Missing 'cart' field in response."));
                return;
            }

            UE_LOG(LogTemp, Log, TEXT("Product added to cart successfully for CartId: %s"), *CartId);

            // Uncomment if fetching updated cart items is required
            // UCartWidget::GetInstance()->FetchCartItems();
        });

    UE_LOG(LogTemp, Log, TEXT("Sending HTTP request now."));
    Request->ProcessRequest();
}

void CartManager::RemoveFromCart(const FString& CartId, const FString& LineItemId)
{
    if (CartId.IsEmpty() || LineItemId.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid Cart ID or Line Item ID. Cannot remove product from cart."));
        return;
    }

    FString GraphQLQuery = FString::Printf(TEXT(R"({"query":"mutation{cartLinesRemove(cartId:\"%s\",lineIds:[\"%s\"]){cart{id}}}"})"), *CartId, *LineItemId);

    TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(ConfigLoader.GetStorefrontApiLink());
    Request->SetVerb("POST");
    Request->SetHeader("Content-Type", "application/json");
    Request->SetHeader("X-Shopify-Storefront-Access-Token", ConfigLoader.GetStorefrontAccessToken());
    Request->SetContentAsString(GraphQLQuery);

    Request->OnProcessRequestComplete().BindLambda([LineItemId](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            if (!bWasSuccessful || !Response.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("Failed to remove product with LineItemId: %s. No valid response."), *LineItemId);
                return;
            }

            FString ResponseBody = Response->GetContentAsString();
            if (Response->GetResponseCode() != 200)
            {
                UE_LOG(LogTemp, Error, TEXT("Failed to remove product with LineItemId: %s. Response: %s"), *LineItemId, *ResponseBody);
                return;
            }

            UE_LOG(LogTemp, Log, TEXT("Product with LineItemId: %s removed successfully. Response: %s"), *LineItemId, *ResponseBody);
        });

    UE_LOG(LogTemp, Log, TEXT("Removing product with LineItemId: %s from cart."), *LineItemId);
    Request->ProcessRequest();
}

void CartManager::CreateCheckout(const FString& CartId)
{
    FString ApiLink = ConfigLoader.GetStorefrontApiLink();
    FString AccessToken = ConfigLoader.GetStorefrontAccessToken();

    // Step 1: Fetch the cart details (to get the line items) before creating the checkout
    FString FetchCartQuery = FString::Printf(TEXT(R"(
    {
        cart(id: "%s") {
            id
            lines(first: 10) {
                edges {
                    node {
                        id
                        merchandise {
                            ... on ProductVariant {
                                id
                                title
                            }
                        }
                        quantity
                    }
                }
            }
        }
    }
)"), *CartId);


    TSharedRef<IHttpRequest> FetchCartRequest = FHttpModule::Get().CreateRequest();
    FetchCartRequest->SetURL(ApiLink);
    FetchCartRequest->SetVerb("POST");
    FetchCartRequest->SetHeader("Content-Type", "application/graphql");
    FetchCartRequest->SetHeader("X-Shopify-Storefront-Access-Token", AccessToken);
    FetchCartRequest->SetContentAsString(FetchCartQuery);

    FetchCartRequest->OnProcessRequestComplete().BindLambda([this, ApiLink, AccessToken, CartId](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
        {
            if (bWasSuccessful && Response.IsValid())
            {
                FString ResponseBody = Response->GetContentAsString();
                UE_LOG(LogTemp, Warning, TEXT("Cart Fetched. Response: %s"), *ResponseBody);

                // Extract cart lines from the response
                TSharedPtr<FJsonObject> JsonObject;
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);

                if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
                {
                    const TSharedPtr<FJsonObject> DataObject = JsonObject->GetObjectField(TEXT("data"));
                    const TSharedPtr<FJsonObject> CartObject = DataObject->GetObjectField(TEXT("cart"));
                    const TArray<TSharedPtr<FJsonValue>> LineEdges = CartObject->GetObjectField(TEXT("lines"))->GetArrayField(TEXT("edges"));

                    FString LineItems;

                    // Construct line items for the checkout query
                    for (const TSharedPtr<FJsonValue>& Edge : LineEdges)
                    {
                        const TSharedPtr<FJsonObject> NodeObject = Edge->AsObject()->GetObjectField(TEXT("node"));
                        const FString VariantId = NodeObject->GetObjectField(TEXT("merchandise"))->GetStringField(TEXT("id"));
                        const int32 Quantity = NodeObject->GetIntegerField(TEXT("quantity"));

                        // Add each line item to the GraphQL mutation string
                        LineItems += FString::Printf(TEXT("{ variantId: \"%s\", quantity: %d }, "), *VariantId, Quantity);

                        // Log the variant details
                        //UE_LOG(LogTemp, Warning, TEXT("Variant ID: %s, Quantity: %d"), *VariantId, Quantity);
                    }

                    // Remove the trailing comma and space
                    LineItems.RemoveFromEnd(", ");

                    // Step 2: Create checkout with the correct line items from the cart
                    FString CheckoutQuery = FString::Printf(TEXT(R"(
                        mutation {
                            checkoutCreate(input: {
                                lineItems: [%s]
                            }) {
                                checkout {
                                    id
                                    webUrl
                                }
                            }
                        }
                    )"), *LineItems);

                    TSharedRef<IHttpRequest> CheckoutRequest = FHttpModule::Get().CreateRequest();
                    CheckoutRequest->SetURL(ApiLink);
                    CheckoutRequest->SetVerb("POST");
                    CheckoutRequest->SetHeader("Content-Type", "application/graphql");
                    CheckoutRequest->SetHeader("X-Shopify-Storefront-Access-Token", AccessToken);
                    CheckoutRequest->SetContentAsString(CheckoutQuery);

                    CheckoutRequest->OnProcessRequestComplete().BindLambda([](FHttpRequestPtr CheckoutRequest, FHttpResponsePtr CheckoutResponse, bool bWasSuccessful)
                        {
                            if (bWasSuccessful && CheckoutResponse.IsValid())
                            {
                                FString CheckoutResponseBody = CheckoutResponse->GetContentAsString();
                                // UE_LOG(LogTemp, Warning, TEXT("Checkout Created. Response: %s"), *CheckoutResponseBody);

                                 // Extract checkout URL from the response
                                TSharedPtr<FJsonObject> CheckoutJsonObject;
                                TSharedRef<TJsonReader<>> CheckoutReader = TJsonReaderFactory<>::Create(CheckoutResponseBody);

                                if (FJsonSerializer::Deserialize(CheckoutReader, CheckoutJsonObject) && CheckoutJsonObject.IsValid())
                                {
                                    if (CheckoutJsonObject->HasField(TEXT("data")) &&
                                        CheckoutJsonObject->GetObjectField(TEXT("data"))->HasField(TEXT("checkoutCreate")) &&
                                        CheckoutJsonObject->GetObjectField(TEXT("data"))
                                        ->GetObjectField(TEXT("checkoutCreate"))
                                        ->HasField(TEXT("checkout")) &&
                                        CheckoutJsonObject->GetObjectField(TEXT("data"))
                                        ->GetObjectField(TEXT("checkoutCreate"))
                                        ->GetObjectField(TEXT("checkout"))
                                        ->HasField(TEXT("webUrl")))
                                    {
                                        FString CheckoutUrl = CheckoutJsonObject->GetObjectField(TEXT("data"))
                                            ->GetObjectField(TEXT("checkoutCreate"))
                                            ->GetObjectField(TEXT("checkout"))
                                            ->GetStringField(TEXT("webUrl"));

                                        // Log the checkout URL
                                      //  UE_LOG(LogTemp, Warning, TEXT("Checkout URL: %s"), *CheckoutUrl);

                                        // Launch the URL in the default browser
                                        if (!CheckoutUrl.IsEmpty())
                                        {
                                            FPlatformProcess::LaunchURL(*CheckoutUrl, nullptr, nullptr);
                                        }
                                        else
                                        {
                                            UE_LOG(LogTemp, Error, TEXT("Checkout URL is empty or invalid."));
                                        }
                                    }
                                    else
                                    {
                                        UE_LOG(LogTemp, Error, TEXT("Required fields (checkoutCreate, checkout, webUrl) were not found in the response."));
                                    }
                                }
                            }
                            else
                            {
                                UE_LOG(LogTemp, Error, TEXT("Failed to create checkout. Response: %s"), *CheckoutResponse->GetContentAsString());
                            }
                        });

                    CheckoutRequest->ProcessRequest();
                }
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("Failed to fetch cart details. Response: %s"), *Response->GetContentAsString());
            }
        });

    FetchCartRequest->ProcessRequest();
}

FString CartManager::GetStoredCartId() const
{
    return StoredCartId;
}

