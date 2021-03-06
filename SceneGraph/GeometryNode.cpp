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

#include <SceneGraph/GeometryNode.h>

#include <string.h>
#include <SceneGraph/VRMLFile.h>

namespace SceneGraph {

/*****************************
Methods of class GeometryNode:
*****************************/

GeometryNode::GeometryNode(void)
	:needTexCoords(false),needColors(false),needNormals(false)
	{
	}

void GeometryNode::parseField(const char* fieldName,VRMLFile& vrmlFile)
	{
	if(strcmp(fieldName,"pointTransform")==0)
		{
		vrmlFile.parseSFNode(pointTransform);
		}
	else
		Node::parseField(fieldName,vrmlFile);
	}

void GeometryNode::mustProvideTexCoords(void)
	{
	needTexCoords=true;
	}

void GeometryNode::mustProvideColors(void)
	{
	needColors=true;
	}

void GeometryNode::mustProvideNormals(void)
	{
	needNormals=true;
	}

}
