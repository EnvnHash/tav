//
//  AssimpAnimation.h
//  adapted from of original created by Lukasz Karluk on 4/12/12.
//

#pragma once

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <memory>
#include "math_utils.h"

namespace tav
{
class AssimpAnimation
{
public:
	enum loopType
	{
		LOOP_NONE, LOOP_NORMAL
	};

	AssimpAnimation(std::shared_ptr<const aiScene> scene,
			aiAnimation * animation);
	~AssimpAnimation();

	aiAnimation * getAnimation();

	void update(double time);

	void play();
	void stop();
	void reset();

	bool isFrameNew();
	bool isPaused();
	bool isPlaying();
	bool isFinished();

	float getPosition();
	float getPositionInSeconds();
	int getPositionInMilliSeconds();
	float getSpeed();
	float getDurationInSeconds();
	int getDurationInMilliSeconds();

	void setPaused(bool paused);
	void setPosition(float position);
	void setLoopState(loopType state);
	void setSpeed(float speed);

protected:

	void updateAnimationNodes();

	std::shared_ptr<const aiScene> scene;
	aiAnimation * animation;
	float animationCurrTime;
	float animationPrevTime;
	bool bPlay;
	bool bPause;
	loopType loopType;
	float progress;
	float progressInSeconds;
	int progressInMilliSeconds;
	float durationInSeconds;
	int durationInMilliSeconds;
	float speed;
};
}
