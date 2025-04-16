#include "CartManager.h"
#include "HttpModule.h"
#include "ShopConfigLoader.h"
#include "Interfaces/IHttpResponse.h"

// Revised Singleton Accessor (Meyers' Singleton)
UCartManager* UCartManager::Get()
{
    static UCartManager* SingletonInstance = nullptr;
    
    if (!SingletonInstance)
    {
        if (UWorld* World = GEngine->GetWorldContexts().Num() > 0 ? GEngine->GetWorldContexts()[0].World() : nullptr)
        {
            SingletonInstance = NewObject<UCartManager>(World);
            SingletonInstance->AddToRoot();  // Prevent garbage collection
        }
    }

    return SingletonInstance;
}

// Constructor initialization
UCartManager::UCartManager()
    : ConfigLoader(&ShopConfigLoader::Get())  // Get a pointer by using the address-of operator
{
    // If ConfigLoader is null, handle it (Optional for safety)
    if (!ConfigLoader)
    {
        UE_LOG(LogTemp, Error, TEXT("ConfigLoader is not initialized correctly."));
    }
}

// Main Function Implementations

void UCartManager::CreateShopifyCart(FOnCartCreated OnCartCreated)
{
    if (!OnCartCreated.IsBound())
    {
        UE_LOG(LogTemp, Warning, TEXT("[CartManager] OnCartCreated callback is not bound!"));
        return;
    }

    if (!StoredCartId.IsEmpty())
    {
        UE_LOG(LogTemp, Display, TEXT("[CartManager] Cart already exists. Using stored Cart ID: %s"), *StoredCartId);
        OnCartCreated.Execute(StoredCartId, StoredCheckoutUrl);
        return;
    }
    
    if (!ConfigLoader)
    {
        UE_LOG(LogTemp, Error, TEXT("[CartManager] ConfigLoader is null!"));
        OnCartCreated.Execute(TEXT(""), TEXT(""));
        return;
    }

    const FString ApiLink = ConfigLoader->GetStorefrontApiLink();
    const FString StorefrontToken = ConfigLoader->GetStorefrontAccessToken();
    if (ApiLink.IsEmpty() || StorefrontToken.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("[CartManager] Missing API credentials."));
        OnCartCreated.Execute(TEXT(""), TEXT(""));
        return;
    }

    const FString RequestBody = TEXT(R"({"query": "mutation { cartCreate { cart { id checkoutUrl } userErrors { message } } }"})");

    const auto Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(ApiLink);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("X-Shopify-Storefront-Access-Token"), StorefrontToken);
    Request->SetHeader(TEXT("Content-Length"), FString::FromInt(RequestBody.Len()));
    Request->SetTimeout(10);
    Request->SetContentAsString(RequestBody);

    TWeakObjectPtr<UCartManager> WeakThis(this);
    Request->OnProcessRequestComplete().BindLambda([WeakThis, OnCartCreated](FHttpRequestPtr, const FHttpResponsePtr& Response, bool bWasSuccessful)
    {
        // Move all logic to the GameThread to ensure safe UObject access
        AsyncTask(ENamedThreads::GameThread, [WeakThis, OnCartCreated, Response, bWasSuccessful]()
        {
            if (!WeakThis.IsValid())
            {
                UE_LOG(LogTemp, Warning, TEXT("[CartManager] Instance no longer valid; skipping response handling."));
                return;
            }

            // Validate the delegate is still bound (owner might be GC'd)
            if (!OnCartCreated.IsBound())
            {
                UE_LOG(LogTemp, Warning, TEXT("[CartManager] OnCartCreated delegate owner is no longer valid!"));
                return;
            }

            if (!bWasSuccessful || !Response.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("[CartManager] HTTP request failed."));
                OnCartCreated.Execute(TEXT(""), TEXT(""));
                return;
            }
        
            const int32 ResponseCode = Response->GetResponseCode();
            if (ResponseCode != 200)
            {
                UE_LOG(LogTemp, Error, TEXT("[CartManager] HTTP request failed with code: %d"), ResponseCode);
                OnCartCreated.Execute(TEXT(""), TEXT(""));
                return;
            }
        
            const FString ResponseContent = Response->GetContentAsString();
            UE_LOG(LogTemp, Display, TEXT("[CartManager] Response Content: %s"), *ResponseContent);
        
            TSharedPtr<FJsonObject> JsonObject;
            if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ResponseContent), JsonObject) || !JsonObject.IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("[CartManager] Failed to parse JSON response."));
                OnCartCreated.Execute(TEXT(""), TEXT(""));
                return;
            }
        
            // Safely parse the "data" field
            const TSharedPtr<FJsonObject>* DataObjectPtr = nullptr;
            if (!JsonObject->TryGetObjectField(TEXT("data"), DataObjectPtr) || !DataObjectPtr || !DataObjectPtr->IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("[CartManager] JSON response is missing 'data' field."));
                OnCartCreated.Execute(TEXT(""), TEXT(""));
                return;
            }

            const TSharedPtr<FJsonObject> DataObject = *DataObjectPtr;

            // Safely parse the "cartCreate" field
            const TSharedPtr<FJsonObject>* CartCreateObjPtr = nullptr;
            if (!DataObject->TryGetObjectField(TEXT("cartCreate"), CartCreateObjPtr) || !CartCreateObjPtr || !CartCreateObjPtr->IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("[CartManager] JSON response is missing 'cartCreate' field."));
                OnCartCreated.Execute(TEXT(""), TEXT(""));
                return;
            }

            const TSharedPtr<FJsonObject> CartCreateObj = *CartCreateObjPtr;
            
            // Check for userErrors
            if (CartCreateObj->HasField(TEXT("userErrors")))
            {
                const TArray<TSharedPtr<FJsonValue>> UserErrors = CartCreateObj->GetArrayField(TEXT("userErrors"));
                for (const TSharedPtr<FJsonValue>& Error : UserErrors)
                {
                    const TSharedPtr<FJsonObject> ErrorObj = Error->AsObject();
                    if (ErrorObj && ErrorObj->HasField(TEXT("message")))
                    {
                        UE_LOG(LogTemp, Error, TEXT("[CartManager] Shopify Error: %s"), *ErrorObj->GetStringField(TEXT("message")));
                    }
                }
                if (UserErrors.Num() > 0)
                {
                    OnCartCreated.Execute(TEXT(""), TEXT(""));
                    return;
                }
            }

            // Safely parse the "cart" field
            const TSharedPtr<FJsonObject>* CartObjPtr = nullptr;
            if (!CartCreateObj->TryGetObjectField(TEXT("cart"), CartObjPtr) || !CartObjPtr || !CartObjPtr->IsValid())
            {
                UE_LOG(LogTemp, Error, TEXT("[CartManager] JSON response is missing 'cart' field."));
                OnCartCreated.Execute(TEXT(""), TEXT(""));
                return;
            }

            const TSharedPtr<FJsonObject> CartObj = *CartObjPtr;
            FString CartId;
            FString CheckoutUrl;

            if (!CartObj->TryGetStringField(TEXT("id"), CartId) || CartId.IsEmpty())
            {
                UE_LOG(LogTemp, Error, TEXT("[CartManager] Received empty or invalid cart ID."));
                OnCartCreated.Execute(TEXT(""), TEXT(""));
                return;
            }

            if (!CartObj->TryGetStringField(TEXT("checkoutUrl"), CheckoutUrl))
            {
                UE_LOG(LogTemp, Warning, TEXT("[CartManager] No checkout URL in response."));
                CheckoutUrl = TEXT("");
            }

            WeakThis->StoredCartId = CartId;
            WeakThis->StoredCheckoutUrl = CheckoutUrl;

            UE_LOG(LogTemp, Display, TEXT("[CartManager] Successfully created cart (ID: %s, Checkout URL: %s)"), *CartId, *CheckoutUrl);
            OnCartCreated.Execute(CartId, CheckoutUrl);
        });
    });
    
    if (!Request->ProcessRequest())
    {
        UE_LOG(LogTemp, Error, TEXT("[CartManager] Failed to start HTTP request."));
        OnCartCreated.Execute(TEXT(""), TEXT(""));
    }
}

void UCartManager::AddItemToCart(const FString CartId, const FString VariantId, const int32 Quantity, FOnItemAdded OnItemAdded)
{
    if (!ConfigLoader)
    {
        UE_LOG(LogTemp, Error, TEXT("[CartManager] ConfigLoader is null!"));
        (void)OnItemAdded.ExecuteIfBound(false);
        return;
    }

    if (CartId.IsEmpty() || VariantId.IsEmpty() || Quantity <= 0 || !ConfigLoader)
    {
        UE_LOG(LogTemp, Error, TEXT("[CartManager] Invalid parameters - CartId: %s, VariantId: %s, Quantity: %d, ConfigLoader: %s"),
            *CartId, *VariantId, Quantity, ConfigLoader ? TEXT("Valid") : TEXT("Null"));
        (void)OnItemAdded.ExecuteIfBound(false);
        return;
    }

    const FString ApiLink = ConfigLoader->GetStorefrontApiLink();
    const FString StorefrontToken = ConfigLoader->GetStorefrontAccessToken();
    if (ApiLink.IsEmpty() || StorefrontToken.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("[CartManager] Missing API credentials."));
        (void)OnItemAdded.ExecuteIfBound(false);
        return;
    }

    // Updated GraphQL mutation with cart and lines return
    const FString RequestBody = FString::Printf(
        TEXT("{\"query\":\"mutation{cartLinesAdd(cartId:\\\"%s\\\",lines:[{merchandiseId:\\\"%s\\\",quantity:%d}]){cart{id lines(first:10){edges{node{id quantity merchandise{...on ProductVariant{id title}}}}} } userErrors{field message}}}\"}"),
        *CartId, *VariantId, Quantity
    );

    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(ApiLink);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json; charset=utf-8"));
    Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
    Request->SetHeader(TEXT("X-Shopify-Storefront-Access-Token"), StorefrontToken);
    Request->SetContentAsString(RequestBody);

    TWeakObjectPtr<UCartManager> WeakThis(this);

    Request->OnProcessRequestComplete().BindLambda([WeakThis, OnItemAdded](FHttpRequestPtr, const FHttpResponsePtr& Response, const bool bWasSuccessful)
    {
        if (!WeakThis.IsValid())
        {
            UE_LOG(LogTemp, Warning, TEXT("[CartManager] Owner destroyed before request completed"));
            return;
        }

        if (!bWasSuccessful || !Response.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("[CartManager] HTTP request failed."));
            (void)OnItemAdded.ExecuteIfBound(false);
            return;
        }

        FString ResponseContent = Response->GetContentAsString();
        UE_LOG(LogTemp, Display, TEXT("[CartManager] Full Response: %s"), *ResponseContent);

        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);

        if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("[CartManager] Failed to parse JSON response."));
            (void)OnItemAdded.ExecuteIfBound(false);
            return;
        }

        if (!JsonObject->HasField(TEXT("data")))
        {
            UE_LOG(LogTemp, Error, TEXT("[CartManager] Response missing 'data' field."));
            (void)OnItemAdded.ExecuteIfBound(false);
            return;
        }

        TSharedPtr<FJsonObject> DataObject = JsonObject->GetObjectField(TEXT("data"));
        if (!DataObject.IsValid() || !DataObject->HasField(TEXT("cartLinesAdd")))
        {
            UE_LOG(LogTemp, Error, TEXT("[CartManager] Response missing 'cartLinesAdd' field."));
            (void)OnItemAdded.ExecuteIfBound(false);
            return;
        }

        TSharedPtr<FJsonObject> CartLinesAddObj = DataObject->GetObjectField(TEXT("cartLinesAdd"));
        if (!CartLinesAddObj.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("[CartManager] Invalid 'cartLinesAdd' object."));
            (void)OnItemAdded.ExecuteIfBound(false);
            return;
        }

        // Handle userErrors
        if (CartLinesAddObj->HasField(TEXT("userErrors")))
        {
            const TArray<TSharedPtr<FJsonValue>> Errors = CartLinesAddObj->GetArrayField(TEXT("userErrors"));
            if (Errors.Num() > 0)
            {
                for (const auto& Error : Errors)
                {
                    if (Error.IsValid() && Error->AsObject()->HasField(TEXT("message")))
                    {
                        UE_LOG(LogTemp, Error, TEXT("[CartManager] Error: %s"), *Error->AsObject()->GetStringField("message"));
                    }
                }
                (void)OnItemAdded.ExecuteIfBound(false);
                return;
            }
        }

        // Optionally verify cart structure
        if (CartLinesAddObj->HasField(TEXT("cart")))
        {
            const TSharedPtr<FJsonObject> CartObj = CartLinesAddObj->GetObjectField(TEXT("cart"));
            FString CartIdentifier = CartObj->GetStringField(TEXT("id"));

            if (CartObj->HasField(TEXT("lines")))
            {
                const TSharedPtr<FJsonObject> LinesObj = CartObj->GetObjectField(TEXT("lines"));
                const TArray<TSharedPtr<FJsonValue>> Edges = LinesObj->GetArrayField(TEXT("edges"));

                for (const auto& Edge : Edges)
                {
                    if (const TSharedPtr<FJsonObject>* EdgeObjPtr; Edge->TryGetObject(EdgeObjPtr))
                    {
                        const TSharedPtr<FJsonObject> Node = (*EdgeObjPtr)->GetObjectField(TEXT("node"));
                        FString LineId = Node->GetStringField(TEXT("id"));
                        int32 LineQty = Node->GetIntegerField(TEXT("quantity"));

                        const TSharedPtr<FJsonObject> Merchandise = Node->GetObjectField(TEXT("merchandise"));
                        FString VariantTitle = Merchandise->GetStringField(TEXT("title"));
                        FString VariantIdent = Merchandise->GetStringField(TEXT("id"));

                        UE_LOG(LogTemp, Display, TEXT("[CartManager] Added Item - Line ID: %s | Variant ID: %s | Title: %s | Quantity: %d"),
                            *LineId, *VariantIdent, *VariantTitle, LineQty);
                    }
                }
            }
        }

        (void)OnItemAdded.ExecuteIfBound(true);
    });

    Request->ProcessRequest();
}

void UCartManager::GetCartContents(const FString CartId, FOnGetCartContents OnCartContentLoaded)
{
    if (!ConfigLoader)
    {
        UE_LOG(LogTemp, Error, TEXT("[CartManager] ConfigLoader is null!"));
        OnCartContentLoaded.ExecuteIfBound(TArray<FCartItem>());
        return;
    }

    if (CartId.IsEmpty() || !CartId.StartsWith("gid://shopify/Cart/"))
    {
        UE_LOG(LogTemp, Error, TEXT("[CartManager] Invalid CartId: %s"), *CartId);
        OnCartContentLoaded.ExecuteIfBound(TArray<FCartItem>());
        return;
    }

    const FString ApiLink = ConfigLoader->GetStorefrontApiLink();
    const FString StorefrontToken = ConfigLoader->GetStorefrontAccessToken();

    
    if (ApiLink.IsEmpty() || StorefrontToken.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("[CartManager] Missing API credentials. API Link: %s, Token: %s"), 
            *ApiLink, *FString::Printf(TEXT("%s..."), *StorefrontToken.Left(4)));
        OnCartContentLoaded.ExecuteIfBound(TArray<FCartItem>());
        return;
    }

    // Updated query to include lineId and currencyCode
    const TSharedPtr<FJsonObject> RequestObject = MakeShared<FJsonObject>();
    RequestObject->SetStringField("query", 
        "query getCart($cartId: ID!) { "
        "cart(id: $cartId) { "
        "lines(first: 10) { "
        "edges { "
        "node { "
        "id "               // This is the LineId
        "quantity "
        "merchandise { "
        "... on ProductVariant { "
        "id "               // VariantId
        "title "           // Variant Name
        "product { "
        "   title "       // Product Name
        "} "
        "priceV2 { "
        "   amount "      // Price
        "   currencyCode " // Currency Code
        "} "
        "} "
        "} "
        "} "
        "} "
        "} "
        "} "
        "} ");

    // Prepare the variables object for the query
    TSharedPtr<FJsonObject> VariablesObject = MakeShared<FJsonObject>();
    VariablesObject->SetStringField("cartId", CartId);
    RequestObject->SetObjectField("variables", VariablesObject);

    FString RequestBody;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
    FJsonSerializer::Serialize(RequestObject.ToSharedRef(), Writer);

    // Create HTTP request
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(ApiLink);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("X-Shopify-Storefront-Access-Token"), StorefrontToken);
    Request->SetTimeout(10);
    Request->SetContentAsString(RequestBody);

    TWeakObjectPtr<UCartManager> WeakThis(this);
    Request->OnProcessRequestComplete().BindLambda([WeakThis, OnCartContentLoaded](FHttpRequestPtr, const FHttpResponsePtr& Response, const bool bWasSuccessful)
    {
        if (!WeakThis.IsValid()) return;

        TArray<FCartItem> EmptyArray;
        
        if (!bWasSuccessful || !Response.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("[CartManager] HTTP request failed"));
            (void)OnCartContentLoaded.ExecuteIfBound(EmptyArray);
            return;
        }

        const int32 ResponseCode = Response->GetResponseCode();
        const FString ResponseContent = Response->GetContentAsString();

        // Log raw response for debugging
        UE_LOG(LogTemp, Warning, TEXT("[CartManager] Raw response: %s"), *ResponseContent);

        if (ResponseCode != 200)
        {
            UE_LOG(LogTemp, Error, TEXT("[CartManager] HTTP error: %d, Content: %s"), ResponseCode, *ResponseContent);
            (void)OnCartContentLoaded.ExecuteIfBound(EmptyArray);
            return;
        }

        // Parse JSON response
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);

        if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("[CartManager] Failed to deserialize JSON response"));
            (void)OnCartContentLoaded.ExecuteIfBound(EmptyArray);
            return;
        }

        const TSharedPtr<FJsonObject>* DataObject;
        if (!JsonObject->TryGetObjectField(TEXT("data"), DataObject))
        {
            UE_LOG(LogTemp, Error, TEXT("[CartManager] Missing 'data' field in the JSON response"));
            (void)OnCartContentLoaded.ExecuteIfBound(EmptyArray);
            return;
        }

        const TSharedPtr<FJsonObject>* CartObject;
        if (!(*DataObject)->TryGetObjectField(TEXT("cart"), CartObject))
        {
            UE_LOG(LogTemp, Error, TEXT("[CartManager] Missing 'cart' object"));
            (void)OnCartContentLoaded.ExecuteIfBound(EmptyArray);
            return;
        }

        TArray<FCartItem> CartContents;
        const TSharedPtr<FJsonObject>* LinesObject;
        if ((*CartObject)->TryGetObjectField(TEXT("lines"), LinesObject))
        {
            const TArray<TSharedPtr<FJsonValue>>* Edges;
            if (LinesObject->Get()->TryGetArrayField(TEXT("edges"), Edges))
            {
                for (const TSharedPtr<FJsonValue>& Edge : *Edges)
                {
                    const TSharedPtr<FJsonObject>* EdgeObject;
                    if (Edge->TryGetObject(EdgeObject))
                    {
                        const TSharedPtr<FJsonObject>* Node;
                        if (EdgeObject->Get()->TryGetObjectField(TEXT("node"), Node))
                        {
                            FCartItem NewItem;
                            
                            // Get line ID (cart line item ID)
                            Node->Get()->TryGetStringField(TEXT("id"), NewItem.LineId);
                            
                            // Get quantity
                            Node->Get()->TryGetNumberField(TEXT("quantity"), NewItem.Quantity);

                            const TSharedPtr<FJsonObject>* MerchandiseObject;
                            if (Node->Get()->TryGetObjectField(TEXT("merchandise"), MerchandiseObject))
                            {
                                // Get variant ID
                                MerchandiseObject->Get()->TryGetStringField(TEXT("id"), NewItem.VariantId);
                                
                                // Get variant name
                                MerchandiseObject->Get()->TryGetStringField(TEXT("title"), NewItem.VariantName);
                                
                                // Get product name
                                const TSharedPtr<FJsonObject>* ProductObject;
                                if (MerchandiseObject->Get()->TryGetObjectField(TEXT("product"), ProductObject))
                                {
                                    ProductObject->Get()->TryGetStringField(TEXT("title"), NewItem.ProductName);
                                }

                                // Get price and currency
                                const TSharedPtr<FJsonObject>* PriceObject;
                                if (MerchandiseObject->Get()->TryGetObjectField(TEXT("priceV2"), PriceObject))
                                {
                                    PriceObject->Get()->TryGetNumberField(TEXT("amount"), NewItem.Price);
                                    PriceObject->Get()->TryGetStringField(TEXT("currencyCode"), NewItem.CurrencyCode);
                                }

                                UE_LOG(LogTemp, Display, TEXT("Found Cart Item: LineID: %s, Product: %s, Variant: %s, Quantity: %d, Price: %.2f %s"),
                                    *NewItem.LineId, *NewItem.ProductName, *NewItem.VariantName, NewItem.Quantity, NewItem.Price, *NewItem.CurrencyCode);

                                CartContents.Add(NewItem);
                            }
                        }
                    }
                }
            }
            else
            {
                UE_LOG(LogTemp, Error, TEXT("[CartManager] Missing 'edges' field in cart lines"));
            }
        }

        UE_LOG(LogTemp, Display, TEXT("[CartManager] Successfully retrieved %d cart items"), CartContents.Num());
        (void)OnCartContentLoaded.ExecuteIfBound(CartContents);
    });

    UE_LOG(LogTemp, Display, TEXT("[CartManager] Starting cart request for ID: %s"), *CartId);
    Request->ProcessRequest();
}
void UCartManager::UpdateCartItem(const FString CartId, const FString LineId, const int32 NewQuantity, FOnItemUpdated OnItemUpdated)
{
  if (CartId.IsEmpty() || LineId.IsEmpty() || NewQuantity <= 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[CartManager] Invalid parameters - CartId: %s, LineId: %s, NewQuantity: %d"), *CartId, *LineId, NewQuantity);
        (void)OnItemUpdated.ExecuteIfBound(false);
        return;
    }

    if (!ConfigLoader)
    {
        UE_LOG(LogTemp, Error, TEXT("[CartManager] ConfigLoader is null!"));
        (void)OnItemUpdated.ExecuteIfBound(false);
        return;
    }

    const FString ApiLink = ConfigLoader->GetStorefrontApiLink();
    const FString StorefrontToken = ConfigLoader->GetStorefrontAccessToken();
    if (ApiLink.IsEmpty() || StorefrontToken.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("[CartManager] Missing API credentials."));
        (void)OnItemUpdated.ExecuteIfBound(false);
        return;
    }

    // Create the GraphQL mutation to update the cart line item quantity
    const FString MutationBody = FString::Printf(
        TEXT("{\"query\":\"mutation {cartLinesUpdate(cartId: \\\"%s\\\", lines: [{id: \\\"%s\\\", quantity: %d}]) {cart {id} userErrors {message}}}\"}"),
        *CartId, *LineId, NewQuantity
    );

    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(ApiLink);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json; charset=utf-8"));
    Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
    Request->SetHeader(TEXT("X-Shopify-Storefront-Access-Token"), StorefrontToken);
    Request->SetContentAsString(MutationBody);

    TWeakObjectPtr<UCartManager> WeakThis(this);

    Request->OnProcessRequestComplete().BindLambda([WeakThis, OnItemUpdated](FHttpRequestPtr, const FHttpResponsePtr& Response, bool bWasSuccessful)
    {
        if (!WeakThis.IsValid())
        {
            UE_LOG(LogTemp, Warning, TEXT("[CartManager] Owner destroyed before request completed"));
            return;
        }

        if (!bWasSuccessful || !Response.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("[CartManager] HTTP request failed."));
            (void)OnItemUpdated.ExecuteIfBound(false);
            return;
        }

        FString ResponseContent = Response->GetContentAsString();
        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);

        if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("[CartManager] Failed to parse JSON response."));
            (void)OnItemUpdated.ExecuteIfBound(false);
            return;
        }

        if (!JsonObject->HasField(TEXT("data")))
        {
            UE_LOG(LogTemp, Error, TEXT("[CartManager] Response missing 'data' field."));
            (void)OnItemUpdated.ExecuteIfBound(false);
            return;
        }

        TSharedPtr<FJsonObject> DataObject = JsonObject->GetObjectField(TEXT("data"));
        if (!DataObject.IsValid() || !DataObject->HasField(TEXT("cartLinesUpdate")))
        {
            UE_LOG(LogTemp, Error, TEXT("[CartManager] Response missing 'cartLinesUpdate' field."));
            (void)OnItemUpdated.ExecuteIfBound(false);
            return;
        }

        TSharedPtr<FJsonObject> CartLinesUpdateObj = DataObject->GetObjectField(TEXT("cartLinesUpdate"));
        if (!CartLinesUpdateObj.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("[CartManager] Invalid 'cartLinesUpdate' object."));
            (void)OnItemUpdated.ExecuteIfBound(false);
            return;
        }

        // Check for errors
        if (CartLinesUpdateObj->HasField(TEXT("userErrors")))
        {
            TArray<TSharedPtr<FJsonValue>> Errors = CartLinesUpdateObj->GetArrayField(TEXT("userErrors"));
            if (Errors.Num() > 0)
            {
                for (const auto& Error : Errors)
                {
                    if (Error.IsValid() && Error->AsObject())
                    {
                        UE_LOG(LogTemp, Error, TEXT("[CartManager] Error: %s"), *Error->AsObject()->GetStringField(TEXT("message")));
                    }
                }
                (void)OnItemUpdated.ExecuteIfBound(false);
                return;
            }
        }

        // Success
        (void)OnItemUpdated.ExecuteIfBound(true);
    });

    Request->ProcessRequest();

}

void UCartManager::RemoveItemFromCart(const FString CartId, const FString LineId, FOnItemRemoved OnItemRemoved)
{
    if (CartId.IsEmpty() || LineId.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("[CartManager] Invalid parameters - CartId: %s, LineId: %s"), *CartId, *LineId);
        (void)OnItemRemoved.ExecuteIfBound(false);
        return;
    }

    if (!ConfigLoader)
    {
        UE_LOG(LogTemp, Error, TEXT("[CartManager] ConfigLoader is null!"));
        (void)OnItemRemoved.ExecuteIfBound(false);
        return;
    }

    const FString ApiLink = ConfigLoader->GetStorefrontApiLink();
    const FString StorefrontToken = ConfigLoader->GetStorefrontAccessToken();
    if (ApiLink.IsEmpty() || StorefrontToken.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("[CartManager] Missing API credentials."));
        (void)OnItemRemoved.ExecuteIfBound(false);
        return;
    }

    // Create the GraphQL mutation to remove the line item
    const FString MutationBody = FString::Printf(
        TEXT("{\"query\":\"mutation {cartLinesRemove(cartId: \\\"%s\\\", lineIds: [\\\"%s\\\"]) {cart {id} userErrors {message}}}\"}"),
        *CartId, *LineId
    );

    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(ApiLink);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json; charset=utf-8"));
    Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
    Request->SetHeader(TEXT("X-Shopify-Storefront-Access-Token"), StorefrontToken);
    Request->SetContentAsString(MutationBody);

    TWeakObjectPtr<UCartManager> WeakThis(this);

    Request->OnProcessRequestComplete().BindLambda([WeakThis, OnItemRemoved](FHttpRequestPtr, const FHttpResponsePtr& Response, bool bWasSuccessful)
    {
        if (!WeakThis.IsValid())
        {
            UE_LOG(LogTemp, Warning, TEXT("[CartManager] Owner destroyed before request completed"));
            return;
        }

        if (!bWasSuccessful || !Response.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("[CartManager] HTTP request failed."));
            (void)OnItemRemoved.ExecuteIfBound(false);
            return;
        }

        FString ResponseContent = Response->GetContentAsString();
        UE_LOG(LogTemp, Display, TEXT("[CartManager] Full response: %s"), *ResponseContent); // Log full response for debugging

        TSharedPtr<FJsonObject> JsonObject;
        TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseContent);

        if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("[CartManager] Failed to parse JSON response."));
            (void)OnItemRemoved.ExecuteIfBound(false);
            return;
        }

        // Check for top-level errors first
        if (JsonObject->HasField(TEXT("errors")))
        {
            TArray<TSharedPtr<FJsonValue>> Errors = JsonObject->GetArrayField(TEXT("errors"));
            for (const auto& Error : Errors)
            {
                if (Error.IsValid() && Error->AsObject())
                {
                    UE_LOG(LogTemp, Error, TEXT("[CartManager] API Error: %s"), *Error->AsObject()->GetStringField(TEXT("message")));
                }
            }
            (void)OnItemRemoved.ExecuteIfBound(false);
            return;
        }

        if (!JsonObject->HasField(TEXT("data")))
        {
            UE_LOG(LogTemp, Error, TEXT("[CartManager] Response missing 'data' field."));
            (void)OnItemRemoved.ExecuteIfBound(false);
            return;
        }

        TSharedPtr<FJsonObject> DataObject = JsonObject->GetObjectField(TEXT("data"));
        if (!DataObject.IsValid() || !DataObject->HasField(TEXT("cartLinesRemove")))
        {
            UE_LOG(LogTemp, Error, TEXT("[CartManager] Response missing 'cartLinesRemove' field."));
            (void)OnItemRemoved.ExecuteIfBound(false);
            return;
        }

        // More defensive JSON parsing
        const TSharedPtr<FJsonValue> CartLinesRemoveValue = DataObject->TryGetField(TEXT("cartLinesRemove"));
        if (!CartLinesRemoveValue.IsValid() || CartLinesRemoveValue->IsNull())
        {
            UE_LOG(LogTemp, Error, TEXT("[CartManager] 'cartLinesRemove' is missing or null"));
            (void)OnItemRemoved.ExecuteIfBound(false);
            return;
        }

        TSharedPtr<FJsonObject> CartLinesRemoveObj;
        if (CartLinesRemoveValue->Type == EJson::Object)
        {
            CartLinesRemoveObj = CartLinesRemoveValue->AsObject();
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("[CartManager] 'cartLinesRemove' is not an object"));
            (void)OnItemRemoved.ExecuteIfBound(false);
            return;
        }

        // Check for user errors
        if (CartLinesRemoveObj->HasField(TEXT("userErrors")))
        {
            TArray<TSharedPtr<FJsonValue>> Errors = CartLinesRemoveObj->GetArrayField(TEXT("userErrors"));
            if (Errors.Num() > 0)
            {
                for (const auto& Error : Errors)
                {
                    if (Error.IsValid() && Error->AsObject())
                    {
                        UE_LOG(LogTemp, Error, TEXT("[CartManager] Error: %s"), *Error->AsObject()->GetStringField(TEXT("message")));
                    }
                }
                (void)OnItemRemoved.ExecuteIfBound(false);
                return;
            }
        }

        // Success
        (void)OnItemRemoved.ExecuteIfBound(true);
    });

    Request->ProcessRequest();
}
UCartManager* UCartManager::GetCartManagerInstance()
{
    return Get();
}
