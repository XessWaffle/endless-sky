/* SpriteParameters.cpp
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

#include "SpriteParameters.h"

#include "Sprite.h"

SpriteParameters::SpriteParameters()
{

}

SpriteParameters::SpriteParameters(const Sprite *sprite)
{
	AnimationParameters initDefault;
	auto tuple = std::tuple<const Sprite*, AnimationParameters>{sprite, initDefault};
	this->sprites.insert(std::pair<std::string, std::tuple<const Sprite*,AnimationParameters>>("default", tuple));
	this->exposed = initDefault;
}

void SpriteParameters::SetSprite(std::string trigger, const Sprite *sprite, AnimationParameters params)
{
	auto tuple = std::tuple<const Sprite*, AnimationParameters>{sprite, params};
	this->sprites.insert(std::pair<std::string, std::tuple<const Sprite*, AnimationParameters>>(trigger, tuple));
	if(trigger == "default")
		this->exposed = params;
}

const Sprite *SpriteParameters::GetSprite() const
{
	auto it = this->sprites.find(this->trigger);
	return (it == this->sprites.end()) ? nullptr : std::get<0>(it->second);
}

const Sprite *SpriteParameters::GetSprite(std::string trigger) const
{
	auto it = this->sprites.find(trigger);
	return (it == this->sprites.end()) ? nullptr : std::get<0>(it->second);
}

void SpriteParameters::SetTrigger(std::string trigger)
{
	this->trigger = trigger;

	auto it = this->sprites.find(trigger);
	auto def = this->sprites.find("default");
	auto use = it == this->sprites.end() ? def : it;

	AnimationParameters toExpose = std::get<1>(use->second);

	this->exposed = toExpose;
}

bool SpriteParameters::IsTrigger(std::string trigger) const
{
	auto it = this->sprites.find(trigger);
	return it != this->sprites.end();
}

const std::map<std::string, std::tuple<const Sprite*, AnimationParameters>> *SpriteParameters::GetAllSprites() const
{
	return &this->sprites;
}