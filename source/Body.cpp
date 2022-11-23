/* Body.cpp
Copyright (c) 2016 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*/

#include "Body.h"

#include "DataNode.h"
#include "DataWriter.h"
#include "GameData.h"
#include "Mask.h"
#include "MaskManager.h"
#include "Outfit.h"
#include "Random.h"
#include "Screen.h"
#include "Sprite.h"
#include "SpriteSet.h"

#include <algorithm>
#include <cmath>

using namespace std;



// Constructor, based on a Sprite.
Body::Body(const Sprite *sprite, Point position, Point velocity, Angle facing, double zoom)
	: position(position), velocity(velocity), angle(facing), zoom(zoom)
{
	SpriteParameters *spriteState = &this->sprites[Body::BodyState::FLYING];
	SpriteParameters::AnimationParameters spriteAnimationParameters;
	spriteAnimationParameters.randomizeStart = true;
	spriteState->SetSprite("default", sprite, spriteAnimationParameters);
	anim.randomize = true;
}



// Constructor, based on the animation from another Body object.
Body::Body(const Body &sprite, Point position, Point velocity, Angle facing, double zoom)
{
	*this = sprite;
	this->position = position;
	this->velocity = velocity;
	this->angle = facing;
	this->zoom = zoom;
}



// Check that this Body has a sprite and that the sprite has at least one frame.
bool Body::HasSprite() const
{
	const Sprite *sprite = this->GetSprite();
	return (sprite && sprite->Frames());
}



// Check that this Body has a sprite for a specific BodyState
bool Body::HasSpriteFor(Body::BodyState state) const
{
	const Sprite *sprite = this->sprites[state].GetSprite();
	return (sprite && sprite->Frames());
}



// Access the underlying Sprite object.
const Sprite *Body::GetSprite(Body::BodyState state) const
{
	Body::BodyState selected = state != Body::BodyState::CURRENT ? state : this->currentState;

	SpriteParameters *spriteState = &this->sprites[selected];

	// Return flying sprite if the requested state's sprite does not exist.
	const Sprite* sprite = spriteState->GetSprite();
	if(sprite != nullptr && !returnDefaultSprite)
		return sprite;
	else
		return this->sprites[Body::BodyState::FLYING].GetSprite();

}



Body::BodyState Body::GetState() const
{
	return this->currentState;
}



// Get the width of this object, in world coordinates (i.e. taking zoom and scale into account).
double Body::Width() const
{
	const Sprite *sprite = this->GetSprite();
	return static_cast<double>(sprite ? (.5f * zoom) * scale * sprite->Width() : 0.f);
}



// Get the height of this object, in world coordinates (i.e. taking zoom and scale into account).
double Body::Height() const
{
	const Sprite *sprite = this->GetSprite();
	return static_cast<double>(sprite ? (.5f * zoom) * scale * sprite->Height() : 0.f);
}



// Get the farthest a part of this sprite can be from its center.
double Body::Radius() const
{
	return .5 * Point(Width(), Height()).Length();
}



// Which color swizzle should be applied to the sprite?
int Body::GetSwizzle() const
{
	return swizzle;
}

// Get the frame index for the given time step. If no time step is given, this
// will return the frame from the most recently given step.
float Body::GetFrame(int step) const
{
	if(step >= 0)
		SetStep(step);
	// Choose between frame and random frame based on the randomize parameter
	return !this->anim.randomize || stateTransitionRequested ? frame : randomFrame;
}



// Get the mask for the given time step. If no time step is given, this will
// return the mask from the most recently given step.
const Mask &Body::GetMask(int step) const
{
	const Sprite *sprite = this->GetSprite();

	static const Mask EMPTY;
	int current = round(frame);
	if(!sprite || current < 0)
		return EMPTY;

	const vector<Mask> &masks = GameData::GetMaskManager().GetMasks(sprite, Scale());

	// Assume that if a masks array exists, it has the right number of frames.
	return masks.empty() ? EMPTY : masks[current % masks.size()];
}



// Position, in world coordinates (zero is the system center).
const Point &Body::Position() const
{
	return position;
}



// Velocity, in pixels per second.
const Point &Body::Velocity() const
{
	return velocity;
}



// Direction this Body is facing in.
const Angle &Body::Facing() const
{
	return angle;
}



// Unit vector in the direction this body is facing. This represents the scale
// and transform that should be applied to the sprite before drawing it.
Point Body::Unit() const
{
	return angle.Unit() * (.5 * Zoom());
}



// Zoom factor. This controls how big the sprite should be drawn.
double Body::Zoom() const
{
	return max(zoom, 0.f);
}



double Body::Scale() const
{
	return static_cast<double>(scale);
}



// Check if this object is marked for removal from the game.
bool Body::ShouldBeRemoved() const
{
	return shouldBeRemoved;
}



// Store the government here too, so that collision detection that is based
// on the Body class can figure out which objects will collide.
const Government *Body::GetGovernment() const
{
	return government;
}



// Load the sprite specification, including all animation attributes.
void Body::LoadSprite(const DataNode &node, Body::BodyState state)
{
	if(node.Size() < 2)
		return;

	const Sprite *sprite = SpriteSet::Get(node.Token(1));
	SpriteParameters *spriteData = &this->sprites[state];
	SpriteParameters::AnimationParameters spriteAnimationParameters;
	std::vector<DataNode> triggerSpriteDefer;

	// The only time the animation does not start on a specific frame is if no
	// start frame is specified and it repeats. Since a frame that does not
	// start at zero starts when the game started, it does not make sense for it
	// to do that unless it is repeating endlessly.
	for(const DataNode &child : node)
	{
		if(child.Token(0) == "trigger" && child.Size() >= 3)
		{
			// Defer loading any trigger sprites until after the main sprite is loaded
			// Ensures that all "default" parameters are loaded first.
			triggerSpriteDefer.push_back(child);
		}
		else if(child.Token(0) == "frame rate" && child.Size() >= 2 && child.Value(1) >= 0.)
			spriteAnimationParameters.frameRate = child.Value(1) / 60.;
		else if(child.Token(0) == "frame time" && child.Size() >= 2 && child.Value(1) > 0.)
			spriteAnimationParameters.frameRate = 1. / child.Value(1);
		else if(child.Token(0) == "delay" && child.Size() >= 2 && child.Value(1) > 0.)
			spriteAnimationParameters.delay = child.Value(1);
		else if(child.Token(0) == "transition delay" && child.Size() >= 2 && child.Value(1) >= 0.)
			spriteAnimationParameters.transitionDelay = child.Value(1);
		else if(child.Token(0) == "scale" && child.Size() >= 2 && child.Value(1) > 0.)
			spriteAnimationParameters.scale = static_cast<float>(child.Value(1));
		else if(child.Token(0) == "start frame" && child.Size() >= 2)
		{
			spriteAnimationParameters.startFrame = static_cast<float>(child.Value(1));
			spriteAnimationParameters.startAtZero = true;
		}
		else if(child.Token(0) == "indicate percentage" && child.Size() >= 2 && child.Value(1) > 0. && child.Value(1) < 1.)
		{
			spriteAnimationParameters.indicateReady = true;
			spriteAnimationParameters.repeat = false;
			spriteAnimationParameters.startAtZero = true;
			spriteAnimationParameters.indicatePercentage = static_cast<float>(child.Value(1));
		}
		else if(child.Token(0) == "no indicate")
			spriteAnimationParameters.indicateReady = false;
		else if(child.Token(0) == "indicate")
		{
			spriteAnimationParameters.indicateReady = true;
			spriteAnimationParameters.repeat = false;
			spriteAnimationParameters.startAtZero = true;
			spriteAnimationParameters.indicatePercentage = 1.0f;
		}
		else if(child.Token(0) == "random")
			spriteAnimationParameters.randomize = true;
		else if(child.Token(0) == "random start frame")
			spriteAnimationParameters.randomizeStart = true;
		else if(child.Token(0) == "no repeat")
		{
			spriteAnimationParameters.repeat = false;
			spriteAnimationParameters.startAtZero = true;
		}
		else if(child.Token(0) == "transition rewind")
		{
			spriteAnimationParameters.transitionFinish = false;
			spriteAnimationParameters.transitionRewind = true;
		}
		else if(child.Token(0) == "transition finish")
		{
			spriteAnimationParameters.transitionFinish = true;
			spriteAnimationParameters.transitionRewind = false;
		}
		else if(child.Token(0) == "rewind")
			spriteAnimationParameters.rewind = true;
		else
			child.PrintTrace("Skipping unrecognized attribute:");
	}
	
	bool defaultTriggerOnUse = false;
	// Now load the trigger sprites.
	for(int val = 0; val < static_cast<int>(triggerSpriteDefer.size()); ++val)
	{
		DataNode node = triggerSpriteDefer.at(val);
		defaultTriggerOnUse |= this->LoadTriggerSprite(node, state, spriteAnimationParameters);
	}
	
	spriteAnimationParameters.triggerOnUse = defaultTriggerOnUse;
	
	if(defaultTriggerOnUse && spriteAnimationParameters.indicateReady)
	{
		node.PrintTrace("Default animation will not indicate if trigger animation triggers on use!");
		spriteAnimationParameters.repeat = false;
		spriteAnimationParameters.startAtZero = true;
		spriteAnimationParameters.indicateReady = false;
		spriteAnimationParameters.indicatePercentage = -1.0f;
	}
	
	spriteData->SetSprite("default", sprite, spriteAnimationParameters);

	if(scale != 1.f)
		GameData::GetMaskManager().RegisterScale(sprite, Scale());
}


// Returns whether the trigger sprite is based on the outfit being used
bool Body::LoadTriggerSprite(const DataNode &node, Body::BodyState state, SpriteParameters::AnimationParameters params)
{
	if(node.Size() < 2)
		return false;

	const Sprite *sprite = SpriteSet::Get(node.Token(2));
	std::string trigger = node.Token(1);

	if(!GameData::Outfits().Has(trigger))
	{
		node.PrintTrace("Invalid trigger " + trigger);
		return false;
	}

	SpriteParameters *spriteData = &this->sprites[state];
	SpriteParameters::AnimationParameters spriteAnimationParameters = params;

	if(node.Size() > 3 && node.Token(3) == "on use")
		spriteAnimationParameters.triggerOnUse = true;

	// The only time the animation does not start on a specific frame is if no
	// start frame is specified and it repeats. Since a frame that does not
	// start at zero starts when the game started, it does not make sense for it
	// to do that unless it is repeating endlessly.
	for(const DataNode &child : node)
	{
		if(child.Token(0) == "frame rate" && child.Size() >= 2 && child.Value(1) >= 0.)
			spriteAnimationParameters.frameRate = child.Value(1) / 60.;
		else if(child.Token(0) == "frame time" && child.Size() >= 2 && child.Value(1) > 0.)
			spriteAnimationParameters.frameRate = 1. / child.Value(1);
		else if(child.Token(0) == "delay" && child.Size() >= 2 && child.Value(1) > 0.)
			spriteAnimationParameters.delay = child.Value(1);
		else if(child.Token(0) == "transition delay" && child.Size() >= 2 && child.Value(1) >= 0.)
			spriteAnimationParameters.transitionDelay = child.Value(1);
		else if(child.Token(0) == "scale" && child.Size() >= 2 && child.Value(1) > 0.)
			spriteAnimationParameters.scale = static_cast<float>(child.Value(1));
		else if(child.Token(0) == "start frame" && child.Size() >= 2)
		{
			spriteAnimationParameters.startFrame = static_cast<float>(child.Value(1));
			spriteAnimationParameters.startAtZero = true;
		}
		else if(child.Token(0) == "indicate percentage" && child.Size() >= 2 && child.Value(1) > 0. && child.Value(1) < 1.)
		{
			spriteAnimationParameters.indicateReady = true;
			spriteAnimationParameters.repeat = false;
			spriteAnimationParameters.startAtZero = true;
			spriteAnimationParameters.indicatePercentage = static_cast<float>(child.Value(1));
		}
		else if(child.Token(0) == "no indicate")
		{
			spriteAnimationParameters.indicateReady = false;
			spriteAnimationParameters.indicatePercentage = -1.0f;
		}
		else if(child.Token(0) == "indicate")
		{
			spriteAnimationParameters.indicateReady = true;
			spriteAnimationParameters.repeat = false;
			spriteAnimationParameters.startAtZero = true;
			spriteAnimationParameters.indicatePercentage = 1.0f;
		}
		else if(child.Token(0) == "random")
			spriteAnimationParameters.randomize = true;
		else if(child.Token(0) == "random start frame")
			spriteAnimationParameters.randomizeStart = true;
		else if(child.Token(0) == "no repeat")
		{
			spriteAnimationParameters.repeat = false;
			spriteAnimationParameters.startAtZero = true;
		}
		else if(child.Token(0) == "transition rewind")
		{
			spriteAnimationParameters.transitionFinish = false;
			spriteAnimationParameters.transitionRewind = true;
		}
		else if(child.Token(0) == "transition finish")
		{
			spriteAnimationParameters.transitionFinish = true;
			spriteAnimationParameters.transitionRewind = false;
		}
		else if(child.Token(0) == "rewind")
			spriteAnimationParameters.rewind = true;
		else
			child.PrintTrace("Skipping unrecognized attribute:");
	}
	
	if(spriteAnimationParameters.triggerOnUse && spriteAnimationParameters.indicateReady)
	{
		node.PrintTrace("Trigger animation will not indicate if trigger is on use!");
		spriteAnimationParameters.repeat = false;
		spriteAnimationParameters.startAtZero = true;
		spriteAnimationParameters.indicateReady = false;
		spriteAnimationParameters.indicatePercentage = -1.0f;
	}
	
	spriteData->SetSprite(trigger, sprite, spriteAnimationParameters);

	if(scale != 1.f)
		GameData::GetMaskManager().RegisterScale(sprite, Scale());
	
	return spriteAnimationParameters.triggerOnUse;
}


// Save the sprite specification, including all animation attributes.
void Body::SaveSprite(DataWriter &out, const string &tag, bool allStates) const
{
	const std::string tags[Body::BodyState::NUM_STATES] = {allStates ? "sprite" : tag, "sprite-firing",
												"sprite-launching", "sprite-landing",
												"sprite-jumping", "sprite-disabled"};

	for(int i = 0; i < Body::BodyState::NUM_STATES; i++)
	{
		SpriteParameters *spriteState = &this->sprites[i];
		const Sprite *sprite = spriteState->GetSprite("default");

		if(sprite)
		{
			std::string currTrigger = spriteState->GetTrigger();
			spriteState->RequestTrigger("default");
			spriteState->CompleteTriggerRequest();
			out.Write(tags[i], sprite->Name());
			out.BeginChild();
			{
				this->SaveSpriteParameters(out, spriteState);
				// The map of all spriteStates
				auto map = spriteState->GetAllSprites();

				for(auto it = map->begin(); it != map->end(); ++it)
					if(it->first.compare("default") != 0)
					{
						const Sprite *triggerSprite = spriteState->GetSprite(it->first);
						spriteState->RequestTrigger(it->first);
						spriteState->CompleteTriggerRequest();
						out.Write("trigger", it->first, triggerSprite->Name(), spriteState->exposed.triggerOnUse ? "on use" : "on have");
						out.BeginChild();
						{
							this->SaveSpriteParameters(out, spriteState);
						}
						out.EndChild();
					}
			}
			out.EndChild();
			// Reset any applied triggers
			spriteState->RequestTrigger(currTrigger);
			spriteState->CompleteTriggerRequest();
		}
		if(!allStates)
			return;
	}
}

void Body::SaveSpriteParameters(DataWriter &out, SpriteParameters *state) const
{
	SpriteParameters::AnimationParameters *exposed = &(state->exposed);
	if(exposed->frameRate != static_cast<float>(2. / 60.))
		out.Write("frame rate", exposed->frameRate * 60.);
	if(exposed->delay)
		out.Write("delay", exposed->delay);
	if(exposed->scale != 1.f)
		out.Write("scale", exposed->scale);
	if(exposed->randomizeStart)
		out.Write("random start frame");
	if(exposed->randomize)
		out.Write("random");
	if(!exposed->repeat)
		out.Write("no repeat");
	if(exposed->rewind)
		out.Write("rewind");
	if(exposed->indicateReady)
	{
		if(exposed->indicatePercentage == 1.0f)
			out.Write("indicate");
		else
			out.Write("indicate percentage", exposed->indicatePercentage);
	}
	else
	{
		out.Write("no indicate");
	}
	if(exposed->transitionFinish)
		out.Write("transition finish");
	if(exposed->transitionRewind)
		out.Write("transition rewind");
	if(exposed->transitionDelay)
		out.Write("transition delay", exposed->transitionDelay);
}



// Set the sprite.
void Body::SetSprite(const Sprite *sprite, Body::BodyState state)
{
	SpriteParameters::AnimationParameters init;
	this->sprites[state].SetSprite("default", sprite, init);
	currentStep = -1;
}

// Set the state.
void Body::SetState(Body::BodyState state)
{
	// If the transition has a delay, ensure that rapid changes from transitions
	// which ignore the delay to transitions that don't (ignore delays)
	// keep smooth animations.
	bool delayIgnoredPreviousStep = this->transitionState == Body::BodyState::JUMPING ||
									this->transitionState == Body::BodyState::DISABLED ||
									this->transitionState == Body::BodyState::LANDING;
	bool delayActiveCurrentStep = (state == Body::BodyState::FLYING ||
									state == Body::BodyState::LAUNCHING || state == Body::BodyState::FIRING)
									&& delayed < anim.transitionDelay;
	if(state == this->currentState && this->transitionState != this->currentState
			&& this->transitionState != Body::BodyState::TRIGGER)
	{
		// Cancel transition
		stateTransitionRequested = false;

		// Start replaying animation from current frame
		frameOffset = -currentStep * anim.frameRate;
		frameOffset += frame;

	}
	else if(delayIgnoredPreviousStep && delayActiveCurrentStep && stateTransitionRequested)
	{
		// Start replaying animation from current frame
		frameOffset = -currentStep * anim.frameRate;
		frameOffset += frame;

	}
	else if(state != this->currentState)
	{
		// Set the current frame to be the rewindFrame upon first request to state transition
		if(!stateTransitionRequested)
		{
			if(anim.transitionRewind)
			{
				rewindFrame = frame;
				
				// Ensures that rewinding starts from correct frame.
				frameOffset = -currentStep * anim.frameRate;
				frameOffset += rewindFrame;
			}
			stateTransitionRequested = true;
		}
	}

	this->transitionState = this->transitionState != Body::BodyState::TRIGGER ? state : Body::BodyState::TRIGGER;

	// If state transition has no animation needed, then immediately transition.
	if(!this->anim.transitionFinish && !this->anim.transitionRewind && stateTransitionRequested)
		this->FinishStateTransition();
}


// Set the color swizzle.
void Body::SetSwizzle(int swizzle)
{
	this->swizzle = swizzle;
}



// Set the frame rate of the sprite. This is used for objects that just specify
// a sprite instead of a full animation data structure.
void Body::SetFrameRate(float framesPerSecond)
{
	anim.frameRate = framesPerSecond / 60.f;
}



// Add the given amount to the frame rate.
void Body::AddFrameRate(float framesPerSecond)
{
	anim.frameRate += framesPerSecond / 60.f;
}



void Body::PauseAnimation()
{
	++pause;
}

void Body::ShowDefaultSprite(bool defaultSprite)
{
	this->returnDefaultSprite = defaultSprite;
}

// Indicate whether the body can perform the requested action (depending on its state)
// only if a signal is needed for the action.
bool Body::ReadyForAction() const
{
	// Never ready for action if transitioning between states
	return this->anim.indicateReady ? this->stateReady : (!this->stateTransitionRequested || this->transitionState == Body::BodyState::TRIGGER);
}



// Used to assign triggers for all states according to the outfits present
void Body::AssignStateTriggers(std::map<const Outfit*, int> &outfits)
{
	bool triggerSet[Body::BodyState::NUM_STATES];
	for(int i = 0; i < Body::BodyState::NUM_STATES; i++)
		triggerSet[i] = false;

	for(const auto it : outfits)
		for(int i = 0; i < Body::BodyState::NUM_STATES; i++)
			if(!triggerSet[i])
			{
				SpriteParameters *toSet = &this->sprites[i];
				if(toSet->IsTrigger(it.first->TrueName()))
					triggerSet[i] = toSet->RequestTriggerOnUse(it.first->TrueName(), false);
			}
	// Switch back to default sprite if the outfit is no longer found
	for(int i = 0; i < Body::BodyState::NUM_STATES; i++)
	{
		SpriteParameters *toSet = &this->sprites[i];
		if(!triggerSet[i])
			toSet->RequestTrigger("default");
		toSet->CompleteTriggerRequest();
	}
}

bool Body::AssignStateTriggerOnUse(Body::BodyState state, std::string trigger)
{
	bool triggerSet = false;
	if(this->HasSpriteFor(state))
	{
		SpriteParameters *spriteState = &this->sprites[state];
		if(spriteState->IsTrigger(trigger) && !spriteState->IsCurrentTrigger(trigger))
		{
			if(spriteState->RequestTriggerOnUse(trigger, true) && this->anim.triggerOnUse
					&& state == this->currentState)
				this->SetState(Body::BodyState::TRIGGER);
			triggerSet = true;
		}
	}
	return triggerSet;
}

// Called when the body is ready to transition between states.
void Body::FinishStateTransition() const
{
	if(this->stateTransitionRequested)
	{
		frameOffset = 0.0f;
		pause = 0;

		Body::BodyState requestedTransitionState = this->transitionState == Body::BodyState::TRIGGER ?
									this->currentState : this->transitionState;
		// Default to Flying sprite if requested sprite does not exist.
		Body::BodyState trueTransitionState = this->sprites[requestedTransitionState].GetSprite() ?
									requestedTransitionState : Body::BodyState::FLYING;
		SpriteParameters *transitionedState = &this->sprites[trueTransitionState],
						*currentState = &this->sprites[this->currentState];
		// Handle hanging trigger requests
		if(this->postTriggerTransition)
		{
			currentState->RequestTrigger("default");
			currentState->CompleteTriggerRequest();
			this->postTriggerTransition = false;
		}
		
		if(this->transitionState == Body::BodyState::TRIGGER)
		{
			transitionedState->CompleteTriggerRequest();
			this->postTriggerTransition = true;
		}
		// Update animation parameters.
		this->anim = transitionedState->exposed;
		this->currentState = requestedTransitionState;
		// No longer need to change states
		this->stateTransitionRequested = false;
		this->transitionState = this->currentState;
	}
}



// Mark this object to be removed from the game.
void Body::MarkForRemoval()
{
	shouldBeRemoved = true;
}



// Mark this object to not be removed from the game.
void Body::UnmarkForRemoval()
{
	shouldBeRemoved = false;
}


// Set the current time step.
void Body::SetStep(int step) const
{
	const Sprite *sprite = this->GetSprite();

	// If the animation is paused, reduce the step by however many frames it has
	// been paused for.
	step -= pause;

	// If the step is negative or there is no sprite, do nothing. This updates
	// and caches the mask and the frame so that if further queries are made at
	// this same time step, we don't need to redo the calculations.
	if(step == currentStep || step < 0 || !sprite || !sprite->Frames())
		return;

	// If the sprite only has one frame, no need to animate anything.
	float frames = sprite->Frames();
	if(frames <= 1.f)
	{
		frame = 0.f;
		return;
	}
	float lastFrame = frames - 1.f;
	// This is the number of frames per full cycle. If rewinding, a full cycle
	// includes the first and last frames once and every other frame twice.
	float cycle = (this->anim.rewind ? 2.f * lastFrame : frames) + anim.delay;

	// If this is the very first step, fill in some values that we could not set
	// until we knew the sprite's frame count and the starting step.
	if(this->anim.randomizeStart)
	{
		this->anim.randomizeStart = false;
		// The random offset can be a fractional frame.
		frameOffset += static_cast<float>(Random::Real()) * cycle;
	}
	else if(this->anim.startAtZero)
	{
		this->anim.startAtZero = false;
		// Adjust frameOffset so that this step's frame is exactly 0 (no fade).
		frameOffset = -this->anim.frameRate * step;
		frameOffset += this->anim.startFrame;
	}

	// Figure out what fraction of the way in between frames we are. Avoid any
	// possible floating-point glitches that might result in a negative frame.
	int prevFrame = static_cast<int>(frame), nextFrame = -1;
	frame = max(0.f, this->anim.frameRate * step + frameOffset);

	if(!stateTransitionRequested)
	{
		// For when it needs to be applied to transition
		delayed = 0.f;

		// If repeating, wrap the frame index by the total cycle time.
		if(this->anim.repeat)
			frame = fmod(frame, cycle);

		if(!this->anim.rewind)
		{
			// If not repeating, frame should never go higher than the index of the
			// final frame.
			if(!this->anim.repeat)
			{
				frame = min(frame, lastFrame);
				framePercentage = (frame + 1) / frames;
				if(framePercentage >= this->anim.indicatePercentage)
					stateReady = this->anim.indicateReady;
				else
					stateReady = false;
			}
			else if(frame >= frames)
			{
				stateReady = false;
				// If in the delay portion of the loop, set the frame to zero.
				frame = 0.f;
			}
		}
		else if(frame >= lastFrame)
		{
			// In rewind mode, once you get to the last frame, count backwards.
			// Regardless of whether we're repeating, if the frame count gets to
			// be less than 0, clamp it to 0.

			frame = max(0.f, lastFrame * 2.f - frame);

			stateReady = false;
		}

		nextFrame = static_cast<int>(frame);
		if(nextFrame != prevFrame)
			randomFrame = static_cast<int>(static_cast<float>(Random::Real()) * frames);
		else
			randomFrame += frame - nextFrame;
	}
	else
	{
		// Override any delay if the ship wants to jump, become disabled, or land
		bool ignoreDelay = this->transitionState == Body::BodyState::JUMPING ||
							this->transitionState == Body::BodyState::DISABLED ||
							this->transitionState == Body::BodyState::LANDING ||
							this->transitionState == Body::BodyState::TRIGGER;

		if(delayed >= this->anim.transitionDelay || ignoreDelay)
		{
			if(this->anim.transitionFinish && !this->anim.transitionRewind)
			{
				// Finish the ongoing state's animation, then transition
				frame = min(frame, lastFrame);

				if(frame >= lastFrame)
					this->FinishStateTransition();
			}
			else if(!this->anim.transitionFinish && this->anim.transitionRewind)
			{
				// Rewind the ongoing state's animation, then transition.
				frame = max(0.f, rewindFrame * 2.f - frame);

				if(frame == 0.f)
					this->FinishStateTransition();

			}
			stateReady = false;
		}
		else
		{
			delayed += (step - currentStep) * this->anim.frameRate;
			// Maintain last frame of animation in delay
			if(frame >= lastFrame)
				frameOffset -= (step - currentStep) * this->anim.frameRate;
			frame = min(frame, lastFrame);
			// Rewind frame needs to be set since transition state can change during delay period.
			rewindFrame = frame;
		}
		// Prevent any flickers from transitioning into states with randomized frames.
		randomFrame = frame;
	}
	currentStep = step;
}
