#ifndef SNOW_H
#define SNOW_H

#include "pico/stdlib.h"
#include "helpers_rand.h"

#include "display/weather_effect_base.h"
#include "particles/particle.h"

#include <cmath>
#include <memory>

namespace weather
	{
		/**
		 * @brief Snow effect - renders falling snowflakes with accumulation
		 */
		class SnowEffect : public WeatherEffectBase
			{
		private:
			static constexpr float MM_TO_INCH_CONST = 0.0393700787f;
			static constexpr float STICK_PROBABILITY = 0.3f; // 30% chance to stick

			uint16_t accumulation_depth_pixels_;
			uint16_t current_accumulation_height_ = 0; // Current snow height on ground
			Color snow_color_ = kWhite;
			bool is_ice_; // True for ice pellets
			DisplaySegProperties &seg_properties_;

			// Convert snow accumulation (mm) to pixels
			static uint16_t depth_to_pixels(float snow_depth, int16_t display_height)
				{
				const float inches = snow_depth * MM_TO_INCH_CONST;
				const float height_div = (display_height / 4.0f);
				const uint16_t height_pixels = roundf((inches / height_div) * 100.0f);
				return height_pixels;
				}

		public:
			SnowEffect(DisplaySegProperties &seg_properties, const float accumulation, const bool is_ice = false)
				: WeatherEffectBase(seg_properties), is_ice_(is_ice), seg_properties_(seg_properties)
				{
				// Calculate accumulation depth in pixels
				accumulation_depth_pixels_ = depth_to_pixels(accumulation, seg_properties_.get_seg_bounds().y_end);
				// Calculate spawn rate and max particles based on intensity
				max_particles_ = roundf(seg_properties_.get_seg_bounds().w * 1.2f * seg_properties_.get_intensity());
				}

			void update_particles() override
				{
				// Spawn new snowflakes based on spawn rate
				if (particles_.size() < static_cast<size_t>(max_particles_) && get_rand_float() < spawn_rate_)
					{
					particles_.push_back(std::make_unique<Snow>(seg_properties_));
					}

				windgust_.update();

				if (windgust_.is_gust_alive())
					{
					// Update all particles...
					for (const auto &flake: particles_)
						{
						windgust_.apply_wind(flake->get_positions(), flake->get_acceleration());
						flake->update();
						}
					}
				else
					{
					for (const auto &flake: particles_)
						{
						flake->update();
						}
					}
				}

			void draw(pimoroni::PicoZGraphics &graphics) override
				{
				// Draw falling snowflakes
				for (const auto &flake: particles_)
					{
					if (flake->is_drawable())
						{
						Position &position = flake->get_positions();
						graphics.set_pen((position.z * snow_color_.r), (position.z * snow_color_.g), (position.z * snow_color_.b));
						graphics.set_depth(position.z);
						pimoroni::Point new_point(position.x, position.y);
						graphics.set_pixel(new_point);
						}
					}

				graphics.set_pen(255, 0, 0);
				graphics.set_depth(255);
				graphics.set_pixel(pimoroni::Point(windgust_.get_positions().x, windgust_.get_positions().y));
				}

			void stop() override
				{
				particles_.clear();
				current_accumulation_height_ = 0;
				}

			// Get current accumulation height for debugging/display
			uint16_t get_accumulation_height() const
				{
				return current_accumulation_height_;
				}

			// Get target accumulation for debugging/display
			uint16_t get_target_accumulation() const
				{
				return accumulation_depth_pixels_;
				}
			};
	} // namespace weather

#endif // SNOW_H
