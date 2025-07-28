#pragma once

#include "engine.hpp"

#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

namespace okami {
	struct Transform {
		glm::vec3 m_position = glm::zero<glm::vec3>();
		glm::quat m_rotation = glm::identity<glm::quat>(); 
		glm::mat3 m_scaleShear = glm::identity<glm::mat3>();

		Transform() = default;

		explicit inline Transform(glm::vec3 const& pos, glm::quat const& rot, glm::mat3 const& scaleShear)
			: m_position(pos), m_rotation(rot), m_scaleShear(scaleShear) {
		}
		explicit inline Transform(glm::vec3 const& pos, glm::quat const& rot, const float scale)
			: Transform(pos, rot, glm::mat3{ {scale, 0.f, 0.f}, {0.f, scale, 0.f}, {0.f, 0.f, scale} }) {
		}
		explicit inline Transform(glm::vec3 const& pos, const float scale)
			: Transform(pos, glm::identity<glm::quat>(), scale) {
		}
		explicit inline Transform(glm::vec3 const& pos)
			: Transform(pos, glm::identity<glm::quat>(), 1.0f) {
		}
		explicit inline Transform(glm::quat const& rot)
			: Transform(glm::zero<glm::vec3>(), rot, 1.0f) {
		}
		explicit inline Transform(glm::mat3 const& scaleShear)
			: Transform(glm::zero<glm::vec3>(), glm::identity<glm::quat>(), scaleShear) {
		}

		inline glm::vec3 TransformPoint(glm::vec3 const& point) const {
			return m_position + m_rotation * (m_scaleShear * point);
		}
		inline glm::vec3 TransformVector(glm::vec3 const& vector) const {
			return m_rotation * (m_scaleShear * vector);
		}
		inline glm::mat4 AsMatrix() const {
			glm::mat3 matrix3x3 = glm::mat3_cast(m_rotation) * m_scaleShear;
			// Construct a 4x4 matrix from the 3x3 matrix and position
			return glm::mat4{
				matrix3x3[0][0], matrix3x3[0][1], matrix3x3[0][2], 0.0f,
				matrix3x3[1][0], matrix3x3[1][1], matrix3x3[1][2], 0.0f,
				matrix3x3[2][0], matrix3x3[2][1], matrix3x3[2][2], 0.0f,
				m_position.x, m_position.y, m_position.z, 1.0f
			};
		}

		inline Transform Inverse() const;
	};

	inline Transform operator*(Transform const& A, Transform const& B) {
		auto rotation = A.m_rotation * B.m_rotation;
		glm::mat3 scaleShear = (glm::mat3(glm::inverse(A.m_rotation)) * A.m_scaleShear * glm::mat3(A.m_rotation)) * B.m_scaleShear;
		auto position = A.TransformPoint(B.m_position);
		return Transform(position, rotation, scaleShear);
	}

	inline Transform Inverse(Transform const& transform) {
		auto invRotation = glm::inverse(transform.m_rotation);
		auto invScaleShear = glm::mat3(transform.m_rotation) * glm::inverse(transform.m_scaleShear) * glm::mat3(invRotation);
		auto invPosition = invRotation * (invScaleShear * -transform.m_position);
		return Transform(invPosition, invRotation, invScaleShear);
	}

	Transform Transform::Inverse() const {
		return okami::Inverse(*this);
	}
}