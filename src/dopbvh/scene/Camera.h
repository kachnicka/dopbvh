#pragma once

#if defined(_MSC_VER)
#    pragma warning(push)
#    pragma warning(disable : 4201)
#endif

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#if defined(_MSC_VER)
#    pragma warning(pop)
#endif

#include <algorithm>
#include <array>
#include <iostream>

#include "../backend/data/Input.h"
#include <berries/lib_helper/spdlog.h>
#include <vLime/types.h>

class Camera {
    static f32 constexpr RADIUS { 1.1f };
    static f32 constexpr SQRT_2 { 1.414213562373095f };
    static f32 constexpr ZOOM_SCALE { 75.f };
    static f32 constexpr RAD_PER_PIXEL { glm::pi<f32>() / 400.f };
    static f32 constexpr NAVIGATION_EPSILON { 1e-3f };

public:
    glm::vec3 pivot { 0.f, 0.f, 25.f };
    glm::vec3 up { 0.f, 0.f, 1.f };
    // position relative to pivot point for the orbiting camera
    glm::vec3 position { 0.f, -3500.f, 0.f };
    glm::quat orientation { glm::identity<glm::quat>() };

    glm::vec3 pivotCache { 0.f, 0.f, 0.f };
    glm::quat zOrientation { glm::identity<glm::quat>() };
    glm::quat xOrientation { glm::identity<glm::quat>() };
    glm::quat zOrientationCache { glm::identity<glm::quat>() };
    glm::quat xOrientationCache { glm::identity<glm::quat>() };

    glm::mat4 viewMat { glm::identity<glm::mat4>() };
    // OpenGL NDC with Y axis pointing up
    glm::mat4 projectionOpenGL { glm::identity<glm::mat4>() };
    // Vulkan NDC with Y axis pointing down
    glm::mat4 projectionVulkan { glm::identity<glm::mat4>() };

    glm::u32vec2 screenResolution { 1, 1 };
    f32 yFov { .33f };
    f32 zNear { 1.3f };
    f32 zFar { 33333.f };

    f32 panSpeedFactor { 1.f };
    f32 zoomSpeedFactor { 1.f };
    f32 zoomLimit { -5.f };

    void updateProjection()
    {
        f32 aspect { static_cast<f32>(screenResolution.x) / static_cast<f32>(screenResolution.y) };
        projectionOpenGL = glm::perspective(glm::pi<f32>() * yFov, aspect, zNear, zFar);
        projectionVulkan = projectionOpenGL;
        projectionVulkan[1][1] *= -1.f;
        changed = true;
    }

    bool changed { true };

    enum class NavigationState : u8 {
        eIdle,
        eRotation,
        ePanning,
    } navigationState { NavigationState::eIdle };

    double xPosCurrent { 0. };
    double yPosCurrent { 0. };
    double xPosCache { 0. };
    double yPosCache { 0. };

public:
    [[maybe_unused]] bool UpdateCamera()
    {
        ProcessNavigation();

        auto const p { glm::vec3(mat4_cast(orientation) * glm::vec4(position, 1.f)) + pivot };
        auto const u { glm::vec3(mat4_cast(orientation) * glm::vec4(up, 0.f)) };
        viewMat = glm::lookAt(p, pivot, u);

        auto result { changed };
        changed = false;
        return result;
    }

    enum class AngleUnit {
        eRad,
        eDeg,
    };

    [[maybe_unused]] void SetFovY(f32 angle, AngleUnit unit = AngleUnit::eRad)
    {
        if (unit == AngleUnit::eRad)
            yFov = angle;
        else
            yFov = glm::radians(angle);
        updateProjection();
    }

    void SetNearFar(f32 n = .1f, f32 f = 100.f)
    {
        zNear = n;
        zFar = f;
        updateProjection();
    }

    bool ScreenResize(u32 x, u32 y)
    {
        if (x == screenResolution.x && y == screenResolution.y)
            return false;

        static u32 constexpr RESOLUTION_MIN { 16 };
        static u32 constexpr RESOLUTION_MAX { 8192 };
        screenResolution.x = std::clamp(x, RESOLUTION_MIN, RESOLUTION_MAX);
        screenResolution.y = std::clamp(y, RESOLUTION_MIN, RESOLUTION_MAX);
        updateProjection();
        return true;
    }

    void Zoom(f32 units)
    {
        position.y += units * ZOOM_SCALE * zoomSpeedFactor;
        if (position.y > zoomLimit)
            position.y = zoomLimit;
        changed = true;
    }

    void RotationStart()
    {
        navigationState = NavigationState::eRotation;
        xPosCache = xPosCurrent;
        yPosCache = yPosCurrent;

        zOrientationCache = zOrientation;
        xOrientationCache = xOrientation;
    }

    void PanStart()
    {
        navigationState = NavigationState::ePanning;
        xPosCache = xPosCurrent;
        yPosCache = yPosCurrent;

        pivotCache = pivot;
    }

    void SetIdleState()
    {
        navigationState = NavigationState::eIdle;
    }

    void ProcessNavigation()
    {
        switch (navigationState) {
        case NavigationState::eIdle:
            return;
        case NavigationState::eRotation:
            Rotate();
            return;
        case NavigationState::ePanning:
            Pan();
            return;
        }
    }

    // rotates around global Z-up axis and local Y axis
    void Rotate()
    {
        f32 dx { static_cast<f32>(xPosCache - xPosCurrent) };
        f32 dy { static_cast<f32>(yPosCache - yPosCurrent) };

        if ((std::abs(dx) < NAVIGATION_EPSILON) && (std::abs(dy) < NAVIGATION_EPSILON))
            return;

        glm::quat change_1 { glm::vec3 { 0.f, 0.f, dx } * RAD_PER_PIXEL };
        glm::quat change_2 { glm::vec3 { dy, 0.f, 0.f } * RAD_PER_PIXEL };

        zOrientation = zOrientationCache * change_1;
        xOrientation = xOrientationCache * change_2;
        orientation = zOrientation * xOrientation;

        changed = true;
    }

    void AnimationRotation_TMP(f32 change = .2f)
    {
        glm::quat change_1 { glm::vec3 { 0.f, 0.f, change } * RAD_PER_PIXEL };

        zOrientation = zOrientationCache * change_1;
        orientation = zOrientation * xOrientation;

        changed = true;
    }

    // TODO: perspective correction
    void Pan()
    {
        f32 dx { static_cast<f32>(xPosCache - xPosCurrent) };
        f32 dy { static_cast<f32>(yPosCache - yPosCurrent) };

        // pivot cancels out in view vector calculation, as position is in relative coordinates
        auto const view { glm::normalize(glm::vec3(mat4_cast(orientation) * glm::vec4(position, 1.f))) };
        auto const actualUp { glm::vec3(mat4_cast(orientation) * glm::vec4(up, 0.f)) };
        auto const right { glm::cross(view, actualUp) };

        pivot = pivotCache - (right * dx + actualUp * dy) * panSpeedFactor;

        changed = true;
    }

    void AnimationPan_TMP(f32 dx, f32 dy)
    {
        // pivot cancels out in view vector calculation, as position is in relative coordinates
        auto const view { glm::normalize(glm::vec3(mat4_cast(orientation) * glm::vec4(position, 1.f))) };
        auto const actualUp { glm::vec3(mat4_cast(orientation) * glm::vec4(up, 0.f)) };
        auto const right { glm::cross(view, actualUp) };

        pivot = pivotCache - (right * dx + actualUp * dy) * panSpeedFactor;

        changed = true;
    }

    [[nodiscard]] backend::input::Camera GetMatrices() const
    {
        backend::input::Camera c;
        memcpy(c.view.data(), glm::value_ptr(viewMat), sizeof(c.view));
        memcpy(c.viewInv.data(), glm::value_ptr(glm::inverse(viewMat)), sizeof(c.viewInv));
        memcpy(c.projection.data(), glm::value_ptr(projectionVulkan), sizeof(c.projection));
        memcpy(c.projectionInv.data(), glm::value_ptr(glm::inverse(projectionVulkan)), sizeof(c.projectionInv));
        return c;
    }

    friend std::ostream& operator<<(std::ostream& os, Camera const& c);
    friend std::istream& operator>>(std::istream& is, Camera& c);
};

inline static std::ostream& serialize(std::ostream& os, glm::vec3 const& v)
{
    os << v.x << " " << v.y << " " << v.z << " ";
    return os;
}
inline static std::ostream& serialize(std::ostream& os, glm::quat const& q)
{
    os << q.x << " " << q.y << " " << q.z << " " << q.w << " ";
    return os;
}
inline static std::istream& deserialize(std::istream& is, glm::vec3& v)
{
    is >> v.x >> v.y >> v.z;
    return is;
}
inline static std::istream& deserialize(std::istream& is, glm::quat& q)
{
    is >> q.x >> q.y >> q.z >> q.w;
    return is;
}

inline std::ostream& operator<<(std::ostream& os, Camera const& c)
{
    serialize(os, c.pivot);
    serialize(os, c.up);
    serialize(os, c.position);
    serialize(os, c.orientation);

    serialize(os, c.zOrientation);
    serialize(os, c.xOrientation);
    serialize(os, c.zOrientationCache);
    serialize(os, c.xOrientationCache);

    os << c.yFov << " " << c.zNear << " " << c.zFar << "\n";
    return os;
}

inline std::istream& operator>>(std::istream& is, Camera& c)
{
    deserialize(is, c.pivot);
    deserialize(is, c.up);
    deserialize(is, c.position);
    deserialize(is, c.orientation);

    deserialize(is, c.zOrientation);
    deserialize(is, c.xOrientation);
    deserialize(is, c.zOrientationCache);
    deserialize(is, c.xOrientationCache);

    is >> c.yFov >> c.zNear >> c.zFar;
    return is;
}
