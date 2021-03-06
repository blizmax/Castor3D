/*
See LICENSE file in root folder
*/
#ifndef ___C3DSMAA_Reproject_H___
#define ___C3DSMAA_Reproject_H___

#include "SmaaPostEffect/SmaaConfig.hpp"

#include <Castor3D/Render/PostEffect/PostEffect.hpp>
#include <Castor3D/Render/PostEffect/PostEffectSurface.hpp>
#include <Castor3D/Render/Passes/RenderQuad.hpp>

#include <ShaderAST/Shader.hpp>

namespace smaa
{
	class Reproject
		: public castor3d::RenderQuad
	{
	public:
		Reproject( castor3d::RenderTarget & renderTarget
			, castor3d::RenderDevice const & device
			, ashes::ImageView const & currentColourView
			, ashes::ImageView const & previousColourView
			, ashes::ImageView const * velocityView
			, SmaaConfig const & config );
		castor3d::CommandsSemaphore prepareCommands( castor3d::RenderPassTimer const & timer
			, uint32_t passIndex );
		void accept( castor3d::PipelineVisitorBase & visitor );

		inline castor3d::TextureLayoutSPtr getSurface()const
		{
			return m_surface.colourTexture;
		}

	private:
		ashes::RenderPassPtr m_renderPass;
		castor3d::PostEffectSurface m_surface;
		ashes::ImageView const & m_currentColourView;
		ashes::ImageView const & m_previousColourView;
		ashes::ImageView const * m_velocityView;
		castor3d::ShaderModule m_vertexShader;
		castor3d::ShaderModule m_pixelShader;
	};
}

#endif
