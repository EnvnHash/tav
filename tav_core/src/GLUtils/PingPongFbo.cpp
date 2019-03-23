//
//  PingPongFbo.cpp
//  Tav_App
//
//  Created by Sven Hahne on 4/5/15.
//  Copyright (c) 2015 Sven Hahne. All rights reserved.
//

#include "pch.h"
#include "GLUtils/PingPongFbo.h"

namespace tav
{
PingPongFbo::PingPongFbo(ShaderCollector* _shCol, int _width, int _height,
		GLenum _type, GLenum _target, bool _depthBuf, int _nrAttachments,
		int _mipMapLevels, int _nrSamples, GLenum _wrapMode, bool _layered) :
		flag(0)
{
	FBOs = new FBO*[2];
	for (int i = 0; i < 2; i++)
	{
		FBOs[i] = new FBO(_shCol, _width, _height, _type, _target, _depthBuf,
				_nrAttachments, _mipMapLevels, _nrSamples, _wrapMode, _layered);
		FBOs[i]->bind();
		FBOs[i]->clearAlpha(1.f);
		FBOs[i]->unbind();
	}

	src = FBOs[0];
	dst = FBOs[1];
	inited = true;
}

PingPongFbo::PingPongFbo(ShaderCollector* _shCol, int _width, int _height,
		int _depth, GLenum _type, GLenum _target, bool _depthBuf,
		int _nrAttachments, int _mipMapLevels, int _nrSamples, GLenum _wrapMode,
		bool _layered) :
		flag(0)
{
	FBOs = new FBO*[2];
	for (int i = 0; i < 2; i++)
	{
		FBOs[i] = new FBO(_shCol, _width, _height, _depth, _type, _target,
				_depthBuf, _nrAttachments, _mipMapLevels, _nrSamples, _wrapMode, _layered);
		FBOs[i]->bind();
		FBOs[i]->clearAlpha(1.f);
		FBOs[i]->unbind();
	}

	src = FBOs[0];
	dst = FBOs[1];
	inited = true;
}

PingPongFbo::~PingPongFbo()
{
	for (int i = 0; i < 2; i++)
		delete FBOs[i];
}

void PingPongFbo::swap()
{
	flag = (flag + 1) % 2;
	src = FBOs[flag % 2];
	dst = FBOs[(flag + 1) % 2];
}

void PingPongFbo::clear(float _alpha)
{
	for (int i = 0; i < 2; i++)
	{
		FBOs[i]->bind();
		FBOs[i]->clearAlpha(_alpha);
		FBOs[i]->unbind();
	}
}

void PingPongFbo::clearWhite()
{
	for (int i = 0; i < 2; i++)
	{
		FBOs[i]->bind();
		FBOs[i]->clearWhite();
		FBOs[i]->unbind();
	}
}

void PingPongFbo::clearAlpha(float _alpha, float _col)
{
	for (int i = 0; i < 2; i++)
		FBOs[i]->clearToColor(_col, _col, _col, _alpha);
}

void PingPongFbo::setMinFilter(GLenum _type)
{
	for (int i = 0; i < 2; i++)
		FBOs[i]->setMinFilter(_type);
}

void PingPongFbo::setMagFilter(GLenum _type)
{
	for (int i = 0; i < 2; i++)
		FBOs[i]->setMagFilter(_type);
}

GLuint PingPongFbo::getSrcTexId()
{
	return src->getColorImg();
}

GLuint PingPongFbo::getDstTexId()
{
	return dst->getColorImg();
}

GLuint PingPongFbo::getSrcTexId(int _index)
{
	return src->getColorImg(_index);
}

GLuint PingPongFbo::getDstTexId(int _index)
{
	return dst->getColorImg(_index);
}

void PingPongFbo::cleanUp()
{
	if (inited)
	{
		for (int i = 0; i < 2; i++)
			if (FBOs[i])
				FBOs[i]->cleanUp();
	}
}
}
