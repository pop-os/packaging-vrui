/***********************************************************************
VruiSceneGraphDemo - Demonstration program for the Vrui scene graph
architecture; shows how to construct a scene graph programmatically, or
load one from one or more VRML 2.0 / 97 files.
Copyright (c) 2010-2021 Oliver Kreylos

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

#include <stdio.h>
#include <string>
#include <vector>
#include <iostream>
#include <Geometry/ComponentArray.h>
#include <Geometry/Point.h>
#include <Geometry/OrthogonalTransformation.h>
#include <GLMotif/PopupMenu.h>
#include <GLMotif/ToggleButton.h>
#include <SceneGraph/GroupNode.h>
#include <SceneGraph/TransformNode.h>
#include <SceneGraph/AppearanceNode.h>
#include <SceneGraph/MaterialNode.h>
#include <SceneGraph/BoxNode.h>
#include <SceneGraph/ShapeNode.h>
#include <SceneGraph/NodeCreator.h>
#include <SceneGraph/VRMLFile.h>
#include <Vrui/Vrui.h>
#include <Vrui/Application.h>
#include <Vrui/SceneGraphManager.h>

class VruiSceneGraphDemo:public Vrui::Application
	{
	/* Embedded classes: */
	private:
	typedef std::vector<SceneGraph::GroupNodePointer> SGList; // Type for lists of scene graph roots
	
	/* Elements: */
	private:
	SGList sceneGraphs; // List of root nodes of all loaded scene graphs
	std::vector<bool> sceneGraphEnableds; // List of enable flags for each loaded scene graph
	GLMotif::PopupMenu* mainMenu;
	
	/* Private methods: */
	void sceneGraphToggleCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData,const unsigned int& index);
	
	/* Constructors and destructors: */
	public:
	VruiSceneGraphDemo(int& argc,char**& argv);
	~VruiSceneGraphDemo(void);
	
	/* Methods: */
	virtual void resetNavigation(void);
	};

/***********************************
Methods of class VruiSceneGraphDemo:
***********************************/

void VruiSceneGraphDemo::sceneGraphToggleCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData,const unsigned int& index)
	{
	/* Enable or disable the selected scene graph: */
	sceneGraphEnableds[index]=cbData->set;
	
	/* Add or remove the selected scene graph from Vrui's central scene graph: */
	if(cbData->set)
		Vrui::getSceneGraphManager()->addNavigationalNode(*sceneGraphs[index]);
	else
		Vrui::getSceneGraphManager()->removeNavigationalNode(*sceneGraphs[index]);
	}

VruiSceneGraphDemo::VruiSceneGraphDemo(int& argc,char**& argv)
	:Vrui::Application(argc,argv),
	 mainMenu(0)
	{
	/* Collect a list of scene graph names to build a menu later: */
	std::vector<std::string> sceneGraphNames;
	
	if(argc>1)
		{
		/*************************************************
		Load scene graphs from one or more VRML 2.0 files:
		*************************************************/
		
		/* Create a node creator to parse the VRML files: */
		SceneGraph::NodeCreator nodeCreator;
		
		/* Load all VRML files from the command line: */
		for(int i=1;i<argc;++i)
			{
			try
				{
				/* Create the new scene graph's root node: */
				SceneGraph::GroupNodePointer root=new SceneGraph::GroupNode;
				
				/* Load and parse the VRML file: */
				SceneGraph::VRMLFile vrmlFile(argv[i],nodeCreator);
				vrmlFile.parse(*root);
				
				/* Add the new scene graph to the list: */
				sceneGraphs.push_back(root);
				sceneGraphEnableds.push_back(true);
				}
			catch(std::runtime_error err)
				{
				/* Print an error message and try the next file: */
				std::cerr<<"Ignoring input file "<<argv[i]<<" due to exception "<<err.what()<<std::endl;
				}
			
			/* Remember the name of this scene graph: */
			const char* start=argv[i];
			const char* end=0;
			for(const char* cPtr=argv[i];*cPtr!='\0';++cPtr)
				{
				if(*cPtr=='/')
					{
					start=cPtr+1;
					end=0;
					}
				if(*cPtr=='.')
					end=cPtr;
				}
			if(end!=0)
				sceneGraphNames.push_back(std::string(start,end));
			else
				sceneGraphNames.push_back(std::string(start));
			}
		}
	else
		{
		/* Create a small scene graph: */
		SceneGraph::TransformNode* transform=new SceneGraph::TransformNode;
		SceneGraph::GroupNodePointer root=transform;
		
		SceneGraph::ShapeNode* shape=new SceneGraph::ShapeNode;
		
		SceneGraph::AppearanceNode* appearance=new SceneGraph::AppearanceNode;
		shape->appearance.setValue(appearance);
		
		SceneGraph::MaterialNode* material=new SceneGraph::MaterialNode;
		appearance->material.setValue(material);
		material->ambientIntensity.setValue(1.0f);
		material->diffuseColor.setValue(SceneGraph::Color(1.0f,0.0f,0.0f));
		material->specularColor.setValue(SceneGraph::Color(1.0f,1.0f,1.0f));
		material->shininess.setValue(0.2f);
		material->update();
		
		appearance->update();
		
		SceneGraph::BoxNode* box=new SceneGraph::BoxNode;
		shape->geometry.setValue(box);
		box->size.setValue(SceneGraph::Size(2,2,2));
		box->update();
		
		shape->update();
		
		root->addChild(*shape);
		
		sceneGraphs.push_back(root);
		sceneGraphEnableds.push_back(true);
		sceneGraphNames.push_back("Box");
		}
	
	/* Add all created or loaded scene graphs to Vrui's central scene graph: */
	for(SGList::iterator sgIt=sceneGraphs.begin();sgIt!=sceneGraphs.end();++sgIt)
		Vrui::getSceneGraphManager()->addNavigationalNode(**sgIt);
	
	/* Create the main menu shell: */
	mainMenu=new GLMotif::PopupMenu("MainMenu",Vrui::getWidgetManager());
	mainMenu->setTitle("Scene Graph Viewer");
	
	/* Add a toggle button for each scene graph: */
	unsigned int numSceneGraphs=sceneGraphs.size();
	for(unsigned int i=0;i<numSceneGraphs;++i)
		{
		char toggleName[40];
		snprintf(toggleName,sizeof(toggleName),"SceneGraphToggle%u",i);
		GLMotif::ToggleButton* sceneGraphToggle=new GLMotif::ToggleButton(toggleName,mainMenu,sceneGraphNames[i].c_str());
		sceneGraphToggle->setToggle(sceneGraphEnableds[i]);
		sceneGraphToggle->getValueChangedCallbacks().add(this,&VruiSceneGraphDemo::sceneGraphToggleCallback,i);
		}
	
	/* Finish and install the main menu: */
	mainMenu->manageMenu();
	Vrui::setMainMenu(mainMenu);
	}

VruiSceneGraphDemo::~VruiSceneGraphDemo(void)
	{
	delete mainMenu;
	}

void VruiSceneGraphDemo::resetNavigation(void)
	{
	/* Set the navigation transformation: */
	SceneGraph::Box bbox=SceneGraph::Box::empty;
	unsigned int numSceneGraphs=sceneGraphs.size();
	for(unsigned int i=0;i<numSceneGraphs;++i)
		bbox.addBox(sceneGraphs[i]->calcBoundingBox());
	Vrui::setNavigationTransformation(Geometry::mid(bbox.min,bbox.max),Geometry::dist(bbox.min,bbox.max));
	}

VRUI_APPLICATION_RUN(VruiSceneGraphDemo)
