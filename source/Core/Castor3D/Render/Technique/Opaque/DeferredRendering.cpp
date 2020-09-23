#include "Castor3D/Render/Technique/Opaque/DeferredRendering.hpp"

#include "Castor3D/Cache/SceneCache.hpp"
#include "Castor3D/Cache/MaterialCache.hpp"
#include "Castor3D/Cache/OverlayCache.hpp"
#include "Castor3D/Cache/PluginCache.hpp"
#include "Castor3D/Cache/SamplerCache.hpp"
#include "Castor3D/Material/Texture/Sampler.hpp"
#include "Castor3D/Render/RenderLoop.hpp"
#include "Castor3D/Render/RenderPassTimer.hpp"
#include "Castor3D/Render/Passes/LineariseDepthPass.hpp"
#include "Castor3D/Render/Technique/RenderTechniqueVisitor.hpp"
#include "Castor3D/Render/Technique/Opaque/OpaquePass.hpp"
#include "Castor3D/Scene/Camera.hpp"
#include "Castor3D/Scene/Scene.hpp"
#include "Castor3D/Scene/Background/Background.hpp"
#include "Castor3D/Shader/PassBuffer/PassBuffer.hpp"

#include <ashespp/RenderPass/FrameBuffer.hpp>

using namespace castor;

namespace castor3d
{
	//*********************************************************************************************

	namespace
	{
		SamplerSPtr doCreateSampler( Engine & engine
			, String const & name )
		{
			SamplerSPtr result;
			auto & cache = engine.getSamplerCache();

			if ( cache.has( name ) )
			{
				result = cache.find( name );
			}
			else
			{
				result = engine.getSamplerCache().add( cuT( "TextureProjection" ) );
				result->setMinFilter( VK_FILTER_NEAREST );
				result->setMagFilter( VK_FILTER_NEAREST );
				result->setWrapS( VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE );
				result->setWrapT( VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE );
				result->setWrapR( VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE );
				result->initialise();
			}

			return result;
		}

		TextureLayoutSPtr createTexture( Engine & engine
			, String const & name
			, TextureLayout const & source )
		{
			auto & renderSystem = *engine.getRenderSystem();
			ashes::ImageCreateInfo image
			{
				0u,
				source.getType(),
				source.getPixelFormat(),
				source.getDimensions(),
				source.getMipmapCount(),
				source.getLayersCount(),
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			};
			auto texture = std::make_shared< TextureLayout >( renderSystem
				, std::move( image )
				, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
				, name );
			texture->initialise();
			return texture;
		}

		size_t getIndex( SsaoConfig const & ssaoConfig
			, Scene const & scene )
		{
			return 2u * ( ssaoConfig.enabled ? 1u : 0u )
				+ ( scene.needsSubsurfaceScattering() ? 1u : 0u );
		}
	}

	//*********************************************************************************************

	DeferredRendering::DeferredRendering( Engine & engine
		, OpaquePass & opaquePass
		, TextureUnit const & depthTexture
		, TextureUnit const & velocityTexture
		, TextureUnit const & resultTexture
		, ShadowMapResult const & smDirectionalResult
		, ShadowMapResult const & smPointResult
		, ShadowMapResult const & smSpotResult
		, Size const & size
		, Scene & scene
		, HdrConfigUbo const & hdrConfigUbo
		, GpInfoUbo const & gpInfoUbo
		, SsaoConfig & ssaoConfig )
		: m_engine{ engine }
		, m_ssaoConfig{ ssaoConfig }
		, m_opaquePass{ opaquePass }
		, m_size{ size }
		, m_gpInfoUbo{ gpInfoUbo }
		, m_opaquePassResult{ m_engine
			, depthTexture
			, velocityTexture }
		, m_linearisePass{ std::make_unique< LineariseDepthPass >( m_engine
			, cuT( "Deferred" )
			, m_size
			, depthTexture.getTexture()->getDefaultView().getSampledView() ) }
		, m_lightingPass{ std::make_unique< LightingPass >( m_engine
			, m_size
			, scene
			, m_opaquePassResult
			, smDirectionalResult
			, smPointResult
			, smSpotResult
			, depthTexture.getTexture()->getDefaultView().getTargetView()
			, m_opaquePass.getSceneUbo()
			, m_gpInfoUbo ) }
		, m_ssao{ std::make_unique< SsaoPass >( m_engine
			, m_size
			, m_ssaoConfig
			, m_linearisePass->getResult()
			, m_opaquePassResult
			, m_gpInfoUbo ) }
		, m_subsurfaceScattering{ std::make_unique< SubsurfaceScatteringPass >( m_engine
			, m_gpInfoUbo
			, m_opaquePass.getSceneUbo()
			, m_size
			, m_opaquePassResult
			, m_lightingPass->getResult() ) }
	{
		//SSAO Off
		//	SSSSS Off
		m_resolve.emplace_back( std::make_unique< OpaqueResolvePass >( m_engine
			, scene
			, m_opaquePassResult
			, m_lightingPass->getResult()[LpTexture::eDiffuse]
			, m_lightingPass->getResult()[LpTexture::eSpecular]
			, resultTexture
			, m_opaquePass.getSceneUbo()
			, m_gpInfoUbo
			, hdrConfigUbo
			, nullptr ) );
		//	SSSSS On
		m_resolve.emplace_back( std::make_unique< OpaqueResolvePass >( m_engine
			, scene
			, m_opaquePassResult
			, m_subsurfaceScattering->getResult()
			, m_lightingPass->getResult()[LpTexture::eSpecular]
			, resultTexture
			, m_opaquePass.getSceneUbo()
			, m_gpInfoUbo
			, hdrConfigUbo
			, nullptr ) );
		//SSAO On
		//	SSSSS Off
		m_resolve.emplace_back( std::make_unique< OpaqueResolvePass >( m_engine
			, scene
			, m_opaquePassResult
			, m_lightingPass->getResult()[LpTexture::eDiffuse]
			, m_lightingPass->getResult()[LpTexture::eSpecular]
			, resultTexture
			, m_opaquePass.getSceneUbo()
			, m_gpInfoUbo
			, hdrConfigUbo
			, m_ssao.raw() ) );
		//	SSSSS On
		m_resolve.emplace_back( std::make_unique< OpaqueResolvePass >( m_engine
			, scene
			, m_opaquePassResult
			, m_subsurfaceScattering->getResult()
			, m_lightingPass->getResult()[LpTexture::eSpecular]
			, resultTexture
			, m_opaquePass.getSceneUbo()
			, m_gpInfoUbo
			, hdrConfigUbo
			, m_ssao.raw() ) );
		m_opaquePass.initialiseRenderPass( m_opaquePassResult );

		m_linearisePass.initialise();
		m_ssao.initialise();
		m_subsurfaceScattering.initialise();

		for ( auto & pass : m_resolve )
		{
			pass.initialise();
		}
	}

	DeferredRendering::~DeferredRendering()
	{
		for ( auto & pass : m_resolve )
		{
			pass.cleanup();
		}

		m_subsurfaceScattering.cleanup();
		m_ssao.cleanup();
		m_linearisePass.cleanup();
	}

	void DeferredRendering::update( CpuUpdater & updater )
	{
		auto & camera = *updater.camera;
		auto & scene = *camera.getScene();
		m_opaquePass.update( updater );

		if ( m_ssaoConfig.enabled )
		{
			m_linearisePass->update( updater );
		}

		if ( m_ssaoConfig.enabled )
		{
			m_ssao->update( updater );
		}
	}

	void DeferredRendering::update( GpuUpdater & updater )
	{
		auto & scene = *updater.scene;
		auto & camera = *updater.camera;
		m_opaquePass.update( updater );

		auto index = getIndex( m_ssaoConfig, scene );
		m_resolve[index]->update( camera );

		if ( m_ssaoConfig.enabled )
		{
			m_linearisePass->update( updater );
		}
	}

	ashes::Semaphore const & DeferredRendering::render( Scene const & scene
		, Camera const & camera
		, ashes::Semaphore const & toWait )
	{
		ashes::Semaphore const * result = &toWait;
		result = &m_opaquePass.render( *result );
		result = &m_lightingPass->render( scene
			, camera
			, m_opaquePassResult
			, *result );

		if ( m_ssaoConfig.enabled )
		{
			result = &m_linearisePass->linearise( *result );
		}

		if ( m_ssaoConfig.enabled )
		{
			result = &m_ssao->render( *result );
		}

		if ( scene.needsSubsurfaceScattering() )
		{
			result = &m_subsurfaceScattering->render( *result );
		}

		auto index = getIndex( m_ssaoConfig, scene );
		result = &m_resolve[index]->render( *result );

		return *result;
	}

	void DeferredRendering::accept( RenderTechniqueVisitor & visitor )
	{
		visitor.visit( "Opaque Data1", m_opaquePassResult[DsTexture::eData1].getTexture()->getDefaultView().getSampledView() );
		visitor.visit( "Opaque Data2", m_opaquePassResult[DsTexture::eData2].getTexture()->getDefaultView().getSampledView() );
		visitor.visit( "Opaque Data3", m_opaquePassResult[DsTexture::eData3].getTexture()->getDefaultView().getSampledView() );
		visitor.visit( "Opaque Data4", m_opaquePassResult[DsTexture::eData4].getTexture()->getDefaultView().getSampledView() );
		visitor.visit( "Opaque Data5", m_opaquePassResult[DsTexture::eData5].getTexture()->getDefaultView().getSampledView() );

		m_opaquePass.accept( visitor );
		m_lightingPass->accept( visitor );

		if ( m_ssaoConfig.enabled
			|| visitor.forceSubPassesVisit )
		{
			m_linearisePass.raw()->accept( visitor );
		}

		if ( m_ssaoConfig.enabled
			|| visitor.forceSubPassesVisit )
		{
			m_ssao.raw()->accept( visitor );
		}

		if ( visitor.getScene().needsSubsurfaceScattering()
			|| visitor.forceSubPassesVisit )
		{
			m_subsurfaceScattering.raw()->accept( visitor );
		}

		auto index = getIndex( m_ssaoConfig, visitor.getScene() );
		m_resolve[index].raw()->accept( visitor );
	}
}