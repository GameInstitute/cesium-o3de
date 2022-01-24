#pragma once

#include <Cesium/EBus/TilesetComponentBus.h>
#include <AzFramework/Viewport/ViewportId.h>
#include <AzFramework/Visibility/BoundsBus.h>
#include <AzCore/Component/Component.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/Component/TickBus.h>
#include <AzCore/Component/EntityBus.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace Cesium
{
    class TilesetComponent
        : public AZ::Component
        , public AZ::TickBus::Handler
        , public AZ::EntityBus::Handler
        , public AzFramework::BoundsRequestBus::Handler
        , public TilesetRequestBus::Handler
    {
    public:
        AZ_COMPONENT(TilesetComponent, "{56948418-6C82-4DF2-9A8D-C292C22FCBDF}", AZ::Component)

        static void Reflect(AZ::ReflectContext* context);

        static void GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided);

        static void GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible);

        static void GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required);

        static void GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent);

        TilesetComponent();

        void SetConfiguration(const TilesetConfiguration& configration) override;

        const TilesetConfiguration& GetConfiguration() const override;

        AZ::Aabb GetWorldBounds() override;

        AZ::Aabb GetLocalBounds() override;

        TilesetBoundingVolume GetBoundingVolumeInECEF() const override;

        void LoadTileset(const TilesetSource& source) override;

        void Init() override;

        void Activate() override;

        void Deactivate() override;

        using AZ::Component::SetEntity;

    private:
        static void ReflectTilesetBoundingVolume(AZ::ReflectContext* context);

        void OnTick(float deltaTime, AZ::ScriptTimePoint time) override;

        class CameraConfigurations;
        struct BoundingVolumeConverter;
        struct BoundingVolumeToAABB;
        struct BoundingVolumeTransform;
        enum class TilesetBoundingVolumeType;
        struct Impl;

        AZStd::unique_ptr<Impl> m_impl;
        TilesetConfiguration m_tilesetConfiguration;
        TilesetSource m_tilesetSource;
    };
} // namespace Cesium
