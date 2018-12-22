#ifndef __ANIMATION_H__
#define __ANIMATION_H__

#include "trModule.h"
#include "trDefs.h"
#include "MathGeoLib\MathGeoLib.h"
#include "ResourceAnimation.h"

#include <vector>
#include <map>
#include <list>

class GameObject;
class ComponentBone;

enum AnimationState
{
	NOT_DEF_STATE = -1,
	PLAYING,
	PAUSED,
	STOPPED,
	BLENDING
};

class trAnimation : public trModule
{
public:

	trAnimation();
	~trAnimation();

	// Called before render is available
	bool Awake(JSON_Object* config = nullptr);

	// Called before the first frame
	bool Start();
	bool CleanUp();
	bool Update(float dt);

	void SetAnimationGos(ResourceAnimation* res);
	void DeformMesh(ComponentBone* component_bone);
	void ResetMesh(ComponentBone* component_bone);

	float GetCurrentAnimationTime() const;
	const char* GetAnimationName(int index) const;
	uint GetAnimationsNumber() const;
	ResourceAnimation* GetCurrentAnimation() const;

	void SetCurrentAnimation(uint index);

	void PlayAnimation();
	void PauseAnimation();
	void StopAnimation();
	void StepBackwards();
	void StepForward();

private:

	void RecursiveGetAnimableGO(GameObject* go, ResourceAnimation::BoneTransformation* bone_transformation);
	void MoveAnimationForward(float t);

public:
	
	std::vector<GameObject*> animable_gos;
	std::map<GameObject*, ResourceAnimation::BoneTransformation*> animable_data_map;

	bool loop = false;
	bool interpolate = false;
	float anim_speed = 1.0f;

private:

	float anim_timer = 0.0f;
	float duration = 0.0f;
	ResourceAnimation* current_anim = nullptr;

	AnimationState anim_state = AnimationState::NOT_DEF_STATE;

	std::vector<ResourceAnimation*> available_animations;

};

#endif // __ANIMATION_H__
