/*
See LICENSE file in root folder
*/
#ifndef ___C3D_GaussianBlur_H___
#define ___C3D_GaussianBlur_H___

#include "PassesModule.hpp"

#include "Castor3D/Buffer/UniformBufferOffset.hpp"
#include "Castor3D/Render/Passes/RenderQuad.hpp"
#include "Castor3D/Material/Texture/TextureUnit.hpp"

#include <ashespp/Image/ImageView.hpp>
#include <ashespp/RenderPass/FrameBuffer.hpp>
#include <ashespp/RenderPass/RenderPass.hpp>

#include <ShaderAST/Shader.hpp>

namespace castor3d
{
	using TextureViewCRef = std::reference_wrapper< TextureView const >;
	using TextureViewCRefArray = std::vector< TextureViewCRef >;

	class GaussianBlur
		: public castor::OwnedBy< Engine >
	{
	public:
		/**
		 *\~english
		 *\brief		Constructor.
		 *\param[in]	engine		The engine.
		 *\param[in]	texture		The texture.
		 *\param[in]	textureSize	The render area dimensions.
		 *\param[in]	format		The pixel format for the textures to blur.
		 *\param[in]	kernelSize	The kernel coefficients count.
		 *\~french
		 *\brief		Constructeur.
		 *\param[in]	engine		Le moteur.
		 *\param[in]	texture		La texture.
		 *\param[in]	textureSize	Les dimensions de la zone de rendu.
		 *\param[in]	format		Le format de pixel des textures à flouter.
		 *\param[in]	kernelSize	Le nombre de coefficients du kernel.
		 */
		C3D_API GaussianBlur( Engine & engine
			, RenderDevice const & device
			, castor::String const & prefix
			, TextureView const & view
			, uint32_t kernelSize );
		/**
		 *\~english
		 *\brief		Blurs given texture.
		 *\~french
		 *\brief		Applique le flou sur la texture.
		 */
		C3D_API ashes::Semaphore const & blur( ashes::Semaphore const & toWait );
		/**
		 *\~english
		 *\param[in]	timer	The render timer.
		 *\return		The commands used to render the pass.
		 *\~french
		 *\param[in]	timer	Le timer de rendu.
		 *\return		Les commandes utilisées pour rendre la passe.
		 */
		C3D_API CommandsSemaphore getCommands( bool generateMipmaps = false
			, uint32_t layer = 0u )const;
		/**
		 *\~english
		 *\param[in]	timer	The render timer.
		 *\return		The commands used to render the pass.
		 *\~french
		 *\param[in]	timer	Le timer de rendu.
		 *\return		Les commandes utilisées pour rendre la passe.
		 */
		C3D_API CommandsSemaphore getCommands( RenderPassTimer const & timer
			, uint32_t layer
			, bool generateMipmaps = false )const;
		/**
		 *\copydoc		castor3d::RenderTechniquePass::accept
		 */
		C3D_API void accept( PipelineVisitorBase & visitor );
		/**
		*\~english
		*name
		*	Getters.
		*\~french
		*name
		*	Accesseurs.
		**/
		/**@{*/
		inline ashes::RenderPass const & getRenderPass()const
		{
			CU_Require( m_renderPass );
			return *m_renderPass;
		}

		inline ashes::FrameBuffer const & getBlurXFrameBuffer( uint32_t level )const
		{
			CU_Require( ( !m_blurX.fbos.empty() )
				&& m_blurX.fbos[0u].size() > level
				&& m_blurX.fbos[0u][level] );
			return *m_blurX.fbos[0u][level];
		}

		inline ashes::FrameBuffer const & getBlurYFrameBuffer( uint32_t layer, uint32_t level )const
		{
			CU_Require( m_blurY.fbos.size() > layer
				&& m_blurY.fbos[layer].size() > level
				&& m_blurY.fbos[layer][level] );
			return *m_blurY.fbos[layer][level];
		}

		inline ashes::CommandBuffer const & getBlurXCommandBuffer()const
		{
			return *m_blurX.commandBuffer;
		}

		inline ashes::CommandBuffer const & getBlurYCommandBuffer()const
		{
			return *m_blurY.commandBuffer;
		}

		inline ShaderModule const & getBlurXVertexModule()const
		{
			return m_blurX.vertexShader;
		}

		inline ShaderModule const & getBlurXPixelModule()const
		{
			return m_blurX.pixelShader;
		}

		inline ShaderModule const & getBlurYVertexModule()const
		{
			return m_blurY.vertexShader;
		}

		inline ShaderModule const & getBlurYPixelModule()const
		{
			return m_blurY.pixelShader;
		}

		inline ast::Shader const & getBlurXVertexShader()const
		{
			CU_Require( m_blurX.vertexShader.shader );
			return *m_blurX.vertexShader.shader;
		}

		inline ast::Shader const & getBlurXPixelShader()const
		{
			CU_Require( m_blurX.pixelShader.shader );
			return *m_blurX.pixelShader.shader;
		}

		inline ast::Shader const & getBlurYVertexShader()const
		{
			CU_Require( m_blurY.vertexShader.shader );
			return *m_blurY.vertexShader.shader;
		}

		inline ast::Shader const & getBlurYPixelShader()const
		{
			CU_Require( m_blurY.pixelShader.shader );
			return *m_blurY.pixelShader.shader;
		}
		/**@}*/

	public:
		C3D_API static castor::String const Config;
		C3D_API static castor::String const Coefficients;
		C3D_API static castor::String const CoefficientsCount;
		C3D_API static castor::String const TextureSize;
		C3D_API static constexpr uint32_t MaxCoefficients{ 60u };

		struct Configuration
		{
			castor::Point2f textureSize;
			uint32_t blurCoeffsCount;
			uint32_t dump; // to keep a 16 byte alignment.
			std::array< castor::Point4f, GaussianBlur::MaxCoefficients / 4u > blurCoeffs; // We then allow for 60 coeffs max, to have a 256 bytes struct.
		};

		class RenderQuad
			: public castor3d::RenderQuad
		{
		public:
			RenderQuad( RenderSystem & renderSystem
				, RenderDevice const & device
				, castor::String const & name
				, ashes::ImageView const & src
				, VkImageSubresourceRange const & srcRange
				, UniformBufferOffsetT< Configuration > const & blurUbo
				, ShaderModule const & vertexShader
				, ShaderModule const & pixelShader
				, ashes::RenderPass const & renderPass
				, VkExtent2D const & textureSize );

		public:
			ashes::ImageView srcView;

		private:
			UniformBufferOffsetT< Configuration > const & m_blurUbo;
		};
		using RenderQuadPtr = std::unique_ptr< RenderQuad >;

		struct BlurPass
		{
			ShaderModule vertexShader;
			ShaderModule pixelShader;
			RenderQuad quad;
			ashes::SemaphorePtr semaphore;
			ashes::ImageViewArray views;
			std::vector< ashes::FrameBufferPtrArray > fbos;
			ashes::CommandBufferPtr commandBuffer;
			bool isHorizontal;

			BlurPass( Engine & engine
				, RenderDevice const & device
				, castor::String const & name
				, ashes::ImageView const & input
				, ashes::ImageView const & output
				, UniformBufferOffsetT< GaussianBlur::Configuration > const & blurUbo
				, VkFormat format
				, VkExtent2D const & textureSize
				, ashes::RenderPass const & renderPass
				, bool isHorizontal );
			void getCommands( ashes::CommandBuffer & cmd
				, ashes::RenderPass const & renderPass
				, uint32_t layer
				, bool generateMipmaps )const;

		private:
			Engine & m_engine;
		};

	private:
		TextureView const & m_source;
		RenderDevice const & m_device;
		castor::String m_prefix;
		VkExtent2D m_size;
		VkFormat m_format;
		TextureUnit m_intermediate;
		ashes::RenderPassPtr m_renderPass;
		UniformBufferOffsetT< Configuration > m_blurUbo;
		std::vector< float > m_kernel;
		BlurPass m_blurX;
		BlurPass m_blurY;
	};
}

#endif
