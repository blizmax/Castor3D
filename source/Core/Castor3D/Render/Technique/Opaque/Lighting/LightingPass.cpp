#include "Castor3D/Render/Technique/Opaque/Lighting/LightingPass.hpp"

#include "Castor3D/Engine.hpp"
#include "Castor3D/Cache/LightCache.hpp"
#include "Castor3D/Material/Texture/TextureLayout.hpp"
#include "Castor3D/Material/Texture/TextureUnit.hpp"
#include "Castor3D/Miscellaneous/PipelineVisitor.hpp"
#include "Castor3D/Render/RenderPassTimer.hpp"
#include "Castor3D/Render/RenderPipeline.hpp"
#include "Castor3D/Render/RenderSystem.hpp"
#include "Castor3D/Render/Technique/RenderTechniquePass.hpp"
#include "Castor3D/Render/Technique/Opaque/OpaquePass.hpp"
#include "Castor3D/Render/Technique/Opaque/Lighting/DirectionalLightPass.hpp"
#include "Castor3D/Render/Technique/Opaque/ReflectiveShadowMapGI/LightPassReflectiveShadow.hpp"
#include "Castor3D/Render/Technique/Opaque/LightVolumeGI/LightPassVolumePropagationShadow.hpp"
#include "Castor3D/Render/Technique/Opaque/LightVolumeGI/LightPassLayeredVolumePropagationShadow.hpp"
#include "Castor3D/Render/Technique/Opaque/Lighting/LightPassShadow.hpp"
#include "Castor3D/Render/Technique/Opaque/Lighting/PointLightPass.hpp"
#include "Castor3D/Render/Technique/Opaque/Lighting/SpotLightPass.hpp"
#include "Castor3D/Scene/Scene.hpp"
#include "Castor3D/Scene/SceneNode.hpp"
#include "Castor3D/Scene/Camera.hpp"
#include "Castor3D/Scene/Light/PointLight.hpp"
#include "Castor3D/Shader/Shaders/GlslLight.hpp"
#include "Castor3D/Shader/Ubos/MatrixUbo.hpp"
#include "Castor3D/Shader/Ubos/ModelMatrixUbo.hpp"

#include <CastorUtils/Graphics/RgbaColour.hpp>

#include <ashespp/Buffer/VertexBuffer.hpp>
#include <ashespp/RenderPass/FrameBuffer.hpp>

#include <ShaderWriter/Source.hpp>

namespace castor3d
{
	namespace
	{
		constexpr uint32_t getDirectionalGIidx( LightingPass::Type type )
		{
			return std::min( uint32_t( type ), uint32_t( LightingPass::Type::eShadowLayeredLpvGI ) );
		}

		constexpr uint32_t getPointGIidx( LightingPass::Type type )
		{
			return std::min( uint32_t( type ), uint32_t( LightingPass::Type::eShadowRsmGI ) );
		}

		constexpr uint32_t getSpotGIidx( LightingPass::Type type )
		{
			return std::min( uint32_t( type ), uint32_t( LightingPass::Type::eShadowLpvGI ) );
		}

		constexpr LightingPass::Type getLightType( LightingPass::Type passType
			, LightType lightType )
		{
			switch ( lightType )
			{
			case LightType::eDirectional:
				return LightingPass::Type( getDirectionalGIidx( passType ) );
			case LightType::ePoint:
				return LightingPass::Type( getPointGIidx( passType ) );
			case LightType::eSpot:
				return LightingPass::Type( getSpotGIidx( passType ) );
			default:
				CU_Failure( "Unsupported LightType" );
				return LightingPass::Type( std::min( uint32_t( passType ), uint32_t( LightingPass::Type::eShadowNoGI ) ) );
			}
		}

		LightPass * getLightPass( LightingPass::Type passType
			, LightType lightType
			, LightingPass::LightPasses const & passes )
		{
			auto lt = uint32_t( lightType );

			switch ( lightType )
			{
			case LightType::eDirectional:
				return passes[getDirectionalGIidx( passType )][lt].get();
			case LightType::ePoint:
				return passes[getPointGIidx( passType )][lt].get();
			case LightType::eSpot:
				return passes[getSpotGIidx( passType )][lt].get();
			default:
				CU_Failure( "Unsupported LightType" );
				return nullptr;
			}
		}

		template< LightingPass::Type PassT, LightType LightT >
		struct PassInfoT;

		template<>
		struct PassInfoT< LightingPass::Type::eNoShadow, LightType::eDirectional >
		{
			using Type = DirectionalLightPass;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eNoShadow, LightType::ePoint >
		{
			using Type = PointLightPass;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eNoShadow, LightType::eSpot >
		{
			using Type = SpotLightPass;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eShadowNoGI, LightType::eDirectional >
		{
			using Type = DirectionalLightPassShadow;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eShadowNoGI, LightType::ePoint >
		{
			using Type = PointLightPassShadow;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eShadowNoGI, LightType::eSpot >
		{
			using Type = SpotLightPassShadow;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eShadowRsmGI, LightType::eDirectional >
		{
			using Type = DirectionalLightPassReflectiveShadow;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eShadowRsmGI, LightType::ePoint >
		{
			using Type = PointLightPassReflectiveShadow;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eShadowRsmGI, LightType::eSpot >
		{
			using Type = SpotLightPassReflectiveShadow;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eShadowLpvGI, LightType::eDirectional >
		{
			using Type = DirectionalLightPassVolumePropagationShadow;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eShadowLpvGI, LightType::ePoint >
		{
			using Type = PointLightPassShadow;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eShadowLpvGI, LightType::eSpot >
		{
			using Type = SpotLightPassVolumePropagationShadow;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eShadowLayeredLpvGI, LightType::eDirectional >
		{
			using Type = DirectionalLightPassLayeredVolumePropagationShadow;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eShadowLayeredLpvGI, LightType::ePoint >
		{
			using Type = PointLightPassShadow;
		};

		template<>
		struct PassInfoT< LightingPass::Type::eShadowLayeredLpvGI, LightType::eSpot >
		{
			using Type = SpotLightPassVolumePropagationShadow;
		};

		template< LightingPass::Type PassT, LightType LightT >
		using PassTypeT = typename PassInfoT< PassT, LightT >::Type;

		template< LightingPass::Type PassT, LightType LightT >
		LightingPass::DelayedLightPass makeLightPassTT( Engine & engine
			, LightCache const & lightCache
			, OpaquePassResult const & gpResult
			, ShadowMapResult const & smResult
			, LightPassResult const & lpResult
			, GpInfoUbo const & gpInfoUbo )
		{
			if constexpr ( PassT > getLightType( PassT, LightT ) )
			{
				return LightingPass::DelayedLightPass{};
			}
			else if constexpr ( PassT > LightingPass::Type::eShadowNoGI )
			{
				return castor::DelayedInitialiserT< castor3d::LightPass >::template make< PassTypeT< PassT, LightT > >( engine
					, lightCache
					, gpResult
					, smResult
					, lpResult
					, gpInfoUbo );
			}
			else
			{
				return castor::DelayedInitialiserT< castor3d::LightPass >::template make< PassTypeT< PassT, LightT > >( engine
					, lpResult
					, gpInfoUbo );
			}
		}

		template< LightingPass::Type PassT >
		LightingPass::DelayedLightPass makeLightPassT( LightType lightType
			, Engine & engine
			, LightCache const & lightCache
			, OpaquePassResult const & gpResult
			, ShadowMapResult const & smDirectionalResult
			, ShadowMapResult const & smPointResult
			, ShadowMapResult const & smSpotResult
			, LightPassResult const & lpResult
			, GpInfoUbo const & gpInfoUbo )
		{
			switch ( lightType )
			{
			case LightType::eDirectional:
				return makeLightPassTT< PassT, LightType::eDirectional >( engine
					, lightCache
					, gpResult
					, smDirectionalResult
					, lpResult
					, gpInfoUbo );
			case LightType::ePoint:
				return makeLightPassTT< PassT, LightType::ePoint >( engine
					, lightCache
					, gpResult
					, smPointResult
					, lpResult
					, gpInfoUbo );
			case LightType::eSpot:
				return makeLightPassTT< PassT, LightType::eSpot >( engine
					, lightCache
					, gpResult
					, smSpotResult
					, lpResult
					, gpInfoUbo );
			default:
				CU_Failure( "Unsupported LightType" );
				return LightingPass::DelayedLightPass{};
			}
		}

		LightingPass::DelayedLightPass makeLightPass( LightingPass::Type passType
			, LightType lightType
			, Engine & engine
			, LightCache const & lightCache
			, OpaquePassResult const & gpResult
			, ShadowMapResult const & smDirectionalResult
			, ShadowMapResult const & smPointResult
			, ShadowMapResult const & smSpotResult
			, LightPassResult const & lpResult
			, GpInfoUbo const & gpInfoUbo )
		{
			switch ( passType )
			{
			case LightingPass::Type::eNoShadow:
				return makeLightPassT< LightingPass::Type::eNoShadow >( lightType
					, engine
					, lightCache
					, gpResult
					, smDirectionalResult
					, smPointResult
					, smSpotResult
					, lpResult
					, gpInfoUbo );
			case LightingPass::Type::eShadowNoGI:
				return makeLightPassT< LightingPass::Type::eShadowNoGI >( lightType
					, engine
					, lightCache
					, gpResult
					, smDirectionalResult
					, smPointResult
					, smSpotResult
					, lpResult
					, gpInfoUbo );
			case LightingPass::Type::eShadowRsmGI:
				return makeLightPassT< LightingPass::Type::eShadowRsmGI >( lightType
					, engine
					, lightCache
					, gpResult
					, smDirectionalResult
					, smPointResult
					, smSpotResult
					, lpResult
					, gpInfoUbo );
			case LightingPass::Type::eShadowLpvGI:
				if ( engine.getRenderSystem()->getGpuInformations().hasShaderType( VK_SHADER_STAGE_GEOMETRY_BIT ) )
				{
					return makeLightPassT< LightingPass::Type::eShadowLpvGI >( lightType
						, engine
						, lightCache
						, gpResult
						, smDirectionalResult
						, smPointResult
						, smSpotResult
						, lpResult
						, gpInfoUbo );
				}
				return LightingPass::DelayedLightPass{};
			case LightingPass::Type::eShadowLayeredLpvGI:
				if ( engine.getRenderSystem()->getGpuInformations().hasShaderType( VK_SHADER_STAGE_GEOMETRY_BIT ) )
				{
					return makeLightPassT< LightingPass::Type::eShadowLayeredLpvGI >( lightType
						, engine
						, lightCache
						, gpResult
						, smDirectionalResult
						, smPointResult
						, smSpotResult
						, lpResult
						, gpInfoUbo );
				}
				return LightingPass::DelayedLightPass{};
			default:
				CU_Failure( "Unsupported LightingPass::Type" );
				return LightingPass::DelayedLightPass{};
			}
		}

		LightingPass::TypeLightPasses makeTypeLightPasses( LightingPass::Type passType
			, Engine & engine
			, LightCache const & lightCache
			, OpaquePassResult const & gpResult
			, ShadowMapResult const & smDirectionalResult
			, ShadowMapResult const & smPointResult
			, ShadowMapResult const & smSpotResult
			, LightPassResult const & lpResult
			, GpInfoUbo const & gpInfoUbo )
		{
			LightingPass::TypeLightPasses result;

			for ( uint32_t i = uint32_t( LightType::eMin ); i < uint32_t( LightType::eCount ); ++i )
			{
				result[i] = makeLightPass( passType, LightType( i )
					, engine
					, lightCache
					, gpResult
					, smDirectionalResult
					, smPointResult
					, smSpotResult
					, lpResult
					, gpInfoUbo );
			}

			return result;
		}

		LightingPass::LightPasses makeLightPasses( Engine & engine
			, LightCache const & lightCache
			, OpaquePassResult const & gpResult
			, ShadowMapResult const & smDirectionalResult
			, ShadowMapResult const & smPointResult
			, ShadowMapResult const & smSpotResult
			, LightPassResult const & lpResult
			, GpInfoUbo const & gpInfoUbo )
		{
			LightingPass::LightPasses result;

			for ( uint32_t i = uint32_t( LightingPass::Type::eMin ); i < uint32_t( LightingPass::Type::eCount ); ++i )
			{
				result[i] = makeTypeLightPasses( LightingPass::Type( i )
					, engine
					, lightCache
					, gpResult
					, smDirectionalResult
					, smPointResult
					, smSpotResult
					, lpResult
					, gpInfoUbo );
			}

			return result;
		}
	}

	LightingPass::LightingPass( Engine & engine
		, castor::Size const & size
		, Scene & scene
		, OpaquePassResult const & gpResult
		, ShadowMapResult const & smDirectionalResult
		, ShadowMapResult const & smPointResult
		, ShadowMapResult const & smSpotResult
		, ashes::ImageView const & depthView
		, SceneUbo & sceneUbo
		, GpInfoUbo const & gpInfoUbo )
		: m_engine{ engine }
		, m_size{ size }
		, m_result{ engine, size }
		, m_timer{ std::make_shared< RenderPassTimer >( engine, cuT( "Opaque" ), cuT( "Lighting pass" ) ) }
		, m_srcDepth{ depthView }
		, m_blitDepth
		{
			getCurrentRenderDevice( engine ).graphicsCommandPool->createCommandBuffer( "LPBlitDepth", VK_COMMAND_BUFFER_LEVEL_PRIMARY ),
			getCurrentRenderDevice( engine )->createSemaphore( "LPBlitDepth" ),
		}
		, m_lpResultBarrier
		{
			getCurrentRenderDevice( engine ).graphicsCommandPool->createCommandBuffer( "LPResultBarrier", VK_COMMAND_BUFFER_LEVEL_PRIMARY ),
			getCurrentRenderDevice( engine )->createSemaphore( "LPResultBarrier" ),
		}
	{
		auto & lightCache = scene.getLightCache();
		lightCache.initialise();
		m_result.initialise();
		m_lightPasses = makeLightPasses( engine
			, lightCache
			, gpResult
			, smDirectionalResult
			, smPointResult
			, smSpotResult
			, m_result
			, gpInfoUbo );

		for ( auto & lightPasses : m_lightPasses )
		{
			for ( auto & lightPass : lightPasses )
			{
				if ( lightPass )
				{
					lightPass.initialise( scene
						, gpResult
						, sceneUbo
						, *m_timer );
				}
			}
		}

		auto & device = getCurrentRenderDevice( engine );
		VkImageCopy copy
		{
			{ m_srcDepth->subresourceRange.aspectMask, 0u, 0u, 1u },
			VkOffset3D{ 0, 0, 0 },
			{ m_srcDepth->subresourceRange.aspectMask, 0u, 0u, 1u },
			VkOffset3D{ 0, 0, 0 },
			m_srcDepth.image->getDimensions(),
		};
		m_blitDepth.commandBuffer->begin();
		m_blitDepth.commandBuffer->beginDebugBlock(
			{
				"Deferred - Ligth Depth Blit",
				makeFloatArray( m_engine.getNextRainbowColour() ),
			} );
		// Src depth buffer from depth attach to transfer source
		m_blitDepth.commandBuffer->memoryBarrier( VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
			, VK_PIPELINE_STAGE_TRANSFER_BIT
			, m_srcDepth.makeTransferSource( VK_IMAGE_LAYOUT_UNDEFINED ) );
		// Dst depth buffer from unknown to transfer destination
		m_blitDepth.commandBuffer->memoryBarrier( VK_PIPELINE_STAGE_TRANSFER_BIT
			, VK_PIPELINE_STAGE_TRANSFER_BIT
			, m_result[LpTexture::eDepth].getTexture()->getDefaultView().getTargetView().makeTransferDestination( VK_IMAGE_LAYOUT_UNDEFINED ) );
		// Copy Src to Dst
		m_blitDepth.commandBuffer->copyImage( copy
			, *m_srcDepth.image
			, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
			, m_result[LpTexture::eDepth].getTexture()->getTexture()
			, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL );
		// Dst depth buffer from transfer destination to depth attach
		m_blitDepth.commandBuffer->memoryBarrier( VK_PIPELINE_STAGE_TRANSFER_BIT
			, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
			, m_result[LpTexture::eDepth].getTexture()->getDefaultView().getTargetView().makeDepthStencilAttachment( VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL ) );
		// Src depth buffer from transfer source to depth stencil read only
		m_blitDepth.commandBuffer->memoryBarrier( VK_PIPELINE_STAGE_TRANSFER_BIT
			, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
			, m_srcDepth.makeDepthStencilReadOnly( VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL ) );
		m_blitDepth.commandBuffer->endDebugBlock();
		m_blitDepth.commandBuffer->end();

		m_lpResultBarrier.commandBuffer->begin();
		m_lpResultBarrier.commandBuffer->beginDebugBlock(
			{
				"Deferred - Ligth Pass Result Barrier",
				makeFloatArray( m_engine.getNextRainbowColour() ),
			} );
		// Diffuse view to shader read only
		m_lpResultBarrier.commandBuffer->memoryBarrier( VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
			, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
			, m_result[LpTexture::eDiffuse].getTexture()->getDefaultView().getSampledView().makeShaderInputResource( VK_IMAGE_LAYOUT_UNDEFINED ) );
		// Specular view to shader read only
		m_lpResultBarrier.commandBuffer->memoryBarrier( VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
			, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
			, m_result[LpTexture::eSpecular].getTexture()->getDefaultView().getSampledView().makeShaderInputResource( VK_IMAGE_LAYOUT_UNDEFINED ) );
		m_lpResultBarrier.commandBuffer->endDebugBlock();
		m_lpResultBarrier.commandBuffer->end();
	}

	LightingPass::~LightingPass()
	{
		m_lpResultBarrier.semaphore.reset();
		m_lpResultBarrier.commandBuffer.reset();
		m_blitDepth.semaphore.reset();
		m_blitDepth.commandBuffer.reset();

		for ( auto & lightPasses : m_lightPasses )
		{
			for ( auto & lightPass : lightPasses )
			{
				if ( lightPass )
				{
					lightPass.cleanup();
					lightPass.reset();
				}
			}
		}

		m_result.cleanup();
	}

	ashes::Semaphore const & LightingPass::render( Scene const & scene
		, Camera const & camera
		, OpaquePassResult const & gp
		, ashes::Semaphore const & toWait )
	{
		auto & cache = scene.getLightCache();
		ashes::Semaphore const * result = &toWait;

		if ( !cache.isEmpty() )
		{
			RenderPassTimerBlock timerBlock{ m_timer->start() };
			using LockType = std::unique_lock< LightCache const >;
			LockType lock{ castor::makeUniqueLock( cache ) };
			auto count = cache.getLightsCount( LightType::eDirectional )
				+ cache.getLightsCount( LightType::ePoint )
				+ cache.getLightsCount( LightType::eSpot );

			if ( timerBlock->getCount() != count )
			{
				timerBlock->updateCount( count );
			}

			auto & device = getCurrentRenderDevice( m_engine );
			device.graphicsQueue->submit( *m_blitDepth.commandBuffer
				, *result
				, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
				, *m_blitDepth.semaphore
				, nullptr );
			result = m_blitDepth.semaphore.get();

			uint32_t index = 0;
			result = &doRenderLights( scene
				, camera
				, LightType::eDirectional
				, gp
				, *result
				, index );
			result = &doRenderLights( scene
				, camera
				, LightType::ePoint
				, gp
				, *result
				, index );
			result = &doRenderLights( scene
				, camera
				, LightType::eSpot
				, gp
				, *result
				, index );
		}
		else
		{
			auto & device = getCurrentRenderDevice( m_engine );
			device.graphicsQueue->submit( *m_lpResultBarrier.commandBuffer
				, *result
				, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
				, *m_lpResultBarrier.semaphore
				, nullptr );
			result = m_lpResultBarrier.semaphore.get();
		}

		return *result;
	}

	void LightingPass::accept( PipelineVisitorBase & visitor )
	{
		visitor.visit( "Light Diffuse"
			, m_result[LpTexture::eDiffuse].getTexture()->getDefaultView().getSampledView() );
		visitor.visit( "Light Specular"
			, m_result[LpTexture::eSpecular].getTexture()->getDefaultView().getSampledView() );

		for ( auto & lightPasses : m_lightPasses )
		{
			for ( auto & lightPass : lightPasses )
			{
				if ( lightPass )
				{
					lightPass.raw()->accept( visitor );
				}
			}
		}
	}

	ashes::Semaphore const & LightingPass::doRenderLights( Scene const & scene
		, Camera const & camera
		, LightType type
		, OpaquePassResult const & gp
		, ashes::Semaphore const & toWait
		, uint32_t & index )
	{
		auto result = &toWait;
		auto & cache = scene.getLightCache();

		if ( cache.getLightsCount( type ) )
		{
			for ( auto & light : cache.getLights( type ) )
			{
				if ( light->getLightType() == LightType::eDirectional
					|| camera.isVisible( light->getBoundingBox(), light->getParent()->getDerivedTransformationMatrix() ) )
				{
					LightPass * pass = ( light->isShadowProducer() && light->getShadowMap() )
						? doGetShadowLightPass( type, light->getGlobalIlluminationType() )
						: doGetLightPass( type );
					pass->update( index == 0u
						, camera.getSize()
						, *light
						, camera
						, light->getShadowMap()
						, light->getShadowMapIndex() );
					result = &pass->render( index
						, *result );
					++index;
				}
			}
		}

		return *result;
	}

	LightPass * LightingPass::doGetLightPass( LightType lightType )const
	{
		return getLightPass( Type::eNoShadow
			, lightType
			, m_lightPasses );
	}

	LightPass * LightingPass::doGetShadowLightPass( LightType lightType
		, GlobalIlluminationType giType )const
	{
		if ( !m_engine.getRenderSystem()->getGpuInformations().hasShaderType( VK_SHADER_STAGE_GEOMETRY_BIT ) )
		{
			giType = std::min( GlobalIlluminationType::eRsm, giType );
		}

		return getLightPass( Type( uint32_t( giType ) + 1u )
			, lightType
			, m_lightPasses );
	}
}