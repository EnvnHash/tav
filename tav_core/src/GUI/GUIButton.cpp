//
//  GUIButton.cpp
//  tav_core
//
//  Created by Sven Hahne on 29/9/15.
//  Copyright (c) 2015 Sven Hahne. All rights reserved.
//

#include "pch.h"
#include "GUIButton.h"

namespace tav
{
GUIButton::GUIButton() :
		GUIObject()
{
	init();
}

//--------------------------------------------------

void GUIButton::init()
{
	back = new Quad(-1.f, -1.f, 2.f, 2.f);
	firstRun = true;
}

//--------------------------------------------------

void GUIButton::draw(std::vector<glm::vec4*>* _viewports,
		std::vector<glm::mat4*>* _matrixStack, Shaders* _guiColShdr,
		Shaders* _guiTexShdr)
{
	if (firstRun)
	{
		animBackCol = backColor;
		firstRun = false;
	}

	setViewport(_viewports, _matrixStack);

	if (!hasBackTex)
	{
		_guiColShdr->begin();
		_guiColShdr->setUniformMatrix4fv("m_pvm", objMat.getTransPtr());
		_guiColShdr->setUniform4fv("col", &animBackCol[0]);

	}
	else
	{
		glm::vec4 white = glm::vec4(1.f);
		_guiTexShdr->begin();
		_guiTexShdr->setUniformMatrix4fv("m_pvm", objMat.getTransPtr());
		_guiTexShdr->setUniform1i("tex", 0);
		_guiTexShdr->setUniform4fv("frontColor", &white[0]);
		glActiveTexture(GL_TEXTURE0);
		backTex->bind();
	}

	back->draw();

	if (hasLabel)
		drawLabel(_guiTexShdr);
}

//--------------------------------------------------

void GUIButton::startAnim(widgetEvent _event)
{
	animBackCol = backColor;

	if (!anim.isRunning())
	{
		// printf("GUIButton::startAnim \n");
		anim.setAnimVal<glm::vec4>(actionAnimFuncs[_event], &animBackCol,
				&highLightCol[_event], [this]()
				{	return this->resetBackCol();});
		anim.start();
		// push the animation to the animation queue in Root Widget for optimized updating
		animUpdtQ->push_back(&anim);

	}
}

//--------------------------------------------------

void GUIButton::resetBackCol()
{
	animBackCol = backColor;
	removeAnimFromQ();
}

//--------------------------------------------------

GUIButton::~GUIButton()
{
}
}
