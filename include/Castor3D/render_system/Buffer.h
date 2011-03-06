/*
This source file is part of Castor3D (http://dragonjoker.co.cc

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with
the program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place - Suite 330, Boston, MA 02111-1307, USA, or go to
http://www.gnu.org/copyleft/lesser.txt.
*/
#ifndef ___C3D_Buffer___
#define ___C3D_Buffer___

#include "../geometry/basic/Vertex.h"
#include "BufferElement.h"

namespace Castor3D
{
	//! Buffer lock modes enumerator
	typedef enum
	{
		eLockReadOnly	= 1 << 0,
		eLockWriteOnly	= 1 << 1,
	}
	eLOCK_FLAG;
	//! Buffer creation modes enumerator
	typedef enum
	{
        eBufferStatic  = 1 << 0, //< Static buffer
        eBufferDynamic = 1 << 1  //< Dynamic buffer
	}
	eBUFFER_MODE;
	//! 3D Buffer management class
	/*!
	Class which holds the buffers used in 3D (texture, VBO ...)
	Not to be used to manage usual buffers of data, use Castor::Resource::Buffer for that
	\author Sylvain DOREMUS
	\version 0.1
	\date 09/02/2010
	*/
	template <typename T>
	class C3D_API Buffer3D : public Castor::Resources::Buffer<T>
	{
	protected:
		bool m_bToDelete;
		bool m_bAssigned;

	public:
		Buffer3D()
			:	m_bAssigned( false)
			,	m_bToDelete( false)
		{}
		virtual ~Buffer3D(){}
		/**
		* Initialisation function, used by VBOs
		*/
		virtual void Initialise( eBUFFER_MODE p_eMode){}
		/**
		 * Locks the buffer, id est maps it into memory so we can modify it
		 */
		virtual void * Lock( size_t p_uiOffset, size_t p_uiSize, size_t p_uiFlags) = 0;
		/**
		 * Unlocks the buffer, id est unmaps it from memory so no modification can be made after that
		 */
		virtual void Unlock() = 0;
		/**
		* Activation function, to activate the buffer and show it's content in 3D
		*/
		virtual void Activate() = 0;
		/**
		* De-activation function, to deactivate the buffer and hide it's content in 3D
		*/
		virtual void Deactivate() = 0;

	public:
		/**@name Accessors */
		//@{
		virtual bool IsToDelete	()const	{ return m_bToDelete; }
		virtual bool IsAssigned	()const	{ return m_bAssigned; }
		//@}
	};

	//! Vertex buffer representation
	/*!
	Holds the vertex coordinates of a submesh
	\author Sylvain DOREMUS
	\version 0.1
	\date 09/02/2010
	*/
	class C3D_API IndexBuffer : public Buffer3D<unsigned int>
	{
	public:
		/**
		 * Activation function, to activate the buffer and show the vertex in it
		 */
		virtual void Activate();
		/**
		 * De-activation function, to deactivate the buffer and hide it's content
		 */
		virtual void Deactivate();
		/**
		 * Locks the buffer, id est maps it into memory so we can modify it
		 */
		virtual void * Lock( size_t p_uiOffset, size_t p_uiSize, size_t p_uiFlags) = 0;
		/**
		 * Unlocks the buffer, id est unmaps it from memory so no modification can be made after that
		 */
		virtual void Unlock() = 0;
	};

	//! Vertex buffer representation
	/*!
	Holds the vertex coordinates of a submesh
	\author Sylvain DOREMUS
	\version 0.1
	\date 09/02/2010
	*/
	class C3D_API VertexBuffer : public Buffer3D<real>
	{
	protected:
		BufferDeclaration m_bufferDeclaration;

	public:
		VertexBuffer( const BufferElementDeclaration * p_pElements, size_t p_uiNbElements);
		virtual ~VertexBuffer();
		/**
		 * Activation function, to activate the buffer and show the vertex in it
		 */
		virtual void Activate();
		/**
		 * De-activation function, to deactivate the buffer and hide it's content
		 */
		virtual void Deactivate();
		/**
		 * Locks the buffer, id est maps it into memory so we can modify it
		 */
		virtual void * Lock( size_t p_uiOffset, size_t p_uiSize, size_t p_uiFlags) = 0;
		/**
		 * Unlocks the buffer, id est unmaps it from memory so no modification can be made after that
		 */
		virtual void Unlock() = 0;
		/**
		 * Adds an element to the buffer
		 */
		void AddElement( Vertex & p_pVertex, bool p_bIncrease);
	};

	//! Attribs buffer representation
	/*!
	Holds vertex attributes such as normals, texture coordinates, ...
	\author Sylvain DOREMUS
	\version 0.1
	\date 09/02/2010
	*/
	template <typename T>
	class C3D_API VertexAttribsBuffer : public Buffer3D<T>
	{
	public:
		VertexAttribsBuffer( const String & p_strName)
		{
		}
		virtual ~VertexAttribsBuffer()
		{
		}
		/**
		 * Activation function, to activate the buffer and show the vertex in it
		 */
		virtual void Activate(){}
		/**
		 * De-activation function, to deactivate the buffer and hide it's content
		 */
		virtual void Deactivate(){}
		/**
		 * Locks the buffer, id est maps it into memory so we can modify it
		 */
		virtual void * Lock( size_t p_uiOffset, size_t p_uiSize, size_t p_uiFlags) { throw false; }
		/**
		 * Unlocks the buffer, id est unmaps it from memory so no modification can be made after that
		 */
		virtual void Unlock(){}
		/**
		 * Sets this buffer shader
		 */
		virtual void SetShaderProgram( ShaderProgramPtr p_pProgram){}
	};

	//! Texture buffer representation, holds texture data
	/*!
	Holds the texture coordinates of a submesh
	\author Sylvain DOREMUS
	\version 0.1
	\date 09/02/2010
	*/
	class C3D_API TextureBufferObject : public Buffer3D<unsigned char>
	{
	protected:
		Castor::Resources::ePIXEL_FORMAT  m_pixelFormat;
		size_t m_uiSize;
		const unsigned char * m_pBytes;

	public:
		TextureBufferObject();
		~TextureBufferObject();
		/**
		 * Activation function, void, use next
		 */
		virtual void Activate();
		/**
		 * Activation function, to activate the buffer and apply the textures in it
		 */
		virtual void Activate( PassPtr p_pass);
		/**
		 * De-activation function, to deactivate the buffer and hide it's content
		 */
		virtual void Deactivate();
		/**
		 * Locks the buffer, id est maps it into memory so we can modify it
		 */
		virtual void * Lock( size_t p_uiOffset, size_t p_uiSize, size_t p_uiFlags) = 0;
		/**
		 * Unlocks the buffer, id est unmaps it from memory so no modification can be made after that
		 */
		virtual void Unlock() = 0;
		/**
		* Initialisation function
		*/
		virtual void Initialise( const Castor::Resources::ePIXEL_FORMAT & p_format, size_t p_uiSize, const unsigned char * p_pBytes);
		/**
		* Cleanup function
		*/
		virtual void Cleanup();
	};

	template <typename T> struct BufferFunctor;
	//! 3D Buffer manager
	/*!
	Holds the buffers used by all the meshes and other things.
	Manages the deletion of the buffers during the Update() function, in order to make it during a render loop (needed for OpenGL's VBO)
	\author Sylvain DOREMUS
	\version 0.1
	\date 09/02/2010
	*/
	class C3D_API BufferManager : public Castor::Theory::AutoSingleton <BufferManager>
	{
		friend class Castor::Theory::AutoSingleton <BufferManager>;

	private:
		template <typename T>
		class TBuffers
		{
			friend class BufferManager;

		private:
			typedef shared_ptr< Buffer3D<T> >	Buffer3DPtr;
			typedef typename KeyedContainer<	size_t,			Buffer3D<T> *	>::Map		Buffer3DPtrUIntMap;
			typedef typename KeyedContainer<	Buffer3D<T> *,	size_t			>::Map		UIntBuffer3DPtrMap;
			typedef typename Container<		Buffer3DPtr							>::Vector	Buffer3DPtrArray;
			typedef bool BufferFunction( Buffer3DPtr p_pBuffer, size_t p_uiID);
			typedef BufferFunction PBufferFunction;

		public:
			Buffer3DPtrArray		m_arrayBuffersToDelete;
			Buffer3DPtrArray		m_arrayBuffers;
			Buffer3DPtrUIntMap		m_mapBuffers;
			UIntBuffer3DPtrMap		m_mapBuffersIndexes;

		public:
			void Cleanup();
			void Update();
			bool AddBuffer( Buffer3DPtr p_pBuffer);
			bool AssignBuffer( Buffer3D<T> * p_pBuffer, size_t p_uiID);
			bool UnassignBuffer( Buffer3D<T> * p_pBuffer, size_t p_uiID);
			bool DeleteBuffer( Buffer3DPtr p_pBuffer);
		};

	public:
		TBuffers<bool> m_stBoolBuffers;
		TBuffers<char> m_stCharBuffers;
		TBuffers<unsigned char> m_stUCharBuffers;
		TBuffers<int> m_stIntBuffers;
		TBuffers<unsigned int> m_stUIntBuffers;
		TBuffers<float> m_stFloatBuffers;
		TBuffers<double> m_stDoubleBuffers;

	private:
		BufferManager(){}
		~BufferManager(){} // cleanup must be made with the update function and by all owners of buffers

	public:
		/**
		 * Cleans up the manager
		 */
		static void Cleanup();
		/**
		 * Updates the manager, id est deletes unused buffers
		 */
		static void Update();
		/**
		 * Creates a buffer and adds it to the manager
		 *@return The newly created buffer
		 */
		template <typename T, typename _Ty>
		static shared_ptr<_Ty> CreateBuffer();
		/**
		 * Creates a buffer and adds it to the manager
		 *@return The newly created buffer
		 */
		template <typename _Ty>
		static shared_ptr<_Ty> CreateVertexBuffer( const BufferElementDeclaration * p_pElements, size_t p_uiCount);
		/**
		 * Creates a buffer to be linked in a shader and adds it to the manager
		 *@param p_strArg : [in] The buffer's name (as it appears in the shader)
		 *@return The newly created buffer
		 */
		template <typename T, typename _Ty>
		static shared_ptr<_Ty> CreateBuffer( const String & p_strArg);
		/**
		 * Assigns the buffer
		 *@param p_uiID : the ID of the assigned buffer
		 *@param p_pBuffer : pointer to the buffer
		 *@return true if successful, false if not
		 */
		template <typename T>
		static bool AssignBuffer( size_t p_uiID, Buffer3D<T> * p_pBuffer);
		/**
		 * Unassigns the buffer
		 *@param p_uiID : the ID of the assigned buffer
		 *@param p_pBuffer : pointer to the buffer
		 *@return true if successful, false if not
		 */
		template <typename T>
		static bool UnassignBuffer( size_t p_uiID, Buffer3D<T> * p_pBuffer);
		/**
		 * Deletes the buffer, id est mark it as unused
		 *@param p_pBuffer : pointer to the buffer
		 *@return true if successful, false if not
		 */
		template <typename T>
		static bool DeleteBuffer( shared_ptr< Buffer3D<T> > p_pBuffer);
	};

#include "Buffer.inl"
}

#endif