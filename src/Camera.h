#pragma once

#include <DirectXMath.h>

namespace sv {

class Camera {
public:
    void Reset();
    void Orbit(float deltaYaw, float deltaPitch);
    void Zoom(float delta);
    void Pan(float deltaX, float deltaY);
    void FocusOn(const DirectX::XMFLOAT3& center, float radius);
    void SetOrbitState(const DirectX::XMFLOAT3& target, float yaw, float pitch, float distance);
    void SetAspect(float aspect);

    DirectX::XMMATRIX ViewMatrix() const;
    DirectX::XMMATRIX ProjectionMatrix() const;
    DirectX::XMFLOAT3 Position() const { return position_; }
    DirectX::XMFLOAT3 Target() const { return target_; }
    DirectX::XMFLOAT3 ComputeKeyLightDirection(float pitchOffset, float sideOffset) const;

    float yaw() const { return yaw_; }
    float pitch() const { return pitch_; }
    float distance() const { return distance_; }

private:
    DirectX::XMFLOAT3 target_{0.f, 0.f, 0.f};
    DirectX::XMFLOAT3 position_{0.f, 0.f, 3.f};
    float yaw_ = 0.f;
    float pitch_ = 0.3f;
    float distance_ = 3.f;
    float fovY_ = DirectX::XM_PI / 4.f;
    float aspect_ = 16.f / 9.f;
    float nearZ_ = 0.01f;
    float farZ_ = 1000.f;

    void UpdatePosition();
    void SyncAnglesFromOffset(const DirectX::XMVECTOR& offset);
};

} // namespace sv
