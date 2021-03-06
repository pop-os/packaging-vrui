/***********************************************************************
ShowEarthModel - Simple Vrui application to render a model of Earth,
with the ability to additionally display earthquake location data and
other geology-related stuff.
Copyright (c) 2005-2021 Oliver Kreylos

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or (at your
option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
***********************************************************************/

#include "ShowEarthModel.h"

#define CLIP_SCREEN 0

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <vector>
#include <Misc/FunctionCalls.h>
#include <Misc/ThrowStdErr.h>
#include <Misc/File.h>
#include <Misc/StandardValueCoders.h>
#include <Misc/ConfigurationFile.h>
#include <IO/Directory.h>
#include <Math/Math.h>
#include <Math/Constants.h>
#include <Geometry/LinearUnit.h>
#include <GL/gl.h>
#include <GL/GLColorTemplates.h>
#include <GL/GLMatrixTemplates.h>
#include <GL/GLMaterial.h>
#include <GL/GLColorMap.h>
#include <GL/GLContextData.h>
#include <GL/GLModels.h>
#include <GL/GLGeometryWrappers.h>
#include <GL/GLPolylineTube.h>
#include <GL/GLExtensionManager.h>
#include <GL/Extensions/GLARBVertexBufferObject.h>
#include <GL/GLTransformationWrappers.h>
#include <GL/GLFrustum.h>
#include <Images/Config.h>
#include <Images/RGBImage.h>
#include <Images/ReadPNGImage.h>
#include <Images/ReadImageFile.h>
#include <GLMotif/StyleSheet.h>
#include <GLMotif/PopupMenu.h>
#include <GLMotif/PopupWindow.h>
#include <GLMotif/RowColumn.h>
#include <GLMotif/Blind.h>
#include <GLMotif/Label.h>
#include <GLMotif/Button.h>
#include <GLMotif/CascadeButton.h>
#include <GLMotif/TextField.h>
#include <SceneGraph/NodeCreator.h>
#include <SceneGraph/GroupNode.h>
#include <SceneGraph/VRMLFile.h>
#include <SceneGraph/GLRenderState.h>
#include <Vrui/Vrui.h>
#include <Vrui/CoordinateManager.h>
#include <Vrui/Viewer.h>
#if CLIP_SCREEN
#include <Vrui/VRScreen.h>
#endif
#include <Vrui/SurfaceNavigationTool.h>
#include <Vrui/Application.h>
#include <Vrui/SceneGraphSupport.h>

#if USE_COLLABORATION
#include <Collaboration2/Client.h>
#endif

#include "EarthFunctions.h"
#include "EarthquakeSet.h"
#include "EarthquakeTool.h"
#include "EarthquakeQueryTool.h"
#include "PointSet.h"
#include "SeismicPath.h"

/*******************************************************************
Methods of class ShowEarthModel::RotatedGeodeticCoordinateTransform:
*******************************************************************/

ShowEarthModel::RotatedGeodeticCoordinateTransform::RotatedGeodeticCoordinateTransform(void)
	:Vrui::GeodeticCoordinateTransform(0.001),
	 rotationAngle(0),raSin(0),raCos(1)
	{
	}

const char* ShowEarthModel::RotatedGeodeticCoordinateTransform::getUnitName(int componentIndex) const
	{
	switch(componentIndex)
		{
		case 0:
		case 1:
			return "degree";
			break;
		
		case 2:
			return "kilometer";
			break;
		
		default:
			return "";
		}
	}

const char* ShowEarthModel::RotatedGeodeticCoordinateTransform::getUnitAbbreviation(int componentIndex) const
	{
	switch(componentIndex)
		{
		case 0:
		case 1:
			return "deg";
			break;
		
		case 2:
			return "km";
			break;
		
		default:
			return "";
		}
	}

Vrui::Point ShowEarthModel::RotatedGeodeticCoordinateTransform::transform(const Vrui::Point& navigationPoint) const
	{
	/* First undo the rotation: */
	Vrui::Point p;
	p[0]=raCos*navigationPoint[0]+raSin*navigationPoint[1];
	p[1]=raCos*navigationPoint[1]-raSin*navigationPoint[0];
	p[2]=navigationPoint[2];
	
	/* Then convert the point to geodetic coordinates: */
	return Vrui::GeodeticCoordinateTransform::transform(p);
	}

Vrui::Point ShowEarthModel::RotatedGeodeticCoordinateTransform::inverseTransform(const Vrui::Point& userPoint) const
	{
	/* First convert the point to Cartesian coordinates: */
	Vrui::Point p=Vrui::GeodeticCoordinateTransform::inverseTransform(userPoint);
	
	/* Then do the rotation: */
	Vrui::Point result;
	result[0]=raCos*p[0]-raSin*p[1];
	result[1]=raCos*p[1]+raSin*p[0];
	result[2]=p[2];
	return result;
	}

void ShowEarthModel::RotatedGeodeticCoordinateTransform::setRotationAngle(Vrui::Scalar newRotationAngle)
	{
	rotationAngle=newRotationAngle;
	raSin=Math::sin(Math::rad(rotationAngle));
	raCos=Math::cos(Math::rad(rotationAngle));
	}

/*****************************************
Methods of class ShowEarthModel::DataItem:
*****************************************/

ShowEarthModel::DataItem::DataItem(void)
	:hasVertexBufferObjectExtension(false), // GLARBVertexBufferObject::isSupported()),
	 surfaceVertexBufferObjectId(0),surfaceIndexBufferObjectId(0),
	 surfaceTextureObjectId(0),
	 displayListIdBase(0)
	{
	/* Use buffer objects for the Earth surface if supported: */
	if(hasVertexBufferObjectExtension)
		{
		/* Initialize the vertex buffer object extension: */
		GLARBVertexBufferObject::initExtension();
		
		/* Create vertex buffer objects: */
		GLuint bufferObjectIds[2];
		glGenBuffersARB(2,bufferObjectIds);
		surfaceVertexBufferObjectId=bufferObjectIds[0];
		surfaceIndexBufferObjectId=bufferObjectIds[1];
		}
	
	/* Generate a texture object for the Earth's surface texture: */
	glGenTextures(1,&surfaceTextureObjectId);
	
	/* Generate display lists for the Earth model components: */
	displayListIdBase=glGenLists(4);
	}

ShowEarthModel::DataItem::~DataItem(void)
	{
	if(hasVertexBufferObjectExtension)
		{
		/* Delete vertex buffer objects: */
		GLuint bufferObjectIds[2];
		bufferObjectIds[0]=surfaceVertexBufferObjectId;
		bufferObjectIds[1]=surfaceIndexBufferObjectId;
		glDeleteBuffersARB(2,bufferObjectIds);
		}
	
	/* Delete the Earth surface texture object: */
	glDeleteTextures(1,&surfaceTextureObjectId);
	
	/* Delete the Earth model components display lists: */
	glDeleteLists(displayListIdBase,4);
	}

/*******************************
Methods of class ShowEarthModel:
*******************************/

void ShowEarthModel::settingsChangedCallback(Misc::CallbackData* cbData)
	{
	#if USE_COLLABORATION
	if(koinonia!=0)
		{
		/* Share the new render settings with the server: */
		koinonia->replaceSharedObject(settingsId);
		}
	#endif
	
	/* Update rendering materials: */
	surfaceMaterial.diffuse[3]=settings.surfaceAlpha;
	outerCoreMaterial.diffuse[3]=settings.outerCoreAlpha;
	innerCoreMaterial.diffuse[3]=settings.innerCoreAlpha;
	
	/* Update the UI: */
	mainMenu->updateVariables();
	renderDialog->updateVariables();
	animationDialog->updateVariables();
	}

#if USE_COLLABORATION

void ShowEarthModel::settingsUpdatedCallback(KoinoniaClient* client,KoinoniaProtocol::ObjectID id,void* object,void* userData)
	{
	ShowEarthModel* thisPtr=static_cast<ShowEarthModel*>(userData);
	
	/* Update rendering materials: */
	thisPtr->surfaceMaterial.diffuse[3]=thisPtr->settings.surfaceAlpha;
	thisPtr->outerCoreMaterial.diffuse[3]=thisPtr->settings.outerCoreAlpha;
	thisPtr->innerCoreMaterial.diffuse[3]=thisPtr->settings.innerCoreAlpha;
	
	/* Update application state: */
	thisPtr->userTransform->setRotationAngle(Vrui::Scalar(thisPtr->settings.rotationAngle));
	
	/* Update the UI: */
	thisPtr->mainMenu->updateVariables();
	thisPtr->renderDialog->updateVariables();
	thisPtr->animationDialog->updateVariables();
	bool timeChanged=false;
	if(thisPtr->settings.playSpeed!=thisPtr->playSpeedSlider->getValue())
		{
		thisPtr->playSpeedSlider->setValue(Math::log10(thisPtr->settings.playSpeed));
		thisPtr->currentTimeSlider->setValueRange(thisPtr->earthquakeTimeRange.getMin()-thisPtr->settings.playSpeed,thisPtr->earthquakeTimeRange.getMax()+thisPtr->settings.playSpeed,thisPtr->settings.playSpeed);
		timeChanged=true;
		}
	if(thisPtr->settings.currentTime!=thisPtr->currentTimeSlider->getValue())
		{
		thisPtr->currentTimeSlider->setValue(thisPtr->settings.currentTime);
		timeChanged=true;
		}
	if(timeChanged)
		thisPtr->updateCurrentTime();
	}

#endif

GLMotif::PopupMenu* ShowEarthModel::createRenderTogglesMenu(void)
	{
	/* Create the submenu shell: */
	GLMotif::PopupMenu* renderTogglesMenu=new GLMotif::PopupMenu("RenderTogglesMenu",Vrui::getWidgetManager());
	
	/* Create a toggle button to render the Earth's surface: */
	GLMotif::ToggleButton* showSurfaceToggle=new GLMotif::ToggleButton("ShowSurfaceToggle",renderTogglesMenu,"Show Surface");
	showSurfaceToggle->track(settings.showSurface);
	showSurfaceToggle->getValueChangedCallbacks().add(this,&ShowEarthModel::settingsChangedCallback);
	
	/* Create a toggle button to render the Earth's surface transparently: */
	GLMotif::ToggleButton* surfaceTransparentToggle=new GLMotif::ToggleButton("SurfaceTransparentToggle",renderTogglesMenu,"Surface Transparent");
	surfaceTransparentToggle->track(settings.surfaceTransparent);
	surfaceTransparentToggle->getValueChangedCallbacks().add(this,&ShowEarthModel::settingsChangedCallback);
	
	/* Create a toggle button to render the lat/long grid: */
	GLMotif::ToggleButton* showGridToggle=new GLMotif::ToggleButton("ShowGridToggle",renderTogglesMenu,"Show Grid");
	showGridToggle->track(settings.showGrid);
	showGridToggle->getValueChangedCallbacks().add(this,&ShowEarthModel::settingsChangedCallback);
	
	/* Create toggles for each earthquake set: */
	for(unsigned int i=0;i<earthquakeSets.size();++i)
		{
		/* Create a unique name for the toggle button widget: */
		char toggleName[256];
		snprintf(toggleName,sizeof(toggleName),"ShowEarthquakeSetToggle%04d",i);
		
		/* Create a label to display in the submenu: */
		char toggleLabel[256];
		snprintf(toggleLabel,sizeof(toggleLabel),"Show Earthquake Set %d",i);
		
		/* Create a toggle button to render the earthquake set: */
		GLMotif::ToggleButton* showEarthquakeSetToggle=new GLMotif::ToggleButton(toggleName,renderTogglesMenu,toggleLabel);
		showEarthquakeSetToggle->track(settings.showEarthquakeSets[i]);
		showEarthquakeSetToggle->getValueChangedCallbacks().add(this,&ShowEarthModel::settingsChangedCallback);
		}
	
	/* Create toggles for each additional point set: */
	for(unsigned int i=0;i<pointSets.size();++i)
		{
		/* Create a unique name for the toggle button widget: */
		char toggleName[256];
		snprintf(toggleName,sizeof(toggleName),"ShowPointSetToggle%04d",i);
		
		/* Create a label to display in the submenu: */
		char toggleLabel[256];
		snprintf(toggleLabel,sizeof(toggleLabel),"Show Point Set %d",i);
		
		/* Create a toggle button to render the additional point set: */
		GLMotif::ToggleButton* showPointSetToggle=new GLMotif::ToggleButton(toggleName,renderTogglesMenu,toggleLabel);
		showPointSetToggle->track(settings.showPointSets[i]);
		showPointSetToggle->getValueChangedCallbacks().add(this,&ShowEarthModel::settingsChangedCallback);
		}
	
	/* Check if there are seismic paths: */
	if(seismicPaths.size()>0)
		{
		/* Create a toggle button to render all seismic paths: */
		GLMotif::ToggleButton* showSeismicPathsToggle=new GLMotif::ToggleButton("ShowSeismicPathsToggle",renderTogglesMenu,"Show Seismic Paths");
		showSeismicPathsToggle->track(settings.showSeismicPaths);
		showSeismicPathsToggle->getValueChangedCallbacks().add(this,&ShowEarthModel::settingsChangedCallback);
		}
	
	/* Create toggles for each scene graph: */
	for(unsigned int i=0;i<sceneGraphs.size();++i)
		{
		/* Create a unique name for the toggle button widget: */
		char toggleName[256];
		snprintf(toggleName,sizeof(toggleName),"ShowSceneGraphToggle%04d",i);
		
		/* Create a label to display in the submenu: */
		char toggleLabel[256];
		snprintf(toggleLabel,sizeof(toggleLabel),"Show Scene Graph %d",i);
		
		/* Create a toggle button to render the scene graph: */
		GLMotif::ToggleButton* showSceneGraphToggle=new GLMotif::ToggleButton(toggleName,renderTogglesMenu,toggleLabel);
		showSceneGraphToggle->track(settings.showSceneGraphs[i]);
		showSceneGraphToggle->getValueChangedCallbacks().add(this,&ShowEarthModel::settingsChangedCallback);
		}
	
	/* Create a toggle button to render the outer core: */
	GLMotif::ToggleButton* showOuterCoreToggle=new GLMotif::ToggleButton("ShowOuterCoreToggle",renderTogglesMenu,"Show Outer Core");
	showOuterCoreToggle->track(settings.showOuterCore);
	showOuterCoreToggle->getValueChangedCallbacks().add(this,&ShowEarthModel::settingsChangedCallback);
	
	/* Create a toggle button to render the outer core transparently: */
	GLMotif::ToggleButton* outerCoreTransparentToggle=new GLMotif::ToggleButton("OuterCoreTransparentToggle",renderTogglesMenu,"Outer Core Transparent");
	outerCoreTransparentToggle->track(settings.outerCoreTransparent);
	outerCoreTransparentToggle->getValueChangedCallbacks().add(this,&ShowEarthModel::settingsChangedCallback);
	
	/* Create a toggle button to render the inner core: */
	GLMotif::ToggleButton* showInnerCoreToggle=new GLMotif::ToggleButton("ShowInnerCoreToggle",renderTogglesMenu,"Show Inner Core");
	showInnerCoreToggle->track(settings.showInnerCore);
	showInnerCoreToggle->getValueChangedCallbacks().add(this,&ShowEarthModel::settingsChangedCallback);
	
	/* Create a toggle button to render the inner core transparently: */
	GLMotif::ToggleButton* innerCoreTransparentToggle=new GLMotif::ToggleButton("InnerCoreTransparentToggle",renderTogglesMenu,"Inner Core Transparent");
	innerCoreTransparentToggle->track(settings.innerCoreTransparent);
	innerCoreTransparentToggle->getValueChangedCallbacks().add(this,&ShowEarthModel::settingsChangedCallback);
	
	/* Calculate the submenu's proper layout: */
	renderTogglesMenu->manageMenu();
	
	/* Return the created top-level shell: */
	return renderTogglesMenu;
	}

void ShowEarthModel::rotateEarthValueChangedCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData)
	{
	rotateEarth=cbData->set;
	if(rotateEarth)
		lastFrameTime=Vrui::getApplicationTime();
	}

void ShowEarthModel::resetRotationCallback(Misc::CallbackData* cbData)
	{
	/* Reset the Earth's rotation angle: */
	settings.rotationAngle=0.0f;
	userTransform->setRotationAngle(Vrui::Scalar(settings.rotationAngle));
	}

void ShowEarthModel::lockToSphereCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData)
	{
	if(cbData->set)
		{
		/* Calculate display center and up vector in navigation coordinates: */
		Vrui::Point center=Vrui::getInverseNavigationTransformation().transform(Vrui::getDisplayCenter());
		Vrui::Vector up=Vrui::getInverseNavigationTransformation().transform(Vrui::getUpDirection());
		
		/* Calculate current distance from Earth's center: */
		Vrui::Vector rad=center-Vrui::Point::origin;
		sphereRadius=Geometry::mag(rad);
		rad/=sphereRadius;
		
		/* Align the up direction with a radial vector from the Earth's center: */
		sphereTransform=Vrui::NavTransform::identity;
		sphereTransform*=Vrui::NavTransform::translateFromOriginTo(center);
		sphereTransform*=Vrui::NavTransform::rotate(Vrui::Rotation::rotateFromTo(rad,up));
		sphereTransform*=Vrui::NavTransform::translateToOriginFrom(center);
		
		/* Enable sphere locking: */
		lockToSphere=true;
		}
	else
		{
		/* Update the navigation transformation to reflect the current visible transformation: */
		Vrui::concatenateNavigationTransformation(sphereTransform);
		
		/* Disable sphere locking: */
		lockToSphere=false;
		}
	}

void ShowEarthModel::showRenderDialogCallback(Misc::CallbackData* cbData)
	{
	Vrui::popupPrimaryWidget(renderDialog);
	}

void ShowEarthModel::showAnimationDialogCallback(Misc::CallbackData* cbData)
	{
	Vrui::popupPrimaryWidget(animationDialog);
	}

GLMotif::PopupMenu* ShowEarthModel::createMainMenu(void)
	{
	/* Create the main menu shell: */
	GLMotif::PopupMenu* mainMenu=new GLMotif::PopupMenu("MainMenu",Vrui::getWidgetManager());
	mainMenu->setTitle("Interactive Globe");
	
	/* Create a cascade button to show the "Rendering Modes" submenu: */
	GLMotif::CascadeButton* renderTogglesCascade=new GLMotif::CascadeButton("RenderTogglesCascade",mainMenu,"Rendering Modes");
	renderTogglesCascade->setPopup(createRenderTogglesMenu());
	
	/* Create a toggle button to rotate the Earth model: */
	GLMotif::ToggleButton* rotateEarthToggle=new GLMotif::ToggleButton("RotateEarthToggle",mainMenu,"Rotate Earth");
	rotateEarthToggle->track(rotateEarth);
	rotateEarthToggle->getValueChangedCallbacks().add(this,&ShowEarthModel::rotateEarthValueChangedCallback);
	
	/* Create a button to reset the Earth model's rotation: */
	GLMotif::Button* resetRotationButton=new GLMotif::Button("ResetRotationButton",mainMenu,"Reset Rotation");
	resetRotationButton->getSelectCallbacks().add(this,&ShowEarthModel::resetRotationCallback);
	
	#if 0
	
	/* Create a toggle button to lock navigation coordinates to a fixed-radius sphere: */
	GLMotif::ToggleButton* lockToSphereToggle=new GLMotif::ToggleButton("LockToSphereToggle",mainMenu,"Lock to Sphere");
	lockToSphereToggle->setToggle(lockToSphere);
	lockToSphereToggle->getValueChangedCallbacks().add(this,&ShowEarthModel::menuToggleSelectCallback);
	
	#endif
	
	/* Create a button to show the render settings dialog: */
	GLMotif::Button* showRenderDialogButton=new GLMotif::Button("ShowRenderDialogButton",mainMenu,"Show Render Dialog");
	showRenderDialogButton->getSelectCallbacks().add(this,&ShowEarthModel::showRenderDialogCallback);
	
	/* Create a button to show the animation dialog: */
	GLMotif::Button* showAnimationDialogButton=new GLMotif::Button("ShowAnimationDialogButton",mainMenu,"Show Animation Dialog");
	showAnimationDialogButton->getSelectCallbacks().add(this,&ShowEarthModel::showAnimationDialogCallback);
	
	/* Calculate the main menu's proper layout: */
	mainMenu->manageMenu();
	
	/* Return the created top-level shell: */
	return mainMenu;
	}

void ShowEarthModel::useFogCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData)
	{
	fog=cbData->set;
	}

void ShowEarthModel::backplaneDistCallback(GLMotif::Slider::ValueChangedCallbackData* cbData)
	{
	bpDist=float(cbData->value);
	Vrui::setBackplaneDist(bpDist);
	}

GLMotif::PopupWindow* ShowEarthModel::createRenderDialog(void)
	{
	const GLMotif::StyleSheet& ss=*Vrui::getUiStyleSheet();
	
	GLMotif::PopupWindow* renderDialogPopup=new GLMotif::PopupWindow("RenderDialogPopup",Vrui::getWidgetManager(),"Display Settings");
	renderDialogPopup->setResizableFlags(true,false);
	renderDialogPopup->setCloseButton(true);
	renderDialogPopup->popDownOnClose();
	
	GLMotif::RowColumn* renderDialog=new GLMotif::RowColumn("RenderDialog",renderDialogPopup,false);
	renderDialog->setOrientation(GLMotif::RowColumn::VERTICAL);
	renderDialog->setPacking(GLMotif::RowColumn::PACK_TIGHT);
	renderDialog->setNumMinorWidgets(2);
	
	GLMotif::ToggleButton* showSurfaceToggle=new GLMotif::ToggleButton("ShowSurfaceToggle",renderDialog,"Show Surface");
	showSurfaceToggle->setBorderWidth(0.0f);
	showSurfaceToggle->setMarginWidth(0.0f);
	showSurfaceToggle->setHAlignment(GLFont::Left);
	showSurfaceToggle->track(settings.showSurface);
	showSurfaceToggle->getValueChangedCallbacks().add(this,&ShowEarthModel::settingsChangedCallback);
	
	GLMotif::ToggleButton* surfaceTransparentToggle=new GLMotif::ToggleButton("SurfaceTransparentToggle",renderDialog,"Transparent");
	surfaceTransparentToggle->setBorderWidth(0.0f);
	surfaceTransparentToggle->setMarginWidth(0.0f);
	surfaceTransparentToggle->setHAlignment(GLFont::Left);
	surfaceTransparentToggle->track(settings.surfaceTransparent);
	surfaceTransparentToggle->getValueChangedCallbacks().add(this,&ShowEarthModel::settingsChangedCallback);
	
	new GLMotif::Label("SurfaceTransparencyLabel",renderDialog,"Surface Transparency");
	
	GLMotif::Slider* surfaceTransparencySlider=new GLMotif::Slider("SurfaceTransparencySlider",renderDialog,GLMotif::Slider::HORIZONTAL,ss.fontHeight*5.0f);
	surfaceTransparencySlider->setValueRange(0.0,1.0,0.001);
	surfaceTransparencySlider->track(settings.surfaceAlpha);
	surfaceTransparencySlider->getValueChangedCallbacks().add(this,&ShowEarthModel::settingsChangedCallback);
	
	new GLMotif::Label("GridTransparencyLabel",renderDialog,"Grid Transparency");
	
	GLMotif::Slider* gridTransparencySlider=new GLMotif::Slider("GridTransparencySlider",renderDialog,GLMotif::Slider::HORIZONTAL,ss.fontHeight*5.0f);
	gridTransparencySlider->setValueRange(0.0,1.0,0.001);
	gridTransparencySlider->track(settings.gridAlpha);
	gridTransparencySlider->getValueChangedCallbacks().add(this,&ShowEarthModel::settingsChangedCallback);
	
	GLMotif::ToggleButton* showOuterCoreToggle=new GLMotif::ToggleButton("ShowOuterCoreToggle",renderDialog,"Show Outer Core");
	showOuterCoreToggle->setBorderWidth(0.0f);
	showOuterCoreToggle->setMarginWidth(0.0f);
	showOuterCoreToggle->setHAlignment(GLFont::Left);
	showOuterCoreToggle->track(settings.showOuterCore);
	showOuterCoreToggle->getValueChangedCallbacks().add(this,&ShowEarthModel::settingsChangedCallback);
	
	GLMotif::ToggleButton* outerCoreTransparentToggle=new GLMotif::ToggleButton("OuterCoreTransparentToggle",renderDialog,"Transparent");
	outerCoreTransparentToggle->setBorderWidth(0.0f);
	outerCoreTransparentToggle->setMarginWidth(0.0f);
	outerCoreTransparentToggle->setHAlignment(GLFont::Left);
	outerCoreTransparentToggle->track(settings.outerCoreTransparent);
	outerCoreTransparentToggle->getValueChangedCallbacks().add(this,&ShowEarthModel::settingsChangedCallback);
	
	new GLMotif::Label("OuterCoreTransparencyLabel",renderDialog,"Outer Core Transparency");
	
	GLMotif::Slider* outerCoreTransparencySlider=new GLMotif::Slider("OuterCoreTransparencySlider",renderDialog,GLMotif::Slider::HORIZONTAL,ss.fontHeight*5.0f);
	outerCoreTransparencySlider->setValueRange(0.0,1.0,0.001);
	outerCoreTransparencySlider->track(settings.outerCoreAlpha);
	outerCoreTransparencySlider->getValueChangedCallbacks().add(this,&ShowEarthModel::settingsChangedCallback);
	
	GLMotif::ToggleButton* showInnerCoreToggle=new GLMotif::ToggleButton("ShowInnerCoreToggle",renderDialog,"Show Inner Core");
	showInnerCoreToggle->setBorderWidth(0.0f);
	showInnerCoreToggle->setMarginWidth(0.0f);
	showInnerCoreToggle->setHAlignment(GLFont::Left);
	showInnerCoreToggle->track(settings.showInnerCore);
	showInnerCoreToggle->getValueChangedCallbacks().add(this,&ShowEarthModel::settingsChangedCallback);
	
	GLMotif::ToggleButton* innerCoreTransparentToggle=new GLMotif::ToggleButton("InnerCoreTransparentToggle",renderDialog,"Transparent");
	innerCoreTransparentToggle->setBorderWidth(0.0f);
	innerCoreTransparentToggle->setMarginWidth(0.0f);
	innerCoreTransparentToggle->setHAlignment(GLFont::Left);
	innerCoreTransparentToggle->track(settings.innerCoreTransparent);
	innerCoreTransparentToggle->getValueChangedCallbacks().add(this,&ShowEarthModel::settingsChangedCallback);
	
	new GLMotif::Label("InnerCoreTransparencyLabel",renderDialog,"Inner Core Transparency");
	
	GLMotif::Slider* innerCoreTransparencySlider=new GLMotif::Slider("InnerCoreTransparencySlider",renderDialog,GLMotif::Slider::HORIZONTAL,ss.fontHeight*5.0f);
	innerCoreTransparencySlider->setValueRange(0.0,1.0,0.001);
	innerCoreTransparencySlider->track(settings.innerCoreAlpha);
	innerCoreTransparencySlider->getValueChangedCallbacks().add(this,&ShowEarthModel::settingsChangedCallback);
	
	new GLMotif::Label("EarthquakePointSizeLabel",renderDialog,"Earthquake Point Size");
	
	GLMotif::Slider* earthquakePointSizeSlider=new GLMotif::Slider("EarthquakePointSizeSlider",renderDialog,GLMotif::Slider::HORIZONTAL,ss.fontHeight*5.0f);
	earthquakePointSizeSlider->setValueRange(1.0,10.0,0.5);
	earthquakePointSizeSlider->track(settings.earthquakePointSize);
	earthquakePointSizeSlider->getValueChangedCallbacks().add(this,&ShowEarthModel::settingsChangedCallback);
	
	GLMotif::ToggleButton* useFogToggle=new GLMotif::ToggleButton("UseFogToggle",renderDialog,"Use Fog");
	useFogToggle->setBorderWidth(0.0f);
	useFogToggle->setMarginWidth(0.0f);
	useFogToggle->setHAlignment(GLFont::Left);
	useFogToggle->setToggle(fog);
	useFogToggle->getValueChangedCallbacks().add(this,&ShowEarthModel::useFogCallback);
	
	new GLMotif::Blind("Blind4",renderDialog);
	
	new GLMotif::Label("BackplaneDistanceLabel",renderDialog,"Backplane Distance");
	
	GLMotif::Slider* backplaneDistanceSlider=new GLMotif::Slider("BackplaneDistanceSlider",renderDialog,GLMotif::Slider::HORIZONTAL,ss.fontHeight*5.0f);
	backplaneDistanceSlider->setValueRange(Vrui::getFrontplaneDist()*2.0,Vrui::getBackplaneDist()*2.0,0.0);
	backplaneDistanceSlider->setValue(Vrui::getBackplaneDist());
	backplaneDistanceSlider->getValueChangedCallbacks().add(this,&ShowEarthModel::backplaneDistCallback);
	
	renderDialog->manageChild();
	
	return renderDialogPopup;
	}

void ShowEarthModel::updateCurrentTime(void)
	{
	time_t ctT=time_t(settings.currentTime);
	struct tm ctTm;
	localtime_r(&ctT,&ctTm);
	char ctBuffer[72];
	snprintf(ctBuffer,sizeof(ctBuffer),"%04d/%02d/%02d %02d:%02d:%02d",ctTm.tm_year+1900,ctTm.tm_mon+1,ctTm.tm_mday,ctTm.tm_hour,ctTm.tm_min,ctTm.tm_sec);
	currentTimeValue->setString(ctBuffer);
	
	for(std::vector<EarthquakeSet*>::iterator esIt=earthquakeSets.begin();esIt!=earthquakeSets.end();++esIt)
		{
		(*esIt)->setHighlightTime(settings.playSpeed);
		(*esIt)->setCurrentTime(settings.currentTime);
		}
	}

void ShowEarthModel::currentTimeCallback(GLMotif::Slider::ValueChangedCallbackData* cbData)
	{
	settings.currentTime=cbData->value;
	settingsChangedCallback(0);
	updateCurrentTime();
	}

void ShowEarthModel::playSpeedCallback(GLMotif::Slider::ValueChangedCallbackData* cbData)
	{
	settings.playSpeed=Math::pow(10.0,double(cbData->value));
	playSpeedValue->setValue(Math::log10(settings.playSpeed));
	currentTimeSlider->setValueRange(earthquakeTimeRange.getMin()-settings.playSpeed,earthquakeTimeRange.getMax()+settings.playSpeed,settings.playSpeed);
	updateCurrentTime();
	}

GLMotif::PopupWindow* ShowEarthModel::createAnimationDialog(void)
	{
	const GLMotif::StyleSheet& ss=*Vrui::getUiStyleSheet();
	
	GLMotif::PopupWindow* animationDialogPopup=new GLMotif::PopupWindow("AnimationDialogPopup",Vrui::getWidgetManager(),"Animation");
	animationDialogPopup->setResizableFlags(true,false);
	animationDialogPopup->setCloseButton(true);
	animationDialogPopup->popDownOnClose();
	
	GLMotif::RowColumn* animationDialog=new GLMotif::RowColumn("AnimationDialog",animationDialogPopup,false);
	animationDialog->setNumMinorWidgets(3);
	
	new GLMotif::Label("CurrentTimeLabel",animationDialog,"Current Time");
	
	currentTimeValue=new GLMotif::TextField("CurrentTimeValue",animationDialog,19);
	updateCurrentTime();
	
	currentTimeSlider=new GLMotif::Slider("CurrentTimeSlider",animationDialog,GLMotif::Slider::HORIZONTAL,ss.fontHeight*15.0f);
	currentTimeSlider->setValueRange(earthquakeTimeRange.getMin()-settings.playSpeed,earthquakeTimeRange.getMax()+settings.playSpeed,settings.playSpeed);
	currentTimeSlider->setValue(settings.currentTime);
	currentTimeSlider->getValueChangedCallbacks().add(this,&ShowEarthModel::currentTimeCallback);
	
	new GLMotif::Label("PlaySpeedLabel",animationDialog,"Playback Speed");
	
	playSpeedValue=new GLMotif::TextField("PlaySpeedValue",animationDialog,6);
	playSpeedValue->setFieldWidth(6);
	playSpeedValue->setPrecision(3);
	playSpeedValue->setValue(Math::log10(settings.playSpeed));
	
	playSpeedSlider=new GLMotif::Slider("PlaySpeedSlider",animationDialog,GLMotif::Slider::HORIZONTAL,ss.fontHeight*10.0f);
	playSpeedSlider->setValueRange(0.0,9.0,0.1);
	playSpeedSlider->setValue(Math::log10(settings.playSpeed));
	playSpeedSlider->getValueChangedCallbacks().add(this,&ShowEarthModel::playSpeedCallback);
	
	playToggle=new GLMotif::ToggleButton("PlayToggle",animationDialog,"Playback");
	playToggle->track(play);
	
	animationDialog->manageChild();
	
	return animationDialogPopup;
	}

GLPolylineTube* ShowEarthModel::readSensorPathFile(const char* sensorPathFileName,double scaleFactor)
	{
	/* Open the file: */
	Misc::File sensorPathFile(sensorPathFileName,"rt");
	
	/* Read the file header: */
	unsigned int numSamples;
	char line[256];
	while(true)
		{
		/* Read next line from file: */
		sensorPathFile.gets(line,sizeof(line));
		
		/* Parse header line: */
		if(strncmp(line,"PROF_ID=",8)==0) // This line contains number of samples
			{
			if(sscanf(line+8,"%u",&numSamples)!=1)
				Misc::throwStdErr("ShowEarthModel::readSensorPathFile: Unable to parse number of samples in sensor path file %s",sensorPathFileName);
			}
		else if(strncmp(line,"NUMOBS=",7)==0) // This line marks the end of the header (for now)
			break;
		}
	
	/* Create the result sensor path: */
	GLPolylineTube* result=new GLPolylineTube(0.1f,numSamples);
	result->setNumTubeSegments(12);
	
	/* Read the samples: */
	GLPolylineTube::Point lastPos=GLPolylineTube::Point::origin;
	for(unsigned int i=0;i<numSamples;++i)
		{
		/* Read next line from file: */
		sensorPathFile.gets(line,sizeof(line));
		
		/* Get the next sample: */
		float lon,lat,depth,value;
		
		/* Parse the sample's values from the line just read: */
		if(sscanf(line,"%f %f %f %f",&lon,&lat,&depth,&value)!=4)
			Misc::throwStdErr("ShowEarthModel::readSensorPathFile: Error while reading sensor path file %s",sensorPathFileName);
		
		/* Convert position to Cartesian: */
		GLPolylineTube::Point pos;
		calcDepthPos(Math::rad(lat),Math::rad(lon),depth*1000.0f,scaleFactor,pos.getComponents());
		
		/* Store sample point: */
		if(i==0||pos!=lastPos)
			result->addVertex(pos);
		lastPos=pos;
		}
	
	return result;
	}

ShowEarthModel::ShowEarthModel(int& argc,char**& argv)
	:Vrui::Application(argc,argv),
	 geoid(Geoid::getDefaultRadius()*Vrui::Scalar(0.001),Geoid::getDefaultFlatteningFactor()), // Use default WGS84 geoid in kilometers
	 #if USE_COLLABORATION
	 koinonia(0),
	 #endif
	 scaleToEnvironment(true),
	 rotateEarth(true),
	 lastFrameTime(0.0),rotationSpeed(5.0f),
	 userTransform(0),
	 surfaceMaterial(GLMaterial::Color(1.0f,1.0f,1.0f),GLMaterial::Color(0.333f,0.333f,0.333f),10.0f),
	 outerCoreMaterial(GLMaterial::Color(1.0f,0.5f,0.0f),GLMaterial::Color(1.0f,1.0f,1.0f),50.0f),
	 innerCoreMaterial(GLMaterial::Color(1.0f,0.0f,0.0f),GLMaterial::Color(1.0f,1.0f,1.0f),50.0f),
	 sensorPathMaterial(GLMaterial::Color(1.0f,1.0f,0.0f),GLMaterial::Color(1.0f,1.0f,1.0f),50.0f),
	 fog(false),bpDist(float(Vrui::getBackplaneDist())),
	 lockToSphere(false),
	 mainMenu(0),renderDialog(0),animationDialog(0)
	{
	/* Set default rendering settings: */
	settings.rotationAngle=0.0f;
	settings.showSurface=true;
	settings.surfaceTransparent=false;
	settings.surfaceAlpha=0.333f;
	settings.showGrid=true;
	settings.gridAlpha=0.1f;
	for(int i=0;i<Settings::maxNumObjectFlags;++i)
		{
		settings.showEarthquakeSets[i]=false;
		settings.showPointSets[i]=false;
		settings.showSceneGraphs[i]=false;
		}
	settings.showSeismicPaths=false;
	settings.showOuterCore=false;
	settings.outerCoreTransparent=true;
	settings.outerCoreAlpha=0.333f;
	settings.showInnerCore=false;
	settings.innerCoreTransparent=true;
	settings.innerCoreAlpha=0.333f;
	settings.earthquakePointSize=3.0f;
	settings.playSpeed=365.0*24.0*60.0*60.0; // One second per year
	settings.currentTime=0.0;
	
	/* Load initial render settings from a configuration file: */
	bool showEarthquakes=false;
	try
		{
		/* Open the configuration file: */
		std::string configFileName=SHOWEARTHMODEL_CONFIGDIR;
		configFileName.push_back('/');
		configFileName.append(SHOWEARTHMODEL_APPNAME);
		configFileName.append(".cfg");
		Misc::ConfigurationFile configFile(configFileName.c_str());
		Misc::ConfigurationFileSection cfg=configFile.getSection(SHOWEARTHMODEL_APPNAME);
		
		/* Read rendering settings: */
		settings.showSurface=cfg.retrieveValue<bool>("./showSurface",settings.showSurface);
		settings.surfaceTransparent=cfg.retrieveValue<bool>("./surfaceTransparent",settings.surfaceTransparent);
		settings.surfaceAlpha=1.0f-cfg.retrieveValue<GLfloat>("./surfaceTransparency",1.0f-settings.surfaceAlpha);
		settings.showGrid=cfg.retrieveValue<bool>("./showGrid",settings.showGrid);
		settings.showOuterCore=cfg.retrieveValue<bool>("./showOuterCode",settings.showOuterCore);
		settings.outerCoreTransparent=cfg.retrieveValue<bool>("./outerCoreTransparent",settings.outerCoreTransparent);
		settings.outerCoreAlpha=1.0f-cfg.retrieveValue<GLfloat>("./outerCoreTransparency",1.0f-settings.outerCoreAlpha);
		settings.showInnerCore=cfg.retrieveValue<bool>("./showInnerCode",settings.showInnerCore);
		settings.innerCoreTransparent=cfg.retrieveValue<bool>("./innerCoreTransparent",settings.innerCoreTransparent);
		settings.innerCoreAlpha=1.0f-cfg.retrieveValue<GLfloat>("./innerCoreTransparency",1.0f-settings.innerCoreAlpha);
		settings.earthquakePointSize=cfg.retrieveValue<float>("./earthquakePointSize",settings.earthquakePointSize);
		showEarthquakes=cfg.retrieveValue<bool>("./showEarthquakes",showEarthquakes);
		}
	catch(const std::runtime_error&)
		{
		/* Just ignore the error */
		}
	
	/* Initialize rendering materials: */
	surfaceMaterial.diffuse[3]=settings.surfaceAlpha;
	outerCoreMaterial.diffuse[3]=settings.surfaceAlpha;
	innerCoreMaterial.diffuse[3]=settings.surfaceAlpha;
	
	/* Parse the command line: */
	enum FileMode
		{
		POINTSETFILE,EARTHQUAKESETFILE,SEISMICPATHFILE,SENSORPATHFILE,SCENEGRAPHFILE
		} fileMode=POINTSETFILE; // Treat all file names as point set files initially
	float colorMask[3]={1.0f,1.0f,1.0f}; // Initial color mask for point set files
	
	/* Create an initial color map for event magnitudes: */
	static const GLColorMap::Color magnitudeColors[]=
		{
		GLColorMap::Color(0.0f,1.0f,0.0f), // Magnitude 5
		GLColorMap::Color(0.0f,1.0f,1.0f), // Magnitude 6
		GLColorMap::Color(0.0f,0.0f,1.0f), // Magnitude 7
		GLColorMap::Color(1.0f,0.0f,1.0f), // Magnitude 8
		GLColorMap::Color(1.0f,0.0f,0.0f)  // Magnitude 9
		};
	static const GLdouble magnitudeKeys[]={5.0,6.0,7.0,8.0,9.0};
	GLColorMap magnitudeColorMap(5,magnitudeColors,magnitudeKeys,5);
	
	SceneGraph::NodeCreator* sceneGraphNodeCreator=0;
	for(int i=1;i<argc;++i)
		{
		/* Check if the current command line argument is a switch or a file name: */
		if(argv[i][0]=='-')
			{
			/* Determine the kind of switch and change the file mode: */
			if(strcasecmp(argv[i]+1,"points")==0)
				fileMode=POINTSETFILE; // Treat all following file names as point set files
			else if(strcasecmp(argv[i]+1,"quakes")==0)
				fileMode=EARTHQUAKESETFILE; // Treat all following file names as earthquake set files
			else if(strcasecmp(argv[i]+1,"seismicpath")==0)
				fileMode=SEISMICPATHFILE; // Treat all following file names as seismic path files
			else if(strcasecmp(argv[i]+1,"sensorpath")==0)
				fileMode=SENSORPATHFILE; // Treat all following file names as sensor path files
			else if(strcasecmp(argv[i]+1,"scenegraph")==0)
				fileMode=SCENEGRAPHFILE; // Treat all following file names as scene graph files
			else if(strcasecmp(argv[i]+1,"rotate")==0)
				rotateEarth=true;
			else if(strcasecmp(argv[i]+1,"norotate")==0)
				rotateEarth=false;
			else if(strcasecmp(argv[i]+1,"scale")==0)
				scaleToEnvironment=true;
			else if(strcasecmp(argv[i]+1,"noscale")==0)
				scaleToEnvironment=false;
			else if(strcasecmp(argv[i]+1,"pointsize")==0)
				{
				++i;
				settings.earthquakePointSize=float(atof(argv[i]));
				}
			else if(strcasecmp(argv[i]+1,"color")==0)
				{
				for(int j=0;j<3;++j)
					{
					++i;
					colorMask[j]=float(atof(argv[i]));
					}
				}
			else if(strcasecmp(argv[i]+1,"fog")==0)
				fog=true;
			else if(strcasecmp(argv[i]+1,"bpdist")==0)
				{
				++i;
				bpDist=float(atof(argv[i]));
				Vrui::setBackplaneDist(bpDist);
				}
			else
				std::cout<<"Unrecognized switch "<<argv[i]<<std::endl;
			}
		else
			{
			/* Load the file of the given name using the current file mode: */
			switch(fileMode)
				{
				case POINTSETFILE:
					{
					/* Load a point set: */
					PointSet* pointSet=new PointSet(argv[i],1.0e-3,colorMask);
					settings.showPointSets[pointSets.size()]=false;
					pointSets.push_back(pointSet);
					break;
					}
				
				case EARTHQUAKESETFILE:
					{
					/* Load an earthquake set: */
					EarthquakeSet* earthquakeSet=new EarthquakeSet(IO::Directory::getCurrent(),argv[i],geoid,Geometry::Vector<double,3>::zero,magnitudeColorMap);
					settings.showEarthquakeSets[earthquakeSets.size()]=showEarthquakes;
					earthquakeSets.push_back(earthquakeSet);
					
					/* Enable layered rendering on the earthquake set: */
					earthquakeSet->enableLayeredRendering(EarthquakeSet::Point::origin); // Earth's center is at the origin
					break;
					}
				
				case SEISMICPATHFILE:
					{
					/* Load a seismic path: */
					SeismicPath* path=new SeismicPath(argv[i],1.0e-3);
					seismicPaths.push_back(path);
					break;
					}
				
				case SENSORPATHFILE:
					{
					/* Load a sensor path: */
					GLPolylineTube* path=readSensorPathFile(argv[i],1.0e-3);
					sensorPaths.push_back(path);
					break;
					}
				
				case SCENEGRAPHFILE:
					{
					/* Create a node creator if there isn't one already: */
					if(sceneGraphNodeCreator==0)
						sceneGraphNodeCreator=new SceneGraph::NodeCreator;
					
					/* Create the scene graph's root node: */
					SceneGraph::GroupNode* root=new SceneGraph::GroupNode;
					root->ref();
					
					/* Load the VRML file: */
					try
						{
						SceneGraph::VRMLFile vrmlFile(argv[i],*sceneGraphNodeCreator);
						vrmlFile.parse(*root);
						
						/* Store the scene graph's root node: */
						settings.showSceneGraphs[sceneGraphs.size()]=false;
						sceneGraphs.push_back(root);
						}
					catch(const std::runtime_error& err)
						{
						/* Print an error message and ignore the scene graph: */
						std::cerr<<"Ignoring scene graph file "<<argv[i]<<" due to exception "<<err.what()<<std::endl;
						delete root;
						}
					break;
					}
				}
			}
		}
	
	/* Calculate the time range of all earthquake events: */
	if(!earthquakeSets.empty())
		{
		earthquakeTimeRange=EarthquakeSet::TimeRange::empty;
		for(std::vector<EarthquakeSet*>::const_iterator esIt=earthquakeSets.begin();esIt!=earthquakeSets.end();++esIt)
			earthquakeTimeRange.addInterval((*esIt)->getTimeRange());
		}
	else
		earthquakeTimeRange=EarthquakeSet::TimeRange(0.0,0.0);
	
	/* Initialize the earthquake animation: */
	settings.currentTime=earthquakeTimeRange.getMin()-settings.playSpeed;
	play=false;
	for(std::vector<EarthquakeSet*>::iterator esIt=earthquakeSets.begin();esIt!=earthquakeSets.end();++esIt)
		{
		(*esIt)->setHighlightTime(settings.playSpeed);
		(*esIt)->setCurrentTime(settings.currentTime);
		}
	
	delete sceneGraphNodeCreator;
	
	/* Create the default surface image file name: */
	std::string topographyFileName=SHOWEARTHMODEL_IMAGEDIR;
	#if IMAGES_CONFIG_HAVE_PNG
	topographyFileName.append("/EarthTopography.png");
	#else
	topographyFileName.append("/EarthTopography.ppm");
	#endif
	
	/* Load the Earth surface texture image from an image file: */
	surfaceImage=Images::readGenericImageFile(topographyFileName.c_str());
	
	#if 0
	/* Reduce the Earth surface texture's saturation to make it 3D-friendly: */
	for(unsigned int y=0;y<surfaceImage.getHeight();++y)
		{
		Images::RGBImage::Color* rowPtr=surfaceImage.modifyPixelRow(y);
		for(unsigned int x=0;x<earthTexture.getWidth();++x)
			{
			float value=float(rowPtr[x][0])*0.299f+float(rowPtr[x][1])*0.587f+float(rowPtr[x][2])*0.114f;
			for(int i=0;i<3;++i)
				{
				float newC=(float(rowPtr[x][i])-value)*0.25f+value;
				if(newC<0.5f)
					rowPtr[x][i]=0;
				else if(newC>=254.5f)
					rowPtr[x][i]=255;
				else
					rowPtr[x][i]=Images::RGBImage::Color::Scalar(newC+0.5f);
				}
			}
		}
	#endif
	
	/* Create the user interface: */
	mainMenu=createMainMenu();
	Vrui::setMainMenu(mainMenu);
	renderDialog=createRenderDialog();
	animationDialog=createAnimationDialog();
	
	if(!earthquakeSets.empty())
		{
		/* Register the custom tool classes with the Vrui tool manager: */
		EarthquakeToolFactory* earthquakeToolFactory=new EarthquakeToolFactory(*Vrui::getToolManager(),earthquakeSets);
		Vrui::getToolManager()->addClass(earthquakeToolFactory,EarthquakeToolFactory::factoryDestructor);
		EarthquakeQueryToolFactory* earthquakeQueryToolFactory=new EarthquakeQueryToolFactory(*Vrui::getToolManager(),earthquakeSets,Misc::createFunctionCall(this,&ShowEarthModel::setEventTime));
		Vrui::getToolManager()->addClass(earthquakeQueryToolFactory,EarthquakeQueryToolFactory::factoryDestructor);
		}
	
	/* Set the navigational coordinate system unit: */
	Vrui::getCoordinateManager()->setUnit(Geometry::LinearUnit(Geometry::LinearUnit::KILOMETER,1));
	
	/* Register a geodetic coordinate transformer with Vrui's coordinate manager: */
	userTransform=new RotatedGeodeticCoordinateTransform;
	Vrui::getCoordinateManager()->setCoordinateTransform(userTransform); // userTransform now owned by coordinate manager
	
	#if USE_COLLABORATION
	/* Check if there is a collaboration client: */
	Collab::Client* client=Collab::Client::getTheClient();
	if(client!=0)
		{
		/* Request a Koinonia client to share the array of enabled flags: */
		koinonia=static_cast<KoinoniaClient*>(client->requestPluginProtocol("Koinonia"));
		
		/* Create a data type to represent the settings structure: */
		DataType settingsType;
		DataType::TypeID flagArrayTypeId=settingsType.createFixedArray(Settings::maxNumObjectFlags,DataType::Bool);
		
		DataType::StructureElement settingsElements[]=
			{
			{DataType::Float32,offsetof(Settings,rotationAngle)},
			{DataType::Bool,offsetof(Settings,showSurface)},
			{DataType::Bool,offsetof(Settings,surfaceTransparent)},
			{DataType::Float32,offsetof(Settings,surfaceAlpha)},
			{DataType::Bool,offsetof(Settings,showGrid)},
			{DataType::Float32,offsetof(Settings,gridAlpha)},
			{flagArrayTypeId,offsetof(Settings,showEarthquakeSets)},
			{flagArrayTypeId,offsetof(Settings,showPointSets)},
			{flagArrayTypeId,offsetof(Settings,showSceneGraphs)},
			{DataType::Bool,offsetof(Settings,showSeismicPaths)},
			{DataType::Bool,offsetof(Settings,showOuterCore)},
			{DataType::Bool,offsetof(Settings,outerCoreTransparent)},
			{DataType::Float32,offsetof(Settings,outerCoreAlpha)},
			{DataType::Bool,offsetof(Settings,showInnerCore)},
			{DataType::Bool,offsetof(Settings,innerCoreTransparent)},
			{DataType::Float32,offsetof(Settings,innerCoreAlpha)},
			{DataType::Float32,offsetof(Settings,earthquakePointSize)},
			{DataType::Float64,offsetof(Settings,playSpeed)},
			{DataType::Float64,offsetof(Settings,currentTime)}
			};
		DataType::TypeID settingsTypeId=settingsType.createStructure(18,settingsElements,sizeof(Settings));
		
		/* Share the settings structure: */
		settingsId=koinonia->shareObject("ShowEarthModel.settings",settingsType,settingsTypeId,&settings,&ShowEarthModel::settingsUpdatedCallback,this);
		}
	#endif
	}

ShowEarthModel::~ShowEarthModel(void)
	{
	/* Delete all earthquake sets: */
	for(std::vector<EarthquakeSet*>::iterator esIt=earthquakeSets.begin();esIt!=earthquakeSets.end();++esIt)
		delete *esIt;
	
	/* Delete all point sets: */
	for(std::vector<PointSet*>::iterator psIt=pointSets.begin();psIt!=pointSets.end();++psIt)
		delete *psIt;
	
	/* Delete all seismic paths: */
	for(std::vector<SeismicPath*>::iterator spIt=seismicPaths.begin();spIt!=seismicPaths.end();++spIt)
		delete *spIt;
	
	/* Delete all sensor paths: */
	for(std::vector<GLPolylineTube*>::iterator spIt=sensorPaths.begin();spIt!=sensorPaths.end();++spIt)
		delete *spIt;
	
	/* Delete all scene graphs: */
	for(std::vector<SceneGraph::GroupNode*>::iterator sgIt=sceneGraphs.begin();sgIt!=sceneGraphs.end();++sgIt)
		delete *sgIt;
	
	/* Delete the user interface: */
	delete mainMenu;
	delete renderDialog;
	delete animationDialog;
	}

void ShowEarthModel::initContext(GLContextData& contextData) const
	{
	/* Create a new context data item: */
	DataItem* dataItem=new DataItem();
	contextData.addDataItem(this,dataItem);
	
	/* Select the Earth surface texture object: */
	glBindTexture(GL_TEXTURE_2D,dataItem->surfaceTextureObjectId);
	
	/* Upload the Earth surface texture image: */
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_BASE_LEVEL,0);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,0);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	surfaceImage.glTexImage2D(GL_TEXTURE_2D,0,GL_RGB);
	
	/* Protect the Earth surface texture object: */
	glBindTexture(GL_TEXTURE_2D,0);
	
	/* Create the Earth surface display list: */
	glNewList(dataItem->displayListIdBase+0,GL_COMPILE);
	if(dataItem->hasVertexBufferObjectExtension)
		drawEarth(90,180,1.0e-3,dataItem->surfaceVertexBufferObjectId,dataItem->surfaceIndexBufferObjectId);
	else
		drawEarth(90,180,1.0e-3);
	glEndList();
	
	/* Create the lat/long grid display list: */
	glNewList(dataItem->displayListIdBase+1,GL_COMPILE);
	drawGrid(18,36,10,1.0e-3); // Grid lines every ten degrees, with ten intermediate points
	glEndList();
	
	/* Create the outer core display list: */
	glNewList(dataItem->displayListIdBase+2,GL_COMPILE);
	glDrawSphereIcosahedron(3480.0f,8);
	glEndList();
	
	/* Create the inner core display list: */
	glNewList(dataItem->displayListIdBase+3,GL_COMPILE);
	glDrawSphereIcosahedron(1221.0f,8);
	glEndList();
	}

void ShowEarthModel::toolCreationCallback(Vrui::ToolManager::ToolCreationCallbackData* cbData)
	{
	/* Check if the new tool is a surface navigation tool: */
	Vrui::SurfaceNavigationTool* surfaceNavigationTool=dynamic_cast<Vrui::SurfaceNavigationTool*>(cbData->tool);
	if(surfaceNavigationTool!=0)
		{
		/* Set the new tool's alignment function: */
		surfaceNavigationTool->setAlignFunction(Misc::createFunctionCall(this,&ShowEarthModel::alignSurfaceFrame));
		}
	}

void ShowEarthModel::frame(void)
	{
	/* Get the current application time: */
	double newFrameTime=Vrui::getApplicationTime();
	
	#if USE_COLLABORATION
	bool settingsUpdated=false;
	#endif
	
	/* Check if Earth animation is enabled: */
	if(rotateEarth)
		{
		/* Update the rotation angle: */
		settings.rotationAngle+=rotationSpeed*float(newFrameTime-lastFrameTime);
		if(settings.rotationAngle>=360.0f)
			settings.rotationAngle-=360.0f;
		userTransform->setRotationAngle(Vrui::Scalar(settings.rotationAngle));
		#if USE_COLLABORATION
		settingsUpdated=true;
		#endif
		
		/* Request another frame: */
		Vrui::scheduleUpdate(Vrui::getNextAnimationTime());
		}
	
	/* Animate the earthquake sets: */
	if(play)
		{
		settings.currentTime+=settings.playSpeed*(newFrameTime-lastFrameTime);
		if(settings.currentTime>=earthquakeTimeRange.getMax()+settings.playSpeed)
			{
			settings.currentTime=earthquakeTimeRange.getMin()-settings.playSpeed;
			play=false;
			playToggle->setToggle(false);
			}
		updateCurrentTime();
		currentTimeSlider->setValue(settings.currentTime);
		#if USE_COLLABORATION
		settingsUpdated=true;
		#endif
		
		/* Request another frame: */
		Vrui::scheduleUpdate(Vrui::getNextAnimationTime());
		}
	
	/* Align the navigation transformation with a sphere of fixed radius: */
	if(lockToSphere)
		{
		/* Calculate display center and up vector in locked navigation coordinates: */
		Vrui::Point center=Vrui::getInverseNavigationTransformation().transform(Vrui::getDisplayCenter());
		center=sphereTransform.inverseTransform(center);
		Vrui::Vector up=Vrui::getInverseNavigationTransformation().transform(Vrui::getUpDirection());
		up=sphereTransform.inverseTransform(up);
		up.normalize();
		
		/* Calculate current distance from Earth's center: */
		Vrui::Vector rad=center-Vrui::Point::origin;
		Vrui::Scalar radius=Geometry::mag(rad);
		rad/=radius;
		
		/* Align the up direction with a radial vector from the Earth's center: */
		sphereTransform*=Vrui::NavTransform::translateFromOriginTo(center);
		sphereTransform*=Vrui::NavTransform::rotate(Vrui::Rotation::rotateFromTo(rad,up));
		sphereTransform*=Vrui::NavTransform::translateToOriginFrom(center);
		sphereTransform*=Vrui::NavTransform::translate(up*(radius-sphereRadius));
		sphereTransform.renormalize();
		}
	
	/* Store the current application time: */
	lastFrameTime=newFrameTime;
		
	#if USE_COLLABORATION
	if(settingsUpdated&&koinonia!=0)
		{
		/* Share the new render settings with the server: */
		koinonia->replaceSharedObject(settingsId);
		}
	#endif
	}

void ShowEarthModel::display(GLContextData& contextData) const
	{
	#if 0
	/* Print the modelview and projection matrices: */
	GLdouble mv[16],p[16];
	glGetDoublev(GL_MODELVIEW_MATRIX,mv);
	glGetDoublev(GL_PROJECTION_MATRIX,p);
	
	for(int i=0;i<4;++i)
		{
		for(int j=0;j<4;++j)
			std::cout<<" "<<std::setw(12)<<mv[i+j*4];
		#if 0
		std::cout<<"        ";
		for(int j=0;j<4;++j)
			std::cout<<" "<<std::setw(12)<<p[i+j*4];
		#endif
		std::cout<<std::endl;
		}
	std::cout<<std::endl;
	#endif
	
	/* Get context data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Save OpenGL state: */
	glPushAttrib(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_ENABLE_BIT|GL_LIGHTING_BIT|GL_POLYGON_BIT);
	
	if(lockToSphere)
		{
		glPushMatrix();
		glMultMatrix(sphereTransform);
		}
	
	#if CLIP_SCREEN
	/* Add a clipping plane in the screen plane: */
	Vrui::VRScreen* screen=Vrui::getMainScreen();
	Vrui::ONTransform screenT=screen->getScreenTransformation();
	Vrui::Vector screenNormal=Vrui::getInverseNavigationTransformation().transform(screenT.getDirection(2));
	Vrui::Scalar screenOffset=screenNormal*Vrui::getInverseNavigationTransformation().transform(screenT.getOrigin());
	GLdouble cuttingPlane[4];
	for(int i=0;i<3;++i)
		cuttingPlane[i]=screenNormal[i];
	cuttingPlane[3]=-screenOffset;
	glEnable(GL_CLIP_PLANE0);
	glClipPlane(GL_CLIP_PLANE0,cuttingPlane);
	#endif
	
	/* Rotate all 3D models by the Earth rotation angle: */
	glPushMatrix();
	glRotate(settings.rotationAngle,0.0f,0.0f,1.0f);
	
	/* Calculate the scaled point size and eye position for this frustum: */
	glPointSize(settings.earthquakePointSize);
	GLFrustum<float> frustum;
	frustum.setFromGL();
	float pointRadius=settings.earthquakePointSize*float(Vrui::getUiSize())*0.1f;
	pointRadius*=frustum.getPixelSize()/frustum.getEyeScreenDistance();
	EarthquakeSet::Point eyePos1=Vrui::getHeadPosition();
	EarthquakeSet::Point::Scalar rac=Math::cos(Math::rad(EarthquakeSet::Point::Scalar(settings.rotationAngle)));
	EarthquakeSet::Point::Scalar ras=Math::sin(Math::rad(EarthquakeSet::Point::Scalar(settings.rotationAngle)));
	EarthquakeSet::Point eyePos(eyePos1[0]*rac+eyePos1[1]*ras,-eyePos1[0]*ras+eyePos1[1]*rac,eyePos1[2]);
	
	if(fog)
		{
		/* Enable fog: */
		glEnable(GL_FOG);
		glFogi(GL_FOG_MODE,GL_LINEAR);
		
		/* Calculate the minimum and maximum distance values: */
		float centerDist=-(1.0f/frustum.getEyeScreenDistance()-frustum.getScreenPlane().calcDistance(GLFrustum<float>::Point::origin))*Vrui::getNavigationTransformation().getScaling();
		float radius=float(6378.137*Vrui::getNavigationTransformation().getScaling());
		// std::cout<<centerDist<<", "<<radius<<std::endl;
		glFogf(GL_FOG_START,centerDist-radius);
		glFogf(GL_FOG_END,centerDist+radius);
		glFogfv(GL_FOG_COLOR,Vrui::getBackgroundColor().getRgba());
		}
	
	/* Render all opaque surfaces: */
	glDisable(GL_CULL_FACE);
	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,GL_TRUE);
	if(settings.showSurface&&!settings.surfaceTransparent)
		{
		/* Set up OpenGL to render the Earth's surface: */
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D,dataItem->surfaceTextureObjectId);
		glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
		glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL,GL_SEPARATE_SPECULAR_COLOR);
		glMaterial(GLMaterialEnums::FRONT_AND_BACK,surfaceMaterial);
		
		/* Call the Earth surface display list: */
		glCallList(dataItem->displayListIdBase+0);
		
		/* Reset OpenGL state: */
		glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL,GL_SINGLE_COLOR);
		glBindTexture(GL_TEXTURE_2D,0);
		glDisable(GL_TEXTURE_2D);
		}
	if(settings.showOuterCore&&!settings.outerCoreTransparent)
		{
		/* Set up OpenGL to render the outer core: */
		glMaterial(GLMaterialEnums::FRONT_AND_BACK,outerCoreMaterial);
		
		/* Call the outer core's display list: */
		glCallList(dataItem->displayListIdBase+2);
		}
	if(settings.showInnerCore&&!settings.innerCoreTransparent)
		{
		/* Set up OpenGL to render the inner core: */
		glMaterial(GLMaterialEnums::FRONT_AND_BACK,innerCoreMaterial);
		
		/* Call the inner core's display list: */
		glCallList(dataItem->displayListIdBase+3);
		}
	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,GL_FALSE);
	glEnable(GL_CULL_FACE);
	
	/* Render all sensor paths: */
	glMaterial(GLMaterialEnums::FRONT,sensorPathMaterial);
	for(std::vector<GLPolylineTube*>::const_iterator spIt=sensorPaths.begin();spIt!=sensorPaths.end();++spIt)
		(*spIt)->glRenderAction(contextData);
	
	if(!sceneGraphs.empty())
		{
		/* Save OpenGL state: */
		glPushAttrib(GL_ENABLE_BIT|GL_LIGHTING_BIT|GL_TEXTURE_BIT);
		
		/* Calculate the navigation-relative sphere transformation: */
		Vrui::NavTransform t=Vrui::NavTransform::rotate(Vrui::Rotation::rotateZ(Math::rad(settings.rotationAngle)));
		if(lockToSphere)
			t.leftMultiply(sphereTransform);
		
		/* Save the modelview matrix: */
		glPushMatrix();
		
		/* Create a render state to traverse the scene graph: */
		SceneGraph::GLRenderState* renderState=Vrui::createRenderState(t,true,contextData);
		
		/* Render all scene graphs: */
		for(unsigned int i=0;i<sceneGraphs.size();++i)
			if(settings.showSceneGraphs[i])
				sceneGraphs[i]->glRenderAction(*renderState);
		
		/* Delete the render state: */
		delete renderState;
		
		/* Restore the modelview matrix: */
		glPopMatrix();
		
		/* Reset OpenGL state: */
		glPopAttrib();
		}
	
	/* Disable lighting to render point/line models: */
	glDisable(GL_LIGHTING);
	
	/* Render all additional point sets: */
	static const GLColor<GLfloat,3> pointSetColors[14]=
		{
		GLColor<GLfloat,3>(1.0f,0.0f,0.0f),
		GLColor<GLfloat,3>(1.0f,1.0f,0.0f),
		GLColor<GLfloat,3>(0.0f,1.0f,0.0f),
		GLColor<GLfloat,3>(0.5f,0.5f,0.5f),
		GLColor<GLfloat,3>(0.0f,0.0f,1.0f),
		GLColor<GLfloat,3>(1.0f,0.0f,1.0f),
		GLColor<GLfloat,3>(0.7f,0.7f,0.7f),
		GLColor<GLfloat,3>(1.0f,0.5f,0.5f),
		GLColor<GLfloat,3>(1.0f,1.0f,0.5f),
		GLColor<GLfloat,3>(0.5f,1.0f,0.5f),
		GLColor<GLfloat,3>(0.5f,1.0f,1.0f),
		GLColor<GLfloat,3>(0.5f,0.5f,1.0f),
		GLColor<GLfloat,3>(1.0f,0.5f,1.0f),
		GLColor<GLfloat,3>(0.0f,0.0f,0.0f)
		};
	glPointSize(3.0f);
	for(unsigned int i=0;i<pointSets.size();++i)
		if(settings.showPointSets[i])
			{
			/* Set the color for the point set: */
			glColor(pointSetColors[i%14]);
			
			/* Render the point set: */
			pointSets[i]->glRenderAction(contextData);
			}
	glPointSize(1.0f);
	
	/* Render all seismic paths: */
	if(settings.showSeismicPaths)
		{
		glLineWidth(1.0f);
		glColor3f(1.0f,1.0f,1.0f);
		for(std::vector<SeismicPath*>::const_iterator pIt=seismicPaths.begin();pIt!=seismicPaths.end();++pIt)
			(*pIt)->glRenderAction(contextData);
		}
	
	/* Enable lighting again to render transparent surfaces: */
	glEnable(GL_LIGHTING);
	
	/* Render transparent surfaces in back-to-front order: */
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);
	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,GL_TRUE);
	
	/* Render back parts of surfaces: */
	glCullFace(GL_FRONT);
	if(settings.showSurface&&settings.surfaceTransparent)
		{
		/* Set up OpenGL to render the Earth's surface: */
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D,dataItem->surfaceTextureObjectId);
		glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
		glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL,GL_SEPARATE_SPECULAR_COLOR);
		glMaterial(GLMaterialEnums::FRONT_AND_BACK,surfaceMaterial);
		
		/* Call the Earth surface display list: */
		glCallList(dataItem->displayListIdBase+0);
		
		/* Reset OpenGL state: */
		glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL,GL_SINGLE_COLOR);
		glBindTexture(GL_TEXTURE_2D,0);
		glDisable(GL_TEXTURE_2D);
		}
	if(settings.showGrid)
		{
		glDisable(GL_LIGHTING);
		glBlendFunc(GL_SRC_ALPHA,GL_ONE);
		glLineWidth(1.0f);
		glColor4f(0.0f,1.0f,0.0f,settings.gridAlpha);
		
		/* Call the lat/long grid display list: */
		glCallList(dataItem->displayListIdBase+1);
		
		glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_LIGHTING);
		}
	
	/* Draw earthquakes behind the outer core: */
	glDisable(GL_LIGHTING);
	for(unsigned int i=0;i<earthquakeSets.size();++i)
		if(settings.showEarthquakeSets[i])
			earthquakeSets[i]->glRenderAction(eyePos,false,pointRadius,contextData);
	glEnable(GL_LIGHTING);
	
	if(settings.showOuterCore&&settings.outerCoreTransparent)
		{
		/* Set up OpenGL to render the outer core: */
		glMaterial(GLMaterialEnums::FRONT_AND_BACK,outerCoreMaterial);
		
		/* Call the outer core's display list: */
		glCallList(dataItem->displayListIdBase+2);
		}
	if(settings.showInnerCore&&settings.innerCoreTransparent)
		{
		/* Set up OpenGL to render the inner core: */
		glMaterial(GLMaterialEnums::FRONT_AND_BACK,innerCoreMaterial);
		
		/* Call the inner core's display list: */
		glCallList(dataItem->displayListIdBase+3);
		}
	
	/* Render front parts of surfaces: */
	glCullFace(GL_BACK);
	if(settings.showInnerCore&&settings.innerCoreTransparent)
		{
		/* Set up OpenGL to render the inner core: */
		glMaterial(GLMaterialEnums::FRONT_AND_BACK,innerCoreMaterial);
		
		/* Call the inner core's display list: */
		glCallList(dataItem->displayListIdBase+3);
		}
	if(settings.showOuterCore&&settings.outerCoreTransparent)
		{
		/* Set up OpenGL to render the outer core: */
		glMaterial(GLMaterialEnums::FRONT_AND_BACK,outerCoreMaterial);
		
		/* Call the outer core's display list: */
		glCallList(dataItem->displayListIdBase+2);
		}
	
	/* Draw earthquakes in front of the outer core: */
	glDisable(GL_LIGHTING);
	for(unsigned int i=0;i<earthquakeSets.size();++i)
		if(settings.showEarthquakeSets[i])
			earthquakeSets[i]->glRenderAction(eyePos,true,pointRadius,contextData);
	glEnable(GL_LIGHTING);
	
	if(settings.showSurface&&settings.surfaceTransparent)
		{
		/* Set up OpenGL to render the Earth's surface: */
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D,dataItem->surfaceTextureObjectId);
		glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
		glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL,GL_SEPARATE_SPECULAR_COLOR);
		glMaterial(GLMaterialEnums::FRONT_AND_BACK,surfaceMaterial);
		
		/* Call the Earth surface display list: */
		glCallList(dataItem->displayListIdBase+0);
		
		/* Reset OpenGL state: */
		glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL,GL_SINGLE_COLOR);
		glBindTexture(GL_TEXTURE_2D,0);
		glDisable(GL_TEXTURE_2D);
		}
	
	/* Disable blending: */
	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,GL_FALSE);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	
	/* Go back to original coordinate system: */
	glPopMatrix();
	
	/* Restore OpenGL state: */
	#if CLIP_SCREEN
	glDisable(GL_CLIP_PLANE0);
	#endif
	if(lockToSphere)
		glPopMatrix();
	glPopAttrib();
	}

void ShowEarthModel::resetNavigation(void)
	{
	if(scaleToEnvironment)
		{
		/* Center the Earth model in the available display space: */
		Vrui::setNavigationTransformation(Vrui::Point::origin,Vrui::Scalar(3.0*6.4e3),Vrui::Vector(0,0,1));
		}
	else
		{
		/* Center the Earth model in the available display space, but do not scale it: */
		Vrui::NavTransform nav=Vrui::NavTransform::identity;
		nav*=Vrui::NavTransform::translateFromOriginTo(Vrui::getDisplayCenter());
		nav*=Vrui::NavTransform::rotate(Vrui::Rotation::rotateFromTo(Vrui::Vector(0,0,1),Vrui::getUpDirection()));
		nav*=Vrui::NavTransform::scale(Vrui::Scalar(8)*Vrui::getInchFactor()/Vrui::Scalar(6.4e3));
		Vrui::setNavigationTransformation(nav);
		}
	}

void ShowEarthModel::alignSurfaceFrame(Vrui::SurfaceNavigationTool::AlignmentData& alignmentData)
	{
	/* Convert the surface frame's base point to geodetic latitude/longitude: */
	Geoid::Point base=alignmentData.surfaceFrame.getOrigin();
	Geoid::Point geodeticBase;
	if(Geometry::sqr(base)<Geoid::Scalar(1))
		geodeticBase=Geoid::Point(Math::rad(-121.738056),Math::rad(38.553889),0.0);
	else
		geodeticBase=geoid.cartesianToGeodetic(base);
	
	/* Snap the base point to the surface: */
	geodeticBase[2]=Geoid::Scalar(0);
	
	/* Create an Earth-aligned coordinate frame at the snapped base point's position: */
	Geoid::Frame frame=geoid.geodeticToCartesianFrame(geodeticBase);
	
	/* Update the passed frame: */
	alignmentData.surfaceFrame=Vrui::NavTransform(frame.getTranslation(),frame.getRotation(),alignmentData.surfaceFrame.getScaling());
	}


void ShowEarthModel::setEventTime(double newEventTime)
	{
	/* Set the current animation time to the event's time: */
	settings.currentTime=newEventTime;
	settingsChangedCallback(0);
	updateCurrentTime();
	currentTimeSlider->setValue(settings.currentTime);
	}

/* Create and execute an application object: */
VRUI_APPLICATION_RUN(ShowEarthModel)
