/*
See LICENSE file in root folder
*/
#ifndef ___Castor_shared_ptr___
#define ___Castor_shared_ptr___

#include "CastorUtils/Config/Macros.hpp"

#include <memory>

namespace castor
{
	/**
	\author		Sylvain DOREMUS
	\version	0.6.1.0
	\date		08/12/2011
	\~english
	\brief		Dummy destructor
	\remark		Used as a parameter to shared_ptr, to make deallocation dummy (only sets pointer to nullptr)
	\~french
	\brief		Destructeur zombie
	\remark		Utilisé en tant que paramètre à shared_ptr, afin d'avoir une désallocation zombie (ne fait que mettre le pointeur à nullptr, sans le désallouer)
	*/
	struct DummyDtor
	{
		template< typename T >
		inline void operator()( T *& pointer )noexcept
		{
			pointer = 0;
		}
	};
	CU_API extern DummyDtor g_dummyDtor;

	template< typename TypeT >
	struct Deleter
	{
		inline void operator()( TypeT * pointer )noexcept
		{
			delete pointer;
		}
	};

	template< typename TypeT >
	using UniquePtr = std::unique_ptr< TypeT, Deleter< TypeT > >;

	template< typename TypeT, typename ... ParamsT >
	UniquePtr< TypeT > makeUnique( ParamsT && ... params )
	{
		return UniquePtr< TypeT >( new TypeT{ std::forward< ParamsT >( params )... } );
	}
}

#define CU_DeclareCUSmartPtr( class_name )\
	using class_name##SPtr = std::shared_ptr< class_name >;\
	using class_name##WPtr = std::weak_ptr< class_name >;\
	using class_name##UPtr = castor::UniquePtr< class_name >;\
	using class_name##RPtr = class_name *

#define CU_DeclareCUTemplateSmartPtr( class_name )\
	template< typename T > using class_name##SPtr = std::shared_ptr< class_name< T > >;\
	template< typename T > using class_name##WPtr = std::weak_ptr< class_name< T > >;\
	template< typename T > using class_name##UPtr = castor::UniquePtr< class_name< T > >;\
	template< typename T > using class_name##RPtr = class_name< T > *

#endif