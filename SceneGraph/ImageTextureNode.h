/***********************************************************************
ImageTextureNode - Class for textures loaded from external image files.
Copyright (c) 2009-2021 Oliver Kreylos

This file is part of the Simple Scene Graph Renderer (SceneGraph).

The Simple Scene Graph Renderer is free software; you can redistribute
it and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The Simple Scene Graph Renderer is distributed in the hope that it will
be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License along
with the Simple Scene Graph Renderer; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
***********************************************************************/

#ifndef SCENEGRAPH_IMAGETEXTURENODE_INCLUDED
#define SCENEGRAPH_IMAGETEXTURENODE_INCLUDED

#include <Misc/Autopointer.h>
#include <IO/Directory.h>
#include <GL/gl.h>
#include <GL/GLObject.h>
#include <SceneGraph/FieldTypes.h>
#include <SceneGraph/TextureNode.h>

namespace SceneGraph {

class ImageTextureNode:public TextureNode,public GLObject
	{
	/* Embedded classes: */
	protected:
	struct DataItem:public GLObject::DataItem
		{
		/* Elements: */
		GLuint textureObjectId; // ID of texture object
		unsigned int version; // Version of texture in texture object
		
		/* Constructors and destructors: */
		DataItem(void);
		virtual ~DataItem(void);
		};
	
	/* Elements: */
	public:
	static const char* className; // The class's name
	
	/* Fields: */
	MFString url;
	SFBool repeatS;
	SFBool repeatT;
	SFBool filter; // Enables bilinear (or trilinear if mipmapLevel>0) filtering
	SFInt mipmapLevel; // Maximum mipmap level that should be generated from the texture image; 0 disables mipmapping
	
	/* Derived state: */
	protected:
	IO::DirectoryPtr baseDirectory; // Base directory for image URLs
	unsigned int version; // Version number of texture
	
	/* Constructors and destructors: */
	public:
	ImageTextureNode(void); // Creates a default image texture node with no texture image
	
	/* Methods from class Node: */
	virtual const char* getClassName(void) const;
	virtual void parseField(const char* fieldName,VRMLFile& vrmlFile);
	virtual unsigned int update(void);
	
	/* Methods from class AttributeNode: */
	virtual void setGLState(GLRenderState& renderState) const;
	virtual void resetGLState(GLRenderState& renderState) const;
	
	/* Methods from class GLObject: */
	virtual void initContext(GLContextData& contextData) const;
	
	/* New methods: */
	void setUrl(const std::string& newUrl,IO::Directory& newBaseDirectory); // Sets an image URL and its base directory
	void setUrl(const std::string& newUrl); // Ditto, with URL relative to the current directory
	};

typedef Misc::Autopointer<ImageTextureNode> ImageTextureNodePointer;

}

#endif
