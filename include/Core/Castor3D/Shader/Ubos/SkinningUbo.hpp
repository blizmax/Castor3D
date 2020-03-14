/*
See LICENSE file in root folder
*/
#ifndef ___C3D_SkinningUbo_H___
#define ___C3D_SkinningUbo_H___

#include "UbosModule.hpp"
#include "Castor3D/Scene/Animation/AnimationModule.hpp"

#include "Castor3D/Buffer/UniformBuffer.hpp"

#include <CastorUtils/Math/SquareMatrix.hpp>

namespace castor3d
{
	struct SkinningData
	{
		std::unique_ptr< sdw::ArraySsboT< sdw::Mat4 > > ssbo;
		std::unique_ptr< sdw::Ubo > ubo;
		std::unique_ptr< sdw::SampledImageBufferRgba32 > tbo;
	};

	class SkinningUbo
	{
	public:
		struct Configuration
		{
			castor::Matrix4x4f bonesMatrix[400];
		};

	public:
		/**
		 *\~english
		 *\name			Copy/Move construction/assignment operation.
		 *\~french
		 *\name			Constructeurs/Opérateurs d'affectation par copie/déplacement.
		 */
		/**@{*/
		C3D_API SkinningUbo( SkinningUbo const & ) = delete;
		C3D_API SkinningUbo & operator=( SkinningUbo const & ) = delete;
		C3D_API SkinningUbo( SkinningUbo && ) = default;
		C3D_API SkinningUbo & operator=( SkinningUbo && ) = delete;
		/**@}*/
		/**
		 *\~english
		 *\brief		Constructor.
		 *\param[in]	engine	The engine.
		 *\~french
		 *\brief		Constructeur.
		 *\param[in]	engine	Le moteur.
		 */
		C3D_API explicit SkinningUbo( Engine & engine );
		/**
		 *\~english
		 *\brief		Destructor
		 *\~french
		 *\brief		Destructeur
		 */
		C3D_API ~SkinningUbo();
		/**
		 *\~english
		 *\brief		Initialises the UBO.
		 *\~french
		 *\brief		Initialise l'UBO.
		 */
		C3D_API void initialise();
		/**
		 *\~english
		 *\brief		Cleanup function.
		 *\~french
		 *\brief		Fonction de nettoyage.
		 */
		C3D_API void cleanup();
		/**
		 *\~english
		 *\brief		Updates the UBO from given values.
		 *\param[in]	skeleton	The skeleton to update from.
		 *\~french
		 *\brief		Met à jour l'UBO avec les valeurs données.
		 *\param[in]	skeleton	Le squelette à partir duquel la mise à jour s'effectue.
		 */
		C3D_API void update( AnimatedSkeleton const & skeleton )const;
		/**
		 *\~english
		 *\brief		Declares the variables needed to compute skinning in vertex shader.
		 *\param[in]	writer	The shader writer.
		 *\param[in]	binding	The descriptor set binding point.
		 *\param[in]	set		The descriptor set index.
		 *\param[in]	flags	The program flags.
		 *\return		The skinning data.
		 *\~french
		 *\brief		Déclare les variables nécessaires au calcul du skinning dans le vertex shader.
		 *\param[in]	writer	Le shader writer.
		 *\param[in]	binding	Le point d'attache du descriptor set.
		 *\param[in]	set		L'index du descriptor set.
		 *\param[in]	flags	Les indicateurs du programme.
		 *\return		Les données de skinning.
		 */
		C3D_API static SkinningData declare( sdw::ShaderWriter & writer
			, uint32_t binding
			, uint32_t set
			, ProgramFlags const & flags );
		/**
		 *\~english
		 *\brief		Creates the descriptor set layout binding for the skinning UBO/SSBO.
		 *\param[in]	binding	The descriptor binding index.
		 *\param[in]	flags	The program flags.
		 *\~french
		 *\brief		Crée l'attache de layout de descripteur pour l'UBO/SSBO de skinning.
		 *\param[in]	binding	L'index d'attache du descripteur.
		 *\param[in]	flags	Les indicateurs du programme.
		 */
		C3D_API static VkDescriptorSetLayoutBinding createLayoutBinding( uint32_t binding
			, ProgramFlags const & flags );
		/**
		 *\~english
		 *\brief		Computes skinning transformation in vertex shader.
		 *\param[in]	data	The skinning data.
		 *\param[in]	writer	The shader writer.
		 *\param[in]	flags	The program flags.
		 *\return		The resulting matrix.
		 *\~french
		 *\brief		Effectue le calcul de la transformation du skinning dans le vertex shader.
		 *\param[in]	data	Les données de skinning.
		 *\param[in]	writer	Le shader writer.
		 *\param[in]	flags	Les indicateurs du programme.
		 *\return		La matrice résultat.
		 */
		C3D_API static sdw::Mat4 computeTransform( SkinningData & data
			, sdw::ShaderWriter & writer
			, ProgramFlags const & flags );
		/**
		 *\~english
		 *\name			getters.
		 *\~french
		 *\name			getters.
		 */
		inline UniformBuffer< Configuration > & getUbo()
		{
			return *m_ubo;
		}

		inline UniformBuffer< Configuration > const & getUbo()const
		{
			return *m_ubo;
		}
		/**@}*/

	public:
		C3D_API static uint32_t const BindingPoint;
		//!\~english	Name of the skinning animation frame variable buffer.
		//!\~french		Nom du frame variable buffer contenant les données d'animation de skinning.
		C3D_API static castor::String const BufferSkinning;
		//!\~english	Name of the bones matrices attribute.
		//!\~french		Nom de l'attribut de de matrices d'os.
		C3D_API static castor::String const Bones;

	private:
		Engine & m_engine;
		UniformBufferUPtr< Configuration > m_ubo;
	};
}

#endif
