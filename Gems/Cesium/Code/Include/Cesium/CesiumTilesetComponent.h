#pragma once

#include <Cesium/CesiumTilesetComponentBus.h>
#include <AzFramework/Viewport/ViewportId.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Component/EntityBus.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace Cesium
{
    class CesiumTilesetComponent
        : public AZ::Component
        , public AZ::TickBus::Handler
        , public AZ::EntityBus::Handler
        , public CesiumTilesetRequestBus::Handler
        , private AZ::TransformNotificationBus::Handler
    {
    public:
        AZ_COMPONENT(CesiumTilesetComponent, "{56948418-6C82-4DF2-9A8D-C292C22FCBDF}", AZ::Component)

        static void Reflect(AZ::ReflectContext* context);

        CesiumTilesetComponent();

        void SetConfiguration(const CesiumTilesetConfiguration& configration) override;

        const CesiumTilesetConfiguration& GetConfiguration() const override;

        void SetCoordinateTransform(const AZ::EntityId& coordinateTransformEntityId) override;

        TilesetBoundingVolume GetBoundingVolumeInECEF() const override;

        void LoadTileset(const TilesetSource& source) override;

    private:
        void Init() override;

        void Activate() override;

        void Deactivate() override;

        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        void OnTransformChanged(const AZ::Transform& local, const AZ::Transform& world) override;

        class CameraConfigurations;
        struct BoundingVolumeConverter;
        struct BoundingVolumeTransform;
        struct EntityWrapper;
        struct LocalFileSource;
        struct UrlSource;
        struct CesiumIonSource;
        struct Impl;
        AZStd::unique_ptr<Impl> m_impl;
    };
} // namespace Cesium
