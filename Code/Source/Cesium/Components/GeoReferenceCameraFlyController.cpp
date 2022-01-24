#include <Cesium/Components/GeoReferenceCameraFlyController.h>
#include <Cesium/EBus/OriginShiftComponentBus.h>
#include "Cesium/Math/MathHelper.h"
#include "Cesium/Math/GeoReferenceInterpolator.h"
#include "Cesium/Math/LinearInterpolator.h"
#include "Cesium/Math/MathReflect.h"
#include <AzFramework/Input/Devices/Mouse/InputDeviceMouse.h>
#include <AzFramework/Input/Devices/Keyboard/InputDeviceKeyboard.h>
#include <AzFramework/Components/CameraBus.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/RTTI/BehaviorContext.h>
#include <CesiumGeospatial/Transforms.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Cesium
{
    void GeoReferenceCameraFlyController::Reflect(AZ::ReflectContext* context)
    {
        if (AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context))
        {
            serializeContext->Class<GeoReferenceCameraFlyController, AZ::Component>()->Version(0)->
                Field("mouseSensitivity", &GeoReferenceCameraFlyController::m_mouseSensitivity)->
                Field("movementSpeed", &GeoReferenceCameraFlyController::m_movementSpeed)->
                Field("panningSpeed", &GeoReferenceCameraFlyController::m_panningSpeed)
                ;
        }
    }

    void GeoReferenceCameraFlyController::GetProvidedServices(AZ::ComponentDescriptor::DependencyArrayType& provided)
    {
        provided.push_back(AZ_CRC_CE("GeoReferenceCameraFlyControllerService"));
    }

    void GeoReferenceCameraFlyController::GetIncompatibleServices(AZ::ComponentDescriptor::DependencyArrayType& incompatible)
    {
        incompatible.push_back(AZ_CRC_CE("GeoReferenceCameraFlyControllerService"));
    }

    void GeoReferenceCameraFlyController::GetRequiredServices(AZ::ComponentDescriptor::DependencyArrayType& required)
    {
        required.push_back(AZ_CRC("TransformService", 0x8ee22c50));
    }

    void GeoReferenceCameraFlyController::GetDependentServices(AZ::ComponentDescriptor::DependencyArrayType& dependent)
    {
        dependent.push_back(AZ_CRC("TransformService", 0x8ee22c50));
    }

    GeoReferenceCameraFlyController::GeoReferenceCameraFlyController()
        : m_cameraFlyState{CameraFlyState::NoFly}
        , m_mouseSensitivity{ 1.0 }
        , m_movementSpeed{ 1.0 }
        , m_panningSpeed{ 1.0 }
        , m_cameraPitch{}
        , m_cameraHead{}
        , m_cameraMovement{}
        , m_cameraRotateUpdate{ false }
        , m_cameraMoveUpdate{ false }
    {
    }

    void GeoReferenceCameraFlyController::Init()
    {
    }

    void GeoReferenceCameraFlyController::Activate()
    {
        GeoReferenceCameraFlyControllerRequestBus::Handler::BusConnect(GetEntityId());
        AZ::TickBus::Handler::BusConnect();
        AzFramework::InputChannelEventListener::Connect();
    }

    void GeoReferenceCameraFlyController::Deactivate()
    {
        GeoReferenceCameraFlyControllerRequestBus::Handler::BusDisconnect();
        AZ::TickBus::Handler::BusDisconnect();
        AzFramework::InputChannelEventListener::Disconnect();

        StopFly();
        ResetCameraMovement();
    }

    void GeoReferenceCameraFlyController::SetMouseSensitivity(double mouseSensitivity)
    {
        m_mouseSensitivity = mouseSensitivity;
    }

    double GeoReferenceCameraFlyController::GetMouseSensitivity() const
    {
        return m_mouseSensitivity;
    }

    void GeoReferenceCameraFlyController::SetPanningSpeed(double panningSpeed)
    {
        m_panningSpeed = panningSpeed;
    }

    double GeoReferenceCameraFlyController::GetPanningSpeed() const
    {
        return m_panningSpeed;
    }

    void GeoReferenceCameraFlyController::SetMovementSpeed(double movementSpeed)
    {
        m_movementSpeed = movementSpeed;
    }

    double GeoReferenceCameraFlyController::GetMovementSpeed() const
    {
        return m_movementSpeed;
    }

    void GeoReferenceCameraFlyController::FlyToECEFLocation(const glm::dvec3& location, const glm::dvec3& direction)
    {
        // Get camera current O3DE world transform to calculate its ECEF position and orientation
        AZ::Transform azO3DECameraTransform = AZ::Transform::CreateIdentity();
        AZ::TransformBus::EventResult(azO3DECameraTransform, GetEntityId(), &AZ::TransformBus::Events::GetWorldTM);

        // Get camera configuration for the interpolator
        Camera::Configuration cameraConfiguration;
        Camera::CameraRequestBus::EventResult(
            cameraConfiguration, GetEntityId(), &Camera::CameraRequestBus::Events::GetCameraConfiguration);

        // Get current origin
        glm::dvec3 origin{ 0.0 };
        OriginShiftRequestBus::BroadcastResult(origin, &OriginShiftRequestBus::Events::GetOrigin);

        // Get the current ecef position and orientation of the camera
        glm::dmat4 o3deCameraTransform = MathHelper::ConvertTransformAndScaleToDMat4(azO3DECameraTransform, AZ::Vector3::CreateOne());
        o3deCameraTransform[3] += glm::dvec4(origin, 0.0);
        glm::dvec3 o3deCameraPosition = o3deCameraTransform[3];
        if (m_ecefPositionInterpolator)
        {
            m_ecefPositionInterpolator = AZStd::make_unique<GeoReferenceInterpolator>(
                o3deCameraPosition, o3deCameraTransform[1], location, direction, o3deCameraTransform, cameraConfiguration);
        }
        else
        {
            m_ecefPositionInterpolator =
                AZStd::make_unique<LinearInterpolator>(o3deCameraPosition, o3deCameraTransform[1], location, direction);
        }

        // transition to the new state
        m_cameraFlyState = CameraFlyState::MidFly;
    }

    void GeoReferenceCameraFlyController::BindCameraStopFlyEventHandler(CameraStopFlyEvent::Handler& handler)
    {
        handler.Connect(m_stopFlyEvent);
    }

    void GeoReferenceCameraFlyController::OnTick(float deltaTime, [[maybe_unused]] AZ::ScriptTimePoint time)
    {
        switch (m_cameraFlyState)
        {
        case CameraFlyState::MidFly:
            ProcessMidFlyState(deltaTime);
            break;
        case CameraFlyState::NoFly:
            ProcessNoFlyState();
            break;
        default:
            break;
        }
    }

    void GeoReferenceCameraFlyController::ProcessMidFlyState(float deltaTime)
    {
        assert(m_ecefPositionInterpolator != nullptr);
        m_ecefPositionInterpolator->Update(deltaTime);
        glm::dvec3 cameraPosition = m_ecefPositionInterpolator->GetCurrentPosition();
        glm::dquat cameraOrientation = m_ecefPositionInterpolator->GetCurrentOrientation();

        // find camera relative position. Move origin if its relative position is over 10000.0
        glm::dvec3 origin{ 0.0 };
        OriginShiftRequestBus::BroadcastResult(origin, &OriginShiftRequestBus::Events::GetOrigin);
        glm::dvec3 relativeCameraPosition = cameraPosition - origin;

        AZ::Transform cameraTransform = AZ::Transform::CreateIdentity();
        cameraTransform.SetRotation(AZ::Quaternion(
            static_cast<float>(cameraOrientation.x), static_cast<float>(cameraOrientation.y), static_cast<float>(cameraOrientation.z),
            static_cast<float>(cameraOrientation.w)));
        if (glm::abs(relativeCameraPosition.x) < ORIGIN_SHIFT_DISTANCE && glm::abs(relativeCameraPosition.y) < ORIGIN_SHIFT_DISTANCE &&
            glm::abs(relativeCameraPosition.z) < ORIGIN_SHIFT_DISTANCE)
        {
            cameraTransform.SetTranslation(AZ::Vector3(
                static_cast<float>(cameraPosition.x), static_cast<float>(cameraPosition.y), static_cast<float>(cameraPosition.z)));
        }
        else
        {
            cameraTransform.SetTranslation(AZ::Vector3(0.0));
        }

        AZ::TransformBus::Event(GetEntityId(), &AZ::TransformBus::Events::SetWorldTM, cameraTransform);

        // if the interpolator stops updating, then we transition to the end state
        if (m_ecefPositionInterpolator->IsStop())
        {
            StopFly();
        }
    }

    void GeoReferenceCameraFlyController::ProcessNoFlyState()
    {
        if (m_cameraRotateUpdate || m_cameraMoveUpdate)
        {
            // Get camera current world transform
            AZ::Transform relativeCameraTransform = AZ::Transform::CreateIdentity();
            AZ::TransformBus::EventResult(relativeCameraTransform, GetEntityId(), &AZ::TransformBus::Events::GetWorldTM);

            // calculate ENU
            glm::dvec3 origin{ 0.0 };
            OriginShiftRequestBus::BroadcastResult(origin, &OriginShiftRequestBus::Events::GetOrigin);
            glm::dvec4 currentPosition = glm::dvec4(origin + MathHelper::ToDVec3(relativeCameraTransform.GetTranslation()), 1.0);
            glm::dmat4 enu = CesiumGeospatial::Transforms::eastNorthUpToFixedFrame(currentPosition);

            // calculate new camera orientation, adjust for ENU coordinate
            glm::dquat totalRotationQuat = glm::dquat(enu) * glm::dquat(glm::dvec3(m_cameraPitch, 0.0, m_cameraHead));
            relativeCameraTransform.SetRotation(AZ::Quaternion(
                static_cast<float>(totalRotationQuat.x), static_cast<float>(totalRotationQuat.y), static_cast<float>(totalRotationQuat.z),
                static_cast<float>(totalRotationQuat.w)));

            // calculate camera position
            glm::dvec3 moveX = m_cameraMovement.x * MathHelper::ToDVec3(relativeCameraTransform.GetBasisX());
            glm::dvec3 moveY = m_cameraMovement.y * MathHelper::ToDVec3(relativeCameraTransform.GetBasisY());
            glm::dvec3 moveZ = m_cameraMovement.z * MathHelper::ToDVec3(relativeCameraTransform.GetBasisZ());
            glm::dvec3 newPosition = MathHelper::ToDVec3(relativeCameraTransform.GetTranslation()) + moveX + moveY + moveZ;
            if (glm::abs(newPosition.x) < ORIGIN_SHIFT_DISTANCE && glm::abs(newPosition.y) < ORIGIN_SHIFT_DISTANCE &&
                glm::abs(newPosition.z) < ORIGIN_SHIFT_DISTANCE)
            {
                relativeCameraTransform.SetTranslation(
                    AZ::Vector3{ static_cast<float>(newPosition.x), static_cast<float>(newPosition.y), static_cast<float>(newPosition.z) });
            }
            else
            {
                relativeCameraTransform.SetTranslation(AZ::Vector3{ 0.0f });
                OriginShiftRequestBus::Broadcast(&OriginShiftRequestBus::Events::ShiftOrigin, newPosition);
            }

            AZ::TransformBus::Event(GetEntityId(), &AZ::TransformBus::Events::SetWorldTM, relativeCameraTransform);
        }
    }

    bool GeoReferenceCameraFlyController::OnInputChannelEventFiltered(const AzFramework::InputChannel& inputChannel)
    {
        const AzFramework::InputDevice& inputDevice = inputChannel.GetInputDevice();
        const AzFramework::InputDeviceId& inputDeviceId = inputDevice.GetInputDeviceId();
        if (AzFramework::InputDeviceMouse::IsMouseDevice(inputDeviceId))
        {
            OnMouseEvent(inputChannel);
        }

        if (AzFramework::InputDeviceKeyboard::IsKeyboardDevice(inputDeviceId))
        {
            OnKeyEvent(inputChannel);
        }

        return false;
    }

    void GeoReferenceCameraFlyController::OnMouseEvent(const AzFramework::InputChannel& inputChannel)
    {
        // process mouse inputs
        AzFramework::InputChannel::State state = inputChannel.GetState();
        const AzFramework::InputChannelId& inputChannelId = inputChannel.GetInputChannelId();
        if (state == AzFramework::InputChannel::State::Began || state == AzFramework::InputChannel::State::Updated)
        {
            double inputValue = inputChannel.GetValue();
            if (m_cameraRotateUpdate && inputChannelId == AzFramework::InputDeviceMouse::Movement::X)
            {
                m_cameraHead += glm::radians(-inputValue / 360.0 * m_mouseSensitivity);
            }
            else if (m_cameraRotateUpdate && inputChannelId == AzFramework::InputDeviceMouse::Movement::Y)
            {
                m_cameraPitch += glm::radians(-inputValue / 360.0 * m_mouseSensitivity);
            }
            else if (inputChannelId == AzFramework::InputDeviceMouse::Button::Right)
            {
                m_cameraRotateUpdate = true;
            }
        }
        else if (state == AzFramework::InputChannel::State::Ended)
        {
            if (inputChannelId == AzFramework::InputDeviceMouse::Button::Right)
            {
                m_cameraRotateUpdate = false;
            }
        }
    }

    void GeoReferenceCameraFlyController::OnKeyEvent(const AzFramework::InputChannel& inputChannel)
    {
        // process mouse inputs
        AzFramework::InputChannel::State state = inputChannel.GetState();
        const AzFramework::InputChannelId& inputChannelId = inputChannel.GetInputChannelId();
        if (state == AzFramework::InputChannel::State::Began || state == AzFramework::InputChannel::State::Updated)
        {
            double inputValue = inputChannel.GetValue();
            if (inputChannelId == AzFramework::InputDeviceKeyboard::Key::AlphanumericA)
            {
                m_cameraMovement.x = -inputValue * m_panningSpeed;
                m_cameraMoveUpdate = true;
            }
            else if (inputChannelId == AzFramework::InputDeviceKeyboard::Key::AlphanumericD)
            {
                m_cameraMovement.x = inputValue * m_panningSpeed;
                m_cameraMoveUpdate = true;
            }
            else if (inputChannelId == AzFramework::InputDeviceKeyboard::Key::AlphanumericS)
            {
                m_cameraMovement.y = -inputValue * m_movementSpeed;
                m_cameraMoveUpdate = true;
            }
            else if (inputChannelId == AzFramework::InputDeviceKeyboard::Key::AlphanumericW)
            {
                m_cameraMovement.y = inputValue * m_movementSpeed;
                m_cameraMoveUpdate = true;
            }
            else if (inputChannelId == AzFramework::InputDeviceKeyboard::Key::AlphanumericQ)
            {
                m_cameraMovement.z = -inputValue * m_panningSpeed;
                m_cameraMoveUpdate = true;
            }
            else if (inputChannelId == AzFramework::InputDeviceKeyboard::Key::AlphanumericE)
            {
                m_cameraMovement.z = inputValue * m_panningSpeed;
                m_cameraMoveUpdate = true;
            }
        }
        else if (state == AzFramework::InputChannel::State::Ended)
        {
            m_cameraMovement = glm::dvec3{ 0.0 };
            m_cameraMoveUpdate = false;
        }
    }

    void GeoReferenceCameraFlyController::StopFly()
    {
        // stop mid fly
        if (m_cameraFlyState != CameraFlyState::NoFly)
        {
            assert(m_ecefPositionInterpolator != nullptr);

            // inform camera stop flying
            glm::dvec3 ecefCurrentPosition = m_ecefPositionInterpolator->GetCurrentPosition();
            AZ::Transform worldTM{};
            AZ::TransformBus::EventResult(worldTM, GetEntityId(), &AZ::TransformBus::Events::GetWorldTM);
            AZ::Vector3 worldOrientation = worldTM.GetRotation().GetEulerRadians();
            m_cameraPitch = worldOrientation.GetX();
            m_cameraHead = worldOrientation.GetZ();
            m_ecefPositionInterpolator = nullptr;
            m_stopFlyEvent.Signal(ecefCurrentPosition);

            // transition to no fly state
            m_cameraFlyState = CameraFlyState::NoFly;
        }
    }

    void GeoReferenceCameraFlyController::ResetCameraMovement()
    {
        m_cameraMovement = glm::dvec3{ 0.0 };
        m_cameraRotateUpdate = false;
        m_cameraMoveUpdate = false;
    }
} // namespace Cesium
