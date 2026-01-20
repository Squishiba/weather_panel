#ifndef DEBUG_CONSOLE_H
#define DEBUG_CONSOLE_H

#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/timer.h"

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <sstream>

#include "display/weather_display_handler.h"
#include "misc.h"

void usb_char_available(void *ptr)
{
    bool *point = (bool *)ptr;
    *point = true;
}

namespace weather
{

    /**
     * @brief Mock weather data generator for testing different weather conditions
     */
    class MockWeatherGenerator
    {
    public:
        /**
         * @brief Generate mock weather data for testing
         *
         * Available weather types:
         * - clear, sunny, cloudy, fog
         * - rain, drizzle, heavy_rain
         * - snow, light_snow, heavy_snow, flurries
         * - freezing_rain, ice_pellets
         * - thunderstorm
         */
        static std::vector<std::map<std::string, std::string>> generate(
            const std::string &weather_type, int num_days = 3)
        {
            std::vector<std::map<std::string, std::string>> intervals;

            // Map simple names to weather codes
            static const std::map<std::string, int> weather_codes = {
                {"clear", 10000},
                {"sunny", 10000},
                {"mostly_clear", 11000},
                {"partly_cloudy", 11010},
                {"mostly_cloudy", 11020},
                {"cloudy", 10010},
                {"fog", 20000},
                {"light_fog", 21000},
                {"drizzle", 40000},
                {"light_rain", 42000},
                {"rain", 40010},
                {"heavy_rain", 42010},
                {"flurries", 50010},
                {"light_snow", 51000},
                {"snow", 50000},
                {"heavy_snow", 51010},
                {"freezing_drizzle", 60000},
                {"light_freezing_rain", 62000},
                {"freezing_rain", 60010},
                {"heavy_freezing_rain", 62010},
                {"light_ice_pellets", 71020},
                {"ice_pellets", 70000},
                {"heavy_ice_pellets", 71010},
                {"thunderstorm", 80000}};

            // Get weather code or default to clear
            int code = 10000;
            auto it = weather_codes.find(weather_type);
            if (it != weather_codes.end())
            {
                code = it->second;
            }

            // Determine precipitation type and intensity
            std::string precip_type = "0"; // N/A
            float precip_intensity = 0.0f;
            float snow_accumulation = 0.0f;
            float ice_accumulation = 0.0f;

            std::string type_lower = weather_type;
            std::transform(type_lower.begin(), type_lower.end(), type_lower.begin(),
                           [](unsigned char c)
                           { return std::tolower(c); });

            if (type_lower.find("rain") != std::string::npos ||
                type_lower.find("drizzle") != std::string::npos)
            {
                if (type_lower.find("freezing") != std::string::npos)
                {
                    precip_type = "3"; // Freezing Rain
                    precip_intensity = type_lower.find("light") != std::string::npos ? 0.5f : type_lower.find("heavy") != std::string::npos ? 2.0f
                                                                                                                                            : 1.0f;
                }
                else
                {
                    precip_type = "1"; // Rain
                    precip_intensity = type_lower.find("light") != std::string::npos ||
                                               type_lower.find("drizzle") != std::string::npos
                                           ? 0.3f
                                       : type_lower.find("heavy") != std::string::npos ? 3.0f
                                                                                       : 1.5f;
                }
            }
            else if (type_lower.find("snow") != std::string::npos ||
                     type_lower.find("flurries") != std::string::npos)
            {
                precip_type = "2"; // Snow
                precip_intensity = type_lower.find("light") != std::string::npos ||
                                           type_lower.find("flurries") != std::string::npos
                                       ? 0.2f
                                   : type_lower.find("heavy") != std::string::npos ? 2.5f
                                                                                   : 1.0f;
                snow_accumulation = 2.5f * precip_intensity;
            }
            else if (type_lower.find("ice") != std::string::npos)
            {
                precip_type = "4"; // Ice Pellets
                precip_intensity = type_lower.find("light") != std::string::npos ? 0.3f : type_lower.find("heavy") != std::string::npos ? 2.0f
                                                                                                                                        : 1.0f;
                ice_accumulation = 1.0f * precip_intensity;
            }

            // Generate intervals for each day
            for (int i = 0; i < num_days; ++i)
            {
                std::map<std::string, std::string> interval;

                interval["weather_description"] = weather_code_parser(code);
                interval["weatherCodeDay"] = std::to_string(code);
                interval["temperature"] = std::to_string(65 + (i * 2));

                int cloud_cover = 10;
                if (type_lower.find("mostly_cloudy") != std::string::npos)
                    cloud_cover = 80;
                else if (type_lower.find("partly_cloudy") != std::string::npos)
                    cloud_cover = 60;
                else if (type_lower.find("cloudy") != std::string::npos)
                    cloud_cover = 90;
                else if (type_lower.find("fog") != std::string::npos)
                    cloud_cover = 100;
                interval["cloudCover"] = std::to_string(cloud_cover);

                interval["windDirection"] = std::to_string(180 + (i * 45));
                interval["windSpeed"] = std::to_string(10 + (i * 3));
                interval["precipitationProbability"] = precip_type != "0" ? "90" : "5";
                interval["precipitationType"] = precip_type;
                interval["precipitationIntensity"] = std::to_string(precip_intensity);
                interval["snowAccumulation"] = std::to_string(snow_accumulation);
                interval["iceAccumulation"] = std::to_string(ice_accumulation);
                interval["sunriseTime"] = "07:30:00";
                interval["sunsetTime"] = "18:45:00";

                // Day names
                const char *days[] = {"Monday", "Tuesday", "Wednesday"};
                interval["day"] = days[i % 3];

                intervals.push_back(interval);
            }

            return intervals;
        }

        /**
         * @brief Get list of all valid weather types for testing
         */
        static std::vector<std::string> get_valid_types()
        {
            return {
                "clear", "sunny", "mostly_clear", "partly_cloudy", "mostly_cloudy", "cloudy",
                "fog", "light_fog", "drizzle", "light_rain", "rain", "heavy_rain",
                "flurries", "light_snow", "snow", "heavy_snow",
                "freezing_drizzle", "light_freezing_rain", "freezing_rain", "heavy_freezing_rain",
                "light_ice_pellets", "ice_pellets", "heavy_ice_pellets", "thunderstorm"};
        }
    };

    /**
     * @brief Interactive debug console for testing weather effects
     */
    class DebugConsole
        {
    private:

        static constexpr std::array modifiables = {"FPS", "Gravity Magnitude"};

        WeatherDisplayHandler &weather_handler_;
        std::string current_weather_ = "";
        uint32_t frame_count_ = 0;
        uint32_t prev_time_ = 0;

        static float read_target()
        {
            char input[64];
            printf("Input new target: \n");

            read_line(input, sizeof(input));
            std::string line = process_line(input);

            float target;

            std::stringstream ss(line);
            ss >> target;

            printf("Read : %f \n", target);

            return target;
        }

        void update_fps_target()
        {
            weather_handler_.set_new_fps_target(read_target());
        }

        void update_gravity()
        {
            weather_handler_.set_new_gravity(read_target());
        }

        static void print_help()
        {
            printf("\n=== Weather Display Testing Mode ===\n");
            printf("Type a weather condition to test:\n\n");

            printf("Clear/Sunny:\n");
            printf("  clear, sunny, mostly_clear, partly_cloudy, mostly_cloudy, cloudy\n\n");

            printf("Fog:\n");
            printf("  fog, light_fog\n\n");

            printf("Rain:\n");
            printf("  drizzle, light_rain, rain, heavy_rain\n\n");

            printf("Snow:\n");
            printf("  flurries, light_snow, snow, heavy_snow\n\n");

            printf("Freezing:\n");
            printf("  freezing_drizzle, light_freezing_rain, freezing_rain, heavy_freezing_rain\n\n");

            printf("Ice:\n");
            printf("  light_ice_pellets, ice_pellets, heavy_ice_pellets\n\n");

            printf("Storms:\n");
            printf("  thunderstorm\n\n");

            printf("Commands:\n");
            printf("  help - Show this help\n");
            printf("  list - List all available weather types\n");
            printf("  exit - Exit test mode\n\n");

            printf("How to use:\n");
            printf("  1. Type a weather type and press Enter\n");
            printf("  2. Weather will animate continuously on the display\n");
            printf("  3. Type 'stop' to change weather\n");
            printf("=====================================\n\n");
        }

        void print_list()
        {
            printf("\nAll available weather types:\n");
            auto types = MockWeatherGenerator::get_valid_types();
            int count = 0;
            for (const auto &type : types)
            {
                printf("%s", type.c_str());
                count++;
                if (count % 4 == 0)
                {
                    printf("\n");
                }
                else
                {
                    printf(", ");
                }
            }
            printf("\n\n");

            printf("\nModifiable values are: \n");

            for (const auto &modify : modifiables)
                {
                printf("%s ,", modify);
                }
        }

        static bool is_valid_type(const std::string &type)
        {
            auto valid_types = MockWeatherGenerator::get_valid_types();
            return std::find(valid_types.begin(), valid_types.end(), type) != valid_types.end();
        }

        static void read_line(char *buffer, size_t max_len)
        {
            size_t pos = 0;
            while (pos < max_len - 1)
            {

                int c = stdio_getchar_timeout_us(100000); // 100ms timeout
                if (c == PICO_ERROR_TIMEOUT)
                {
                    continue;
                }
                if (c == '\n' || c == '\r')
                {
                    buffer[pos] = '\0';
                    printf("\n");
                    return;
                }
                if (c == 127 || c == 8)
                { // Backspace
                    if (pos > 0)
                    {
                        pos--;
                        printf("\b \b");
                    }
                    continue;
                }
                if (c >= 32 && c < 127)
                { // Printable characters
                    buffer[pos++] = c;
                    stdio_putchar(c);
                }
            }
            buffer[pos] = '\0';
        }

        static inline std::string process_line(const char *input)
        {
            // Convert to lowercase
            std::string line(input);
            std::transform(line.begin(), line.end(), line.begin(),
                           [](unsigned char c)
                           { return std::tolower(c); });

            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t\n\r"));
            line.erase(line.find_last_not_of(" \t\n\r") + 1);

            printf("Received: %s\n", line.c_str());
            return line;
        }

    public:
        DebugConsole(WeatherDisplayHandler &weather_handler)
            : weather_handler_(weather_handler) {}

        void run()
        {

            bool serial_waiting = false;

            stdio_set_chars_available_callback(usb_char_available, (void *)&serial_waiting);

            printf("\n=== Weather Display Debug Console ===\n");
            printf("USB is connected - entering debug mode\n\n");

            sleep_ms(1);

            print_help();

            char input[64];

            bool animating = false;
            uint32_t last_status_time = 0;
            uint32_t pos = 0;

            while (true)
            {

                // If animating, continuously update display

                if (animating == true)
                {
                    weather_handler_.refresh_and_update_display();
                    frame_count_++;
                    uint32_t now = time_us_32();
                    uint32_t delta = now - last_status_time;

                    if ((frame_count_) % 100 == 0)
                    {
                        float fps_approx = 1.0f / (delta / 1'000'000.0f);
                        printf("Approximate FPS: %f \n", fps_approx);
                        printf("[Animating - frame %lu. Press any key to halt/change weather.]\n", frame_count_);
                        printf("Total Particle Count: %d. \n", weather_handler_.get_total_particle_count());
                    }

                    last_status_time = now;
                    // Check for input (non-blocking)

                    if (serial_waiting == true)
                    {
                        serial_waiting = false;
                        read_line(input, sizeof(input));
                        std::string line = process_line(input);
                        if (line == "exit" || line == "stop" || line == "halt")
                        {
                            // User pressed a key, stop animating
                            printf("\n[Animation paused. Enter new weather type or command.]\n");
                            animating = false;
                            current_weather_ = "";
                            frame_count_ = 0;
                        }

                        serial_waiting = false;
                    }
                    prev_time_ = time_us_32();
                }
                else
                {
                    // Not animating, wait for user input
                    printf("\n> Enter weather type (or 'help'): ");
                    read_line(input, sizeof(input));
                    std::string line = process_line(input);

                    if (line.empty())
                    {
                        continue;
                    }

                    if (line == "help")
                    {
                        print_help();
                    }

                    else if (line == "set_gravity_magnitude" || line == "set_grav" || line == "gravity")
                    {
                        update_gravity();
                    }

                    else if (line == "set_fps" || line == "fps" || line == "fps_set")
                    {
                        update_fps_target();
                    }

                    else if (line == "unlock" || line == "unlock_fps" || line == "fps_unlock")
                    {
                        weather_handler_.set_new_fps_target(8500.0f);
                        printf("FPS Unlocked successfully.");
                    }

                    else if (line == "list")
                    {
                        print_list();
                    }

                    else if (line == "exit" || line == "quit" || line == "return")
                    {
                        printf("Exiting debug console...\n");
                        return;
                    }

                    else if (is_valid_type(line))
                    {
                        // Generate and display the weather
                        printf("\nGenerating mock data for: %s\n", line.c_str());
                        auto mock_data = MockWeatherGenerator::generate(line, 3);

                        printf("Updating display...\n");
                        weather_handler_.update_weather(mock_data);

                        printf("✓ Display updated successfully!\n");
                        printf("Weather: %s\n", line.c_str());

                        // Start animating
                        current_weather_ = line;
                        frame_count_ = 0;
                        animating = true;

                        printf("\n[Starting animation loop. Press any key to change weather.]\n\n");
                    }
                    else
                    {
                        printf("✗ Error: '%s' is not a valid weather type.\n", line.c_str());
                        printf("Type 'list' to see all available weather types or 'help' for more info.\n");
                    }
                }
            }
        }
    };

} // namespace weather

#endif // DEBUG_CONSOLE_H
