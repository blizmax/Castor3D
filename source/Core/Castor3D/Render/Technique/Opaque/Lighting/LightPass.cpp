#include "Castor3D/Render/Technique/Opaque/Lighting/LightPass.hpp"

#include "Castor3D/Engine.hpp"
#include "Castor3D/Buffer/UniformBuffer.hpp"
#include "Castor3D/Buffer/UniformBufferPools.hpp"
#include "Castor3D/Cache/MaterialCache.hpp"
#include "Castor3D/Material/Texture/Sampler.hpp"
#include "Castor3D/Material/Texture/TextureLayout.hpp"
#include "Castor3D/Material/Texture/TextureUnit.hpp"
#include "Castor3D/Miscellaneous/makeVkType.hpp"
#include "Castor3D/Render/RenderPassTimer.hpp"
#include "Castor3D/Render/RenderSystem.hpp"
#include "Castor3D/Render/ShadowMap/ShadowMap.hpp"
#include "Castor3D/Render/Technique/Opaque/OpaquePassResult.hpp"
#include "Castor3D/Render/Technique/Opaque/Lighting/LightPassResult.hpp"
#include "Castor3D/Scene/Scene.hpp"
#include "Castor3D/Scene/Light/Light.hpp"
#include "Castor3D/Shader/Program.hpp"
#include "Castor3D/Shader/PassBuffer/PassBuffer.hpp"
#include "Castor3D/Shader/Shaders/GlslFog.hpp"
#include "Castor3D/Shader/Shaders/GlslLight.hpp"
#include "Castor3D/Shader/Shaders/GlslLighting.hpp"
#include "Castor3D/Shader/Shaders/GlslMaterial.hpp"
#include "Castor3D/Shader/Shaders/GlslMetallicBrdfLighting.hpp"
#include "Castor3D/Shader/Shaders/GlslOutputComponents.hpp"
#include "Castor3D/Shader/Shaders/GlslPhongLighting.hpp"
#include "Castor3D/Shader/Shaders/GlslSpecularBrdfLighting.hpp"
#include "Castor3D/Shader/Shaders/GlslSssTransmittance.hpp"
#include "Castor3D/Shader/Shaders/GlslUtils.hpp"
#include "Castor3D/Shader/Ubos/GpInfoUbo.hpp"
#include "Castor3D/Shader/Ubos/ModelMatrixUbo.hpp"
#include "Castor3D/Shader/Ubos/SceneUbo.hpp"

#include <CastorUtils/Miscellaneous/Hash.hpp>

#include <ashespp/Command/CommandBuffer.hpp>
#include <ashespp/Descriptor/DescriptorSet.hpp>
#include <ashespp/Descriptor/DescriptorSetLayout.hpp>
#include <ashespp/Descriptor/DescriptorSetPool.hpp>
#include <ashespp/Image/Image.hpp>
#include <ashespp/Image/ImageView.hpp>
#include <ashespp/RenderPass/RenderPass.hpp>

#include <ShaderWriter/Source.hpp>

using namespace castor;
using namespace castor3d;

#define C3D_UseLightPassFence 1
#define C3D_DisableSSSTransmittance 1

namespace castor3d
{
	//************************************************************************************************

	float getMaxDistance( LightCategory const & light
		, Point3f const & attenuation )
	{
		constexpr float threshold = 0.000001f;
		auto constant = std::abs( attenuation[0] );
		auto linear = std::abs( attenuation[1] );
		auto quadratic = std::abs( attenuation[2] );
		float result = std::numeric_limits< float >::max();

		if ( constant >= threshold
			|| linear >= threshold
			|| quadratic >= threshold )
		{
			float maxChannel = std::max( std::max( light.getColour()[0]
				, light.getColour()[1] )
				, light.getColour()[2] );
			result = 256.0f * maxChannel * light.getDiffuseIntensity();

			if ( quadratic >= threshold )
			{
				if ( linear < threshold )
				{
					CU_Require( result >= constant );
					result = sqrtf( ( result - constant ) / quadratic );
				}
				else
				{
					auto delta = linear * linear - 4 * quadratic * ( constant - result );
					CU_Require( delta >= 0 );
					result = ( -linear + sqrtf( delta ) ) / ( 2 * quadratic );
				}
			}
			else if ( linear >= threshold )
			{
				result = ( result - constant ) / linear;
			}
			else
			{
				result = 4000.0f;
			}
		}
		else
		{
			log::error << cuT( "Light's attenuation is set to (0.0, 0.0, 0.0), which results in infinite litten distance, not representable." ) << std::endl;
		}

		return result;
	}

	float getMaxDistance( LightCategory const & light
		, Point3f const & attenuation
		, float max )
	{
		return std::min( max, getMaxDistance( light, attenuation ) );
	}

	namespace
	{
		ashes::PipelineShaderStageCreateInfoArray doCreateProgram( RenderDevice const & device
			, ShaderModule const & vtx
			, ShaderModule const & pxl )
		{
			return ashes::PipelineShaderStageCreateInfoArray
			{
				makeShaderState( device, vtx ),
				makeShaderState( device, pxl ),
			};
		}
	}

	//************************************************************************************************

	LightPass::Program::Program( Engine & engine
		, RenderDevice const & device
		, String const & name
		, ShaderModule const & vtx
		, ShaderModule const & pxl
		, bool hasShadows
		, bool generatesIndirect )
		: castor::Named{ name }
		, m_engine{ engine }
		, m_device{ device }
		, m_program{ ::doCreateProgram( device, vtx, pxl ) }
		, m_shadows{ hasShadows }
		, m_generatesIndirect{ generatesIndirect }
	{
	}

	void LightPass::Program::initialise( ashes::VertexBufferBase & vbo
		, ashes::PipelineVertexInputStateCreateInfo const & vertexLayout
		, ashes::RenderPass const & firstRenderPass
		, ashes::RenderPass const & blendRenderPass
		, MatrixUbo & matrixUbo
		, SceneUbo & sceneUbo
		, GpInfoUbo const & gpInfoUbo
		, UniformBufferT< ModelMatrixUboConfiguration > const * modelMatrixUbo )
	{
		ashes::VkDescriptorSetLayoutBindingArray setLayoutBindings;
		setLayoutBindings.emplace_back( m_engine.getMaterialCache().getPassBuffer().createLayoutBinding() );
		setLayoutBindings.emplace_back( makeDescriptorSetLayoutBinding( MatrixUbo::BindingPoint
			, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
			, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT ) );
		setLayoutBindings.emplace_back( makeDescriptorSetLayoutBinding( SceneUbo::BindingPoint
			, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
			, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT ) );
		setLayoutBindings.emplace_back( makeDescriptorSetLayoutBinding( GpInfoUbo::BindingPoint
			, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
			, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT ) );

		if ( modelMatrixUbo )
		{
			setLayoutBindings.emplace_back( makeDescriptorSetLayoutBinding( ModelMatrixUbo::BindingPoint
				, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
				, VK_SHADER_STAGE_VERTEX_BIT ) );
		}

		setLayoutBindings.emplace_back( makeDescriptorSetLayoutBinding( shader::LightingModel::UboBindingPoint
			, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
			, VK_SHADER_STAGE_FRAGMENT_BIT ) );
		m_uboDescriptorLayout = m_device->createDescriptorSetLayout( getName() + "Ubo"
			, std::move( setLayoutBindings ) );
		m_uboDescriptorPool = m_uboDescriptorLayout->createPool( getName() + "Ubo", 2u );
		uint32_t index = getMinBufferIndex();

		setLayoutBindings = ashes::VkDescriptorSetLayoutBindingArray
		{
			makeDescriptorSetLayoutBinding( index++
			, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
				, VK_SHADER_STAGE_FRAGMENT_BIT ),
			makeDescriptorSetLayoutBinding( index++
				, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
				, VK_SHADER_STAGE_FRAGMENT_BIT ),
			makeDescriptorSetLayoutBinding( index++
				, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
				, VK_SHADER_STAGE_FRAGMENT_BIT ),
			makeDescriptorSetLayoutBinding( index++
				, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
				, VK_SHADER_STAGE_FRAGMENT_BIT ),
			makeDescriptorSetLayoutBinding( index++
				, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
				, VK_SHADER_STAGE_FRAGMENT_BIT ),
			makeDescriptorSetLayoutBinding( index++
				, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
				, VK_SHADER_STAGE_FRAGMENT_BIT ),
		};

		if ( m_shadows )
		{
			setLayoutBindings.emplace_back( makeDescriptorSetLayoutBinding( index++
				, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
				, VK_SHADER_STAGE_FRAGMENT_BIT ) );
			setLayoutBindings.emplace_back( makeDescriptorSetLayoutBinding( index++
				, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
				, VK_SHADER_STAGE_FRAGMENT_BIT ) );
		}

		m_textureDescriptorLayout = m_device->createDescriptorSetLayout( getName() + "Tex"
			, std::move( setLayoutBindings ) );
		m_textureDescriptorPool = m_textureDescriptorLayout->createPool( getName() + "Tex", 2u );

		m_pipelineLayout = m_device->createPipelineLayout( getName()
			, { *m_uboDescriptorLayout, *m_textureDescriptorLayout } );
		m_firstPipeline = doCreatePipeline( vertexLayout, firstRenderPass, false );
		m_blendPipeline = doCreatePipeline( vertexLayout, blendRenderPass, true );
	}

	void LightPass::Program::cleanup()
	{
		m_blendPipeline.reset();
		m_firstPipeline.reset();
		m_pipelineLayout.reset();
		m_textureDescriptorPool.reset();
		m_textureDescriptorLayout.reset();
		m_uboDescriptorPool.reset();
		m_uboDescriptorLayout.reset();
	}

	void LightPass::Program::bind( Light const & light )
	{
		doBind( light );
	}

	void LightPass::Program::render( ashes::CommandBuffer & commandBuffer
		, uint32_t count
		, bool first
		, uint32_t offset )const
	{
		doRender( commandBuffer
			, count
			, first
			, offset );
	}

	void LightPass::Program::doRender( ashes::CommandBuffer & commandBuffer
		, uint32_t count
		, bool first
		, uint32_t offset )const
	{
		if ( first )
		{
			commandBuffer.bindPipeline( *m_firstPipeline );
		}
		else
		{
			commandBuffer.bindPipeline( *m_blendPipeline );
		}

		commandBuffer.draw( count, 1u, offset );
	}

	//************************************************************************************************

	LightPass::RenderPass::RenderPass( std::string const & name
		, ashes::RenderPassPtr renderPass
		, LightPassResult const & lpResult
		, bool generatesIndirect )
		: renderPass{ std::move( renderPass ) }
	{
		ashes::ImageViewCRefArray attaches
		{
			lpResult[LpTexture::eDepth].getTexture()->getDefaultView().getTargetView(),
			lpResult[LpTexture::eDiffuse].getTexture()->getDefaultView().getTargetView(),
			lpResult[LpTexture::eSpecular].getTexture()->getDefaultView().getTargetView(),
		};

		if ( !generatesIndirect )
		{
			attaches.emplace_back( lpResult[LpTexture::eIndirectDiffuse].getTexture()->getDefaultView().getTargetView() );
		}

		frameBuffer = this->renderPass->createFrameBuffer( name
			, { lpResult[LpTexture::eDepth].getTexture()->getWidth()
				, lpResult[LpTexture::eDepth].getTexture()->getHeight() }
			, std::move( attaches ) );
	}

	//************************************************************************************************

	LightPass::LightPass( RenderDevice const & device
		, String const & suffix
		, ashes::RenderPassPtr firstRenderPass
		, ashes::RenderPassPtr blendRenderPass
		, LightPassConfig const & lpConfig )
		: castor::Named{ "LightPass" + suffix }
		, m_engine{ *device.renderSystem.getEngine() }
		, m_device{ device }
		, m_firstRenderPass{ getName() + "First", std::move( firstRenderPass ), lpConfig.lpResult, lpConfig.generatesIndirect }
		, m_blendRenderPass{ getName() + "Blend", std::move( blendRenderPass ), lpConfig.lpResult, lpConfig.generatesIndirect }
		, m_shadows{ lpConfig.hasShadows }
		, m_generatesIndirect{ lpConfig.generatesIndirect }
		, m_matrixUbo{ device }
		, m_gpInfoUbo{ lpConfig.gpInfoUbo }
		, m_sampler{ m_engine.getDefaultSampler() }
		, m_signalImgReady{ m_device->createSemaphore( getName() + "ImgReady" ) }
		, m_signalImgFinished{ m_device->createSemaphore( getName() + "ImgFinished" ) }
		, m_fence{ m_device->createFence( getName(), VK_FENCE_CREATE_SIGNALED_BIT ) }
		, m_vertexShader{ VK_SHADER_STAGE_VERTEX_BIT, getName() }
		, m_pixelShader{ VK_SHADER_STAGE_FRAGMENT_BIT, getName() }
	{
		m_matrixUbo.initialise( m_device );
	}

	void LightPass::update( bool first
		, castor::Size const & size
		, Light const & light
		, Camera const & camera
		, ShadowMap const * shadowMap
		, uint32_t shadowMapIndex )
	{
		m_pipeline = doGetPipeline( first
			, light
			, shadowMap );
		doUpdate( first
			, size
			, light
			, camera
			, shadowMap
			, shadowMapIndex );
	}

	ashes::Semaphore const & LightPass::render( uint32_t index
		, ashes::Semaphore const & toWait )
	{
		CU_Require( m_pipeline );
		auto result = &toWait;
		auto & renderSystem = *m_engine.getRenderSystem();
		auto & commandBuffer = *m_commandBuffers[m_commandBufferIndex];

		commandBuffer.begin( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );
		commandBuffer.beginDebugBlock(
			{
				"Deferred - " + getName(),
				makeFloatArray( m_engine.getNextRainbowColour() ),
			} );
		m_timer->beginPass( commandBuffer, index );
		m_timer->notifyPassRender( index );

		if ( !index )
		{
			commandBuffer.beginRenderPass( *m_firstRenderPass.renderPass
				, *m_firstRenderPass.frameBuffer
				, { defaultClearDepthStencil, opaqueBlackClearColor, opaqueBlackClearColor, doGetIndirectClearColor() }
				, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS );
			commandBuffer.executeCommands( { *m_pipeline->firstCommandBuffer } );
		}
		else
		{
			commandBuffer.beginRenderPass( *m_blendRenderPass.renderPass
				, *m_blendRenderPass.frameBuffer
				, { defaultClearDepthStencil, opaqueBlackClearColor, opaqueBlackClearColor, doGetIndirectClearColor() }
				, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS );
			commandBuffer.executeCommands( { *m_pipeline->blendCommandBuffer } );
		}

		commandBuffer.endRenderPass();
		m_timer->endPass( commandBuffer, index );
		commandBuffer.endDebugBlock();
		commandBuffer.end();

#if C3D_UseLightPassFence
		m_fence->reset();
		m_device.graphicsQueue->submit( { commandBuffer }
			, { *result }
			, { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }
			, { *m_signalImgFinished }
			, m_fence.get() );
		m_fence->wait( ashes::MaxTimeout );
#else
		m_device.graphicsQueue->submit( { commandBuffer }
			, ( !index
				? ashes::SemaphoreCRefArray{ *result }
				: ashes::SemaphoreCRefArray{ *result, *m_signalImgReady } )
			, ( !index
				? ashes::VkPipelineStageFlagsArray{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }
				: ashes::VkPipelineStageFlagsArray{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT } )
			, { *m_signalImgFinished, *m_signalImgReady }
			, nullptr );
		m_commandBufferIndex = 1u - m_commandBufferIndex;
#endif
		result = m_signalImgFinished.get();
		return *result;
	}

	size_t LightPass::makeKey( Light const & light
		, ShadowMap const * shadowMap )
	{
		size_t hash = std::hash< LightType >{}( light.getLightType() );
		castor::hashCombine( hash, shadowMap ? light.getShadowType() : ShadowType::eNone );
		castor::hashCombine( hash, shadowMap ? light.getVolumetricSteps() > 0 : false );
		castor::hashCombine( hash, shadowMap ? light.needsRsmShadowMaps() : false );
		castor::hashCombine( hash, shadowMap );
		return hash;
	}

	LightPass::Pipeline LightPass::createPipeline( LightType lightType
		, ShadowType shadowType
		, bool rsm
		, ShadowMap const * shadowMap )
	{
		Scene const & scene = *m_scene;
		OpaquePassResult const & gp = *m_opaquePassResult;
		ashes::VertexBufferBase & vbo = *m_vertexBuffer;
		ashes::PipelineVertexInputStateCreateInfo const & vertexLayout = *m_pUsedVertexLayout;
		SceneUbo & sceneUbo = *m_sceneUbo;
		SceneFlags sceneFlags{ scene.getFlags() };
		LightPass::Pipeline pipeline;
		m_vertexShader.shader = doGetVertexShaderSource( sceneFlags );

		if ( scene.getMaterialsType() == MaterialType::eMetallicRoughness )
		{
			m_pixelShader.shader = doGetPbrMRPixelShaderSource( sceneFlags
				, lightType
				, shadowType
				, rsm );
		}
		else if ( scene.getMaterialsType() == MaterialType::eSpecularGlossiness )
		{
			m_pixelShader.shader = doGetPbrSGPixelShaderSource( sceneFlags
				, lightType
				, shadowType
				, rsm );
		}
		else
		{
			m_pixelShader.shader = doGetPhongPixelShaderSource( sceneFlags
				, lightType
				, shadowType
				, rsm );
		}

		pipeline.program = doCreateProgram();
		pipeline.program->initialise( vbo
			, vertexLayout
			, *m_firstRenderPass.renderPass
			, *m_blendRenderPass.renderPass
			, m_matrixUbo
			, sceneUbo
			, m_gpInfoUbo
			, m_mmUbo );
		pipeline.uboDescriptorSet = pipeline.program->getUboDescriptorPool().createDescriptorSet( getName() + "Ubo", 0u );
		auto & uboLayout = pipeline.program->getUboDescriptorLayout();
		m_engine.getMaterialCache().getPassBuffer().createBinding( *pipeline.uboDescriptorSet, uboLayout.getBinding( 0u ) );
		m_matrixUbo.createSizedBinding( *pipeline.uboDescriptorSet
			, uboLayout.getBinding( MatrixUbo::BindingPoint ) );
		sceneUbo.createSizedBinding( *pipeline.uboDescriptorSet
			, uboLayout.getBinding( SceneUbo::BindingPoint ) );
		m_gpInfoUbo.createSizedBinding( *pipeline.uboDescriptorSet
			, uboLayout.getBinding( GpInfoUbo::BindingPoint ) );

		if ( m_mmUbo )
		{
			pipeline.uboDescriptorSet->createSizedBinding( uboLayout.getBinding( ModelMatrixUbo::BindingPoint )
				, m_mmUbo->getBuffer() );
		}

		pipeline.uboDescriptorSet->createSizedBinding( uboLayout.getBinding( shader::LightingModel::UboBindingPoint )
			, *m_baseUbo );
		pipeline.uboDescriptorSet->update();

		pipeline.firstCommandBuffer = m_device.graphicsCommandPool->createCommandBuffer( getName() + "First"
			, VK_COMMAND_BUFFER_LEVEL_SECONDARY );
		pipeline.blendCommandBuffer = m_device.graphicsCommandPool->createCommandBuffer( getName() + "Blend"
			, VK_COMMAND_BUFFER_LEVEL_SECONDARY );

		pipeline.textureDescriptorSet = pipeline.program->getTextureDescriptorPool().createDescriptorSet( getName() + "Tex"
			, 1u );
		auto & texLayout = pipeline.program->getTextureDescriptorLayout();
		auto writeBinding = [&gp, this]( uint32_t index, VkImageLayout layout )
		{
			auto & unit = gp[DsTexture( index - getMinBufferIndex() )];
			return ashes::WriteDescriptorSet
			{
				index,
				0u,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				{ { unit.getSampler()->getSampler(), unit.getTexture()->getDefaultView().getSampledView(), layout } }
			};
		};
		uint32_t index = getMinBufferIndex();
		pipeline.textureWrites.push_back( writeBinding( index++, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ) );
		pipeline.textureWrites.push_back( writeBinding( index++, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) );
		pipeline.textureWrites.push_back( writeBinding( index++, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) );
		pipeline.textureWrites.push_back( writeBinding( index++, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) );
		pipeline.textureWrites.push_back( writeBinding( index++, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) );
		pipeline.textureWrites.push_back( writeBinding( index++, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ) );

		if ( shadowMap && m_shadows )
		{
			auto & smResult = shadowMap->getShadowPassResult();
			pipeline.textureWrites.push_back( ashes::WriteDescriptorSet
				{
					index++,
					0u,
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					{
						{
							shadowMap->getSampler( SmTexture::eNormalLinear ),
							shadowMap->getView( SmTexture::eNormalLinear ),
							VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
						}
					}
				} );
			pipeline.textureWrites.push_back( ashes::WriteDescriptorSet
				{
					index++,
					0u,
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					{
						{
							shadowMap->getSampler( SmTexture::eVariance ),
							shadowMap->getView( SmTexture::eVariance ),
							VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
						}
					}
				} );
		}

		pipeline.textureDescriptorSet->setBindings( pipeline.textureWrites );
		pipeline.textureDescriptorSet->update();

		return pipeline;
	}

	LightPass::Pipeline * LightPass::doGetPipeline( bool first
		, Light const & light
		, ShadowMap const * shadowMap )
	{
		auto key = makeKey( light, shadowMap );
		auto it = m_pipelines.emplace( key, nullptr );

		if ( it.second )
		{
			it.first->second = std::make_unique< Pipeline >( createPipeline( light.getLightType()
				, ( shadowMap
					? light.getShadowType()
					: ShadowType::eNone )
				, light.needsRsmShadowMaps()
				, shadowMap ) );
		}

		doPrepareCommandBuffer( *it.first->second
			, shadowMap
			, first );
		return it.first->second.get();
	}

	VkClearValue LightPass::doGetIndirectClearColor()const
	{
		return opaqueBlackClearColor;
	}

	void LightPass::doInitialise( Scene const & scene
		, OpaquePassResult const & gp
		, LightType lightType
		, ashes::VertexBufferBase & vbo
		, ashes::PipelineVertexInputStateCreateInfo const & vertexLayout
		, SceneUbo & sceneUbo
		, UniformBufferT< ModelMatrixUboConfiguration > const * modelMatrixUbo
		, RenderPassTimer & timer )
	{
		m_scene = &scene;
		m_sceneUbo = &sceneUbo;
		m_mmUbo = modelMatrixUbo;
		m_usedVertexLayout = vertexLayout;
		m_pUsedVertexLayout = &m_usedVertexLayout;
		m_timer = &timer;
		m_opaquePassResult = &gp;
		m_commandBuffers[0] = m_device.graphicsCommandPool->createCommandBuffer( getName() + "0"
			, VK_COMMAND_BUFFER_LEVEL_PRIMARY );
		m_commandBuffers[1] = m_device.graphicsCommandPool->createCommandBuffer( getName() + "1"
			, VK_COMMAND_BUFFER_LEVEL_PRIMARY );
	}

	void LightPass::doCleanup()
	{
		m_matrixUbo.cleanup( m_device );
		m_commandBuffers[0].reset();
		m_commandBuffers[1].reset();
		m_pipelines.clear();
	}

	void LightPass::doPrepareCommandBuffer( Pipeline & pipeline
		, ShadowMap const * shadowMap
		, bool first )
	{
		if ( ( first && !pipeline.isFirstSet )
			|| ( !first && !pipeline.isBlendSet ) )
		{
			auto & renderPass = first
				? m_firstRenderPass
				: m_blendRenderPass;
			auto & commandBuffer = first
				? *pipeline.firstCommandBuffer
				: *pipeline.blendCommandBuffer;
			auto & dimensions = renderPass.frameBuffer->getDimensions();

			commandBuffer.begin( VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT
				, makeVkType< VkCommandBufferInheritanceInfo >( *renderPass.renderPass
					, 0u
					, *renderPass.frameBuffer
					, VkBool32( VK_FALSE )
					, 0u
					, 0u ) );
			commandBuffer.setViewport( ashes::makeViewport( dimensions ) );
			commandBuffer.setScissor( ashes::makeScissor( dimensions ) );
			commandBuffer.bindDescriptorSets( { *pipeline.uboDescriptorSet, *pipeline.textureDescriptorSet }, pipeline.program->getPipelineLayout() );
			commandBuffer.bindVertexBuffer( 0u, m_vertexBuffer->getBuffer(), 0u );
			pipeline.program->render( commandBuffer, getCount(), first, m_offset );
			commandBuffer.end();

			pipeline.isFirstSet = pipeline.isFirstSet || first;
			pipeline.isBlendSet = pipeline.isBlendSet || !first;
		}
	}
	
	ShaderPtr LightPass::doGetPhongPixelShaderSource( SceneFlags const & sceneFlags
		, LightType lightType
		, ShadowType shadowType
		, bool rsm )const
	{
		using namespace sdw;
		FragmentWriter writer;
		auto & renderSystem = *m_engine.getRenderSystem();

		// Shader inputs
		UBO_SCENE( writer, SceneUbo::BindingPoint, 0u );
		UBO_GPINFO( writer, GpInfoUbo::BindingPoint, 0u );
		auto index = getMinBufferIndex();
		auto c3d_mapDepth = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eDepth ), index++, 1u );
		auto c3d_mapData1 = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eData1 ), index++, 1u );
		auto c3d_mapData2 = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eData2 ), index++, 1u );
		auto c3d_mapData3 = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eData3 ), index++, 1u );
		auto c3d_mapData4 = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eData4 ), index++, 1u );
		auto c3d_mapData5 = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eData5 ), index++, 1u );
		auto in = writer.getIn();

		shadowType = m_shadows
			? shadowType
			: ShadowType::eNone;

		// Shader outputs
		auto pxl_diffuse = writer.declOutput< Vec3 >( "pxl_diffuse", 0 );
		auto pxl_specular = writer.declOutput< Vec3 >( "pxl_specular", 1 );
		auto pxl_indirect = writer.declOutput< Vec3 >( "pxl_indirect", 2, !m_generatesIndirect );

		// Utility functions
		shader::Fog fog{ getFogType( sceneFlags ), writer };
		shader::Utils utils{ writer };
		utils.declareCalcTexCoord();
		utils.declareCalcVSPosition();
		utils.declareCalcWSPosition();
		utils.declareDecodeReceiver();
		utils.declareInvertVec2Y();
		auto lighting = shader::PhongLightingModel::createModel( writer
			, utils
			, lightType
			, true // lightUbo
			, m_shadows
			, rsm
			, index );
		shader::LegacyMaterials materials{ writer };
		materials.declare( renderSystem.getGpuInformations().hasShaderStorageBuffers() );
		shader::SssTransmittance sss{ writer
			, lighting->getShadowModel()
			, utils
			, m_shadows };
		sss.declare( lightType );

		writer.implementFunction< sdw::Void >( "main"
			, [&]()
			{
				auto texCoord = writer.declLocale( "texCoord"
					, utils.calcTexCoord( in.fragCoord.xy()
						, c3d_renderSize ) );
				auto data1 = writer.declLocale( "data1"
					, c3d_mapData1.lod( texCoord, 0.0_f ) );
				auto data2 = writer.declLocale( "data2"
					, c3d_mapData2.lod( texCoord, 0.0_f ) );
				auto data3 = writer.declLocale( "data3"
					, c3d_mapData3.lod( texCoord, 0.0_f ) );
				auto data4 = writer.declLocale( "data4"
					, c3d_mapData4.lod( texCoord, 0.0_f ) );
				auto data5 = writer.declLocale( "data5"
					, c3d_mapData5.lod( texCoord, 0.0_f ) );
				auto flags = writer.declLocale( "flags"
					, writer.cast< Int >( data1.w() ) );
				auto shadowReceiver = writer.declLocale( "shadowReceiver"
					, 0_i );
				utils.decodeReceiver( flags, shadowReceiver );
				auto materialId = writer.declLocale( "materialId"
					, writer.cast< UInt >( data5.z() )
					, C3D_DisableSSSTransmittance == 0 );
				auto shininess = writer.declLocale( "shininess"
					, data2.w() );
				auto lightDiffuse = writer.declLocale( "lightDiffuse"
					, vec3( 0.0_f ) );
				auto lightSpecular = writer.declLocale( "lightSpecular"
					, vec3( 0.0_f ) );
				auto eye = writer.declLocale( "eye"
					, c3d_cameraPosition.xyz() );
				auto depth = writer.declLocale( "depth"
					, c3d_mapDepth.lod( texCoord, 0.0_f ).x() );
				auto vsPosition = writer.declLocale( "vsPosition"
					, utils.calcVSPosition( texCoord, depth, c3d_mtxInvProj ) );
				auto wsPosition = writer.declLocale( "wsPosition"
					, utils.calcWSPosition( texCoord, depth, c3d_mtxInvViewProj ) );
				auto wsNormal = writer.declLocale( "wsNormal"
					, data1.xyz() );
				auto translucency = writer.declLocale( "translucency"
					, data4.w() );
				auto material = writer.declLocale( "material"
					, materials.getMaterial( materialId )
					, C3D_DisableSSSTransmittance == 0 );

				shader::OutputComponents output{ lightDiffuse, lightSpecular };

				switch ( lightType )
				{
				case LightType::eDirectional:
				{
					auto c3d_light = writer.getVariable< shader::DirectionalLight >( "c3d_light" );
					auto light = writer.declLocale( "light", c3d_light );
					lighting->compute( light
						, eye
						, shininess
						, shadowReceiver
						, shader::FragmentInput( in.fragCoord.xy(), vsPosition, wsPosition, wsNormal )
						, output );
#if !C3D_DisableSSSTransmittance
					lightDiffuse += sss.compute( material
						, light
						, texCoord
						, wsPosition
						, wsNormal
						, translucency );
#endif
					break;
				}

				case LightType::ePoint:
				{
					auto c3d_light = writer.getVariable< shader::PointLight >( "c3d_light" );
					auto light = writer.declLocale( "light", c3d_light );
					lighting->compute( light
						, eye
						, shininess
						, shadowReceiver
						, shader::FragmentInput( in.fragCoord.xy(), vsPosition, wsPosition, wsNormal )
						, output );
#if !C3D_DisableSSSTransmittance
					lightDiffuse += sss.compute( material
						, light
						, texCoord
						, wsPosition
						, wsNormal
						, translucency );
#endif
					break;
				}

				case LightType::eSpot:
				{
					auto c3d_light = writer.getVariable< shader::SpotLight >( "c3d_light" );
					auto light = writer.declLocale( "light", c3d_light );
					lighting->compute( light
						, eye
						, shininess
						, shadowReceiver
						, shader::FragmentInput( in.fragCoord.xy(), vsPosition, wsPosition, wsNormal )
						, output );
#if !C3D_DisableSSSTransmittance
					lightDiffuse += sss.compute( material
						, light
						, texCoord
						, wsPosition
						, wsNormal
						, translucency );
#endif
					break;
				}

				default:
					break;
				}

				pxl_diffuse = lightDiffuse;
				pxl_specular = lightSpecular;
				pxl_indirect = vec3( 1.0_f, 1.0_f, 1.0_f );
			} );

		return std::make_unique< ast::Shader >( std::move( writer.getShader() ) );
	}
	
	ShaderPtr LightPass::doGetPbrMRPixelShaderSource( SceneFlags const & sceneFlags
		, LightType lightType
		, ShadowType shadowType
		, bool rsm )const
	{
		using namespace sdw;
		FragmentWriter writer;
		auto & renderSystem = *m_engine.getRenderSystem();

		// Shader inputs
		UBO_MATRIX( writer, MatrixUbo::BindingPoint, 0u );
		UBO_SCENE( writer, SceneUbo::BindingPoint, 0u );
		UBO_GPINFO( writer, GpInfoUbo::BindingPoint, 0u );
		auto index = getMinBufferIndex();
		auto c3d_mapDepth = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eDepth ), index++, 1u );
		auto c3d_mapData1 = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eData1 ), index++, 1u );
		auto c3d_mapData2 = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eData2 ), index++, 1u );
		auto c3d_mapData3 = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eData3 ), index++, 1u );
		auto c3d_mapData4 = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eData4 ), index++, 1u );
		auto c3d_mapData5 = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eData5 ), index++, 1u );
		auto in = writer.getIn();

		shadowType = m_shadows
			? shadowType
			: ShadowType::eNone;

		// Shader outputs
		auto pxl_diffuse = writer.declOutput< Vec3 >( "pxl_diffuse", 0 );
		auto pxl_specular = writer.declOutput< Vec3 >( "pxl_specular", 1 );
		auto pxl_indirect = writer.declOutput< Vec3 >( "pxl_indirect", 2, !m_generatesIndirect );

		// Utility functions
		shader::Fog fog{ getFogType( sceneFlags ), writer };
		shader::Utils utils{ writer };
		utils.declareCalcTexCoord();
		utils.declareCalcVSPosition();
		utils.declareCalcWSPosition();
		utils.declareDecodeReceiver();
		utils.declareInvertVec2Y();
		auto lighting = shader::MetallicBrdfLightingModel::createModel( writer
			, utils
			, lightType
			, true // lightUbo
			, m_shadows
			, rsm
			, index );
		shader::LegacyMaterials materials{ writer };
		materials.declare( renderSystem.getGpuInformations().hasShaderStorageBuffers() );
		shader::SssTransmittance sss{ writer
			, lighting->getShadowModel()
			, utils
			, m_shadows && shadowType != ShadowType::eNone };
		sss.declare( lightType );

		writer.implementFunction< sdw::Void >( "main"
			, [&]()
			{
				auto texCoord = writer.declLocale( "texCoord"
					, utils.calcTexCoord( in.fragCoord.xy()
						, c3d_renderSize ) );
				auto data1 = writer.declLocale( "data1"
					, c3d_mapData1.lod( texCoord, 0.0_f ) );
				auto data2 = writer.declLocale( "data2"
					, c3d_mapData2.lod( texCoord, 0.0_f ) );
				auto data3 = writer.declLocale( "data3"
					, c3d_mapData3.lod( texCoord, 0.0_f ) );
				auto data4 = writer.declLocale( "data4"
					, c3d_mapData4.lod( texCoord, 0.0_f ) );
				auto data5 = writer.declLocale( "data5"
					, c3d_mapData5.lod( texCoord, 0.0_f ) );
				auto metallic = writer.declLocale( "metallic"
					, data3.r() );
				auto roughness = writer.declLocale( "roughness"
					, data2.w() );
				auto flags = writer.declLocale( "flags"
					, writer.cast< Int >( data1.w() ) );
				auto shadowReceiver = writer.declLocale( "shadowReceiver"
					, 0_i );
				utils.decodeReceiver( flags, shadowReceiver );
				auto materialId = writer.declLocale( "materialId"
					, writer.cast< UInt >( data5.z() )
					, C3D_DisableSSSTransmittance == 0 );
				auto albedo = writer.declLocale( "albedo"
					, data2.xyz() );
				auto lightDiffuse = writer.declLocale( "lightDiffuse"
					, vec3( 0.0_f ) );
				auto lightSpecular = writer.declLocale( "lightSpecular"
					, vec3( 0.0_f ) );
				auto eye = writer.declLocale( "eye"
					, c3d_cameraPosition.xyz() );
				auto depth = writer.declLocale( "depth"
					, c3d_mapDepth.lod( texCoord, 0.0_f ).x() );
				auto vsPosition = writer.declLocale( "vsPosition"
					, utils.calcVSPosition( texCoord, depth, c3d_mtxInvProj ) );
				auto wsPosition = writer.declLocale( "wsPosition"
					, utils.calcWSPosition( texCoord, depth, c3d_mtxInvViewProj ) );
				auto wsNormal = writer.declLocale( "wsNormal"
					, data1.xyz() );
				auto transmittance = writer.declLocale( "transmittance"
					, data4.w() );
				auto material = writer.declLocale( "material"
					, materials.getMaterial( materialId )
					, C3D_DisableSSSTransmittance == 0 );

				shader::OutputComponents output{ lightDiffuse, lightSpecular };

				switch ( lightType )
				{
				case LightType::eDirectional:
				{
					auto c3d_light = writer.getVariable< shader::DirectionalLight >( "c3d_light" );
					auto light = writer.declLocale( "light", c3d_light );
#if !C3D_DisableSSSTransmittance
					IF( writer, !c3d_debugDeferredSSSTransmittance )
					{
						lighting->compute( light
							, eye
							, albedo
							, metallic
							, roughness
							, shadowReceiver
							, shader::FragmentInput( in.fragCoord.xy(), vsPosition, wsPosition, wsNormal )
							, output );
						lightDiffuse += sss.compute( material
							, light
							, texCoord
							, wsPosition
							, wsNormal
							, transmittance );
					}
					ELSE
					{
						lightDiffuse = sss.compute( material
						, light
							, texCoord
							, wsPosition
							, wsNormal
							, transmittance );
					}
					FI;
#else
					lighting->compute( light
						, eye
						, albedo
						, metallic
						, roughness
						, shadowReceiver
						, shader::FragmentInput( in.fragCoord.xy(), vsPosition, wsPosition, wsNormal )
						, output );
#endif
					break;
				}

				case LightType::ePoint:
				{
					auto c3d_light = writer.getVariable< shader::PointLight >( "c3d_light" );
					auto light = writer.declLocale( "light", c3d_light );
#if !C3D_DisableSSSTransmittance
					IF( writer, !c3d_debugDeferredSSSTransmittance )
					{
						lighting->compute( light
							, eye
							, albedo
							, metallic
							, roughness
							, shadowReceiver
							, shader::FragmentInput( wsPosition, wsNormal )
							, output );
						lightDiffuse += sss.compute( material
							, light
							, texCoord
							, wsPosition
							, wsNormal
							, transmittance );
					}
					ELSE
					{
						lightDiffuse = sss.compute( material
							, light
							, texCoord
							, wsPosition
							, wsNormal
							, transmittance );
					}
					FI;
#else
					lighting->compute( light
						, eye
						, albedo
						, metallic
						, roughness
						, shadowReceiver
						, shader::FragmentInput( in.fragCoord.xy(), vsPosition, wsPosition, wsNormal )
						, output );
#endif
					break;
				}

				case LightType::eSpot:
				{
					auto c3d_light = writer.getVariable< shader::SpotLight >( "c3d_light" );
					auto light = writer.declLocale( "light", c3d_light );
#if !C3D_DisableSSSTransmittance
					IF( writer, !c3d_debugDeferredSSSTransmittance )
					{
						lighting->compute( light
							, eye
							, albedo
							, metallic
							, roughness
							, shadowReceiver
							, shader::FragmentInput( in.fragCoord.xy(), vsPosition, wsPosition, wsNormal )
							, output );
						lightDiffuse += sss.compute( material
							, light
							, texCoord
							, wsPosition
							, wsNormal
							, transmittance );
					}
					ELSE
					{
						lightDiffuse = sss.compute( material
							, light
							, texCoord
							, wsPosition
							, wsNormal
							, transmittance );
					}
					FI;
#else
					lighting->compute( light
						, eye
						, albedo
						, metallic
						, roughness
						, shadowReceiver
						, shader::FragmentInput( in.fragCoord.xy(), vsPosition, wsPosition, wsNormal )
						, output );
#endif
					break;
				}

				default:
					break;
				}

				pxl_diffuse = lightDiffuse;
				pxl_specular = lightSpecular;
				pxl_indirect = vec3( 1.0_f, 1.0_f, 1.0_f );
			} );

		return std::make_unique< ast::Shader >( std::move( writer.getShader() ) );
	}
	
	ShaderPtr LightPass::doGetPbrSGPixelShaderSource( SceneFlags const & sceneFlags
		, LightType lightType
		, ShadowType shadowType
		, bool rsm )const
	{
		using namespace sdw;
		FragmentWriter writer;
		auto & renderSystem = *m_engine.getRenderSystem();

		// Shader inputs
		UBO_MATRIX( writer, MatrixUbo::BindingPoint, 0u );
		UBO_SCENE( writer, SceneUbo::BindingPoint, 0u );
		UBO_GPINFO( writer, GpInfoUbo::BindingPoint, 0u );
		auto index = getMinBufferIndex();
		auto c3d_mapDepth = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eDepth ), index++, 1u );
		auto c3d_mapData1 = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eData1 ), index++, 1u );
		auto c3d_mapData2 = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eData2 ), index++, 1u );
		auto c3d_mapData3 = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eData3 ), index++, 1u );
		auto c3d_mapData4 = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eData4 ), index++, 1u );
		auto c3d_mapData5 = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eData5 ), index++, 1u );
		auto in = writer.getIn();

		shadowType = m_shadows
			? shadowType
			: ShadowType::eNone;

		// Utility functions
		shader::Fog fog{ getFogType( sceneFlags ), writer };
		shader::Utils utils{ writer };
		utils.declareCalcTexCoord();
		utils.declareCalcVSPosition();
		utils.declareCalcWSPosition();
		utils.declareDecodeReceiver();
		utils.declareInvertVec2Y();
		auto lighting = shader::SpecularBrdfLightingModel::createModel( writer
			, utils
			, lightType
			, true // lightUbo
			, m_shadows
			, rsm
			, index );
		shader::LegacyMaterials materials{ writer };
		materials.declare( renderSystem.getGpuInformations().hasShaderStorageBuffers() );
		shader::SssTransmittance sss{ writer
			, lighting->getShadowModel()
			, utils
			, m_shadows && shadowType != ShadowType::eNone };
		sss.declare( lightType );

		// Shader outputs
		auto pxl_diffuse = writer.declOutput< Vec3 >( "pxl_diffuse", 0 );
		auto pxl_specular = writer.declOutput< Vec3 >( "pxl_specular", 1 );
		auto pxl_indirect = writer.declOutput< Vec3 >( "pxl_indirect", 2, !m_generatesIndirect );

		writer.implementFunction< sdw::Void >( "main"
			, [&]()
			{
				auto texCoord = writer.declLocale( "texCoord"
					, utils.calcTexCoord( in.fragCoord.xy()
						, c3d_renderSize ) );
				auto data1 = writer.declLocale( "data1"
					, c3d_mapData1.lod( texCoord, 0.0_f ) );
				auto data2 = writer.declLocale( "data2"
					, c3d_mapData2.lod( texCoord, 0.0_f ) );
				auto data3 = writer.declLocale( "data3"
					, c3d_mapData3.lod( texCoord, 0.0_f ) );
				auto data4 = writer.declLocale( "data4"
					, c3d_mapData4.lod( texCoord, 0.0_f ) );
				auto data5 = writer.declLocale( "data5"
					, c3d_mapData5.lod( texCoord, 0.0_f ) );
				auto specular = writer.declLocale( "specular"
					, data3.rgb() );
				auto glossiness = writer.declLocale( "glossiness"
					, data2.a() );
				auto flags = writer.declLocale( "flags"
					, writer.cast< Int >( data1.w() ) );
				auto shadowReceiver = writer.declLocale( "shadowReceiver"
					, 0_i );
				utils.decodeReceiver( flags, shadowReceiver );
				auto materialId = writer.declLocale( "materialId"
					, writer.cast< UInt >( data5.z() )
					, C3D_DisableSSSTransmittance == 0 );
				auto diffuse = writer.declLocale( "diffuse"
					, data2.xyz() );
				auto lightDiffuse = writer.declLocale( "lightDiffuse"
					, vec3( 0.0_f ) );
				auto lightSpecular = writer.declLocale( "lightSpecular"
					, vec3( 0.0_f ) );
				auto eye = writer.declLocale( "eye"
					, c3d_cameraPosition.xyz() );
				auto depth = writer.declLocale( "depth"
					, c3d_mapDepth.lod( texCoord, 0.0_f ).x() );
				auto vsPosition = writer.declLocale( "vsPosition"
					, utils.calcVSPosition( texCoord, depth, c3d_mtxInvProj ) );
				auto wsPosition = writer.declLocale( "wsPosition"
					, utils.calcWSPosition( texCoord, depth, c3d_mtxInvViewProj ) );
				auto wsNormal = writer.declLocale( "wsNormal"
					, data1.xyz() );
				auto translucency = writer.declLocale( "translucency"
					, data4.w() );
				auto material = writer.declLocale( "material"
					, materials.getMaterial( materialId )
					, C3D_DisableSSSTransmittance == 0 );

				shader::OutputComponents output{ lightDiffuse, lightSpecular };

				switch ( lightType )
				{
				case LightType::eDirectional:
				{
					auto c3d_light = writer.getVariable< shader::DirectionalLight >( "c3d_light" );
					auto light = writer.declLocale( "light", c3d_light );
					lighting->compute( light
						, eye
						, specular
						, glossiness
						, shadowReceiver
						, shader::FragmentInput( in.fragCoord.xy(), vsPosition, wsPosition, wsNormal )
						, output );
#if !C3D_DisableSSSTransmittance
					lightDiffuse += sss.compute( material
						, light
						, texCoord
						, wsPosition
						, wsNormal
						, translucency );
#endif
					break;
				}

				case LightType::ePoint:
				{
					auto c3d_light = writer.getVariable< shader::PointLight >( "c3d_light" );
					auto light = writer.declLocale( "light", c3d_light );
					lighting->compute( light
						, eye
						, specular
						, glossiness
						, shadowReceiver
						, shader::FragmentInput( in.fragCoord.xy(), vsPosition, wsPosition, wsNormal )
						, output );
#if !C3D_DisableSSSTransmittance
					lightDiffuse += sss.compute( material
						, light
						, texCoord
						, wsPosition
						, wsNormal
						, translucency );
#endif
					break;
				}

				case LightType::eSpot:
				{
					auto c3d_light = writer.getVariable< shader::SpotLight >( "c3d_light" );
					auto light = writer.declLocale( "light", c3d_light );
					lighting->compute( light
						, eye
						, specular
						, glossiness
						, shadowReceiver
						, shader::FragmentInput( in.fragCoord.xy(), vsPosition, wsPosition, wsNormal )
						, output );
#if !C3D_DisableSSSTransmittance
					lightDiffuse += sss.compute( material
						, light
						, texCoord
						, wsPosition
						, wsNormal
						, translucency );
#endif
					break;
				}

				default:
					break;
				}

				pxl_diffuse = lightDiffuse;
				pxl_specular = lightSpecular;
				pxl_indirect = vec3( 1.0_f, 1.0_f, 1.0_f );
			} );

		return std::make_unique< ast::Shader >( std::move( writer.getShader() ) );
	}

	//************************************************************************************************
}
