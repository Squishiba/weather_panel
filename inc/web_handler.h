#ifndef WEB_HANDLER_H
#define WEB_HANDLER_H

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/time.h"

#include <string>
#include <vector>
#include <map>

namespace weather
{

    /**
     * @brief Handles all web-based functionality (WiFi, HTTP requests, weather API)
     *
     * This class manages:
     * - WiFi connection
     * - NTP time synchronization
     * - Geolocation via IP
     * - Weather data fetching from Tomorrow.io API
     */
    class WebHandler
    {
    private:
        std::string ssid_;
        std::string password_;
        std::string api_key_;
        float latitude_;
        float longitude_;
        bool connected_;

    public:
        WebHandler(const std::string &ssid, const std::string &password, const std::string &api_key)
            : ssid_(ssid), password_(password), api_key_(api_key), latitude_(0.0f), longitude_(0.0f), connected_(false)
        {
        }

        /**
         * @brief Initialize WiFi and connect to network
         * @return true if connection successful
         */
        bool initialize()
        {
            printf("Initializing WiFi...\n");

            // Initialize CYW43 WiFi chip
            if (cyw43_arch_init())
            {
                printf("Failed to initialize WiFi chip\n");
                return false;
            }

            // Enable station mode
            cyw43_arch_enable_sta_mode();

            // Connect to WiFi
            if (!network_connect())
            {
                return false;
            }

            // Get location from IP
            get_location_from_ip();

            return true;
        }

        /**
         * @brief Connect to WiFi network
         * @return true if successful
         */
        bool network_connect()
        {
            printf("Connecting to WiFi '%s'...\n", ssid_.c_str());

            // Disable power saving for better reliability
            cyw43_wifi_pm(&cyw43_state, CYW43_NO_POWERSAVE_MODE);

            // Attempt connection with timeout
            int max_wait = 10; // 10 second timeout
            if (cyw43_arch_wifi_connect_timeout_ms(ssid_.c_str(), password_.c_str(),
                                                   CYW43_AUTH_WPA2_AES_PSK, 10000))
            {
                printf("Failed to connect to WiFi\n");
                connected_ = false;
                return false;
            }

            printf("Connected to WiFi successfully\n");
            connected_ = true;
            return true;
        }

        /**
         * @brief Synchronize time using NTP
         * @return true if successful
         */
        bool sync_time()
        {
            // TODO: Implement NTP client
            // This would require implementing SNTP protocol
            // For now, this is a placeholder
            printf("Time sync not yet implemented\n");
            return false;
        }

        /**
         * @brief Get geographic location from IP address
         */
        void get_location_from_ip()
        {
            // TODO: Implement HTTP GET request to ip-api.com
            // This requires implementing HTTP client functionality
            // For now, use default coordinates
            printf("Geolocation not yet implemented, using default coordinates\n");
            latitude_ = 40.7128f; // New York City as default
            longitude_ = -74.0060f;
        }

        /**
         * @brief Fetch weather data from Tomorrow.io API
         * @param num_days Number of days to fetch (1-3)
         * @return Vector of weather interval data (one map per day)
         */
        std::vector<std::map<std::string, std::string>> get_weather_data(int num_days)
        {
            std::vector<std::map<std::string, std::string>> weather_intervals;

            // TODO: Implement HTTP GET request to Tomorrow.io API
            // This requires:
            // 1. Building the URL with parameters
            // 2. Making HTTP GET request
            // 3. Parsing JSON response
            // 4. Extracting weather data for each day
            //
            // For now, return empty data
            printf("Weather API fetch not yet implemented\n");

            // Return empty intervals for now
            for (int i = 0; i < num_days; ++i)
            {
                std::map<std::string, std::string> interval;
                interval["weather_description"] = "Clear, Sunny";
                interval["weatherCodeDay"] = "10000";
                interval["temperature"] = "65";
                interval["windSpeed"] = "10";
                interval["windDirection"] = "180";
                interval["cloudCover"] = "10";
                interval["precipitationType"] = "0";
                interval["precipitationIntensity"] = "0";
                interval["snowAccumulation"] = "0";
                interval["iceAccumulation"] = "0";
                interval["day"] = "Monday";
                weather_intervals.push_back(interval);
            }

            return weather_intervals;
        }

        // Getters
        float latitude() const { return latitude_; }
        float longitude() const { return longitude_; }
        bool is_connected() const { return connected_; }
    };

} // namespace weather

#endif // WEB_HANDLER_H
