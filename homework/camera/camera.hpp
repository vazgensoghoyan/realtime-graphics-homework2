#pragma once

#include <SDL3/SDL.h>
#include <math/aliases.hpp>

#include <numbers>
#include <unordered_set>
#include <array>

class Camera {
public:
    void handleEvent(const SDL_Event& event);
    void update(float dt);

    math::vector3f position() const { return m_position; }
    math::matrix4f viewMatrix() const;
    math::matrix4f projectionMatrix(float aspectRatio) const;

    std::array<math::vector3f, 8> frustumCorners(float aspectRatio) const;

private:
    std::array<math::vector3f, 3> cameraBasis() const;

private:
    static constexpr float s_speed = 0.15f;
    static constexpr float s_fastSpeed = 0.6f;
    static constexpr float s_mouseSensitivity = 0.003f;
    static constexpr float s_pitchLimit = std::numbers::pi_v<float> / 2.f - 0.01f;

    static constexpr float fovY = std::numbers::pi_v<float> / 3.f;

    math::vector3f m_position{0.f, 0.f, 0.f};
    float m_yaw = 0.f;
    float m_pitch = 0.f;

    std::unordered_set<SDL_Keycode> m_keydown;
};
