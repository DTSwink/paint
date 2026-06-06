#include "Camera.h"
#include <algorithm>
#include <cmath>

namespace sv {

void Camera::Reset() {
    target_ = {0.f, 0.f, 0.f};
    yaw_ = 0.f;
    pitch_ = 0.3f;
    distance_ = 3.f;
    UpdatePosition();
}

void Camera::FocusOn(const DirectX::XMFLOAT3& center, float radius) {
    target_ = center;
    distance_ = std::max(radius * 2.5f, 0.5f);
    yaw_ = 0.f;
    pitch_ = 0.25f;
    UpdatePosition();
}

void Camera::SetOrbitState(const DirectX::XMFLOAT3& target, float yaw, float pitch, float distance) {
    target_ = target;
    yaw_ = yaw;
    const float limit = DirectX::XM_PIDIV2 - 0.01f;
    pitch_ = std::max(-limit, std::min(limit, pitch));
    distance_ = std::max(distance, 0.1f);
    UpdatePosition();
}

void Camera::Orbit(float deltaYaw, float deltaPitch) {
    using namespace DirectX;

    const XMVECTOR targetV = XMLoadFloat3(&target_);
    XMVECTOR offset = XMVectorSubtract(XMLoadFloat3(&position_), targetV);

    const float dist = XMVectorGetX(XMVector3Length(offset));
    if (dist < 1e-5f) return;

    const XMVECTOR worldUp = XMVectorSet(0.f, 1.f, 0.f, 0.f);

    if (deltaYaw != 0.f) {
        offset = XMVector3TransformNormal(offset, XMMatrixRotationAxis(worldUp, deltaYaw));
    }

    if (deltaPitch != 0.f) {
        XMVECTOR right = XMVector3Cross(worldUp, offset);
        const float rightLenSq = XMVectorGetX(XMVector3LengthSq(right));
        if (rightLenSq > 1e-6f) {
            right = XMVector3Normalize(right);
            const XMVECTOR pitched = XMVector3TransformNormal(offset, XMMatrixRotationAxis(right, deltaPitch));
            const float upDot = XMVectorGetX(XMVector3Dot(XMVector3Normalize(pitched), worldUp));
            if (std::abs(upDot) <= 0.98f) {
                offset = pitched;
            }
        }
    }

    offset = XMVector3Normalize(offset);
    offset = XMVectorScale(offset, dist);
    XMStoreFloat3(&position_, XMVectorAdd(targetV, offset));
    distance_ = dist;
    SyncAnglesFromOffset(offset);
}

void Camera::Zoom(float delta) {
    distance_ = std::max(0.1f, distance_ + delta);
    UpdatePosition();
}

void Camera::Pan(float deltaX, float deltaY) {
    using namespace DirectX;
    const XMVECTOR eye = XMLoadFloat3(&position_);
    const XMVECTOR at = XMLoadFloat3(&target_);
    const XMVECTOR up = XMVectorSet(0.f, 1.f, 0.f, 0.f);
    const XMVECTOR forward = XMVector3Normalize(XMVectorSubtract(at, eye));
    const XMVECTOR right = XMVector3Normalize(XMVector3Cross(forward, up));
    const XMVECTOR camUp = XMVector3Normalize(XMVector3Cross(right, forward));
    const XMVECTOR offset = XMVectorAdd(XMVectorScale(right, deltaX), XMVectorScale(camUp, deltaY));
    XMStoreFloat3(&target_, XMVectorAdd(at, offset));
    UpdatePosition();
}

void Camera::SetAspect(float aspect) {
    aspect_ = aspect;
}

DirectX::XMMATRIX Camera::ViewMatrix() const {
    using namespace DirectX;
    return XMMatrixLookAtLH(XMLoadFloat3(&position_), XMLoadFloat3(&target_), XMVectorSet(0.f, 1.f, 0.f, 0.f));
}

DirectX::XMMATRIX Camera::ProjectionMatrix() const {
    using namespace DirectX;
    return XMMatrixPerspectiveFovLH(fovY_, aspect_, nearZ_, farZ_);
}

void Camera::UpdatePosition() {
    using namespace DirectX;
    const float x = distance_ * std::cosf(pitch_) * std::sinf(yaw_);
    const float y = distance_ * std::sinf(pitch_);
    const float z = distance_ * std::cosf(pitch_) * std::cosf(yaw_);
    position_ = {target_.x + x, target_.y + y, target_.z + z};
}

void Camera::SyncAnglesFromOffset(const DirectX::XMVECTOR& offset) {
    using namespace DirectX;
    const float x = XMVectorGetX(offset);
    const float y = XMVectorGetY(offset);
    const float z = XMVectorGetZ(offset);
    pitch_ = std::asin(std::clamp(y / distance_, -1.f, 1.f));
    yaw_ = std::atan2f(x, z);
}

DirectX::XMFLOAT3 Camera::ComputeKeyLightDirection(float pitchOffset, float sideOffset) const {
    using namespace DirectX;
    const XMVECTOR eye = XMLoadFloat3(&position_);
    const XMVECTOR at = XMLoadFloat3(&target_);
    const XMVECTOR worldUp = XMVectorSet(0.f, 1.f, 0.f, 0.f);
    const XMVECTOR forward = XMVector3Normalize(XMVectorSubtract(at, eye));
    const XMVECTOR right = XMVector3Normalize(XMVector3Cross(forward, worldUp));
    const XMVECTOR up = XMVector3Cross(right, forward);
    const XMVECTOR toLight = XMVector3Normalize(XMVectorAdd(
        XMVectorNegate(forward),
        XMVectorAdd(XMVectorScale(up, pitchOffset), XMVectorScale(right, sideOffset))));
    DirectX::XMFLOAT3 lightDirection{};
    XMStoreFloat3(&lightDirection, XMVectorNegate(toLight));
    return lightDirection;
}

} // namespace sv
