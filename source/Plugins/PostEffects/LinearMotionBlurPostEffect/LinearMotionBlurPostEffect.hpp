/*
See LICENSE file in root folder
*/
#ifndef ___C3D_MotionBlurPostEffect___
#define ___C3D_MotionBlurPostEffect___

#include <Castor3D/Render/PostEffect/PostEffect.hpp>
#include <Castor3D/Render/PostEffect/PostEffectSurface.hpp>
#include <Castor3D/Material/Texture/TextureUnit.hpp>
#include <Castor3D/Render/Viewport.hpp>
#include <Castor3D/Shader/Ubos/MatrixUbo.hpp>

#include <ShaderAST/Shader.hpp>

#include <CastorUtils/Design/ChangeTracked.hpp>

namespace motion_blur
{
	struct Configuration
	{
		uint32_t samplesCount{ 4u };
		float vectorDivider = 1.0f;
		float blurScale = 1.0f;
	};

	class PostEffect
		: public castor3d::PostEffect
	{
	private:
		class Quad
			: public castor3d::RenderQuad
		{
		public:
			Quad( castor3d::RenderSystem & renderSystem
				, castor3d::RenderDevice const & device );
		};

	public:
		PostEffect( castor3d::RenderTarget & renderTarget
			, castor3d::RenderSystem & renderSystem
			, castor3d::Parameters const & parameters );
		~PostEffect();
		static castor3d::PostEffectSPtr create( castor3d::RenderTarget & renderTarget
			, castor3d::RenderSystem & renderSystem
			, castor3d::Parameters const & params );
		/**
		 *\copydoc		castor3d::PostEffect::update
		 */
		void update( castor3d::CpuUpdater & updater )override;
		/**
		 *\copydoc		castor3d::PostEffect::accept
		 */
		void accept( castor3d::PipelineVisitorBase & visitor )override;

	private:
		/**
		*\copydoc		castor3d::PostEffect::doInitialise
		*/
		bool doInitialise( castor3d::RenderDevice const & device
			, castor3d::RenderPassTimer const & timer )override;
		/**
		*\copydoc		castor3d::PostEffect::doCleanup
		*/
		void doCleanup( castor3d::RenderDevice const & device )override;
		/**
		 *\copydoc		castor3d::PostEffect::doWriteInto
		 */
		bool doWriteInto( castor::TextFile & file, castor::String const & tabs )override;

	public:
		static castor::String Type;
		static castor::String Name;

	private:
		using Clock = std::chrono::high_resolution_clock;
		using TimePoint = Clock::time_point;
		castor3d::PostEffectSurface m_surface;
		ashes::RenderPassPtr m_renderPass;
		std::unique_ptr< Quad > m_quad;
		Configuration m_configuration;
		castor3d::UniformBufferOffsetT< Configuration > m_ubo;
		castor3d::ShaderModule m_vertexShader;
		castor3d::ShaderModule m_pixelShader;
		TimePoint m_saved;
		bool m_fpsScale{ true };
	};
}

#endif
