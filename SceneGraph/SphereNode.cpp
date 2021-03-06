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

#include <SceneGraph/SphereNode.h>

#include <string.h>
#include <GL/gl.h>
#include <Math/Math.h>
#include <SceneGraph/EventTypes.h>
#include <SceneGraph/VRMLFile.h>
#include <SceneGraph/SphereCollisionQuery.h>
#include <SceneGraph/GLRenderState.h>

namespace SceneGraph {

namespace {

/****************
Helper functions:
****************/

inline void combine(const GLfloat p100[3],const GLfloat p010[3],const GLfloat p001[3],GLfloat w0,GLfloat w1,GLfloat radius,const GLfloat center[3])
	{
	GLfloat w2=1.0f-w0-w1;
	GLfloat result[3];
	GLfloat resultLen=0.0f;
	for(int i=0;i<3;++i)
		{
		result[i]=p100[i]*w0+p010[i]*w1+p001[i]*w2;
		resultLen+=result[i]*result[i];
		}
	resultLen=Math::sqrt(resultLen);
	for(int i=0;i<3;++i)
		result[i]/=resultLen;
	glNormal3fv(result);
	for(int i=0;i<3;++i)
		result[i]*=radius;
	glVertex3f(result[0]+center[0],result[1]+center[1],result[2]+center[2]);
	}

inline void combine(const GLfloat p00[3],const GLfloat p10[3],const GLfloat p01[3],const GLfloat p11[3],GLfloat wx,GLfloat wy,GLfloat radius,const GLfloat center[3])
	{
	GLfloat result[3];
	GLfloat resultLen=0.0f;
	if(wx>wy)
		{
		for(int i=0;i<3;++i)
			{
			result[i]=p00[i]*(1.0f-wx)+p11[i]*wy+p10[i]*(wx-wy);
			resultLen+=result[i]*result[i];
			}
		}
	else
		{
		for(int i=0;i<3;++i)
			{
			result[i]=p11[i]*wx+p00[i]*(1.0f-wy)+p01[i]*(wy-wx);
			resultLen+=result[i]*result[i];
			}
		}
	resultLen=Math::sqrt(resultLen);
	for(int i=0;i<3;++i)
		result[i]/=resultLen;
	glNormal3fv(result);
	for(int i=0;i<3;++i)
		result[i]*=radius;
	glVertex3f(result[0]+center[0],result[1]+center[1],result[2]+center[2]);
	}

}

/***********************************
Static elements of class SphereNode:
***********************************/

const char* SphereNode::className="Sphere";

/***************************
Methods of class SphereNode:
***************************/

void SphereNode::createList(GLContextData& renderState) const
	{
	if(latLong.getValue())
		{
		/*******************************************************************
		Draw a sphere made of latitude-parallel quad strips with or without
		texture coordinates:
		*******************************************************************/
		
		const GLfloat pi=Math::Constants<GLfloat>::pi;
		const GLfloat ns=GLfloat(numSegments.getValue());
		const GLfloat nq=GLfloat(numSegments.getValue()*2);
		const GLfloat cx=center.getValue()[0];
		const GLfloat cy=center.getValue()[1];
		const GLfloat cz=center.getValue()[2];
		const GLfloat r=radius.getValue();
		
		/* Draw quad strips: */
		GLfloat texY1=0.0f;
		GLfloat lat1=-0.5f*pi;
		GLfloat r1=Math::cos(lat1);
		GLfloat y1=Math::sin(lat1);
		for(int i=1;i<=numSegments.getValue();++i)
			{
			GLfloat r0=r1;
			GLfloat y0=y1;
			GLfloat texY0=texY1;
			texY1=GLfloat(i)/ns;
			lat1=GLfloat(i)*pi/ns-0.5f*pi;
			r1=Math::cos(lat1);
			y1=Math::sin(lat1);
			
			glBegin(GL_QUAD_STRIP);
			for(int j=0;j<=numSegments.getValue()*2;++j)
				{
				GLfloat texX=GLfloat(j)/nq;
				GLfloat lng=GLfloat(j)*(2.0f*pi)/nq;
				GLfloat x1=-Math::sin(lng)*r1;
				GLfloat z1=-Math::cos(lng)*r1;
				glNormal3f(x1,y1,z1);
				if(texCoords.getValue())
					glTexCoord2f(texX,texY1);
				glVertex3f(cx+x1*r,cy+y1*r,cz+z1*r);
				GLfloat x0=-Math::sin(lng)*r0;
				GLfloat z0=-Math::cos(lng)*r0;
				glNormal3f(x0,y0,z0);
				if(texCoords.getValue())
					glTexCoord2f(texX,texY0);
				glVertex3f(cx+x0*r,cy+y0*r,cz+z0*r);
				}
			glEnd();
			}
		}
	else
		{
		/*******************************************************************
		Draw a sphere based on a subdivided icosahedron:
		*******************************************************************/
		
		/* Construct static icosahedron model: */
		const GLfloat b0=0.525731112119133606f; // b0=sqrt((5.0-sqrt(5.0))/10);
		const GLfloat b1=0.850650808352039932f; // b1=sqrt((5.0+sqrt(5.0))/10);
		static const GLfloat vUnit[12][3]={{-b0,0.0f, b1},{ b0,0.0f, b1},{-b0,0.0f,-b1},{ b0,0.0f,-b1},
	                                  	 {0.0f, b1, b0},{0.0f, b1,-b0},{0.0f,-b1, b0},{0.0f,-b1,-b0},
	                                  	 { b1, b0,0.0f},{-b1, b0,0.0f},{ b1,-b0,0.0f},{-b1,-b0,0.0f}};
		static const int stripIndices[12]={0,1,4,8,5,3,2,7,11,6,0,1};
		static const int fanIndices[2][7]={{9,0,4,5,2,11,0},{10,1,6,7,3,8,1}};
		
		GLfloat c[3];
		for(int i=0;i<3;++i)
			c[i]=GLfloat(center.getValue()[i]);
		GLfloat r=GLfloat(radius.getValue());
		int numStrips=(numSegments.getValue()+2)/3;
		
		/* Render the central triangle strips: */
		for(int strip=0;strip<numStrips;++strip)
			{
			GLfloat botW=GLfloat(strip)/GLfloat(numStrips);
			GLfloat topW=GLfloat(strip+1)/GLfloat(numStrips);
			glBegin(GL_TRIANGLE_STRIP);
			for(int i=0;i<10;i+=2)
				{
				const GLfloat* p00=vUnit[stripIndices[i+1]];
				const GLfloat* p10=vUnit[stripIndices[i+3]];
				const GLfloat* p01=vUnit[stripIndices[i+0]];
				const GLfloat* p11=vUnit[stripIndices[i+2]];
				for(int j=0;j<numStrips;++j)
					{
					GLfloat leftW=GLfloat(j)/GLfloat(numStrips);
					// GLfloat rightW=GLfloat(j+1)/GLfloat(numStrips);
					combine(p00,p10,p01,p11,leftW,topW,r,c);
					combine(p00,p10,p01,p11,leftW,botW,r,c);
					}
				combine(p00,p10,p01,p11,1.0f,topW,r,c);
				combine(p00,p10,p01,p11,1.0f,botW,r,c);
				}
			glEnd();
			}
		
		for(int cap=0;cap<2;++cap)
			{
			/* Render the cap triangle strips: */
			for(int strip=0;strip<numStrips-1;++strip)
				{
				GLfloat botW=GLfloat(strip)/GLfloat(numStrips);
				GLfloat topW=GLfloat(strip+1)/GLfloat(numStrips);
				glBegin(GL_TRIANGLE_STRIP);
				combine(vUnit[fanIndices[cap][0]],vUnit[fanIndices[cap][2]],vUnit[fanIndices[cap][1]],topW,0.0f,r,c);
				for(int i=1;i<6;++i)
					{
					const GLfloat* p100=vUnit[fanIndices[cap][0]];
					const GLfloat* p010=vUnit[fanIndices[cap][i]];
					const GLfloat* p001=vUnit[fanIndices[cap][i+1]];
					for(int j=0;j<numStrips-strip;++j)
						{
						GLfloat leftW=GLfloat(j)/GLfloat(numStrips);
						combine(p100,p001,p010,botW,leftW,r,c);
						combine(p100,p001,p010,topW,leftW,r,c);
						}
					}
				combine(vUnit[fanIndices[cap][0]],vUnit[fanIndices[cap][2]],vUnit[fanIndices[cap][1]],botW,0.0f,r,c);
				glEnd();
				}
			
			/* Render the cap triangle fan: */
			glBegin(GL_TRIANGLE_FAN);
			combine(vUnit[fanIndices[cap][0]],vUnit[fanIndices[cap][2]],vUnit[fanIndices[cap][1]],1.0f,0.0f,r,c);
			GLfloat botW=GLfloat(numStrips-1)/GLfloat(numStrips);
			for(int i=1;i<6;++i)
				combine(vUnit[fanIndices[cap][0]],vUnit[fanIndices[cap][i+1]],vUnit[fanIndices[cap][i]],botW,0.0f,r,c);
			combine(vUnit[fanIndices[cap][0]],vUnit[fanIndices[cap][2]],vUnit[fanIndices[cap][1]],botW,0.0f,r,c);
			glEnd();
			}
		}
	}

SphereNode::SphereNode(void)
	:center(Point::origin),
	 radius(1.0f),
	 numSegments(12),
	 latLong(true),
	 texCoords(true),
	 ccw(true)
	{
	}

const char* SphereNode::getClassName(void) const
	{
	return className;
	}

EventOut* SphereNode::getEventOut(const char* fieldName) const
	{
	if(strcmp(fieldName,"center")==0)
		return makeEventOut(this,center);
	else if(strcmp(fieldName,"radius")==0)
		return makeEventOut(this,radius);
	else if(strcmp(fieldName,"numSegments")==0)
		return makeEventOut(this,numSegments);
	else if(strcmp(fieldName,"latLong")==0)
		return makeEventOut(this,latLong);
	else if(strcmp(fieldName,"texCoords")==0)
		return makeEventOut(this,texCoords);
	else if(strcmp(fieldName,"ccw")==0)
		return makeEventOut(this,ccw);
	else
		return GeometryNode::getEventOut(fieldName);
	}

EventIn* SphereNode::getEventIn(const char* fieldName)
	{
	if(strcmp(fieldName,"center")==0)
		return makeEventIn(this,center);
	else if(strcmp(fieldName,"radius")==0)
		return makeEventIn(this,radius);
	else if(strcmp(fieldName,"numSegments")==0)
		return makeEventIn(this,numSegments);
	else if(strcmp(fieldName,"latLong")==0)
		return makeEventIn(this,latLong);
	else if(strcmp(fieldName,"texCoords")==0)
		return makeEventIn(this,texCoords);
	else if(strcmp(fieldName,"ccw")==0)
		return makeEventIn(this,ccw);
	else
		return GeometryNode::getEventIn(fieldName);
	}

void SphereNode::parseField(const char* fieldName,VRMLFile& vrmlFile)
	{
	if(strcmp(fieldName,"center")==0)
		vrmlFile.parseField(center);
	else if(strcmp(fieldName,"radius")==0)
		vrmlFile.parseField(radius);
	else if(strcmp(fieldName,"numSegments")==0)
		vrmlFile.parseField(numSegments);
	else if(strcmp(fieldName,"latLong")==0)
		vrmlFile.parseField(latLong);
	else if(strcmp(fieldName,"texCoords")==0)
		vrmlFile.parseField(texCoords);
	else if(strcmp(fieldName,"ccw")==0)
		vrmlFile.parseField(ccw);
	else
		GeometryNode::parseField(fieldName,vrmlFile);
	}

unsigned int SphereNode::update(void)
	{
	/* Invalidate the display list: */
	DisplayList::update();
	
	return NoCascade;
	}

Box SphereNode::calcBoundingBox(void) const
	{
	/* Return a box around the sphere: */
	Point pmin=center.getValue();
	Point pmax=center.getValue();
	for(int i=0;i<3;++i)
		{
		pmin[i]-=radius.getValue();
		pmax[i]+=radius.getValue();
		}
	return Box(pmin,pmax);
	}

void SphereNode::testCollision(SphereCollisionQuery& collisionQuery) const
	{
	/* Check whether the sphere collides outside-in or inside-out: */
	if(ccw.getValue())
		{
		Vector sc0=collisionQuery.getC0()-center.getValue(); // Vector from this sphere's center to sliding sphere's starting point
		
		Scalar a=collisionQuery.getC0c1Sqr(); // Quadratic coefficient from quadratic formula
		Scalar bh=sc0*collisionQuery.getC0c1(); // Halved linear coefficient from quadratic formula
		Scalar c=sc0.sqr()-Math::sqr(radius.getValue()+collisionQuery.getRadius()); // Constant coefficient from quadratic formula
		
		Scalar discq=bh*bh-a*c; // Quarter discriminant from quadratic formula
		if(discq>=Scalar(0))
			{
			/* Find the quadratic equation's smaller root: */
			// Scalar lambda=bh>=Scalar(0)?(-bh-Math::sqrt(discq))/a:c/(-bh+Math::sqrt(discq)); // Slightly more stable formula
			Scalar lambda=c/(-bh+Math::sqrt(discq)); // Stable formulation for negative bh, which is the only bh that counts
			
			/* Check whether this sphere will affect the collision: */
			if(lambda>=Scalar(0))
				{
				if(lambda<collisionQuery.getHitLambda())
					{
					/* Update the hit result: */
					collisionQuery.update(lambda,Geometry::addScaled(sc0,collisionQuery.getC0c1(),lambda));
					}
				}
			else if(c<Scalar(0)&&bh<Scalar(0)&&collisionQuery.getHitLambda()>Scalar(0))
				{
				/* Sphere already penetrates the sphere; prevent it from getting worse: */
				collisionQuery.update(Scalar(0),sc0);
				}
			}
		}
	else
		{
		Vector c0s=center.getValue()-collisionQuery.getC0(); // Vector from sliding sphere's starting point to this sphere's center
		
		Scalar a=collisionQuery.getC0c1Sqr(); // Quadratic coefficient from quadratic formula
		Scalar bh=c0s*collisionQuery.getC0c1(); // Halved linear coefficient from quadratic formula
		Scalar c=c0s.sqr()-Math::sqr(radius.getValue()-collisionQuery.getRadius()); // Constant coefficient from quadratic formula
		
		Scalar discq=bh*bh-a*c; // Quarter discriminant from quadratic formula
		if(discq>=Scalar(0))
			{
			/* Find the quadratic equation's larger root: */
			Scalar lambda=bh>=Scalar(0)?(bh+Math::sqrt(discq))/a:c/(bh-Math::sqrt(discq)); // Slightly more stable formula
			
			/* Check whether this sphere will affect the collision: */
			if(lambda>=Scalar(0))
				{
				if(lambda<collisionQuery.getHitLambda())
					{
					/* Update the hit result: */
					collisionQuery.update(lambda,Geometry::addScaled(c0s,collisionQuery.getC0c1(),-lambda));
					}
				}
			else if(collisionQuery.getHitLambda()>Scalar(0))
				{
				/* Sphere already outside the sphere; prevent it from getting worse: */
				collisionQuery.update(Scalar(0),c0s);
				}
			}
		else if(collisionQuery.getHitLambda()>Scalar(0))
			{
			/* Sphere already outside the sphere; prevent it from getting worse: */
			collisionQuery.update(Scalar(0),c0s);
			}
		}
	}

void SphereNode::glRenderAction(GLRenderState& renderState) const
	{
	/* Set up OpenGL state: */
	renderState.uploadModelview();
	renderState.setFrontFace(ccw.getValue()?GL_CCW:GL_CW);
	renderState.enableCulling(GL_BACK);
	
	/* Render the display list: */
	DisplayList::glRenderAction(renderState.contextData);
	}

}
