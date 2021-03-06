//
//  Cs3FloorWall.h
//  tav_gl4
//
//  Created by Sven Hahne on 14.08.14.
//  Copyright (c) 2014 Sven Hahne. All rights reserved..
//

#pragma GCC visibility push(default)
#pragma once

#include <iostream>
#include <functional>

#include "CameraSet.h"
#include "GLUtils/FboDedistPersp.h"
#include "GLUtils/GWindowManager.h"
#include "GLUtils/TextureManager.h"
#include "GeoPrimitives/Quad.h"

namespace tav
{
class Cs3FloorWall: public CameraSet
{
public:
	Cs3FloorWall(sceneData* _scd, OSCData* _osc, GWindowManager* _winMan);
	~Cs3FloorWall();
	void initProto(ShaderProto* _proto);
	void clearFbo();
	void render(SceneNode* _scene, double time, double dt, unsigned int ctxNr);
	void preDisp(double time, double dt);
	void postRender();
	void renderFbos(double time, double dt, unsigned int ctxNr);
	void onKey(int key, int scancode, int action, int mods);
	void mouseBut(GLFWwindow* window, int button, int action, int mods);
	void mouseCursor(GLFWwindow* window, double xpos, double ypos);
private:
	std::function<void()> setupFunc;
	FboDedistPersp* fboDedist;
	GWindowManager* winMan;

	Shaders* texShdr;
	TextureManager* gradTex;
	Quad* gradquad;

	glm::vec3* camPos;
	glm::vec3* rotAxis;

	float* rotAngle;
	bool lightNotInit = true;
};
}

#pragma GCC visibility pop
