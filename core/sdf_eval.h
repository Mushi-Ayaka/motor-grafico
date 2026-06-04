#pragma once
#include "core.h"
#include <string>

namespace mg {

// Evaluate expression with x,y,z,w context
float evalExpr(const std::string& expr, float x, float y, float z, float w);

// Evaluate a single SDF expression at point p with time w
float evalSdfExpr(const Expr& e, float x, float y, float z, float w);

// Evaluate a single SDF subtree at point p with time w
float evalSdfTree(const Rih& rih, u32 node_idx, const Vec3& p, float w, const f32* transforms);

// Evaluate ALL nodes in the scene at point p, return min distance
float evalScene(const Rih& rih, const Vec3& p, float w, const f32* transforms);

// Which node index is closest at point p?
u32 findClosestNode(const Rih& rih, const Vec3& p, float w, const f32* transforms);

// Compute normal at point p (scene-level gradient)
Vec3 calcNormal(const Rih& rih, const Vec3& p, float w, const f32* transforms);

// Get the material ID of the closest SDF at point p
u32 findMaterial(const Rih& rih, const Vec3& p, float w, const f32* transforms);

} // namespace mg
