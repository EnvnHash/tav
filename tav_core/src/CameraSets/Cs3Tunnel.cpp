//
//  Cs3Tunnel.cpp
//  tav_gl4
//
//  Created by Sven Hahne on 14.08.14.
//  Copyright (c) 2014 Sven Hahne. All rights reserved.
//
//  tunnel mit 6 projektore - 2 an allen waenden + boden led bildschirm
//  es werden 3 kameras fuer die waende und eine fuer den boden gesetzt
//	diese werden mit den realen proportionen (frustrum) der leinwaende gerendert
//	im format 16/9 ausgegeben (viewport verzerrt) und anschliessend per fbodedist
//	wieder in die richtige Proportion skaliert

#define STRINGIFY(A) #A

#include "pch.h"
#include "Cs3Tunnel.h"

namespace tav
{
Cs3Tunnel::Cs3Tunnel(sceneData* _scd, OSCData* _osc, GWindowManager* _winMan) :
		CameraSet(3, _scd, _osc), winMan(_winMan), renderVisu(false)
{
	id = _scd->id;

	destWidth = 1024;
	destHeight = 768;

	cam = new GLMCamera*[nrCameras];
	upVec = new glm::vec3[nrCameras];
	camPos = new glm::vec3[nrCameras];

	// alle werte werden in den norm cubus skaliert
	float tunnelDepth = 7.f;	// referenz = 2.f
	float tunnelHeight = 2.2f;
	float tunnelWidth = 2.2f;

	// skaliere tunnel dimensionen auf normkubus
	// bestimme den skalierungsfaktor
	float scaleF = 2.f / tunnelDepth;

	// skaliere
	tunnelHeight = 2.2f * scaleF;	// 0,628
	tunnelWidth = 2.2f * scaleF;	// 0,628
	tunnelDepth = 2.f;

	float tAspect = tunnelWidth; // durch 2, weil immer zwei leinwaende auf
	// einmal berechnet werden

	lookAt[0] = glm::vec3(0.f, 0.f, -1.f);
	lookAt[1] = glm::vec3(0.f, 0.f, 1.f);
	lookAt[2] = glm::vec3(1.f, 0.f, 0.f);

	upVec[0] = glm::vec3(0.f, 1.f, 0.f);
	upVec[1] = glm::vec3(0.f, 1.f, 0.f);
	upVec[2] = glm::vec3(0.f, 1.f, 0.f);

	camPos[0] = glm::vec3(0.f, 0.f, 0.f);
	camPos[1] = glm::vec3(0.f, 0.f, 0.f);
	camPos[2] = glm::vec3(0.f, 0.f, 0.f);

	float fovY = std::atan2(tunnelHeight * 0.5f, tunnelWidth * 0.5f) * 2.f;
	fovY = fovY / (M_PI * 2.f) * 360.0f;

	// mit dem richtigen aspect (der leiunwaende) skalieren und danach in was auch
	// immer fuer eine Aufloesung rendern (wird verzerrt)
	// dann mit distortfbo korrigieren
	for (int i = 0; i < 2; i++)
	{
		cam[i] = new GLMCamera(GLMCamera::PERSPECTIVE, 10000,
				int(tAspect * 10000.f), -1.f, 1.f,   // left, right
				-1.f, 1.f,           // bottom, top
				camPos[i].x, camPos[i].y, camPos[i].z,	// camPos
				lookAt[i].x, lookAt[i].y, lookAt[i].z,				// lookAt
				upVec[i].x, upVec[i].y, upVec[i].z, 0.1f, 100.f, fovY); // upVec
	}

	tAspect = tunnelDepth * 0.5f; // durch 2, weil immer zwei leinwaende auf
	cam[2] = new GLMCamera(GLMCamera::PERSPECTIVE, 10000,
			int(tAspect * 10000.f), -1.f, 1.f,   // left, right
			-1.f, 1.f,           // bottom, top
			camPos[2].x, camPos[2].y, camPos[2].z,	// camPos
			lookAt[2].x, lookAt[2].y, lookAt[2].z,					// lookAt
			upVec[2].x, upVec[2].y, upVec[2].z, 0.1f, 100.f, fovY);  	// upVec

	setupCamPar();
	cp.near = 0.1f;

	GLMCamera* camMap[3] ={ cam[0], cam[1], cam[2] };

	// first render all 4 cameras (4 sides of the tunnel) into one fbo
	// then render each wall and roof with two fbos
	// the floor will be rendered as one screen
	// all screens exist as one xscreen (xinerama)
	fboDedist = new FboDedistPersp(_scd, _winMan, _scd->fboWidth, _scd->fboHeight);

	// wall and roof as two screens
	for (int i = 0; i < 3; i++)
	{
		fboDedist->add(_winMan->getWindows()->at(0), camMap[i],
				float(destWidth), float(destHeight),
				float(i) * float(destWidth), 0);
	}

	setupFunc = std::bind(&CameraSet::setupCamPar, this, true);
	fboDedist->setCamSetupFunc(&setupFunc);
	fboDedist->setCalibFileName((*_scd->dataPath) + "/calib_cam/" + _scd->setupName + "_Cs3Tunnel.yml");

	if (renderVisu)
	{
		tunVisQuads = new Quad*[3];

		for (auto i = 0; i < 3; i++)
			tunVisQuads[i] = new Quad(-0.5f, -tunnelHeight * 0.5f, 1.f,
					tunnelHeight, glm::vec3(0.f, 0.f, 1.f), 0.f, 0.f, 0.f, 1.f);
		tunVisQuads[4] = new Quad(-tunnelWidth * 0.5f, -tunnelHeight * 0.5f,
				tunnelWidth, tunnelHeight, glm::vec3(0.f, 0.f, 1.f), 0.f, 0.f,
				0.f, 1.f);

		tunVisQuads[0]->translate(-0.5f, 0.f, -tunnelWidth * 0.5f);	// links vorne
		tunVisQuads[1]->translate(-0.5f, 0.f, tunnelWidth * 0.5f);// rechts vorne

		tunVisQuads[2]->translate(0.f, 0.f, tunnelDepth * -0.5f);// oben vorne
		tunVisQuads[2]->rotate(M_PI * -0.5f, 0.f, 1.f, 0.f);

		tunVisCam = new GLMCamera(GLMCamera::PERSPECTIVE, _scd->screenWidth,
				_scd->screenHeight, -1.f, 1.f,  		 // left, right
				-1.f, 1.f,      	 // bottom, top
				0.f, 0.f, 1.f,	// camPos
				0.f, 0.f, 0.f,	// lookAt
				0.f, 1.f, 0.f, 0.5f, 100.f, 0.34f * 180.f);  	// upVec

		glm::mat4 modMatr = glm::translate(glm::mat4(1.f),
				glm::vec3(0.f, 0.f, 0.32f - 1.f));
		modMatr = glm::rotate(modMatr, float(M_PI) * 0.47f,
				glm::vec3(0.f, 1.f, 0.f));

		tunVisCam->setModelMatr(modMatr);

		//    	floorTex = new TextureManager();
		//    	floorTex->loadTexture2D((*scd->dataPath) + "textures/floor_text.jpg");
	}

	initShader();
}

//--------------------------------------------------------------------------------

Cs3Tunnel::~Cs3Tunnel()
{
}

//--------------------------------------------------------------------------------

void Cs3Tunnel::initShader()
{
	std::string shdr_Header = "#version 410\n#pragma optimize(on)\n";

	std::string vert =
			STRINGIFY(
					layout( location = 0 ) in vec4 position; layout( location = 1 ) in vec4 normal; layout( location = 2 ) in vec2 texCoord; layout( location = 3 ) in vec4 color; uniform mat4 m_pvm; uniform vec2 tSize; uniform vec2 tOffs; uniform vec2 fboSize; out vec2 tex_coord;

					void main() { tex_coord = texCoord * (tSize / fboSize) + (tOffs / fboSize); gl_Position = m_pvm * position; });

	vert = "// Cs3Tunnel Visu, vert\n" + shdr_Header + vert;

	// zero is left top
	std::string frag =
			STRINGIFY(
					uniform sampler2D tex;

					in vec2 tex_coord; layout (location = 0) out vec4 color; void main() { color = texture(tex, tex_coord);
//    						color = texture(tex, vec2(distTexCoord.x * texMod.x + texMod.z,
//    												  distTexCoord.y * texMod.y + texMod.w));
					});

	frag = "//  Cs3Tunnel Visu, frag\n" + shdr_Header + frag;

	texShdr = shCol->addCheckShaderText("Cs3TunnelVisu", vert.c_str(),
			frag.c_str());
}

//--------------------------------------------------------------------------------

void Cs3Tunnel::initProto(ShaderProto* _proto)
{
	_proto->defineVert(true);
	if (!_proto->enableShdrType[GEOMETRY])
		_proto->defineGeom(true);
	_proto->defineFrag(true);

	_proto->asmblMultiCamGeo(); // add geo shader for multicam rendering
	_proto->enableShdrType[GEOMETRY] = true;

	_proto->assemble();
}

//--------------------------------------------------------------------------------

void Cs3Tunnel::render(SceneNode* _scene, double time, double dt,
		unsigned int ctxNr)
{
	// call update method of the scene, if blending is enabled
	// this already gets called in the scene blender. the resulting morphscene
	// will have a empty update method
	_scene->updateOscPar(time, dt);
	_scene->update(time, dt);

	// ------ render to FBO -------------

	if (lightProto.find(_scene->protoName) != lightProto.end())
	{
		fboDedist->bindFbo();
//            fboDedist->clearFboAlpha(winMan->getWindows()->at(0), 0.3f);

		lightProto[_scene->protoName]->shader->begin();
		lightProto[_scene->protoName]->sendPar(nrCameras, cp, osc, time);

		const float wot = float(destWidth);

		// set viewports
		for (int i = 0; i < 3; i++)
			glViewportIndexedf(i, wot * float(i), 0.f, wot, destHeight);

		cp.camId = id;
		lightProto[_scene->protoName]->sendPar(nrCameras, cp, osc, time); // 1 cameras

		_scene->draw(time, dt, &cp, lightProto[_scene->protoName]->shader);

		fboDedist->unbindActFbo();
	}
	else
	{
		printf("CameraSet Error: couldn´t get LightProto \n");
	}
}

//--------------------------------------------------------------------------------

void Cs3Tunnel::renderFbos(double time, double dt, unsigned int ctxNr)
{
	// ------ render with perspective Dedistortion each part -------------
	/*
	 delete tunVisCam;

	 tunVisCam = new GLMCamera(
	 GLMCamera::PERSPECTIVE,
	 scd->screenWidth,
	 scd->screenHeight,
	 -1.f, 1.f,  		 // left, right
	 -1.f, 1.f,      	 // bottom, top
	 0.f, 0.f, 1.f,	// camPos
	 0.f, 0.f, 0.f,	// lookAt
	 0.f, 1.f, 0.f,
	 0.5f, 100.f, osc->zoom * 180.f);  	// upVec
	 */

	//glm::mat4 modMatr = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 0.f, osc->blurOffs - 1.f));
	//modMatr = glm::rotate(modMatr, float(M_PI) * osc->blurFdbk, glm::vec3(0.f, 1.f, 0.f));
	//tunVisCam->setModelMatr(modMatr);
	if (!renderVisu)
	{
		fboDedist->drawAllFboViews();

	}
	else
	{
		//fboDedist->drawAllFbo();
		glActiveTexture(GL_TEXTURE0);

		glClearDepth(1.0f);
		glEnable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);

		texShdr->begin();
		texShdr->setUniformMatrix4fv("m_pvm", tunVisCam->getMVPPtr());
		texShdr->setUniform1i("tex", 0);

		for (auto i = 0; i < 5; i++)
		{
			glBindTexture(GL_TEXTURE_2D, fboDedist->getFboTex(i));

			texShdr->setUniform2fv("tSize", &fboDedist->getScreenSize(i)[0]);
			texShdr->setUniform2fv("tOffs", &fboDedist->getScreenOffs(i)[0]);
			texShdr->setUniform2fv("fboSize", &fboDedist->getFboSize(i)[0]);

			tunVisQuads[i]->draw();
		}

		glEnable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
	}
}

//--------------------------------------------------------------------------------

void Cs3Tunnel::preDisp(double time, double dt)
{
}

//--------------------------------------------------------------------------------

void Cs3Tunnel::postRender()
{
}

//--------------------------------------------------------------------------------

void Cs3Tunnel::onKey(int key, int scancode, int action, int mods)
{
	// call onKey Function of active SceneNode
	fboDedist->onKey(key, scancode, action, mods);
}

//--------------------------------------------------------------------------------

void Cs3Tunnel::mouseBut(GLFWwindow* window, int button, int action, int mods)
{
	fboDedist->mouseBut(window, button, action, mods);
}

//--------------------------------------------------------------------------------

void Cs3Tunnel::mouseCursor(GLFWwindow* window, double xpos, double ypos)
{
	fboDedist->mouseCursor(window, xpos, ypos);
}

//--------------------------------------------------------------------------------

void Cs3Tunnel::clearFbo()
{
}

}
