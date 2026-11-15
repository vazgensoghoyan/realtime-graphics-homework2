#include "camera.hpp"

#include <math/affine.hpp>
#include <math/projection.hpp>
#include <math/utils.hpp>

void Camera::handleEvent(const SDL_Event& event) {
    switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
        m_keydown.insert(event.key.key);
        break;
    case SDL_EVENT_KEY_UP:
        m_keydown.erase(event.key.key);
        break;
    case SDL_EVENT_MOUSE_MOTION:
        m_yaw += event.motion.xrel * s_mouseSensitivity;
        m_pitch -= event.motion.yrel * s_mouseSensitivity;
        m_pitch = math::clamp(m_pitch, -s_pitchLimit, s_pitchLimit);
        break;
    }
}

void Camera::update(float dt) {
    const float step = (m_keydown.contains(SDLK_SPACE) ? s_fastSpeed : s_speed) * dt;

    const math::vector3f forward{
        std::sin(m_yaw) * std::cos(m_pitch),
        std::sin(m_pitch),
        -std::cos(m_yaw) * std::cos(m_pitch),
    };
    const math::vector3f right{std::cos(m_yaw), 0.f, std::sin(m_yaw)};

    math::vector3f direction{};
    if (m_keydown.contains(SDLK_W)) direction += forward;
    if (m_keydown.contains(SDLK_S)) direction -= forward;
    if (m_keydown.contains(SDLK_A)) direction -= right;
    if (m_keydown.contains(SDLK_D)) direction += right;

    if (math::length_squared(direction) > 0.f) {
        m_position += math::normalized(direction) * step;
    }
}

math::matrix4f Camera::viewMatrix() const {
    return math::rotation_yz(-m_pitch)
         * math::rotation_xz(-m_yaw)
         * math::translation(-m_position);
}

math::matrix4f Camera::projectionMatrix(float aspectRatio) const {
    return math::perspective(std::numbers::pi_v<float> / 3.f, aspectRatio, 0.01f, 300.f);
}
