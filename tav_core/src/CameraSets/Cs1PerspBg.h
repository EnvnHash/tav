//
//  Cs1PerspBg.h
//  tav_gl4
//
//  Created by Sven Hahne on 14.08.14.
//  Copyright (c) 2014 Sven Hahne. All rights reserved..
//

#pragma GCC visibility push(default)

#pragma once

#include <iosfwd>
#include "CameraSet.h"
#include "GLUtils/FBO.h"
#include "Shaders/Shaders.h"
#include "../GLUtils/GLSL/FastBlur.h"

namespace tav
{
class Cs1PerspBg: public CameraSet
{
public:
	Cs1PerspBg(sceneData* _scd, OSCData* _osc);
	~Cs1PerspBg();
	void initProto(ShaderProto* _proto);
	void clearFbo();
	void render(SceneNode* _scene, double time, double dt, unsigned int ctxNr);
	void preDisp(double time, double dt);
	void postRender();
	void initShader();
	void renderFbos(double time, double dt, unsigned int ctxNr);

	// void setLightProto(string _protoName);
	void onKey(int key, int scancode, int action, int mods);
	void mouseBut(GLFWwindow* window, int button, int action, int mods);
	void mouseCursor(GLFWwindow* window, double xpos, double ypos);
private:
	FBO* fbo;
	FastBlur* fastBlur;
	Shaders* stdTex;
	Shaders* bgShader;
};
}

#pragma GCC visibility pop
