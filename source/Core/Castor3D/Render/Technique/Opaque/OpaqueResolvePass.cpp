#include "Castor3D/Render/Technique/Opaque/OpaqueResolvePass.hpp"

#include "Castor3D/Engine.hpp"
#include "Castor3D/Buffer/GpuBuffer.hpp"
#include "Castor3D/Buffer/PoolUniformBuffer.hpp"
#include "Castor3D/Cache/MaterialCache.hpp"
#include "Castor3D/Material/Texture/Sampler.hpp"
#include "Castor3D/Material/Texture/TextureView.hpp"
#include "Castor3D/Material/Texture/TextureLayout.hpp"
#include "Castor3D/Miscellaneous/PipelineVisitor.hpp"
#include "Castor3D/Render/RenderPassTimer.hpp"
#include "Castor3D/Render/RenderPipeline.hpp"
#include "Castor3D/Render/RenderSystem.hpp"
#include "Castor3D/Scene/Camera.hpp"
#include "Castor3D/Scene/Scene.hpp"
#include "Castor3D/Scene/Background/Background.hpp"
#include "Castor3D/Shader/Program.hpp"
#include "Castor3D/Shader/PassBuffer/PassBuffer.hpp"
#include "Castor3D/Shader/Shaders/GlslFog.hpp"
#include "Castor3D/Shader/Shaders/GlslMaterial.hpp"
#include "Castor3D/Shader/Shaders/GlslMetallicPbrReflection.hpp"
#include "Castor3D/Shader/Shaders/GlslPhongReflection.hpp"
#include "Castor3D/Shader/Shaders/GlslSpecularPbrReflection.hpp"
#include "Castor3D/Shader/Shaders/GlslSssTransmittance.hpp"
#include "Castor3D/Shader/Shaders/GlslUtils.hpp"
#include "Castor3D/Shader/Ubos/GpInfoUbo.hpp"
#include "Castor3D/Shader/Ubos/MatrixUbo.hpp"
#include "Castor3D/Shader/Ubos/SceneUbo.hpp"
#include "Castor3D/Render/Technique/Opaque/OpaquePassResult.hpp"
#include "Castor3D/Render/Ssao/SsaoConfig.hpp"
#include "Castor3D/Render/Technique/Opaque/SsaoPass.hpp"

#include <ashespp/Command/CommandBuffer.hpp>
#include <ashespp/Image/Image.hpp>
#include <ashespp/Image/ImageView.hpp>
#include <ashespp/RenderPass/RenderPassCreateInfo.hpp>

#include <ShaderWriter/Source.hpp>

#include <CastorUtils/Graphics/Image.hpp>

using namespace castor;

namespace castor3d
{
	namespace
	{
		static uint32_t constexpr c_noIblEnvironmentCount = 6u;
		static uint32_t constexpr c_iblTexturesCount = 3u;
		static uint32_t constexpr c_iblEnvironmentCount = c_noIblEnvironmentCount - c_iblTexturesCount;

		ashes::PipelineVertexInputStateCreateInfoPtr doCreateVertexLayout()
		{
			return std::make_unique< ashes::PipelineVertexInputStateCreateInfo >( 0u
				, ashes::VkVertexInputBindingDescriptionArray
				{
					{ 0u, 16u, VK_VERTEX_INPUT_RATE_VERTEX },
				}
				, ashes::VkVertexInputAttributeDescriptionArray
				{
					{ 0u, 0u, VK_FORMAT_R32G32_SFLOAT, 0u },
					{ 1u, 0u, VK_FORMAT_R32G32_SFLOAT, 8u },
				} );
		}

		ashes::VertexBufferBasePtr doCreateVbo( RenderDevice const & device )
		{
			float data[]
			{
				-1, -1, 0, 0,
				+1, +1, 1, 1,
				-1, +1, 0, 1,
				-1, -1, 0, 0,
				+1, -1, 1, 0,
				+1, +1, 1, 1
			};

			auto vertexBuffer = makeVertexBufferBase( device
				, uint32_t( sizeof( data ) )
				, 0u
				, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
				, "DeferredResolve" );

			if ( auto * buffer = vertexBuffer->getBuffer().lock( 0u
				, ~( 0ull )
				, 0u ) )
			{
				std::memcpy( buffer, data, sizeof( data ) );
				vertexBuffer->getBuffer().flush( 0u, ~( 0ull ) );
				vertexBuffer->getBuffer().unlock();
			}

			return vertexBuffer;
		}

		ShaderPtr doCreateVertexProgram( RenderDevice const & device )
		{
			using namespace sdw;
			VertexWriter writer;

			// Shader inputs
			auto position = writer.declInput< Vec2 >( "position", 0 );
			auto uv = writer.declInput< Vec2 >( "uv", 1 );

			// Shader outputs
			auto vtx_texture = writer.declOutput< Vec2 >( "vtx_texture", 0u );
			auto out = writer.getOut();

			writer.implementFunction< sdw::Void >( "main"
				, [&]()
			{
				vtx_texture = uv;
				out.vtx.position = vec4( position.x(), position.y(), 0.0_f, 1.0_f );
			} );
			return std::make_unique< ast::Shader >( std::move( writer.getShader() ) );
		}

		ShaderPtr doCreateLegacyPixelProgram( RenderDevice const & device
			, FogType fogType
			, bool hasSsao
			, bool hasGi )
		{
			auto & renderSystem = device.renderSystem;
			using namespace sdw;
			FragmentWriter writer;

			// Shader inputs
			UBO_SCENE( writer, SceneUbo::BindingPoint, 0u );
			UBO_GPINFO( writer, GpInfoUbo::BindingPoint, 0u );
			UBO_HDR_CONFIG( writer, HdrConfigUbo::BindingPoint, 0u );
			auto index = 1u;
			auto c3d_mapDepth = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eDepth ), index++, 1u );
			auto c3d_mapData1 = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eData1 ), index++, 1u );
			auto c3d_mapData2 = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eData2 ), index++, 1u );
			auto c3d_mapData3 = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eData3 ), index++, 1u );
			auto c3d_mapData4 = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eData4 ), index++, 1u );
			auto c3d_mapData5 = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eData5 ), index++, 1u );
			auto c3d_mapSsao = writer.declSampledImage< FImg2DRg32 >( "c3d_mapSsao", hasSsao ? index++ : 0u, 1u, hasSsao );
			auto c3d_mapLightDiffuse = writer.declSampledImage< FImg2DRgba32 >( "c3d_mapLightDiffuse", index++, 1u );
			auto c3d_mapLightSpecular = writer.declSampledImage< FImg2DRgba32 >( "c3d_mapLightSpecular", index++, 1u );
			auto c3d_mapLightIndirect = writer.declSampledImage< FImg2DRgba32 >( "c3d_mapLightIndirect", hasGi ? index++ : 0u, 1u, hasGi );
			auto c3d_mapEnvironment = writer.declSampledImageArray< FImgCubeRgba32 >( "c3d_mapEnvironment", index++, 1u, c_noIblEnvironmentCount );
			CU_Require( index < device.properties.limits.maxDescriptorSetSampledImages );

			auto vtx_texture = writer.declInput< Vec2 >( "vtx_texture", 0u );

			shader::LegacyMaterials materials{ writer };
			materials.declare( renderSystem.getGpuInformations().hasShaderStorageBuffers() );
			
			shader::Utils utils{ writer };
			utils.declareRemoveGamma();
			utils.declareCalcWSPosition();
			utils.declareCalcVSPosition();
			utils.declareDecodeMaterial();
			utils.declareInvertVec2Y();
			shader::PhongReflectionModel reflections{ writer, utils };
			shader::Fog fog{ fogType, writer };

			// Shader outputs
			auto pxl_fragColor = writer.declOutput< Vec4 >( "pxl_fragColor", 0u );

			writer.implementFunction< sdw::Void >( "main"
				, [&]()
				{
					auto data5 = writer.declLocale( "data5"
						, c3d_mapData5.lod( vtx_texture, 0.0_f ) );
					auto materialId = writer.declLocale( "materialId"
						, writer.cast< UInt >( data5.z() ) );

					IF( writer, materialId == 0_u )
					{
						writer.discard();
					}
					FI;

					auto data1 = writer.declLocale( "data1"
						, c3d_mapData1.lod( vtx_texture, 0.0_f ) );
					auto data2 = writer.declLocale( "data2"
						, c3d_mapData2.lod( vtx_texture, 0.0_f ) );
					auto data3 = writer.declLocale( "data3"
						, c3d_mapData3.lod( vtx_texture, 0.0_f ) );
					auto data4 = writer.declLocale( "data4"
						, c3d_mapData4.lod( vtx_texture, 0.0_f ) );
					auto flags = writer.declLocale( "flags"
						, data1.w() );
					auto envMapIndex = writer.declLocale( "envMapIndex"
						, 0_i );
					auto receiver = writer.declLocale( "receiver"
						, 0_i );
					auto reflection = writer.declLocale( "reflection"
						, 0_i );
					auto refraction = writer.declLocale( "refraction"
						, 0_i );

					utils.decodeMaterial( flags
						, receiver
						, reflection
						, refraction
						, envMapIndex );
					auto material = writer.declLocale( "material"
						, materials.getMaterial( materialId ) );
					auto depth = writer.declLocale( "depth"
						, c3d_mapDepth.lod( vtx_texture, 0.0_f ).x() );
					auto position = writer.declLocale( "position"
						, utils.calcWSPosition( vtx_texture, depth, c3d_mtxInvViewProj ) );
					auto normal = writer.declLocale( "normal"
						, normalize( data1.xyz() ) );
					auto diffuse = writer.declLocale( "diffuse"
						, data2.xyz() );
					auto shininess = writer.declLocale( "shininess"
						, data2.w() );
					auto specular = writer.declLocale( "specular"
						, data3.xyz() );
					auto occlusion = writer.declLocale( "occlusion"
						, data3.a() );
					auto emissive = writer.declLocale( "emissive"
						, data4.xyz() );
					auto ambient = writer.declLocale( "ambient"
						, clamp( c3d_ambientLight.xyz() * material.m_ambient * diffuse
							, vec3( 0.0_f )
							, vec3( 1.0_f ) ) );
					auto lightDiffuse = writer.declLocale( "lightDiffuse"
						, c3d_mapLightDiffuse.lod( vtx_texture, 0.0_f ).xyz() );
					auto lightSpecular = writer.declLocale( "lightSpecular"
						, c3d_mapLightSpecular.lod( vtx_texture, 0.0_f ).xyz() );
					auto lightIndirect = writer.declLocale( "lightIndirect"
						, c3d_mapLightIndirect.lod( vtx_texture, 0.0_f ).rgb()
						, hasGi );
					lightDiffuse += lightIndirect;
					lightSpecular *= specular;

					if ( hasSsao )
					{
						occlusion *= c3d_mapSsao.lod( vtx_texture, 0.0_f ).r();
					}

					IF( writer, envMapIndex > 0_i && ( reflection != 0_i || refraction != 0_i ) )
					{
						auto incident = writer.declLocale( "incident"
							, reflections.computeIncident( position, c3d_cameraPosition.xyz() ) );
						envMapIndex = envMapIndex - 1_i;

						IF( writer, envMapIndex >= Int( c_noIblEnvironmentCount ) )
						{
							envMapIndex = 0_i;
						}
						FI;

						IF( writer, reflection != 0_i && refraction != 0_i )
						{
							for ( auto i = 0; i < c_noIblEnvironmentCount; ++i )
							{
								IF( writer, envMapIndex == Int( i ) )
								{
									reflections.computeReflRefr( incident
										, normal
										, occlusion
										, c3d_mapEnvironment[Int( i )]
										, material.m_refractionRatio
										, shininess
										, ambient
										, diffuse );
								}
								FI;
							}
						}
						ELSEIF( reflection != 0_i )
						{
							for ( auto i = 0; i < c_noIblEnvironmentCount; ++i )
							{
								IF( writer, envMapIndex == Int( i ) )
								{
									reflections.computeRefl( incident
										, normal
										, occlusion
										, c3d_mapEnvironment[Int( i )]
										, shininess
										, ambient
										, diffuse );
								}
								FI;
							}
						}
						ELSE
						{
							for ( auto i = 0; i < c_noIblEnvironmentCount; ++i )
							{
								IF( writer, envMapIndex == Int( i ) )
								{
									reflections.computeRefr( incident
										, normal
										, occlusion
										, c3d_mapEnvironment[Int( i )]
										, material.m_refractionRatio
										, shininess
										, ambient
										, diffuse );
								}
								FI;
							}
						}
						FI;
					}
					ELSE
					{
						ambient *= occlusion;
						diffuse *= lightDiffuse;
					}
					FI;

					ambient *= lightIndirect;
					pxl_fragColor = vec4( diffuse + lightSpecular + emissive + ambient, 1.0_f );

					if ( fogType != FogType::eDisabled )
					{
						position = utils.calcVSPosition( vtx_texture
							, c3d_mapDepth.lod( vtx_texture, 0.0_f ).x()
							, c3d_mtxInvProj );
						pxl_fragColor = fog.apply( vec4( utils.removeGamma( c3d_gamma, c3d_backgroundColour.rgb() ), c3d_backgroundColour.a() )
							, pxl_fragColor
							, length( position )
							, position.z()
							, c3d_fogInfo
							, c3d_cameraPosition );
					}
				} );
			return std::make_unique< ast::Shader >( std::move( writer.getShader() ) );
		}

		ShaderPtr doCreatePbrMRPixelProgram( RenderDevice const & device
			, FogType fogType
			, bool hasSsao
			, bool hasGi )
		{
			auto & renderSystem = device.renderSystem;
			using namespace sdw;
			FragmentWriter writer;

			// Shader inputs
			UBO_SCENE( writer, SceneUbo::BindingPoint, 0u );
			UBO_GPINFO( writer, GpInfoUbo::BindingPoint, 0u );
			UBO_HDR_CONFIG( writer, HdrConfigUbo::BindingPoint, 0u );
			auto index = 1u;
			auto c3d_mapDepth = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eDepth ), index++, 1u );
			auto c3d_mapData1 = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eData1 ), index++, 1u );
			auto c3d_mapData2 = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eData2 ), index++, 1u );
			auto c3d_mapData3 = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eData3 ), index++, 1u );
			auto c3d_mapData4 = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eData4 ), index++, 1u );
			auto c3d_mapData5 = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eData5 ), index++, 1u );
			auto c3d_mapSsao = writer.declSampledImage< FImg2DRg32 >( "c3d_mapSsao", hasSsao ? index++ : 0u, 1u, hasSsao );
			auto c3d_mapLightDiffuse = writer.declSampledImage< FImg2DRgba32 >( "c3d_mapLightDiffuse", index++, 1u );
			auto c3d_mapLightSpecular = writer.declSampledImage< FImg2DRgba32 >( "c3d_mapLightSpecular", index++, 1u );
			auto c3d_mapLightIndirect = writer.declSampledImage< FImg2DRgba32 >( "c3d_mapLightIndirect", hasGi ? index++ : 0u, 1u, hasGi );
			auto c3d_mapBrdf = writer.declSampledImage< FImg2DRgba32 >( "c3d_mapBrdf", index++, 1u );
			auto c3d_mapIrradiance = writer.declSampledImage< FImgCubeRgba32 >( "c3d_mapIrradiance", index++, 1u );
			auto c3d_mapPrefiltered = writer.declSampledImage< FImgCubeRgba32 >( "c3d_mapPrefiltered", index++, 1u );
			auto c3d_mapEnvironment = writer.declSampledImageArray< FImgCubeRgba32 >( "c3d_mapEnvironment", index++, 1u, c_iblEnvironmentCount );
			CU_Require( index < device.properties.limits.maxDescriptorSetSampledImages );

			auto vtx_texture = writer.declInput< Vec2 >( "vtx_texture", 0u );

			shader::PbrMRMaterials materials{ writer };
			materials.declare( renderSystem.getGpuInformations().hasShaderStorageBuffers() );

			shader::Utils utils{ writer };
			utils.declareRemoveGamma();
			utils.declareCalcWSPosition();
			utils.declareCalcVSPosition();
			utils.declareFresnelSchlick();
			utils.declareComputeIBL();
			utils.declareDecodeMaterial();
			utils.declareInvertVec2Y();
			shader::MetallicPbrReflectionModel reflections{ writer, utils };
			shader::Fog fog{ fogType, writer };

			// Shader outputs
			auto pxl_fragColor = writer.declOutput< Vec4 >( "pxl_fragColor", 0u );

			writer.implementFunction< sdw::Void >( "main"
				, [&]()
				{
					auto fixedTexCoord = writer.declLocale( "fixedTexCoord"
						, vtx_texture );
					auto data5 = writer.declLocale( "data5"
						, c3d_mapData5.lod( fixedTexCoord, 0.0_f ) );
					auto materialId = writer.declLocale( "materialId"
						, writer.cast< UInt >( data5.z() ) );

					IF( writer, materialId == 0_u )
					{
						writer.discard();
					}
					FI;

					auto data1 = writer.declLocale( "data1"
						, c3d_mapData1.lod( fixedTexCoord, 0.0_f ) );
					auto flags = writer.declLocale( "flags"
						, data1.w() );
					auto envMapIndex = writer.declLocale( "envMapIndex"
						, 0_i );
					auto receiver = writer.declLocale( "receiver"
						, 0_i );
					auto reflection = writer.declLocale( "reflection"
						, 0_i );
					auto refraction = writer.declLocale( "refraction"
						, 0_i );
					utils.decodeMaterial( flags
						, receiver
						, reflection
						, refraction
						, envMapIndex );

					auto material = writer.declLocale( "material"
						, materials.getMaterial( materialId ) );
					auto data2 = writer.declLocale( "data2"
						, c3d_mapData2.lod( fixedTexCoord, 0.0_f ) );
					auto data3 = writer.declLocale( "data3"
						, c3d_mapData3.lod( fixedTexCoord, 0.0_f ) );
					auto data4 = writer.declLocale( "data4"
						, c3d_mapData4.lod( fixedTexCoord, 0.0_f ) );
					auto normal = writer.declLocale( "normal"
						, data1.rgb() );
					auto albedo = writer.declLocale( "albedo"
						, data2.rgb() );
					auto roughness = writer.declLocale( "roughness"
						, data2.a() );
					auto metalness = writer.declLocale( "metalness"
						, data3.r() );
					auto occlusion = writer.declLocale( "occlusion"
						, data3.a() );
					auto emissive = writer.declLocale( "emissive"
						, data4.rgb() );
					auto depth = writer.declLocale( "depth"
						, c3d_mapDepth.lod( vtx_texture, 0.0_f ).x() );
					auto position = writer.declLocale( "position"
						, utils.calcWSPosition( vtx_texture, depth, c3d_mtxInvViewProj ) );
					auto ambient = writer.declLocale( "ambient"
						, c3d_ambientLight.xyz() );
					auto lightDiffuse = writer.declLocale( "lightDiffuse"
						, c3d_mapLightDiffuse.lod( fixedTexCoord, 0.0_f ).rgb() );
					auto lightSpecular = writer.declLocale( "lightSpecular"
						, c3d_mapLightSpecular.lod( fixedTexCoord, 0.0_f ).rgb() );
					auto lightIndirect = writer.declLocale( "lightIndirect"
						, c3d_mapLightIndirect.lod( vtx_texture, 0.0_f ).rgb()
						, hasGi );
					lightDiffuse += lightIndirect;

					if ( hasSsao )
					{
						occlusion *= c3d_mapSsao.lod( fixedTexCoord, 0.0_f ).r();
					}

					IF( writer, envMapIndex > 0_i )
					{
						envMapIndex = envMapIndex - 1_i;

						IF( writer, envMapIndex >= Int( c_iblEnvironmentCount ) )
						{
							envMapIndex = 0_i;
						}
						FI;

						auto incident = writer.declLocale( "incident"
							, normalize( position - c3d_cameraPosition.xyz() ) );
						auto ratio = writer.declLocale( "ratio"
							, material.m_refractionRatio );

						IF( writer, reflection != 0_i )
						{
							for ( auto i = 0; i < c_iblEnvironmentCount; ++i )
							{
								IF( writer, envMapIndex == Int( i ) )
								{
									// Reflection from environment map.
									ambient = reflections.computeRefl( incident
										, normal
										, occlusion
										, c3d_mapEnvironment[Int( i )]
										, c3d_ambientLight.xyz()
										, albedo );
								}
								FI;
							}
						}
						ELSE
						{
							// Reflection from background skybox.
							ambient = c3d_ambientLight.rgb()
								* occlusion
								* utils.computeMetallicIBL( normal
									, position
									, albedo
									, metalness
									, roughness
									, c3d_cameraPosition.xyz()
									, c3d_mapIrradiance
									, c3d_mapPrefiltered
									, c3d_mapBrdf );
						}
						FI;

						IF( writer, refraction != 0_i )
						{
							for ( auto i = 0; i < c_iblEnvironmentCount; ++i )
							{
								IF( writer, envMapIndex == Int( i ) )
								{
									// Refraction from environment map.
									ambient = reflections.computeRefrEnvMap( incident
										, normal
										, occlusion
										, c3d_mapEnvironment[Int( i )]
										, material.m_refractionRatio
										, ambient
										, albedo
										, roughness );
								}
								FI;
							}
						}
						ELSEIF( ratio != 0.0_f )
						{
							// Refraction from background skybox.
							ambient = reflections.computeRefrSkybox( incident
								, normal
								, occlusion
								, c3d_mapPrefiltered
								, material.m_refractionRatio
								, ambient
								, albedo
								, roughness );
						}
						FI;
					}
					ELSE
					{
						// Reflection from background skybox.
						ambient = c3d_ambientLight.rgb()
							* occlusion
							* utils.computeMetallicIBL( normal
								, position
								, albedo
								, metalness
								, roughness
								, c3d_cameraPosition.xyz()
								, c3d_mapIrradiance
								, c3d_mapPrefiltered
								, c3d_mapBrdf );
						auto ratio = writer.declLocale( "ratio"
							, material.m_refractionRatio );

						IF( writer, ratio != 0.0_f )
						{
							// Refraction from background skybox.
							auto incident = writer.declLocale( "incident"
								, normalize( position - c3d_cameraPosition.xyz() ) );
							ambient = reflections.computeRefrSkybox( incident
								, normal
								, occlusion
								, c3d_mapPrefiltered
								, material.m_refractionRatio
								, ambient
								, albedo
								, roughness );
						}
						FI;
					}
					FI;

					ambient *= lightIndirect;
					pxl_fragColor = vec4( lightDiffuse * albedo + lightSpecular + emissive + ambient, 1.0_f );

					if ( fogType != FogType::eDisabled )
					{
						position = utils.calcVSPosition( fixedTexCoord
							, c3d_mapDepth.lod( fixedTexCoord, 0.0_f ).x()
							, c3d_mtxInvProj );
						pxl_fragColor = fog.apply( vec4( utils.removeGamma( c3d_gamma, c3d_backgroundColour.rgb() ), c3d_backgroundColour.a() )
							, pxl_fragColor
							, length( position )
							, position.z()
							, c3d_fogInfo
							, c3d_cameraPosition );
					}
				} );
			return std::make_unique< ast::Shader >( std::move( writer.getShader() ) );
		}

		ShaderPtr doCreatePbrSGPixelProgram( RenderDevice const & device
			, FogType fogType
			, bool hasSsao
			, bool hasGi )
		{
			auto & renderSystem = device.renderSystem;
			using namespace sdw;
			FragmentWriter writer;

			// Shader inputs
			UBO_SCENE( writer, SceneUbo::BindingPoint, 0u );
			UBO_GPINFO( writer, GpInfoUbo::BindingPoint, 0u );
			UBO_HDR_CONFIG( writer, HdrConfigUbo::BindingPoint, 0u );
			auto index = 1u;
			auto c3d_mapDepth = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eDepth ), index++, 1u );
			auto c3d_mapData1 = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eData1 ), index++, 1u );
			auto c3d_mapData2 = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eData2 ), index++, 1u );
			auto c3d_mapData3 = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eData3 ), index++, 1u );
			auto c3d_mapData4 = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eData4 ), index++, 1u );
			auto c3d_mapData5 = writer.declSampledImage< FImg2DRgba32 >( getTextureName( DsTexture::eData5 ), index++, 1u );
			auto c3d_mapSsao = writer.declSampledImage< FImg2DRg32 >( "c3d_mapSsao", hasSsao ? index++ : 0u, 1u, hasSsao );
			auto c3d_mapLightDiffuse = writer.declSampledImage< FImg2DRgba32 >( "c3d_mapLightDiffuse", index++, 1u );
			auto c3d_mapLightSpecular = writer.declSampledImage< FImg2DRgba32 >( "c3d_mapLightSpecular", index++, 1u );
			auto c3d_mapLightIndirect = writer.declSampledImage< FImg2DRgba32 >( "c3d_mapLightIndirect", hasGi ? index++ : 0u, 1u, hasGi );
			auto c3d_mapBrdf = writer.declSampledImage< FImg2DRgba32 >( "c3d_mapBrdf", index++, 1u );
			auto c3d_mapIrradiance = writer.declSampledImage< FImgCubeRgba32 >( "c3d_mapIrradiance", index++, 1u );
			auto c3d_mapPrefiltered = writer.declSampledImage< FImgCubeRgba32 >( "c3d_mapPrefiltered", index++, 1u );
			auto c3d_mapEnvironment = writer.declSampledImageArray< FImgCubeRgba32 >( "c3d_mapEnvironment", index++, 1u, c_iblEnvironmentCount );
			CU_Require( index < device.properties.limits.maxDescriptorSetSampledImages );

			auto vtx_texture = writer.declInput< Vec2 >( "vtx_texture", 0u );

			shader::PbrSGMaterials materials{ writer };
			materials.declare( renderSystem.getGpuInformations().hasShaderStorageBuffers() );

			shader::Utils utils{ writer };
			utils.declareRemoveGamma();
			utils.declareCalcWSPosition();
			utils.declareCalcVSPosition();
			utils.declareFresnelSchlick();
			utils.declareComputeIBL();
			utils.declareDecodeMaterial();
			utils.declareInvertVec2Y();
			shader::SpecularPbrReflectionModel reflections{ writer, utils };
			shader::Fog fog{ fogType, writer };

			// Shader outputs
			auto pxl_fragColor = writer.declOutput< Vec4 >( "pxl_fragColor", 0u );

			writer.implementFunction< sdw::Void >( "main"
				, [&]()
				{
					auto fixedTexCoord = writer.declLocale( "fixedTexCoord"
						, vtx_texture );
					auto data5 = writer.declLocale( "data5"
						, c3d_mapData5.lod( fixedTexCoord, 0.0_f ) );
					auto materialId = writer.declLocale( "materialId"
						, writer.cast< UInt >( data5.z() ) );

					IF( writer, materialId == 0_u )
					{
						writer.discard();
					}
					FI;

					auto data1 = writer.declLocale( "data1"
						, c3d_mapData1.lod( fixedTexCoord, 0.0_f ) );
					auto flags = writer.declLocale( "flags"
						, data1.w() );
					auto envMapIndex = writer.declLocale( "envMapIndex"
						, 0_i );
					auto receiver = writer.declLocale( "receiver"
						, 0_i );
					auto reflection = writer.declLocale( "reflection"
						, 0_i );
					auto refraction = writer.declLocale( "refraction"
						, 0_i );
					utils.decodeMaterial( flags
						, receiver
						, reflection
						, refraction
						, envMapIndex );

					auto material = writer.declLocale( "material"
						, materials.getMaterial( materialId ) );
					auto data2 = writer.declLocale( "data2"
						, c3d_mapData2.lod( fixedTexCoord, 0.0_f ) );
					auto data3 = writer.declLocale( "data3"
						, c3d_mapData3.lod( fixedTexCoord, 0.0_f ) );
					auto data4 = writer.declLocale( "data4"
						, c3d_mapData4.lod( fixedTexCoord, 0.0_f ) );
					auto normal = writer.declLocale( "normal"
						, data1.xyz() );
					auto diffuse = writer.declLocale( "diffuse"
						, data2.xyz() );
					auto glossiness = writer.declLocale( "glossiness"
						, data2.w() );
					auto specular = writer.declLocale( "specular"
						, data3.xyz() );
					auto occlusion = writer.declLocale( "occlusion"
						, data3.a() );
					auto emissive = writer.declLocale( "emissive"
						, data4.xyz() );
					auto depth = writer.declLocale( "depth"
						, c3d_mapDepth.lod( vtx_texture, 0.0_f ).x() );
					auto position = writer.declLocale( "position"
						, utils.calcWSPosition( vtx_texture, depth, c3d_mtxInvViewProj ) );
					auto ambient = writer.declLocale( "ambient"
						, c3d_ambientLight.xyz() );
					auto lightDiffuse = writer.declLocale( "lightDiffuse"
						, c3d_mapLightDiffuse.lod( fixedTexCoord, 0.0_f ).xyz() );
					auto lightSpecular = writer.declLocale( "lightSpecular"
						, c3d_mapLightSpecular.lod( fixedTexCoord, 0.0_f ).xyz() );
					auto lightIndirect = writer.declLocale( "lightIndirect"
						, c3d_mapLightIndirect.lod( fixedTexCoord, 0.0_f ).rgb()
						, hasGi );
					lightDiffuse += lightIndirect;

					if ( hasSsao )
					{
						occlusion *= c3d_mapSsao.lod( fixedTexCoord, 0.0_f ).r();
					}

					IF( writer, envMapIndex > 0_i )
					{
						envMapIndex = envMapIndex - 1_i;

						IF( writer, envMapIndex >= Int( c_iblEnvironmentCount ) )
						{
							envMapIndex = 0_i;
						}
						FI;

						auto incident = writer.declLocale( "incident"
							, normalize( position - c3d_cameraPosition.xyz() ) );
						auto ratio = writer.declLocale( "ratio"
							, material.m_refractionRatio );

						IF( writer, reflection != 0_i )
						{
							for ( auto i = 0; i < c_iblEnvironmentCount; ++i )
							{
								IF( writer, envMapIndex == Int( i ) )
								{
									// Reflection from environment map.
									ambient = reflections.computeRefl( incident
										, normal
										, occlusion
										, c3d_mapEnvironment[Int( i )]
										, c3d_ambientLight.xyz()
										, diffuse );
								}
								FI;
							}
						}
						ELSE
						{
							// Reflection from background skybox.
							ambient = c3d_ambientLight.xyz()
								* occlusion
								* utils.computeSpecularIBL( normal
									, position
									, diffuse
									, specular
									, glossiness
									, c3d_cameraPosition.xyz()
									, c3d_mapIrradiance
									, c3d_mapPrefiltered
									, c3d_mapBrdf );
						}
						FI;

						IF( writer, refraction != 0_i )
						{
							for ( auto i = 0; i < c_iblEnvironmentCount; ++i )
							{
								IF( writer, envMapIndex == Int( i ) )
								{
									// Refraction from environment map.
									ambient = reflections.computeRefrEnvMap( incident
										, normal
										, occlusion
										, c3d_mapEnvironment[Int( i )]
										, material.m_refractionRatio
										, ambient
										, diffuse
										, glossiness );
								}
								FI;
							}
						}
						ELSEIF( ratio != 0.0_f )
						{
							// Refraction from background skybox.
							ambient = reflections.computeRefrSkybox( incident
								, normal
								, occlusion
								, c3d_mapPrefiltered
								, material.m_refractionRatio
								, ambient
								, diffuse
								, glossiness );
						}
						FI;
					}
					ELSE
					{
						// Reflection from background skybox.
						ambient = c3d_ambientLight.xyz()
							* occlusion
							* utils.computeSpecularIBL( normal
								, position
								, diffuse
								, specular
								, glossiness
								, c3d_cameraPosition.xyz()
								, c3d_mapIrradiance
								, c3d_mapPrefiltered
								, c3d_mapBrdf );
						auto ratio = writer.declLocale( "ratio"
							, material.m_refractionRatio );

						IF( writer, ratio != 0.0_f )
						{
							// Refraction from background skybox.
							auto incident = writer.declLocale( "incident"
								, normalize( position - c3d_cameraPosition.xyz() ) );
							ambient = reflections.computeRefrSkybox( incident
								, normal
								, occlusion
								, c3d_mapPrefiltered
								, material.m_refractionRatio
								, ambient
								, diffuse
								, glossiness );
						}
						FI;
					}
					FI;

					ambient *= lightIndirect;
					pxl_fragColor = vec4( lightDiffuse * diffuse + lightSpecular + emissive + ambient, 1.0_f );

					if ( fogType != FogType::eDisabled )
					{
						position = utils.calcVSPosition( fixedTexCoord
							, c3d_mapDepth.lod( fixedTexCoord, 0.0_f ).x()
							, c3d_mtxInvProj );
						pxl_fragColor = fog.apply( vec4( utils.removeGamma( c3d_gamma, c3d_backgroundColour.rgb() ), c3d_backgroundColour.a() )
							, pxl_fragColor
							, length( position )
							, position.z()
							, c3d_fogInfo
							, c3d_cameraPosition );
					}
				} );
			return std::make_unique< ast::Shader >( std::move( writer.getShader() ) );
		}

		ashes::PipelineShaderStageCreateInfoArray doCreateProgram( RenderDevice const & device
			, FogType fogType
			, bool hasSsao
			, bool hasGi
			, MaterialType matType
			, ShaderModule & vertexShader
			, ShaderModule & pixelShader )
		{
			vertexShader.shader = doCreateVertexProgram( device );
			pixelShader.shader = ( matType == MaterialType::ePhong
				? doCreateLegacyPixelProgram( device, fogType, hasSsao, hasGi )
				: ( matType == MaterialType::eMetallicRoughness
					? doCreatePbrMRPixelProgram( device, fogType, hasSsao, hasGi )
					: doCreatePbrSGPixelProgram( device, fogType, hasSsao, hasGi ) ) );

			return ashes::PipelineShaderStageCreateInfoArray
			{
				makeShaderState( device, vertexShader ),
				makeShaderState( device, pixelShader ),
			};
		}

		inline ashes::DescriptorSetLayoutPtr doCreateUboDescriptorLayout( Engine & engine
			, RenderDevice const & device )
		{
			auto & passBuffer = engine.getMaterialCache().getPassBuffer();
			ashes::VkDescriptorSetLayoutBindingArray bindings
			{
				passBuffer.createLayoutBinding(),
				makeDescriptorSetLayoutBinding( SceneUbo::BindingPoint
					, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
					, VK_SHADER_STAGE_FRAGMENT_BIT ),
				makeDescriptorSetLayoutBinding( GpInfoUbo::BindingPoint
					, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
					, VK_SHADER_STAGE_FRAGMENT_BIT ),
				makeDescriptorSetLayoutBinding( HdrConfigUbo::BindingPoint
					, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
					, VK_SHADER_STAGE_FRAGMENT_BIT ),
			};
			auto result = device->createDescriptorSetLayout( "DeferredResolveUbo"
				, std::move( bindings ) );
			return result;
		}

		inline ashes::DescriptorSetPtr doCreateUboDescriptorSet( Engine & engine
			, ashes::DescriptorSetPool & pool
			, SceneUbo const & sceneUbo
			, GpInfoUbo const & gpInfoUbo
			, HdrConfigUbo const & hdrConfigUbo )
		{
			auto & passBuffer = engine.getMaterialCache().getPassBuffer();
			auto & layout = pool.getLayout();
			auto result = pool.createDescriptorSet( "DeferredResolveUbo", 0u );
			passBuffer.createBinding( *result, layout.getBinding( getPassBufferIndex() ) );
			sceneUbo.createSizedBinding( *result
				, layout.getBinding( SceneUbo::BindingPoint ) );
			gpInfoUbo.createSizedBinding( *result
				, layout.getBinding( GpInfoUbo::BindingPoint ) );
			hdrConfigUbo.createSizedBinding( *result
				, layout.getBinding( HdrConfigUbo::BindingPoint ) );
			result->update();
			return result;
		}

		inline ashes::RenderPassPtr doCreateRenderPass( RenderDevice const & device
			, VkFormat format )
		{
			ashes::VkAttachmentDescriptionArray attaches
			{
				{
					0u,
					format,
					VK_SAMPLE_COUNT_1_BIT,
					VK_ATTACHMENT_LOAD_OP_LOAD,
					VK_ATTACHMENT_STORE_OP_STORE,
					VK_ATTACHMENT_LOAD_OP_DONT_CARE,
					VK_ATTACHMENT_STORE_OP_DONT_CARE,
					VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				}
			};
			ashes::SubpassDescriptionArray subpasses;
			subpasses.emplace_back( ashes::SubpassDescription
				{
					0u,
					VK_PIPELINE_BIND_POINT_GRAPHICS,
					{},
					{ { 0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL } },
					{},
					ashes::nullopt,
					{},
				} );
			ashes::VkSubpassDependencyArray dependencies
			{
				{
					VK_SUBPASS_EXTERNAL,
					0u,
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
					VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					VK_DEPENDENCY_BY_REGION_BIT,
				},
				{
					0u,
					VK_SUBPASS_EXTERNAL,
					VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
					VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
					VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
					VK_ACCESS_SHADER_READ_BIT,
					VK_DEPENDENCY_BY_REGION_BIT,
				},
			};
			ashes::RenderPassCreateInfo createInfo
			{
				0u,
				std::move( attaches ),
				std::move( subpasses ),
				std::move( dependencies ),
			};
			auto result = device->createRenderPass( "DeferredResolve"
				, std::move( createInfo ) );
			return result;
		}

		inline ashes::FrameBufferPtr doCreateFrameBuffer( ashes::RenderPass const & renderPass
			, VkExtent2D const size
			, ashes::ImageView const & view )
		{
			ashes::ImageViewCRefArray attaches
			{
				view
			};
			auto result = renderPass.createFrameBuffer( "DeferredResolve"
				, size
				, std::move( attaches ) );
			return result;
		}

		inline ashes::DescriptorSetLayoutPtr doCreateTexDescriptorLayout( RenderDevice const & device
			, bool hasSsao
			, bool hasGi
			, MaterialType matType )
		{
			uint32_t index = 1u;
			ashes::VkDescriptorSetLayoutBindingArray bindings
			{
				makeDescriptorSetLayoutBinding( index++
					, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
					, VK_SHADER_STAGE_FRAGMENT_BIT ),	// DsTexture::eDepth
				makeDescriptorSetLayoutBinding( index++
					, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
					, VK_SHADER_STAGE_FRAGMENT_BIT ),	// DsTexture::eData1
				makeDescriptorSetLayoutBinding( index++
					, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
					, VK_SHADER_STAGE_FRAGMENT_BIT ),	// DsTexture::eData2
				makeDescriptorSetLayoutBinding( index++
					, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
					, VK_SHADER_STAGE_FRAGMENT_BIT ),	// DsTexture::eData3
				makeDescriptorSetLayoutBinding( index++
					, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
					, VK_SHADER_STAGE_FRAGMENT_BIT ),	// DsTexture::eData4
				makeDescriptorSetLayoutBinding( index++
					, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
					, VK_SHADER_STAGE_FRAGMENT_BIT ),	// DsTexture::eData5
			};

			if ( hasSsao )
			{
				bindings.emplace_back( makeDescriptorSetLayoutBinding( index++
					, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
					, VK_SHADER_STAGE_FRAGMENT_BIT ) );	// c3d_mapSsao
			}

			bindings.emplace_back( makeDescriptorSetLayoutBinding( index++
				, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
				, VK_SHADER_STAGE_FRAGMENT_BIT ) );	// c3d_mapLightDiffuse
			bindings.emplace_back( makeDescriptorSetLayoutBinding( index++
				, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
				, VK_SHADER_STAGE_FRAGMENT_BIT ) );	// c3d_mapLightSpecular

			if ( hasGi )
			{
				bindings.emplace_back( makeDescriptorSetLayoutBinding( index++
					, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
					, VK_SHADER_STAGE_FRAGMENT_BIT ) );	// c3d_mapLightIndirect
			}

			auto envMapCount = c_noIblEnvironmentCount;

			if ( matType != MaterialType::ePhong )
			{
				bindings.emplace_back( makeDescriptorSetLayoutBinding( index++
					, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
					, VK_SHADER_STAGE_FRAGMENT_BIT ) );	// c3d_mapBrdf
				bindings.emplace_back( makeDescriptorSetLayoutBinding( index++
					, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
					, VK_SHADER_STAGE_FRAGMENT_BIT ) );	// c3d_mapIrradiance
				bindings.emplace_back( makeDescriptorSetLayoutBinding( index++
					, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
					, VK_SHADER_STAGE_FRAGMENT_BIT ) );	// c3d_mapPrefiltered
				envMapCount -= c_iblTexturesCount;
			}

			bindings.emplace_back( makeDescriptorSetLayoutBinding( index++
				, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
				, VK_SHADER_STAGE_FRAGMENT_BIT
				, envMapCount ) );	// c3d_mapEnvironment

			auto result = device->createDescriptorSetLayout( "DeferredResolveTex"
				, std::move( bindings ) );
			return result;
		}

		inline ashes::DescriptorSetPtr doCreateTexDescriptorSet( RenderDevice const & device
			, ashes::DescriptorSetPool & pool
			, ashes::DescriptorSetLayout const & layout
			, OpaquePassResult const & gp
			, ashes::ImageView const * ssao
			, ashes::ImageView const * sssss
			, ashes::ImageView const & lightDiffuse
			, ashes::ImageView const & lightSpecular
			, ashes::ImageView const * lightIndirect
			, SamplerSPtr sampler
			, Scene const & scene
			, MaterialType matType
			, OpaqueResolvePass::EnvMapArray const & envMaps )
		{
			sampler->initialise( device );
			auto descriptorSet = pool.createDescriptorSet( "DeferredResolveTex", 1u );
			uint32_t index = 1u;
			auto imgLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			ashes::WriteDescriptorSetArray writes;

			for ( auto unit : gp )
			{
				writes.push_back( {
					index++,
					0u,
					VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					{
						{
							unit->getSampler()->getSampler(),
							unit->getTexture()->getDefaultView().getSampledView(),
							imgLayout
						}
					} } );
			}

			if ( ssao )
			{
				writes.push_back( { index++, 0u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, { { sampler->getSampler(), *ssao, imgLayout } } } );
			}

			if ( sssss )
			{
				writes.push_back( { index++, 0u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, { { sampler->getSampler(), *sssss, imgLayout } } } );
			}
			else
			{
				writes.push_back( { index++, 0u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, { { sampler->getSampler(), lightDiffuse, imgLayout } } } );
			}

			writes.push_back( { index++, 0u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, { { sampler->getSampler(), lightSpecular, imgLayout } } } );

			if ( lightIndirect )
			{
				writes.push_back( { index++, 0u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, { { sampler->getSampler(), *lightIndirect, imgLayout } } } );
			}

			auto & background = *scene.getBackground();
			auto envMapCount = c_noIblEnvironmentCount;

			if ( matType != MaterialType::ePhong
				&& background.hasIbl() )
			{
				auto & ibl = background.getIbl();
				writes.push_back( { index++, 0u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, { { ibl.getPrefilteredBrdfSampler(), ibl.getPrefilteredBrdfTexture(), imgLayout } } } );
				writes.push_back( { index++, 0u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, { { ibl.getIrradianceSampler(), ibl.getIrradianceTexture(), imgLayout } } } );
				writes.push_back( { index++, 0u, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, { { ibl.getPrefilteredEnvironmentSampler(), ibl.getPrefilteredEnvironmentTexture(), imgLayout } } } );
				envMapCount -= c_iblTexturesCount;
			}

			writes.push_back( { index++, 0u, envMapCount, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER } );
			CU_Require( index < device.properties.limits.maxDescriptorSetSampledImages );

			writes.back().imageInfo.reserve( envMapCount );

			auto & envWrites = writes.back();
			auto it = envMaps.begin();
			uint32_t i = 0u;

			while ( it != envMaps.end() && i < envMapCount )
			{
				envWrites.imageInfo.push_back( { it->get().getTexture().getSampler()->getSampler()
					, it->get().getTexture().getTexture()->getDefaultView().getSampledView()
					, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL } );
				++i;
				++it;
			}

			while ( i < envMapCount )
			{
				envWrites.imageInfo.push_back( { sampler->getSampler()
					, background.getView()
					, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL } );
				++i;
			}

			descriptorSet->setBindings( writes );
			descriptorSet->update();
			return descriptorSet;
		}

		ashes::GraphicsPipelinePtr doCreateRenderPipeline( RenderDevice const & device
			, ashes::PipelineLayout const & pipelineLayout
			, ashes::PipelineShaderStageCreateInfoArray program
			, ashes::RenderPass const & renderPass
			, VkExtent2D const & size )
		{

			auto result = device->createPipeline( "DeferredResolve"
				, ashes::GraphicsPipelineCreateInfo
				{
					0u,
					std::move( program ),
					*doCreateVertexLayout(),
					ashes::PipelineInputAssemblyStateCreateInfo{},
					ashes::nullopt,
					ashes::PipelineViewportStateCreateInfo
					{
						0u,
						{ 1u, VkViewport{ 0.0f, 0.0f, float( size.width ), float( size.height ), 0.0f, 1.0f } },
						{ 1u, VkRect2D{ 0, 0, size.width, size.height } }
					},
					ashes::PipelineRasterizationStateCreateInfo{ 0u, VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE },
					ashes::PipelineMultisampleStateCreateInfo{},
					ashes::PipelineDepthStencilStateCreateInfo{ 0u, VK_FALSE, VK_FALSE },
					ashes::PipelineColorBlendStateCreateInfo{},
					ashes::nullopt,
					pipelineLayout,
					renderPass,
				} );
			return result;
		}

		template< typename T >
		struct Reverse
		{
			explicit Reverse( T const & collection )
				: m_collection{ collection }
			{
			}

			auto begin()const
			{
				return m_collection.rbegin();
			}

			auto end()const
			{
				return m_collection.rend();
			}

		private:
			T const & m_collection;
		};

		template< typename T >
		Reverse< T > makeReverse( T const & collection )
		{
			return Reverse< T >( collection );
		}
	}

	//*********************************************************************************************

	OpaqueResolvePass::ProgramPipeline::ProgramPipeline( Engine & engine
		, RenderDevice const & device
		, OpaquePassResult const & gp
		, ashes::DescriptorSetLayout const & uboLayout
		, ashes::RenderPass const & renderPass
		, ashes::ImageView const * ssao
		, ashes::ImageView const * subsurfaceScattering
		, ashes::ImageView const & lightDiffuse
		, ashes::ImageView const & lightSpecular
		, ashes::ImageView const * lightIndirect
		, SamplerSPtr const & sampler
		, VkExtent2D const & size
		, FogType fogType
		, MaterialType matType
		, Scene const & scene )
		: engine{ engine }
		, opaquePassResult{ gp }
		, renderPass{ &renderPass }
		, vertexShader{ VK_SHADER_STAGE_VERTEX_BIT, "DeferredResolve" }
		, pixelShader{ VK_SHADER_STAGE_FRAGMENT_BIT, "DeferredResolve" }
		, program{ doCreateProgram( device, fogType, ssao != nullptr, lightIndirect != nullptr, matType, vertexShader, pixelShader ) }
		, texDescriptorLayout{ doCreateTexDescriptorLayout( device, ssao != nullptr, lightIndirect != nullptr, engine.getMaterialsType() ) }
		, texDescriptorPool{ texDescriptorLayout->createPool( "OpaqueResolvePassTex", 1u ) }
		, texDescriptorSet{ doCreateTexDescriptorSet( device
			, *texDescriptorPool
			, *texDescriptorLayout
			, gp
			, ssao
			, subsurfaceScattering
			, lightDiffuse
			, lightSpecular
			, lightIndirect
			, sampler
			, scene
			, matType
			, scene.getEnvironmentMaps() ) }
		, pipelineLayout{ device->createPipelineLayout( "DeferredResolve", { uboLayout, *texDescriptorLayout } ) }
		, pipeline{ doCreateRenderPipeline( device, *pipelineLayout, program, renderPass, size ) }
		, commandBuffer{ device.graphicsCommandPool->createCommandBuffer( "DeferredResolve", VK_COMMAND_BUFFER_LEVEL_PRIMARY ) }
	{
	}

	void OpaqueResolvePass::ProgramPipeline::updateCommandBuffer( ashes::VertexBufferBase & vbo
		, ashes::DescriptorSet const & uboSet
		, ashes::FrameBuffer const & frameBuffer
		, RenderPassTimer & timer )
	{
		commandBuffer->begin();
		commandBuffer->beginDebugBlock(
			{
				"Deferred - Resolve",
				makeFloatArray( engine.getNextRainbowColour() ),
			} );
		timer.beginPass( *commandBuffer );
		commandBuffer->memoryBarrier( VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
			, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
			, opaquePassResult[DsTexture::eDepth].getTexture()->getDefaultView().getTargetView().makeShaderInputResource( VK_IMAGE_LAYOUT_UNDEFINED ) );
		commandBuffer->beginRenderPass( *renderPass
			, frameBuffer
			, { transparentBlackClearColor }
			, VK_SUBPASS_CONTENTS_INLINE );
		commandBuffer->bindPipeline( *pipeline );
		commandBuffer->bindDescriptorSets( { uboSet, *texDescriptorSet }, *pipelineLayout );
		commandBuffer->bindVertexBuffer( 0u, vbo.getBuffer(), 0u );
		commandBuffer->draw( 6u );
		commandBuffer->endRenderPass();
		timer.endPass( *commandBuffer );
		commandBuffer->endDebugBlock();
		commandBuffer->end();
	}

	void OpaqueResolvePass::ProgramPipeline::accept( PipelineVisitorBase & visitor )
	{
		visitor.visit( vertexShader );
		visitor.visit( pixelShader );
	}

	//*********************************************************************************************

	OpaqueResolvePass::OpaqueResolvePass( Engine & engine
		, RenderDevice const & device
		, Scene & scene
		, OpaquePassResult const & gp
		, SsaoPass const & ssao
		, TextureUnit const & subsurfaceScattering
		, TextureUnit const & lightDiffuse
		, TextureUnit const & lightSpecular
		, TextureUnit const & lightIndirect
		, TextureUnit const & result
		, SceneUbo const & sceneUbo
		, GpInfoUbo const & gpInfoUbo
		, HdrConfigUbo const & hdrConfigUbo )
		: OwnedBy< Engine >{ engine }
		, m_device{ device }
		, m_scene{ scene }
		, m_result{ result }
		, m_sceneUbo{ sceneUbo }
		, m_gpInfoUbo{ gpInfoUbo }
		, m_hdrConfigUbo{ hdrConfigUbo }
		, m_ssao{ ssao }
		, m_sampler{ engine.getDefaultSampler() }
		, m_opaquePassResult{ gp }
		, m_subsurfaceScattering{ subsurfaceScattering }
		, m_lightDiffuse{ lightDiffuse }
		, m_lightSpecular{ lightSpecular }
		, m_lightIndirect{ lightIndirect }
		, m_viewport{ engine }
		, m_programs{ nullptr }
	{
	}

	void OpaqueResolvePass::initialise()
	{
		auto & engine = *getEngine();
		auto & result = *m_result.getTexture();
		VkExtent2D size{ result.getWidth(), result.getHeight() };
		m_vertexBuffer = doCreateVbo( m_device );
		m_uboDescriptorLayout = doCreateUboDescriptorLayout( engine, m_device );
		m_uboDescriptorPool = m_uboDescriptorLayout->createPool( "OpaqueResolvePassUbo", 1u );
		m_uboDescriptorSet = doCreateUboDescriptorSet( engine, *m_uboDescriptorPool, m_sceneUbo, m_gpInfoUbo, m_hdrConfigUbo );
		m_renderPass = doCreateRenderPass( m_device, result.getPixelFormat() );
		m_frameBuffer = doCreateFrameBuffer( *m_renderPass, size, result.getDefaultView().getSampledView() );
		m_finished = m_device->createSemaphore( "OpaqueResolvePass" );
		m_timer = std::make_shared< RenderPassTimer >( engine, m_device, cuT( "Opaque" ), cuT( "Resolve pass" ) );
		m_viewport.setOrtho( 0, 1, 0, 1, 0, 1 );
		m_viewport.resize( { m_size.width, m_size.height } );
		m_viewport.update();
	}

	void OpaqueResolvePass::cleanup()
	{
		m_programs = {};
		m_timer.reset();
		m_finished.reset();
		m_frameBuffer.reset();
		m_renderPass.reset();
		m_uboDescriptorSet.reset();
		m_uboDescriptorPool.reset();
		m_uboDescriptorLayout.reset();
		m_vertexBuffer.reset();
	}

	void OpaqueResolvePass::update( GpuUpdater & updater )
	{
		auto & program = getProgram();
		program.updateCommandBuffer( *m_vertexBuffer
			, *m_uboDescriptorSet
			, *m_frameBuffer
			, *m_timer );
		m_currentProgram = &program;
	}

	ashes::Semaphore const & OpaqueResolvePass::render( ashes::Semaphore const & toWait )const
	{
		auto * result = &toWait;
		RenderPassTimerBlock timerBlock{ m_timer->start() };
		timerBlock->notifyPassRender();

		m_device.graphicsQueue->submit( *m_currentProgram->commandBuffer
			, *result
			, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
			, *m_finished
			, nullptr );
		result = m_finished.get();

		return *result;
	}

	void OpaqueResolvePass::accept( PipelineVisitorBase & visitor )
	{
		auto & program = getProgram();
		program.accept( visitor );
	}

	OpaqueResolvePass::ProgramPipeline & OpaqueResolvePass::getProgram()
	{
		auto fog = m_scene.getFog().getType();
		bool hasSsao = m_ssao.getConfig().enabled;
		bool hasSssss = m_scene.needsSubsurfaceScattering();
		bool hasGi = m_scene.needsGlobalIllumination();
		auto index = ( size_t( fog ) * SsaoCount * SsssCount * GiCount )
			+ ( ( hasSsao ? 1u : 0 ) * SsssCount * GiCount )
			+ ( ( hasSssss ? 1u : 0u ) * GiCount )
			+ ( hasGi ? 1u : 0u );
		auto & result = m_programs[index];

		if ( !result )
		{
			auto & engine = *getEngine();
			auto & tex = *m_result.getTexture();
			VkExtent2D size{ tex.getWidth(), tex.getHeight() };
			result = std::make_unique< ProgramPipeline >( engine
				, m_device
				, m_opaquePassResult
				, *m_uboDescriptorLayout
				, *m_renderPass
				, ( hasSsao
					? &m_ssao.getResult().getTexture()->getDefaultView().getSampledView()
					: nullptr )
				, ( hasSssss
					? &m_subsurfaceScattering.getTexture()->getDefaultView().getSampledView()
					: nullptr )
				, m_lightDiffuse.getTexture()->getDefaultView().getSampledView()
				, m_lightSpecular.getTexture()->getDefaultView().getSampledView()
				, ( hasGi
					? &m_lightIndirect.getTexture()->getDefaultView().getSampledView()
					: nullptr )
				, m_sampler
				, size
				, fog
				, engine.getMaterialsType()
				, m_scene );
		}

		return *result;
	}
}
