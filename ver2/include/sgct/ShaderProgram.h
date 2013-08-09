/*************************************************************************
Copyright (c) 2012-2013 Miroslav Andel
All rights reserved.

For conditions of distribution and use, see copyright notice in sgct.h 
*************************************************************************/

#ifndef _SHADER_PROGRAM_H_
#define _SHADER_PROGRAM_H_

#include "Shader.h"

namespace sgct
{

/*!
Helper class for handling compiling, linking and using shader programs.
Current implementation only supports vertex and fragment shader. Uniform and
attribute handling must be managed explicitly but it is possible to poll the
Shader program for uniform and attribute locations.
*/
class ShaderProgram
{
public:
	/*! If shader source should be loaded from file or read as is */
	enum ShaderSourceType{ SHADER_SRC_FILE, SHADER_SRC_STRING };

	ShaderProgram();
	ShaderProgram( const std::string & name );
	~ShaderProgram( void );

	void deleteProgram();

	void setName( const std::string & name );
	bool setVertexShaderSrc( const std::string & src, ShaderSourceType sSrcType = SHADER_SRC_FILE );
	bool setFragmentShaderSrc( const std::string & src, ShaderSourceType sSrcType = SHADER_SRC_FILE );
	bool setGeometryShaderSrc( const std::string & src, ShaderSourceType sSrcType = SHADER_SRC_FILE );

	bool createAndLinkProgram();

	bool bind() const;
	static void unbind();

	int getAttribLocation( const std::string & name ) const;
	int getUniformLocation( const std::string & name ) const;

	/*! Less than ShaderProgram operator */
	inline bool operator<( const ShaderProgram & rhs ) const { return mName < rhs.mName; }

	/*! Equal to ShaderProgram operator */
	inline bool operator==( const ShaderProgram & rhs ) const { return mName == rhs.mName; }

	/*! Not equal to ShaderProgram operator */
	inline bool operator!=( const ShaderProgram & rhs ) const { return mName != rhs.mName; }

	/*! Equal to string operator */
	inline bool operator==( const std::string & rhs ) const { return mName == rhs; }

	/*! Get the name of the program */
	inline std::string getName() { return mName; }

	/*! Check if the program is linked */
	inline bool isLinked() { return mIsLinked; }

	/*! Get the program ID */
	inline int getId() { return mProgramId; }

private:

	bool createProgram();
	bool checkLinkStatus() const;

private:

	std::string mName;					// Name of the program, has to be unique
	bool mIsLinked;						// If this program has been linked
	int mProgramId;						// Unique program id

	sgct_core::Shader mVertexShader;	// Handler for the vertex shader
	sgct_core::Shader mFragmentShader;	// Handler for the fragment shader
	sgct_core::Shader mGeometryShader;	// Handler for the geometry shader
};

} // sgct
#endif
