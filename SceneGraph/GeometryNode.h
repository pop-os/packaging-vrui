/***********************************************************************
GeometryNode - Base class for nodes that define renderable geometry.
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

#ifndef SCENEGRAPH_GEOMETRYNODE_INCLUDED
#define SCENEGRAPH_GEOMETRYNODE_INCLUDED

#include <Misc/Autopointer.h>
#include <SceneGraph/FieldTypes.h>
#include <SceneGraph/Node.h>
#include <SceneGraph/PointTransformNode.h>

/* Forward declarations: */
namespace SceneGraph {
class SphereCollisionQuery;
class GLRenderState;
}

namespace SceneGraph {

class GeometryNode:public Node
	{
	/* Embedded classes: */
	public:
	typedef SF<PointTransformNodePointer> SFPointTransformNode;
	
	/* Elements: */
	static const char* className; // The class's name
	
	/* Fields: */
	SFPointTransformNode pointTransform;
	
	/* Derived state: */
	protected:
	bool needTexCoords; // Flag whether the geometry node needs to provide per-vertex texture coordinates for rendering
	bool needColors; // Flag whether the geometry node needs to provide per-vertex colors for rendering
	bool needNormals; // Flag whether the geometry node needs to provide per-vertex normal vectors for rendering
	
	/* Constructors and destructors: */
	public:
	GeometryNode(void); // Creates an empty geometry node
	
	/* Methods from class Node: */
	virtual void parseField(const char* fieldName,VRMLFile& vrmlFile);
	
	/* New methods: */
	void mustProvideTexCoords(void); // Flags the geometry node as requiring per-vertex texture coordinates
	void mustProvideColors(void); // Flags the geometry node as requiring per-vertex colors
	void mustProvideNormals(void); // Flags the geometry node as requiring per-vertex normal vectors
	virtual Box calcBoundingBox(void) const =0; // Returns the bounding box of the geometry defined by the node
	virtual void testCollision(SphereCollisionQuery& collisionQuery) const =0; // Tests the node for collision with a moving sphere
	virtual void glRenderAction(GLRenderState& renderState) const =0; // Renders the geometry defined by the node into the current OpenGL context
	};

typedef Misc::Autopointer<GeometryNode> GeometryNodePointer;

}

#endif
