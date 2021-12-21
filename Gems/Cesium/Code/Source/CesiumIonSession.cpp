#include "CesiumIonSession.h"
#include "CesiumTilesetEditorComponent.h"
#include "CesiumIonRasterOverlayEditorComponent.h"
#include "CesiumSystemComponentBus.h"
#include "PlatformInfo.h"
#include <Editor/EditorSettingsAPIBus.h>
#include <AzToolsFramework/Component/EditorComponentAPIBus.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <QDesktopServices>
#include <QUrl>

namespace Cesium
{
    void CesiumIonSession::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serialize = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serialize->Class<CesiumIonSession, AZ::Component>()->Version(0)->Field("ionAccessToken", &CesiumIonSession::m_ionAccessToken);
        }
    }

    void CesiumIonSession::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("CesiumIonSessionService"));
    }

    void CesiumIonSession::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("CesiumIonSessionService"));
    }

    void CesiumIonSession::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC_CE("CesiumService"));
    }

    void CesiumIonSession::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        dependent.push_back(AZ_CRC_CE("CesiumService"));
    }

    CesiumIonSession::CesiumIonSession()
        : m_asyncSystem{ CesiumInterface::Get()->GetTaskProcessor() }
        , m_assetAccessor{ CesiumInterface::Get()->GetAssetAccessor(IOKind::Http) }
    {
        if (CesiumIonSessionInterface::Get() == nullptr)
        {
            CesiumIonSessionInterface::Register(this);
        }
    }

    CesiumIonSession::~CesiumIonSession()
    {
        if (CesiumIonSessionInterface::Get() == this)
        {
            CesiumIonSessionInterface::Unregister(this);
        }
    }

    void CesiumIonSession::Init()
    {
    }

    void CesiumIonSession::Activate()
    {
        AZ::TickBus::Handler::BusConnect();
    }

    void CesiumIonSession::Deactivate()
    {
        AZ::TickBus::Handler::BusDisconnect();
    }

    void CesiumIonSession::OnTick([[maybe_unused]] float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        m_asyncSystem.dispatchMainThreadTasks();
    }

    void CesiumIonSession::SaveAccessToken(const AZStd::string& ionAccessToken)
    {
        m_ionAccessToken = ionAccessToken;
        AzToolsFramework::EditorSettingsAPIRequests::SettingOutcome outcome;
        AzToolsFramework::EditorSettingsAPIBus::BroadcastResult(
            outcome, &AzToolsFramework::EditorSettingsAPIBus::Events::SetValue, AZStd::string_view("CesiumIonSession|IonAccessToken"),
            AZStd::any(m_ionAccessToken));
        if (!outcome.IsSuccess())
        {
            CesiumInterface::Get()->GetLogger()->warn("Cannot save ion access token {}", outcome.GetError().c_str());
        }
    }

    void CesiumIonSession::ReadAccessToken()
    {
        AzToolsFramework::EditorSettingsAPIRequests::SettingOutcome outcome;
        AzToolsFramework::EditorSettingsAPIBus::BroadcastResult(
            outcome, &AzToolsFramework::EditorSettingsAPIBus::Events::GetValue, AZStd::string_view("CesiumIonSession|IonAccessToken"));
        if (!outcome.IsSuccess())
        {
            CesiumInterface::Get()->GetLogger()->warn("Cannot read ion access token {}", outcome.GetError().c_str());
        }
        else
        {
            const AZStd::any& result = outcome.GetValue();
            if (const AZStd::string* saveIonAccessToken = AZStd::any_cast<AZStd::string>(&result))
            {
                m_ionAccessToken = *saveIonAccessToken;
            }
        }
    }

    bool CesiumIonSession::IsConnected() const
    {
        return this->m_connection.has_value();
    }

    bool CesiumIonSession::IsConnecting() const
    {
        return this->m_isConnecting;
    }

    bool CesiumIonSession::IsResuming() const
    {
        return this->m_isResuming;
    }

    bool CesiumIonSession::IsProfileLoaded() const
    {
        return this->m_profile.has_value();
    }

    bool CesiumIonSession::IsLoadingProfile() const
    {
        return this->m_isLoadingProfile;
    }

    bool CesiumIonSession::IsAssetListLoaded() const
    {
        return this->m_assets.has_value();
    }

    bool CesiumIonSession::IsLoadingAssetList() const
    {
        return this->m_isLoadingAssets;
    }

    bool CesiumIonSession::IsTokenListLoaded() const
    {
        return this->m_tokens.has_value();
    }

    bool CesiumIonSession::IsLoadingTokenList() const
    {
        return this->m_isLoadingTokens;
    }

    bool CesiumIonSession::IsAssetAccessTokenLoaded() const
    {
        return this->m_assetAccessToken.has_value();
    }

    bool CesiumIonSession::IsLoadingAssetAccessToken() const
    {
        return this->m_isLoadingAssetAccessToken;
    }

    void CesiumIonSession::Connect()
    {
        if (this->IsConnecting() || this->IsConnected() || this->IsResuming())
        {
            return;
        }

        this->m_isConnecting = true;

        CesiumIonClient::Connection::authorize(
            this->m_asyncSystem, this->m_assetAccessor, "Cesium for Unreal", 190, "/cesium-for-unreal/oauth2/callback",
            { "assets:list", "assets:read", "profile:read", "tokens:read", "tokens:write", "geocode" },
            [this](const std::string& url)
            {
                this->m_authorizeUrl = url;
                QDesktopServices::openUrl(QUrl(this->m_authorizeUrl.c_str()));
            })
            .thenInMainThread(
                [this](CesiumIonClient::Connection&& connection)
                {
                    this->m_isConnecting = false;
                    this->m_connection = std::move(connection);
                    SaveAccessToken(this->m_connection.value().getAccessToken().c_str());
                    RefreshAssetAccessToken();
                    this->ConnectionUpdated.Signal();
                })
            .catchInMainThread(
                [this](std::exception&&)
                {
                    this->m_isConnecting = false;
                    this->m_connection = std::nullopt;
                    this->ConnectionUpdated.Signal();
                });
    }

    void CesiumIonSession::Resume()
    {
        if (this->m_isResuming)
        {
            return;
        }

        ReadAccessToken();

        if (m_ionAccessToken.empty())
        {
            // No existing session to resume.
            return;
        }

        this->m_isResuming = true;

        this->m_connection = CesiumIonClient::Connection(this->m_asyncSystem, this->m_assetAccessor, m_ionAccessToken.c_str());

        // Verify that the connection actually works.
        this->m_connection.value()
            .me()
            .thenInMainThread(
                [this](CesiumIonClient::Response<CesiumIonClient::Profile>&& response)
                {
                    if (!response.value.has_value())
                    {
                        this->m_connection.reset();
                    }
                    this->m_isResuming = false;
                    RefreshAssetAccessToken();
                    this->ConnectionUpdated.Signal();
                })
            .catchInMainThread(
                [this](std::exception&&)
                {
                    this->m_isResuming = false;
                    this->m_connection.reset();
                });
    }

    void CesiumIonSession::Disconnect()
    {
        this->m_connection.reset();
        this->m_profile.reset();
        this->m_assets.reset();
        this->m_tokens.reset();
        this->m_assetAccessToken.reset();

        SaveAccessToken("");

        this->ConnectionUpdated.Signal();
        this->ProfileUpdated.Signal();
        this->AssetsUpdated.Signal();
        this->TokensUpdated.Signal();
        this->AssetAccessTokenUpdated.Signal();
    }

    void CesiumIonSession::RefreshProfile()
    {
        if (!this->m_connection || this->m_isLoadingProfile)
        {
            this->m_loadProfileQueued = true;
            return;
        }

        this->m_isLoadingProfile = true;
        this->m_loadProfileQueued = false;

        this->m_connection->me()
            .thenInMainThread(
                [this](CesiumIonClient::Response<CesiumIonClient::Profile>&& profile)
                {
                    this->m_isLoadingProfile = false;
                    this->m_profile = std::move(profile.value);
                    this->ProfileUpdated.Signal();
                    this->RefreshProfileIfNeeded();
                })
            .catchInMainThread(
                [this](std::exception&&)
                {
                    this->m_isLoadingProfile = false;
                    this->m_profile = std::nullopt;
                    this->ProfileUpdated.Signal();
                    this->RefreshProfileIfNeeded();
                });
    }

    void CesiumIonSession::RefreshAssets()
    {
        if (!this->m_connection || this->m_isLoadingAssets)
        {
            return;
        }

        this->m_isLoadingAssets = true;
        this->m_loadAssetsQueued = false;

        this->m_connection->assets()
            .thenInMainThread(
                [this](CesiumIonClient::Response<CesiumIonClient::Assets>&& assets)
                {
                    this->m_isLoadingAssets = false;
                    this->m_assets = std::move(assets.value);
                    this->AssetsUpdated.Signal();
                    this->RefreshAssetsIfNeeded();
                })
            .catchInMainThread(
                [this](std::exception&&)
                {
                    this->m_isLoadingAssets = false;
                    this->m_assets = std::nullopt;
                    this->AssetsUpdated.Signal();
                    this->RefreshAssetsIfNeeded();
                });
    }

    void CesiumIonSession::RefreshTokens()
    {
        if (!this->m_connection || this->m_isLoadingAssets)
        {
            return;
        }

        this->m_isLoadingTokens = true;
        this->m_loadTokensQueued = false;

        this->m_connection->tokens()
            .thenInMainThread(
                [this](CesiumIonClient::Response<std::vector<CesiumIonClient::Token>>&& tokens)
                {
                    this->m_isLoadingTokens = false;
                    this->m_tokens = std::move(tokens.value);
                    this->TokensUpdated.Signal();
                    this->RefreshTokensIfNeeded();
                    this->RefreshAssetAccessToken();
                })
            .catchInMainThread(
                [this](std::exception&&)
                {
                    this->m_isLoadingTokens = false;
                    this->m_tokens = std::nullopt;
                    this->TokensUpdated.Signal();
                    this->RefreshTokensIfNeeded();
                });
    }

    void CesiumIonSession::RefreshAssetAccessToken()
    {
        if (this->m_isLoadingAssetAccessToken)
        {
            return;
        }

        if (!this->m_connection || !this->IsTokenListLoaded())
        {
            this->m_loadAssetAccessTokenQueued = true;
            this->RefreshTokens();
            return;
        }

        this->m_isLoadingAssetAccessToken = true;
        this->m_loadAssetAccessTokenQueued = false;

        std::string tokenName = PlatformInfo::GetProjectName().c_str();
        tokenName += " (Created by Cesium for O3DE)";

        const std::vector<CesiumIonClient::Token>& tokenList = this->GetTokens();
        const CesiumIonClient::Token* pFound = nullptr;

        for (auto& token : tokenList)
        {
            if (token.name == tokenName)
            {
                pFound = &token;
            }
        }

        if (pFound)
        {
            this->m_assetAccessToken = CesiumIonClient::Token(*pFound);
            this->m_isLoadingAssetAccessToken = false;
            RefreshAssets();
            this->AssetAccessTokenUpdated.Signal();
        }
        else
        {
            this->m_connection->createToken(tokenName, { "assets:read" }, std::nullopt)
                .thenInMainThread(
                    [this](CesiumIonClient::Response<CesiumIonClient::Token>&& token)
                    {
                        this->m_assetAccessToken = std::move(token.value);
                        this->m_isLoadingAssetAccessToken = false;
                        RefreshAssets();
                        this->AssetAccessTokenUpdated.Signal();
                    });
        }
    }

    const std::optional<CesiumIonClient::Connection>& CesiumIonSession::GetConnection() const
    {
        return this->m_connection;
    }

    const CesiumIonClient::Profile& CesiumIonSession::GetProfile()
    {
        static const CesiumIonClient::Profile empty{};
        if (this->m_profile)
        {
            return *this->m_profile;
        }
        else
        {
            this->RefreshProfile();
            return empty;
        }
    }

    const CesiumIonClient::Assets& CesiumIonSession::GetAssets()
    {
        static const CesiumIonClient::Assets empty;
        if (this->m_assets)
        {
            return *this->m_assets;
        }
        else
        {
            this->RefreshAssets();
            return empty;
        }
    }

    const std::vector<CesiumIonClient::Token>& CesiumIonSession::GetTokens()
    {
        static const std::vector<CesiumIonClient::Token> empty;
        if (this->m_tokens)
        {
            return *this->m_tokens;
        }
        else
        {
            this->RefreshTokens();
            return empty;
        }
    }

    const CesiumIonClient::Token& CesiumIonSession::GetAssetAccessToken()
    {
        static const CesiumIonClient::Token empty{};
        if (this->m_assetAccessToken)
        {
            return *this->m_assetAccessToken;
        }
        else
        {
            this->RefreshAssetAccessToken();
            return empty;
        }
    }

    const std::string& CesiumIonSession::GetAuthorizeUrl() const
    {
        return this->m_authorizeUrl;
    }

    AzToolsFramework::EntityIdList CesiumIonSession::GetSelectedEntities() const
    {
        using namespace AzToolsFramework;
        EntityIdList selectedEntities;
        ToolsApplicationRequestBus::BroadcastResult(
            selectedEntities, &AzToolsFramework::ToolsApplicationRequestBus::Events::GetSelectedEntities);
        if (selectedEntities.empty())
        {
            AZ::EntityId levelEntityId{};
            AzToolsFramework::ToolsApplicationRequestBus::BroadcastResult(
                levelEntityId, &AzToolsFramework::ToolsApplicationRequestBus::Events::GetCurrentLevelEntityId);

            selectedEntities.emplace_back(levelEntityId);
        }

        return selectedEntities;
    }

    void CesiumIonSession::AddTilesetToLevel(AZStd::shared_ptr<IonAssetItem> item)
    {
        using namespace AzToolsFramework;

        if (!item)
        {
            return;
        }

        const std::optional<CesiumIonClient::Connection>& connection = CesiumIonSessionInterface::Get()->GetConnection();
        if (!connection)
        {
            AZ_Printf("Cesium", "Cannot add an ion asset without an active connection");
            return;
        }

        connection->asset(item->m_tilesetIonAssetId)
            .thenInMainThread(
                [connection, item](CesiumIonClient::Response<CesiumIonClient::Asset>&& response)
                {
                    if (!response.value.has_value())
                    {
                        return connection->getAsyncSystem().createResolvedFuture<int64_t>(std::move(int64_t(item->m_tilesetIonAssetId)));
                    }

                    if (item->m_imageryIonAssetId >= 0)
                    {
                        return connection->asset(item->m_imageryIonAssetId)
                            .thenInMainThread(
                                [item](CesiumIonClient::Response<CesiumIonClient::Asset>&& overlayResponse)
                                {
                                    return overlayResponse.value.has_value() ? int64_t(-1) : int64_t(item->m_imageryIonAssetId);
                                });
                    }
                    else
                    {
                        return connection->getAsyncSystem().createResolvedFuture<int64_t>(-1);
                    }
                })
            .thenInMainThread(
                [this, item](int64_t missingAsset)
                {
                    if (missingAsset != -1)
                    {
                        return;
                    }

                    auto selectedEntities = GetSelectedEntities();
                    for (const AZ::EntityId& tilesetEntityId : selectedEntities)
                    {
                        AzToolsFramework::ScopedUndoBatch undoBatch("Add Ion Asset");

                        // create new entity and rename it to tileset name
                        AZ::Entity* tilesetEntity = nullptr;
                        AZ::ComponentApplicationBus::BroadcastResult(
                            tilesetEntity, &AZ::ComponentApplicationRequests::FindEntity, tilesetEntityId);
                        tilesetEntity->SetName(item->m_tilesetName);

                        // Add 3D Tiles component to the new entity
                        EditorComponentAPIRequests::AddComponentsOutcome tilesetComponentOutcomes;
                        EditorComponentAPIBus::BroadcastResult(
                            tilesetComponentOutcomes, &EditorComponentAPIBus::Events::AddComponentOfType, tilesetEntityId,
                            azrtti_typeid<CesiumTilesetEditorComponent>());
                        if (tilesetComponentOutcomes.IsSuccess())
                        {
                            TilesetCesiumIonSource ionSource;
                            ionSource.m_cesiumIonAssetId = item->m_tilesetIonAssetId;
                            ionSource.m_cesiumIonAssetToken = CesiumIonSessionInterface::Get()->GetAssetAccessToken().token.c_str();

                            TilesetSource tilesetSource;
                            tilesetSource.SetCesiumIon(ionSource);

                            EditorComponentAPIRequests::PropertyOutcome propertyOutcome;
                            EditorComponentAPIBus::BroadcastResult(
                                propertyOutcome, &EditorComponentAPIBus::Events::SetComponentProperty,
                                tilesetComponentOutcomes.GetValue().front(), AZStd::string_view("Source"), AZStd::any(tilesetSource));
                        }

                        // Add raster overlay to the new entity if there are any
                        if (item->m_imageryIonAssetId >= 0)
                        {
                            EditorComponentAPIRequests::AddComponentsOutcome rasterOverlayComponentOutcomes;
                            EditorComponentAPIBus::BroadcastResult(
                                rasterOverlayComponentOutcomes, &EditorComponentAPIBus::Events::AddComponentOfType, tilesetEntityId,
                                azrtti_typeid<CesiumIonRasterOverlayEditorComponent>());

                            if (rasterOverlayComponentOutcomes.IsSuccess())
                            {
                                CesiumIonRasterOverlaySource rasterOverlaySource;
                                rasterOverlaySource.m_ionAssetId = item->m_imageryIonAssetId;
                                rasterOverlaySource.m_ionToken = CesiumIonSessionInterface::Get()->GetAssetAccessToken().token.c_str();

                                EditorComponentAPIRequests::PropertyOutcome propertyOutcome;
                                EditorComponentAPIBus::BroadcastResult(
                                    propertyOutcome, &EditorComponentAPIBus::Events::SetComponentProperty,
                                    rasterOverlayComponentOutcomes.GetValue().front(), AZStd::string_view("Source"),
                                    AZStd::any(rasterOverlaySource));
                            }
                        }

                        PropertyEditorGUIMessages::Bus::Broadcast(
                            &PropertyEditorGUIMessages::RequestRefresh, PropertyModificationRefreshLevel::Refresh_AttributesAndValues);
                        undoBatch.MarkEntityDirty(tilesetEntityId);
                    }
                });
    }

    void CesiumIonSession::AddImageryToLevel(std::uint32_t ionImageryAssetId)
    {
        using namespace AzToolsFramework;

        const std::optional<CesiumIonClient::Connection>& connection = CesiumIonSessionInterface::Get()->GetConnection();
        if (!connection)
        {
            AZ_Printf("Cesium", "Cannot add an ion asset without an active connection");
            return;
        }

        connection->asset(ionImageryAssetId)
            .thenInMainThread(
                [this, ionImageryAssetId](CesiumIonClient::Response<CesiumIonClient::Asset>&& overlayResponse)
                {
                    if (overlayResponse.value.has_value())
                    {
                        auto selectedEntities = GetSelectedEntities();
                        for (const AZ::EntityId& tilesetEntityId : selectedEntities)
                        {
                            AzToolsFramework::ScopedUndoBatch undoBatch("Drape Ion Imagery");

                            bool hasTilesetComponent = false;
                            EditorComponentAPIBus::BroadcastResult(
                                hasTilesetComponent, &EditorComponentAPIBus::Events::HasComponentOfType, tilesetEntityId,
                                azrtti_typeid<CesiumTilesetEditorComponent>());

                            AZStd::vector<AZ::Uuid> componentsToAdd;
                            componentsToAdd.reserve(2);
                            if (!hasTilesetComponent)
                            {
                                componentsToAdd.emplace_back(azrtti_typeid<CesiumTilesetEditorComponent>());
                            }
                            componentsToAdd.emplace_back(azrtti_typeid<CesiumIonRasterOverlayEditorComponent>());

                            EditorComponentAPIRequests::AddComponentsOutcome componentOutcomes;
                            EditorComponentAPIBus::BroadcastResult(
                                componentOutcomes, &EditorComponentAPIBus::Events::AddComponentsOfType, tilesetEntityId, componentsToAdd);

                            if (componentOutcomes.IsSuccess())
                            {
                                // add CWT if there is no tileset component in the entity
                                if (!hasTilesetComponent)
                                {
                                    TilesetCesiumIonSource ionSource;
                                    ionSource.m_cesiumIonAssetId = 1;
                                    ionSource.m_cesiumIonAssetToken = CesiumIonSessionInterface::Get()->GetAssetAccessToken().token.c_str();

                                    TilesetSource tilesetSource;
                                    tilesetSource.SetCesiumIon(ionSource);

                                    EditorComponentAPIRequests::PropertyOutcome propertyOutcome;
                                    EditorComponentAPIBus::BroadcastResult(
                                        propertyOutcome, &EditorComponentAPIBus::Events::SetComponentProperty,
                                        componentOutcomes.GetValue().front(), AZStd::string_view("Source"), AZStd::any(tilesetSource));
                                }

                                // add raster overlay
                                CesiumIonRasterOverlaySource rasterOverlaySource;
                                rasterOverlaySource.m_ionAssetId = ionImageryAssetId;
                                rasterOverlaySource.m_ionToken = CesiumIonSessionInterface::Get()->GetAssetAccessToken().token.c_str();

                                EditorComponentAPIRequests::PropertyOutcome propertyOutcome;
                                EditorComponentAPIBus::BroadcastResult(
                                    propertyOutcome, &EditorComponentAPIBus::Events::SetComponentProperty,
                                    componentOutcomes.GetValue().back(), AZStd::string_view("Source"), AZStd::any(rasterOverlaySource));
                            }

                            PropertyEditorGUIMessages::Bus::Broadcast(
                                &PropertyEditorGUIMessages::RequestRefresh, PropertyModificationRefreshLevel::Refresh_AttributesAndValues);
                            undoBatch.MarkEntityDirty(tilesetEntityId);
                        }
                    }
                });
    }

    bool CesiumIonSession::RefreshProfileIfNeeded()
    {
        if (this->m_loadProfileQueued || !this->m_profile.has_value())
        {
            this->RefreshProfile();
        }
        return this->IsProfileLoaded();
    }

    bool CesiumIonSession::RefreshAssetsIfNeeded()
    {
        if (this->m_loadAssetsQueued || !this->m_assets.has_value())
        {
            this->RefreshAssets();
        }
        return this->IsAssetListLoaded();
    }

    bool CesiumIonSession::RefreshTokensIfNeeded()
    {
        if (this->m_loadTokensQueued || !this->m_tokens.has_value())
        {
            this->RefreshTokens();
        }
        return this->IsTokenListLoaded();
    }

    bool CesiumIonSession::RefreshAssetAccessTokenIfNeeded()
    {
        if (this->m_loadAssetAccessTokenQueued || !this->m_assetAccessToken.has_value())
        {
            this->RefreshAssetAccessToken();
        }
        return this->IsAssetAccessTokenLoaded();
    }
} // namespace Cesium
