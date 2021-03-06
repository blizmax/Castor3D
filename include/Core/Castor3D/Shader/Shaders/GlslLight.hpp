/*
See LICENSE file in root folder
*/
#ifndef ___C3D_GlslLight_H___
#define ___C3D_GlslLight_H___

#include "SdwModule.hpp"

#include <ShaderWriter/MatTypes/Mat4.hpp>
#include <ShaderWriter/CompositeTypes/StructInstance.hpp>

namespace castor3d
{
	namespace shader
	{
		struct Light
			: public sdw::StructInstance
		{
			C3D_API Light( ast::Shader * shader
				, ast::expr::ExprPtr expr );
			C3D_API Light & operator=( Light const & rhs );

			C3D_API static ast::type::StructPtr makeType( ast::type::TypesCache & cache );
			C3D_API static std::unique_ptr< sdw::Struct > declare( sdw::ShaderWriter & writer );

			// Raw values
			sdw::Vec4 m_colourIndex;
			sdw::Vec4 m_intensityFarPlane;
			sdw::Vec4 m_volumetric;
			sdw::Vec4 m_shadow;
			// Specific values
			sdw::Vec3 m_colour;
			sdw::Vec2 m_intensity;
			sdw::Float m_farPlane;
			sdw::Int m_shadowType;
			sdw::Int m_index;
			sdw::UInt m_volumetricSteps;
			sdw::Float m_volumetricScattering;
			sdw::Vec2 m_shadowOffsets;
			sdw::Vec2 m_shadowVariance;

		private:
			using sdw::StructInstance::getMember;
			using sdw::StructInstance::getMemberArray;
		};

		struct DirectionalLight
			: public sdw::StructInstance
		{
			C3D_API DirectionalLight( ast::Shader * shader
				, ast::expr::ExprPtr expr );

			C3D_API static ast::type::StructPtr makeType( ast::type::TypesCache & cache );
			C3D_API static std::unique_ptr< sdw::Struct > declare( sdw::ShaderWriter & writer );

			// Raw values
			Light m_lightBase;
			sdw::Vec4 m_directionCount;
			sdw::Array< sdw::Mat4 > m_transforms;
			sdw::Vec4 m_splitDepths;
			sdw::Vec4 m_splitScales;
			// Specific values
			sdw::Vec3 m_direction;
			sdw::UInt m_cascadeCount;

		private:
			using sdw::StructInstance::getMember;
			using sdw::StructInstance::getMemberArray;
		};

		struct PointLight
			: public sdw::StructInstance
		{
			C3D_API PointLight( ast::Shader * shader
				, ast::expr::ExprPtr expr );

			C3D_API static ast::type::StructPtr makeType( ast::type::TypesCache & cache );
			C3D_API static std::unique_ptr< sdw::Struct > declare( sdw::ShaderWriter & writer );

			// Raw values
			Light m_lightBase;
			sdw::Vec4 m_position4;
			sdw::Vec4 m_attenuation4;
			// SpecificValues
			sdw::Vec3 m_position;
			sdw::Vec3 m_attenuation;

		private:
			using sdw::StructInstance::getMember;
			using sdw::StructInstance::getMemberArray;
		};

		struct SpotLight
			: public sdw::StructInstance
		{
			C3D_API SpotLight( ast::Shader * shader
				, ast::expr::ExprPtr expr );

			C3D_API static ast::type::StructPtr makeType( ast::type::TypesCache & cache );
			C3D_API static std::unique_ptr< sdw::Struct > declare( sdw::ShaderWriter & writer );

			// Raw values
			Light m_lightBase;
			sdw::Vec4 m_position4;
			sdw::Vec4 m_attenuation4;
			sdw::Vec4 m_direction4;
			sdw::Vec4 m_exponentCutOff;
			sdw::Mat4 m_transform;
			// SpecificValues
			sdw::Vec3 m_position;
			sdw::Vec3 m_attenuation;
			sdw::Vec3 m_direction;
			sdw::Float m_exponent;
			sdw::Float m_cutOff;

		private:
			using sdw::StructInstance::getMember;
			using sdw::StructInstance::getMemberArray;
		};

		Writer_Parameter( Light );
		Writer_Parameter( DirectionalLight );
		Writer_Parameter( PointLight );
		Writer_Parameter( SpotLight );
	}
}

#endif
