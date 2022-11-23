/* SpriteParameters.h
Copyright (c) 2022 by XessWaffle

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef SPRITE_PARAMETERS_H
#define SPRITE_PARAMETERS_H

#include <map>
#include <string>
#include <tuple>

class Sprite;

// Class holding all of the animation parameters required to animate a sprite
class SpriteParameters {
public:
	// A node defining the parameters for a certain trigger sprite
	class AnimationParameters {
	public:
		// Act like a struct
		// Animation parameters as found in Body.h
		float frameRate = 2.f / 60.f;
		float startFrame = 0.f;
		float scale = 1.f;
		float indicatePercentage = -1.0f;
		int delay = 0;
		int transitionDelay = 0;

		bool startAtZero = false;
		bool randomize = false;
		bool randomizeStart = false;
		bool repeat = true;
		bool rewind = false;

		// Defines what to do when a state transition is requested (eg. FLYING to LANDING)
		bool transitionFinish = false;
		bool transitionRewind = false;
		bool indicateReady = false;
		bool triggerOnUse = false;
	};
	
public:
	SpriteParameters();
	explicit SpriteParameters(const Sprite *sprite);

	// Add a sprite-trigger mapping
	void SetSprite(std::string trigger, const Sprite *sprite, SpriteParameters::AnimationParameters data);
	// Get the sprite associated with the current trigger
	const Sprite *GetSprite() const;
	const Sprite *GetSprite(std::string trigger) const;

	// Set the current trigger
	std::string GetTrigger() const;
	bool RequestTrigger(std::string trigger);
	bool RequestTriggerOnUse(std::string trigger, bool use);
	bool IsCurrentTrigger(std::string trigger) const;
	bool IsTrigger(std::string trigger) const;

	void CompleteTriggerRequest();

	// Used for saving the sprites.
	const std::map<std::string, std::tuple<const Sprite *, SpriteParameters::AnimationParameters>> *GetAllSprites() const;


public:
	// Animation parameters exposed to Body
	SpriteParameters::AnimationParameters exposed;


private:
	// Used to trigger different animations
	std::string trigger = "default", requestedTrigger = "default";

	// Sprites to be animated
	std::map<std::string, std::tuple<const Sprite*, SpriteParameters::AnimationParameters>> sprites;
};



#endif
