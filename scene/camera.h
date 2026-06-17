#pragma once
#include "../os/os.h"
#include "../render/scene.h"

namespace mg {
namespace scene {

enum class CameraMode : u8 {
    FREE_FLY = 0,
    ORBIT    = 1,
    FOLLOW   = 2,
};

// ============================================================================
// CameraController — first-class camera as a SceneNode
// ============================================================================

struct CameraController {
    CameraMode mode = CameraMode::ORBIT;
    u32 node_index = 0xFFFFFFFF; // SceneNode index in SceneGraph

    // Orbit mode
    Vec3  orbit_target     = {0, 0, 0};
    f32   orbit_distance   = 5.0f;
    f32   orbit_azimuth    = 0.0f;  // radians
    f32   orbit_elevation  = 0.5f;  // radians

    // Free fly
    Vec3  fly_position     = {0, 2, 5};
    Vec3  fly_velocity     = {0, 0, 0};
    f32   fly_speed        = 3.0f;
    f32   smooth_speed     = 6.0f;    // exponential smoothing factor
    f32   sensitivity      = 0.001f;  // radians per pixel
    f32   yaw              = 0.0f;
    f32   pitch            = 0.0f;

    // Smooth zoom
    f32   target_distance  = 5.0f;
    f32   zoom_smooth_speed = 8.0f;

    // Follow mode
    u32   follow_target    = 0xFFFFFFFF;
    Vec3  follow_offset    = {0, 2, 5};

    // Apply to render::Camera (for use with existing render pipeline)
    void applyTo(Camera& cam) {
        Vec3 pos = getPosition();
        Vec3 tgt = getTarget();
        cam.position = pos;
        cam.target   = tgt;
    }

    void setOrbit(Vec3 target, f32 distance, f32 azimuth, f32 elevation) {
        mode = CameraMode::ORBIT;
        orbit_target     = target;
        orbit_distance   = distance;
        target_distance  = distance;
        orbit_azimuth    = azimuth;
        orbit_elevation  = elevation;
    }

    void setFreeFly(Vec3 pos, f32 yaw_rad, f32 pitch_rad) {
        mode = CameraMode::FREE_FLY;
        yaw   = yaw_rad;
        pitch = pitch_rad;
        if (node_index < 1024) {
            (void)pos; // position derived from yaw/pitch
        }
    }

    void setFollow(u32 target_node, Vec3 offset) {
        mode = CameraMode::FOLLOW;
        follow_target = target_node;
        follow_offset = offset;
    }

    Vec3 getPosition() const {
        if (mode == CameraMode::ORBIT) {
            f32 cx = std::cos(orbit_azimuth);
            f32 sx = std::sin(orbit_azimuth);
            f32 cy = std::cos(orbit_elevation);
            f32 sy = std::sin(orbit_elevation);
            Vec3 offset = {cx * cy, sy, sx * cy};
            offset = offset * orbit_distance;
            return {orbit_target.x - offset.x,
                    orbit_target.y - offset.y,
                    orbit_target.z - offset.z};
        }
        if (mode == CameraMode::FREE_FLY) {
            return fly_position;
        }
        return {0, 2, 5};
    }

    Vec3 getTarget() const {
        if (mode == CameraMode::ORBIT) return orbit_target;
        if (mode == CameraMode::FOLLOW) {
            (void)follow_target;
            return {0, 0, 0};
        }
        // FREE_FLY: look along yaw/pitch
        f32 cy = std::cos(yaw);
        f32 sy = std::sin(yaw);
        f32 cp = std::cos(pitch);
        f32 sp = std::sin(pitch);
        Vec3 fwd = {cy * cp, sp, sy * cp};
        return getPosition() + fwd;
    }

    // Get forward direction
    Vec3 getForward() const {
        Vec3 tgt = getTarget();
        Vec3 pos = getPosition();
        Vec3 fwd = {tgt.x - pos.x, tgt.y - pos.y, tgt.z - pos.z};
        f32 l = std::sqrt(fwd.x*fwd.x + fwd.y*fwd.y + fwd.z*fwd.z);
        if (l > 0) { fwd.x /= l; fwd.y /= l; fwd.z /= l; }
        return fwd;
    }

    // Rotate orbit around target (for mouse drag)
    void orbitRotate(f32 dx, f32 dy) {
        if (mode != CameraMode::ORBIT) {
            mode = CameraMode::ORBIT;
            Vec3 pos = getPosition();
            Vec3 fwd = getForward();
            orbit_distance = std::sqrt(pos.x*pos.x + pos.y*pos.y + pos.z*pos.z);
            target_distance = orbit_distance;
            orbit_azimuth = std::atan2(pos.z, pos.x);
            orbit_elevation = std::asin(pos.y / orbit_distance);
        }
        orbit_azimuth   += dx * sensitivity * 10.0f;
        orbit_elevation += dy * sensitivity * 10.0f;
        orbit_elevation = std::fmax(-1.5f, std::fmin(1.5f, orbit_elevation));
    }

    // Zoom (change orbit distance or fly speed)
    void zoom(f32 delta) {
        if (mode == CameraMode::ORBIT) {
            target_distance = std::fmax(0.1f, target_distance - delta);
        } else {
            fly_speed = std::fmax(0.1f, fly_speed - delta * 0.5f);
        }
    }

    // Per-frame smoothing update (call once per frame with real dt)
    void updateSmoothing(f32 dt) {
        if (mode == CameraMode::ORBIT) {
            f32 factor = 1.0f - std::exp(-zoom_smooth_speed * dt);
            orbit_distance += (target_distance - orbit_distance) * factor;
        }
    }

    // Update position from fly input
    void updateFly(f32 dt, bool forward, bool backward, bool left, bool right, bool up, bool down) {
        if (mode != CameraMode::FREE_FLY) {
            // Auto-switch to FREE_FLY on first WASD input
            if (forward || backward || left || right || up || down) {
                mode = CameraMode::FREE_FLY;
                Vec3 pos = getPosition();
                Vec3 tgt = getTarget();
                Vec3 dir = {tgt.x - pos.x, tgt.y - pos.y, tgt.z - pos.z};
                f32 l = std::sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
                if (l > 0) { dir.x /= l; dir.y /= l; dir.z /= l; }
                yaw = std::atan2(dir.z, dir.x);
                pitch = std::asin(dir.y);
                fly_position = pos;
                fly_velocity = {0, 0, 0};
            } else {
                return;
            }
        }
        Vec3 fwd = {std::cos(yaw) * std::cos(pitch),
                    std::sin(pitch),
                    std::sin(yaw) * std::cos(pitch)};
        Vec3 rgt = {std::cos(yaw + 3.14159265f/2), 0,
                    std::sin(yaw + 3.14159265f/2)};
        // Compute target velocity direction
        Vec3 target_dir = {0, 0, 0};
        if (forward)  target_dir = target_dir + fwd;
        if (backward) target_dir = target_dir - fwd;
        if (left)     target_dir = target_dir - rgt;
        if (right)    target_dir = target_dir + rgt;
        if (up)       target_dir.y += 1;
        if (down)     target_dir.y -= 1;
        f32 len = std::sqrt(target_dir.x*target_dir.x + target_dir.y*target_dir.y + target_dir.z*target_dir.z);
        if (len > 0) { target_dir.x /= len; target_dir.y /= len; target_dir.z /= len; }
        // Exponential smoothing towards target velocity
        Vec3 target_vel = {target_dir.x * fly_speed, target_dir.y * fly_speed, target_dir.z * fly_speed};
        f32 factor = 1.0f - std::exp(-smooth_speed * dt);
        fly_velocity.x += (target_vel.x - fly_velocity.x) * factor;
        fly_velocity.y += (target_vel.y - fly_velocity.y) * factor;
        fly_velocity.z += (target_vel.z - fly_velocity.z) * factor;
        fly_position.x += fly_velocity.x * dt;
        fly_position.y += fly_velocity.y * dt;
        fly_position.z += fly_velocity.z * dt;
    }

    // Mouse look for free fly
    void mouseLook(f32 dx, f32 dy) {
        if (mode != CameraMode::FREE_FLY) return;
        yaw   += dx * sensitivity;
        pitch += dy * sensitivity;
        pitch = std::fmax(-1.5f, std::fmin(1.5f, pitch));
    }
};

} // namespace scene
} // namespace mg
