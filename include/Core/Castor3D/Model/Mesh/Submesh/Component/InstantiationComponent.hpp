/*
See LICENSE file in root folder
*/
#ifndef ___C3D_InstantiationComponent_H___
#define ___C3D_InstantiationComponent_H___

#include "Castor3D/Model/Mesh/Submesh/Component/SubmeshComponent.hpp"

#include <CastorUtils/Math/SquareMatrix.hpp>

#include <ashespp/Buffer/VertexBuffer.hpp>

namespace castor3d
{
	struct InstantiationData
	{
		castor::Matrix4x4f m_matrix;
		int m_material;
	};

	class InstantiationComponent
		: public SubmeshComponent
	{
	public:
		struct Data
		{
			Data( uint32_t count
				, ashes::VertexBufferPtr< InstantiationData > buffer
				, std::vector< InstantiationData > data = {} )
				: count{ count }
				, buffer{ std::move( buffer ) }
				, data{ std::move( data ) }
			{
			}
			uint32_t count;
			ashes::VertexBufferPtr< InstantiationData > buffer;
			std::vector< InstantiationData > data;
		};
		using DataArray = std::vector< Data >;
		using InstanceDataMap = std::map< MaterialSPtr, DataArray >;

	public:
		/**
		 *\~english
		 *\brief		Constructor.
		 *\param[in]	submesh		The parent submesh.
		 *\param[in]	threshold	The threshold from which a submesh will be instantiated.
		 *\~french
		 *\brief		Constructeur.
		 *\param[in]	submesh		Le sous-maillage parent.
		 *\param[in]	threshold	Le seuil à partir duquel un sous-maillage sera instancié.
		 */
		C3D_API explicit InstantiationComponent( Submesh & submesh
			, uint32_t threshold = 1u );
		/**
		 *\~english
		 *\brief		Increments instance count.
		 *\param[in]	material	The material for which the instance count is incremented.
		 *\return		The previous instance count.
		 *\~french
		 *\brief		Incrémente le compte d'instances.
		 *\param[in]	material	Le matériau pour lequel le compte est incrémenté.
		 *\return		Le compte précédent.
		 */
		C3D_API uint32_t ref( MaterialSPtr material );
		/**
		 *\~english
		 *\brief		Decrements instance count.
		 *\param[in]	material	The material for which the instance count is decremented.
		 *\return		The previous instance count.
		 *\~french
		 *\brief		Décrémente le compte d'instances.
		 *\param[in]	material	Le matériau pour lequel le compte est décrémenté.
		 *\return		Le compte précédent.
		 */
		C3D_API uint32_t unref( MaterialSPtr material );
		/**
		 *\~english
		 *\brief		Retrieves the instances count
		 *\param[in]	material	The material for which the instance count is retrieved
		 *\return		The value
		 *\~french
		 *\brief		Récupère le nombre d'instances
		 *\param[in]	material	Le matériau pour lequel le compte est récupéré
		 *\return		La valeur
		 */
		C3D_API uint32_t getRefCount( MaterialSPtr material )const;
		/**
		 *\~english
		 *\param[in]	material	The material.
		 *\return		\p true if the instance count for given material is greater than the threshold.
		 *\~french
		 *\param[in]	material	Le matériau.
		 *\return		\p true si le nombre d'instances pour le matériau donné est plus grand que le seuil.
		 */
		inline bool isInstanced( MaterialSPtr material )const
		{
			return getRefCount( material ) >= getThreshold();
		}
		/**
		 *\~english
		 *\return		The maximum instances count, amongst all materials.
		 *\~french
		 *\return		Le nombre moximum d'instances, tous matériaux confondus,
		 */
		C3D_API uint32_t getMaxRefCount()const;
		/**
		 *\copydoc		castor3d::SubmeshComponent::gather
		 */
		C3D_API void gather( MaterialSPtr material
			, ashes::BufferCRefArray & buffers
			, std::vector< uint64_t > & offsets
			, ashes::PipelineVertexInputStateCreateInfoCRefArray & layouts
			, uint32_t instanceMult )override;
		/**
		 *\copydoc		castor3d::SubmeshComponent::setMaterial
		 */
		C3D_API void setMaterial( MaterialSPtr oldMaterial
			, MaterialSPtr newMaterial
			, bool update )override;
		/**
		*\~english
		*name
		*	Getters.
		*\~french
		*name
		*	Accesseurs.
		*/
		/**@{*/
		C3D_API InstanceDataMap::const_iterator find( MaterialSPtr material
			, uint32_t instanceMult )const;
		C3D_API InstanceDataMap::iterator find( MaterialSPtr material
			, uint32_t instanceMult );
		C3D_API ProgramFlags getProgramFlags( MaterialSPtr material )const override;

		inline uint32_t getThreshold()const
		{
			return m_threshold;
		}

		inline InstanceDataMap::const_iterator end()const
		{
			return m_instances.end();
		}

		inline InstanceDataMap::iterator end()
		{
			return m_instances.end();
		}

		inline InstanceDataMap::const_iterator find( MaterialSPtr material )const
		{
			return m_instances.find( material );
		}

		inline InstanceDataMap::iterator find( MaterialSPtr material )
		{
			needsUpdate();
			return m_instances.find( material );
		}

		uint32_t getIndex( uint32_t instanceMult )const
		{
			return ( instanceMult <= 1u
				? 0u
				: instanceMult - 1u );
		}
		/**@}*/

	private:
		bool doInitialise( RenderDevice const & device )override;
		void doCleanup()override;
		void doFill( RenderDevice const & device )override;
		void doUpload()override;
		inline bool doCheckInstanced( uint32_t count )const
		{
			return count >= getThreshold();
		}

	public:
		C3D_API static castor::String const Name;
		C3D_API static uint32_t constexpr BindingPoint = 2u;

	private:
		InstanceDataMap m_instances;
		ashes::PipelineVertexInputStateCreateInfoPtr m_matrixLayout;
		uint32_t m_threshold;
	};
}

#endif
