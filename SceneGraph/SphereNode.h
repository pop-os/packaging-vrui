/***********************************************************************
SphereNode - Class for spheres as renderable geometry.
Copyright (c) 2013-2021 Oliver Kreylos

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

#ifndef SCENEGRAPH_SPHERENODE_INCLUDED
#define SCENEGRAPH_SPHERENODE_INCLUDED

#include <Geometry/ComponentArray.h>
#include <Geometry/Point.h>
#include <Geometry/Box.h>
#include <SceneGraph/FieldTypes.h>
#include <SceneGraph/DisplayList.h>
#include <SceneGraph/GeometryNode.h>

namespace SceneGraph {

class SphereNode:public GeometryNode,public DisplayList
	{
	/* Elements: */
	public:
	static const char* className; // The class's name
	
	/* Fields: */
	SFPoint center;
	SFFloat radius;
	SFInt numSegments;
	SFBool latLong;
	SFBool texCoords;
	SFBool ccw; // Flag whether to show the outside or inside of the sphere
	
	/* Protected methods from class DisplayList: */
	protected:
	virtual void createList(GLContextData& contextData) const;
	
	/* Constructors and destructors: */
	public:
	SphereNode(void); // Creates a default sphere (centered at origin, radius 1)
	
	/* Methods from class Node: */
	virtual const char* getClassName(void) const;
	virtual EventOut* getEventOut(const char* fieldName) const;
	virtual EventIn* getEventIn(const char* fieldName);
	virtual void parseField(const char* fieldName,VRMLFile& vrmlFile);
	virtual unsigned int update(void);
	
	/* Methods from class GeometryNode: */
	virtual Box calcBoundingBox(void) const;
	virtual void testCollision(SphereCollisionQuery& collisionQuery) const;
	virtual void glRenderAction(GLRenderState& renderState) const;
	};

typedef Misc::Autopointer<SphereNode> SphereNodePointer;

}

#endif
