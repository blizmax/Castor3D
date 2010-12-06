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
#ifndef ___GL4_LightRenderer___
#define ___GL4_LightRenderer___

#include "Module_GLRender.h"

namespace Castor3D
{
	class C3D_GL3_API GL4LightRenderer : public GLLightRenderer
	{
	protected:
		bool		m_bInitialised;
		Colour		m_varAmbient;
		Colour		m_varDiffuse;
		Colour		m_varSpecular;
		Point4f		m_varPosition;
		Point3f		m_varAttenuation;
		Matrix4x4f	m_varOrientation;
		float		m_varExponent;
		float		m_varCutOff;
		bool		m_bChanged;
		int			m_iGLIndex;
		int			m_iGLPreviousIndex;

		GLUBOPoint4fVariablePtr 	m_pAmbients;
		GLUBOPoint4fVariablePtr 	m_pDiffuses;
		GLUBOPoint4fVariablePtr 	m_pSpeculars;
		GLUBOPoint4fVariablePtr 	m_pPositions;
		GLUBOPoint3fVariablePtr 	m_pAttenuations;
		GLUBOMatrix4x4fVariablePtr 	m_pOrientations;
		GLUBOFloatVariablePtr 		m_pExponents;
		GLUBOFloatVariablePtr 		m_pCutOffs;
		GLUniformBufferObjectPtr	m_pUniformBuffer;

	public:
		GL4LightRenderer();
		virtual ~GL4LightRenderer(){}

		virtual void Initialise();

		inline int GetGLIndex			()const { return m_iGLIndex; }
		inline int GetGLPreviousIndex	()const { return m_iGLPreviousIndex; }
		inline void SetGLIndex			( int p_iIndex) { m_iGLIndex = p_iIndex; }
		inline void SetGLPreviousIndex	( int p_iIndex) { m_iGLPreviousIndex = p_iIndex; }

		Point4f 	GetAmbient		() const { return m_varAmbient; }
		Point4f 	GetDiffuse		() const { return m_varDiffuse; }
		Point4f 	GetSpecular		() const { return m_varSpecular; }
		Point4f 	GetPosition		() const { return m_varPosition; }
		Point3f 	GetAttenuation	() const { return m_varAttenuation; }
		Matrix4x4f	GetOrientation	() const { return m_varOrientation; }
		float 		GetExponent		() const { return m_varExponent; }
		float 		GetCutOff		() const { return m_varCutOff; }

	private:
		void _enable			();
		void _disable			();
		void _applyAmbient		( const Colour & p_ambient);
		void _applyDiffuse		( const Colour & p_diffuse);
		void _applySpecular		( const Colour & p_specular);
		void _applyPosition		( const Point4f & p_position);
		void _applyOrientation	( const Matrix4x4r & p_matrix);
		void _applyConstantAtt	( float p_constant);
		void _applyLinearAtt	( float p_linear);
		void _applyQuadraticAtt	( float p_quadratic);
		void _applyExponent		( float p_exponent);
		void _applyCutOff		( float p_cutOff);
	};
}

#endif