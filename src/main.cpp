#include "main.h"

// Display constants
constexpr uint16_t kDisplay_Width = 128;
constexpr uint8_t kDisplay_Height = 128;
constexpr uint8_t NUM_DAYS = 3;
constexpr uint8_t MINS_BEFORE_REFRESH = 5;
constexpr uint32_t kClockspeed = 180'000;

using namespace pimoroni;

// Our PicoGraphics surface.
PicoZGraphics graphics(kDisplay_Width, kDisplay_Height, nullptr, 1);
// This is what is written to when drawing.

Hub75 hub75(kDisplay_Width * 2, kDisplay_Height / 2, nullptr, PANEL_GENERIC, false);
// this is the actual display driver/object.
// The internal driver doesn't technically support 128 x 128 displays... We have to
// map it to a 256 x 64 display in order for it to function properly.
// Note this doesn't change anything about how it's interacted with; the graphics object
// uses 128 x 128 pixels as expected.

static void start_weather_display();
static void debug_console();

// Interrupt callback, required for the HUB75 driver.
void __isr dma_complete()
{
    hub75.dma_complete();
}

bool reserved_addr(uint8_t addr)
{
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

int main()
{

    stdio_usb_init();          // Start the USB printf output.
    hub75.start(dma_complete); // Start the HUB75 driver.
    set_sys_clock_khz(kClockspeed, true);
    graphics.clear_framebuffer();
    hub75.update(&graphics);
    gpio_set_function(pimoroni::I2C_DEFAULT_SDA, GPIO_FUNC_I2C);
    gpio_set_function(pimoroni::I2C_DEFAULT_SCL, GPIO_FUNC_I2C);

    i2c_init(i2c0, pimoroni::I2C_DEFAULT_BAUDRATE);
    
    sleep_ms(100);

    while (1)
    {

        // Initialize display hardware
        printf("Weather Display System Starting...\n");
        printf("Display: %dx%d, Days: %d\n", kDisplay_Width, kDisplay_Height, NUM_DAYS);

        if (stdio_usb_connected())
        {
            printf("USB detected - entering debug mode\n");
            debug_console();
        }
        else
        {
            printf("Running in normal mode\n");
            start_weather_display();
        }
    }
}

using namespace weather;

/**
 * @brief Main weather display program
 *
 * This function:
 * 1. Connects to WiFi
 * 2. Syncs time via NTP
 * 3. Gets location from IP
 * 4. Fetches weather data
 * 5. Continuously updates and displays weather
 */
static void start_weather_display()
{
    printf("Initializing weather display...\n");

    /*
    // Create web handler
    WebHandler web(NETWORK, PASSWORD, TOMORROW_IO_KEY);

    // Initialize WiFi and get location
    if (!web.initialize()) {
        printf("Failed to initialize web handler\n");
        return;
    }
    */

    // Create weather display handler
    // WeatherDisplayHandler weather_handler(graphics, hub75, NUM_DAYS);

    // Fetch initial weather data
    printf("Fetching weather data...\n");
    // auto weather_data = web.get_weather_data(NUM_DAYS);
    // weather_handler.update_weather(weather_data);

    // Get current time for refresh tracking
    // datetime_t current_time;
    // rtc_get_datetime(&current_time);
    // uint8_t prev_minute = current_time.min;

    printf("Entering main display loop...\n");

    /*
    // Main display loop
    while (true) {
        // Check if it's time to refresh weather data
        //rtc_get_datetime(&current_time);
        if (current_time.min >= (prev_minute + MINS_BEFORE_REFRESH)) {
            prev_minute = current_time.min;
            printf("Refreshing weather data at %02d:%02d:%02d...\n",
                   current_time.hour, current_time.min, current_time.sec);
            weather_data = web.get_weather_data(NUM_DAYS);
            weather_handler.update_weather(weather_data);
        }

        // Render frame
        weather_handler.refresh_and_update_display();

        // Small delay to control frame rate
        sleep_us(5);
    }
    */
}

/**
 * @brief Debug console for testing weather effects interactively
 */
static void debug_console()
{
    printf("\n=== Weather Display Debug Mode ===\n");
    printf("USB is connected - entering debug mode\n\n");

    // Create weather display handler (no WiFi needed for debug mode)
    WeatherDisplayHandler weather_handler(graphics, hub75, NUM_DAYS, 75.0f);

    // Create and run debug console
    DebugConsole console(weather_handler);
    console.run();

    printf("Debug console exited.\n");
}
