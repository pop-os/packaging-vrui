/***********************************************************************
AppearanceNode - Class defining the appearance (material properties,
textures) of a shape node.
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

#ifndef SCENEGRAPH_APPEARANCENODE_INCLUDED
#define SCENEGRAPH_APPEARANCENODE_INCLUDED

#include <Misc/Autopointer.h>
#include <SceneGraph/FieldTypes.h>
#include <SceneGraph/BaseAppearanceNode.h>
#include <SceneGraph/MaterialNode.h>
#include <SceneGraph/TextureNode.h>
#include <SceneGraph/TextureTransformNode.h>

namespace SceneGraph {

class AppearanceNode:public BaseAppearanceNode
	{
	/* Embedded classes: */
	public:
	typedef SF<MaterialNodePointer> SFMaterialNode;
	typedef SF<TextureNodePointer> SFTextureNode;
	typedef SF<TextureTransformNodePointer> SFTextureTransformNode;
	
	/* Elements: */
	static const char* className; // The class's name
	
	/* Fields: */
	SFMaterialNode material;
	SFTextureNode texture;
	SFTextureTransformNode textureTransform;
	
	/* Constructors and destructors: */
	AppearanceNode(void); // Creates a default appearance node
	
	/* Methods from class Node: */
	virtual const char* getClassName(void) const;
	virtual EventOut* getEventOut(const char* fieldName) const;
	virtual EventIn* getEventIn(const char* fieldName);
	virtual void parseField(const char* fieldName,VRMLFile& vrmlFile);
	virtual unsigned int cascadingUpdate(Node& child,unsigned int childUpdateResult);
	
	/* Methods from class AttributeNode: */
	virtual void setGLState(GLRenderState& renderState) const;
	virtual void resetGLState(GLRenderState& renderState) const;
	
	/* Methods from class BaseAppearanceNode: */
	virtual bool requiresTexCoords(void) const;
	virtual bool requiresColors(void) const;
	virtual bool requiresNormals(void) const;
	virtual bool isTransparent(void) const;
	};

typedef Misc::Autopointer<AppearanceNode> AppearanceNodePointer;

}

#endif
