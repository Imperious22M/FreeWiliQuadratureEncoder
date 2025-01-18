// The purpose of this program is the simulate the output of a two
// pin quadrature encoder. This version only supports a hardcoded
// number of teeth and 1/4 period delay. 

#include "fwwasm.h"
#include <array>
#include <cstdint>
#include <ranges>
#include <climits>

// Usefult to note that the screen has a resolution of 
// 320 x 240 pixels

// All the X and Y cords of the widgets are harcoded
// due to limitations in dynamic locating of GUI components
// in the current software

// Number of LED's at the top of the device
const auto NUMBER_OF_LEDS = 7;

// Enum list to create indexes for all the GUI components
enum guiIndexes {panelIndex,
                transitionNumIndex,
                teethNumIndex,
                transitionTextIndex,
                plotControlIndex,
                revolutionTextIndex,
                revolutionNumIndex,
                teethTextIndex,
                refreshTextIndex,
                refreshNumberIndex,
                totalRefsNumberIndex,
                totalRefsTextIndex,
                directionTextIndex,
                directionNumberIndex,
                quadModeTextIndex,
                quadModeStateTextIndex};

// Pins to output the quadrature signal
// These correspond to GPIO pins in programming
// 13 -> 1 and 27 -> 3 in the pin numbers on the outside 
#define PinA 13 
#define PinB 27
#define MaxValueControl INT_MAX
#define MinValueControl INT_MIN

// Next state transition table of a quadrature encoder
// Representas all the LEGAL states of a qudrature encoder
// Encoded as a two dimensional table
// The first index is in incremented if the encoder
// is moving forward, otherwise it's decremented
// pinA is the first index, pinB is the second
const int nextStateTable[][4] = {{0,0},{1,0},{1,1},{0,1}};
int nextStateIndex = 0; //Initial state of the sensor
int direction = 1; // Direction of the encoder, 1 for increasing, 0 for decreasing

// How long before the quadrature encoder "pins"
// change state
// the sensor refresh rate will be the DRIVING variable, as it needs to be a positive WHOLE number
// in order for this to work...
unsigned int sensorRefreshRate = 10; // in milliseconds
// sensor refresh rate is essentially the 1/4 the period of freuency of pinA or pinB
unsigned long sensorOldMillis = 0; // temp variable used to store the millis time since the last refreshV

// "Sensor" simulated variables, like teeth# (simulating a gear-based qudrature encoder)
// and other parameters
unsigned int numberTeeth = 25; // Simulates the teeth # in the sensor "gear"
float revPerSecond = (1/(  static_cast<float>(sensorRefreshRate*4*numberTeeth)) )*1000; // Records the speed of the shaft
// We need to calculate the revolution per second
// based on the sensor refresh rate (time that EITHER pinA or pinB changes)
// This will be equal to: refreshrate/4 
int totalRefs = 0; // Stores the total number of revs the sensor has "traveled"

// Stores the mode that the virtual quadrature encoder is in
// 0 is free-running (just runs)
// 1 is up to a set tick limit
uint8_t quadMode = 0;
int tickLimit = 1;

// Struct to store colors as individual channels
struct Color {
    uint8_t red;
    uint8_t green;
    uint8_t blue;

    constexpr Color(uint8_t red, uint8_t green, uint8_t blue) : red(red), green(green), blue(blue) {}
};

// Initialize the color based on its RGB value
constexpr Color RED(255, 0, 0);
constexpr Color ORANGE(255, 127, 0);
constexpr Color YELLOW(255, 255, 0);
constexpr Color GREEN(0, 255, 0);
constexpr Color LIGHT_GREEN(0, 255, 191);
constexpr Color BLUE(0, 0, 255);
constexpr Color LIGHT_BLUE(0, 191, 255);
constexpr Color INDIGO(75, 0, 130);
constexpr Color VIOLET(238, 130, 238);
constexpr Color PINK(255, 192, 203);
[[maybe_unused]] constexpr Color GRAY(0x30, 0x30, 0x30);
constexpr Color WHITE(255, 255, 255);

// Struct to store multiple panels and the events to call them
// Not used for now
struct PanelInfo {
    const uint8_t index;
    const FWGuiEventType event_type;
    const Color color;
    const char* text;
    const char* sub_fname;
};

constexpr std::array Buttons{
    FWGuiEventType::FWGUI_EVENT_GRAY_BUTTON,  FWGuiEventType::FWGUI_EVENT_YELLOW_BUTTON,
    FWGuiEventType::FWGUI_EVENT_GREEN_BUTTON, FWGuiEventType::FWGUI_EVENT_BLUE_BUTTON,
    FWGuiEventType::FWGUI_EVENT_RED_BUTTON,
};

auto setupMainPanelMenu(){
    
    // Clear the event lists
    uint8_t event_data[FW_GET_EVENT_DATA_MAX] = {0};
    getEventData(event_data);
    //setCanDisplayReactToButtons(0);
    
    // Don't log anything, we don't need it
    setPanelMenuText(panelIndex,0,"DNU!");
    setPanelMenuText(panelIndex,1,"DNU!");
    setPanelMenuText(panelIndex,2,"TDir");
    setPanelMenuText(panelIndex,3,"Toggle");
    setPanelMenuText(panelIndex,4,"Exit");
}

// Helper function to setup panels 
auto setup_panels() -> void {
    // Setup the main panel
    // Adds one panel with ID 0 and visble
    addPanel(panelIndex, 1, 0, 0, 0, 0, 0, 0, 1);
    // setup main panel menu
    setupMainPanelMenu();




    // NUMBERS~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // This is the control number to show the number of
    // "ticks" (transitions) of signal A and B
    // It is incremented every transition
    addControlNumber(panelIndex,transitionNumIndex,1,
                    90,128,10,1,1,
                    0,255,0,0,0,0,0);
    setControlValue(panelIndex,transitionNumIndex,0);
    // This is the control number to show the
    // number of revolutions/second the wheel has "taken"
    // based on the number of "teeth" the wheel has
    addControlNumber(panelIndex,revolutionNumIndex,1,
                    205,20,10,1,1,
                    0,255,0,1,3,0,0);
//revPerSecond = 34;
    setControlValueFloat(panelIndex,revolutionNumIndex,revPerSecond);
    // This is the control number to show the number of
    // number of teeth the qudrature "gear" has
    addControlNumber(panelIndex,teethNumIndex,1,
                    205,1,10,1,1,
                    0,255,0,0,0,0,0);
    setControlValue(panelIndex,teethNumIndex,static_cast<int>(numberTeeth));
    // This is the control number to show the number of
    // millisecond delay for each change in the quadrature state
    // Based on this, the # of revolutions is calcuated
    addControlNumber(panelIndex,refreshNumberIndex,1,
                    215,43,10,1,1,
                    0,255,0,0,0,0,0);
    setControlValue(panelIndex,refreshNumberIndex,static_cast<int>(sensorRefreshRate));
    // Shows the total number of revolutions
    addControlNumber(panelIndex,totalRefsNumberIndex,1,
                    125,148,10,1,1,
                    0,255,0,0,0,0,0);
    setControlValue(panelIndex,totalRefsNumberIndex,totalRefs);
    // Shows the direction (1 is forward, 0 is backwards )
    addControlNumber(panelIndex,directionNumberIndex,1,
                    115,168,10,1,1,
                    0,255,0,0,0,0,0);
    setControlValue(panelIndex,directionNumberIndex,direction);

    // TEXT~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // This adds text next to the increment number, all calls to "addControlText" do the same thing
    addControlText(panelIndex,transitionTextIndex, 
                   3, 130, 1, 64, 
                   WHITE.red, WHITE.green, WHITE.blue, "Tick #:");
    // This adds the text "Rev #:" widget to the panel
    addControlText(panelIndex,revolutionTextIndex, 
                   110, 23, 1, 64, 
                   WHITE.red, WHITE.green, WHITE.blue, "Rev/Sec:");
    // This adds the text "Teeth #" widget to the panel
    addControlText(panelIndex,teethTextIndex, 
                   110, 3, 1, 64, 
                   WHITE.red, WHITE.green, WHITE.blue, "Teeth #:");
    // This adds the text for the sensor refresh time (1/4 period)
    // This value controls every transition for the emulated "quadrature"
    addControlText(panelIndex,refreshTextIndex, 
                   110, 46, 1, 64, 
                   WHITE.red, WHITE.green, WHITE.blue, "1/4T(ms):");
    // This adds the text for the total number of revolutions
    addControlText(panelIndex,totalRefsTextIndex, 
                   3, 150, 1, 64, 
                   WHITE.red, WHITE.green, WHITE.blue, "Total Revs:");
    // This adds the text for the "direction"
    addControlText(panelIndex,directionTextIndex, 
                   3, 170, 1, 64, 
                   WHITE.red, WHITE.green, WHITE.blue, "Direction:");
    // This adds the text for the "Mode"
    addControlText(panelIndex,quadModeTextIndex, 
                   110, 66, 1, 64, 
                   WHITE.red, WHITE.green, WHITE.blue, "Mode:");
    // This adds the text for the current quad mode, in text format
    // By default we are in free-running mode
    addControlText(panelIndex,quadModeStateTextIndex, 
                   166, 66, 1, 64, 
                   GREEN.red, GREEN.green, GREEN.blue, "FRun");
    //TODO set min/max for number control values

    // EXPERIMENTAL 
    // Somehow this works...I don't know why, but it does
    // It seems like I am tapping into a limited set of "channels" that the plot uses
    // There is no documentation that I can find that has this feature documented...
    // iPlotDataBitField <- What is that??
    // NOTES: Moving the plot itself to any offset in the Y axis (horizontally)
    // does not move the plot properly, plus other features also seem not ready yet...
    addControlPlot(panelIndex,plotControlIndex,1,
                   3,0,0,100,100,
                   0,2,10,120,30);
    //Adds the Red line control plot
    addControlPlotData(0,255,0,0);
    // EXPERIMENTAL
    // Apparently clears a bunch of the system "plot" channels.
    // Sometimes they carry over from the "sensor" and other applications
    // I do not know why this works, or why the index for my plot (the green line)
    // is actually 3-1, so the index I gave earlier MINUS one, which goes against
    // the current parameter name "iPlotIndexPlusOne"....
    for(int x=0;x<4;x++){
    clearLogOrPlotData(0,x);
    }


    //setCanDisplayReactToButtons(0);
    // Show the panel
    showPanel(panelIndex);
    
}

// Helper function to show of ranbow of LED's
auto show_rainbow_leds(const int max_loops) -> void {
    const std::array colors{RED, ORANGE, YELLOW, GREEN, LIGHT_GREEN, BLUE, LIGHT_BLUE, INDIGO, VIOLET, PINK};
    size_t color_choice = 0;
    // do the whole thing multiple times
    for (int loops = 0; loops < max_loops; loops++) {
        // set every LED one at a time

        for (int led = 0; led < NUMBER_OF_LEDS; led++) {
            // pick a color
            const Color* const color = &colors[color_choice];
            // set the LED
            setBoardLED(led, color->red, color->green, color->blue, 300, LEDManagerLEDMode::ledpulsefade);

            // next time, get a new color.  If we used all of the colors, start over
            color_choice++;
            if (color_choice >= colors.size()) {
                color_choice = 0;
            }
            // wait before setting the next LED
            waitms(50);
        }
    }
}

// Control the state of the simulate sensor outputs
// Arguments are if the simulated qudrature should increase by one tick/state
// or decrease by one tick/state
// direction: 0 = backwards, 1 = forwards
auto quadratureNextTick(int direction) -> void{
    
}

// Process all the events forever
// and do all the computations/IO control
// Essentially become the main "loop" like Arduino
auto process_events() -> void {

    // Flag to continue simulating or to stop the encoder simulation
    // Stops the simulation of the quadrature encoder when it is true
    // By default it starts in the stopped state
    bool stopSimulation = 1;

    // Transition counter WILL OVERFLOW IF LEFT FOR TOO LONG
    // Stores the number of transitions that either pinA or pinB has 
    // Every time we update the "encoder" pins this value changes 
    // so the encoder frequency is this number/4.
    int transitionCount = 0;

    // Temporary counter to determine when
    // to increment the revolution counter
    // It is equal to 4*number of teeth*revs/s
    int revTickThreshold = 4*static_cast<int>(numberTeeth)*static_cast<int>(revPerSecond);
    // Counter to hold number of ticks since last update to the 
    // total revolution counter
    int revTickCount = 0;
    
    // Stores the state of the pins
    // pinA is index 0, and pinB is index 1
    int sensorState[2] = {0};
    // Set the initial state of pinA and B
    setIO(PinA,sensorState[0]);
    setIO(PinB,sensorState[1]);
    



    while (true) {

        // Loop "delay" time, this is the smallest I can do for now
        waitms(1);
        
        // This section does the simulation of every transition change
        // of either PinA or PinB
        // Change only if we need to change the sensors
        // Driven by the sensor refresh rate
        if(millis()>=sensorOldMillis && !stopSimulation){
            sensorOldMillis = millis()+sensorRefreshRate;
            sensorState[0]  = nextStateTable[nextStateIndex][0];
            sensorState[1]  = nextStateTable[nextStateIndex][1];
            setIO(PinA,sensorState[0]);
            setIO(PinB,sensorState[1]);

            // Increment the next state index, and reset to proper index
            // if out of array bounds
            if(direction){
                nextStateIndex++;
                //Increase the transition counter
                transitionCount++;
                // Increase total revolution tick counter
                revTickCount++;

            }else{
                nextStateIndex--;
                transitionCount--;
                revTickCount--;
            }
            if(nextStateIndex>3){
                nextStateIndex = 0;
            }
            if(nextStateIndex<0){
                nextStateIndex = 3;
            }

            // Increment or decrement number of revolutions
            if(revTickCount==revTickThreshold||revTickCount==-revTickThreshold){
                revTickCount=0;
                if(direction){
                    totalRefs++;
                }else{
                    totalRefs--;
                }
            }

            // Check if we are in any other mode and change the behavior as appropriate

            

            // EXPERIMENTAL
            // Show the state of PinA on the plot
            // To be honest, I do not know why this works, it just does...
            // The iSettings parameter (second parameter) does not seem to have
            // any use, it does not change anything that I can see.
            setPlotData(1,1,sensorState[0]);
            //for(int x=2;x<6;x++)
            //    setPlotData(x,1,sensorState[1]);
        }
        

        // Update the GUI's number of transititions
        setControlValue(panelIndex,transitionNumIndex,transitionCount);
        // Update the GUI's total number of revolutions
        setControlValue(panelIndex,totalRefsNumberIndex,totalRefs);

        // Keep adding values to the Plot "buffer" in order for the scrolling to show.
        // It seems to work based on the # of values you add, aka every value
        // causes the plot to scroll to the left
        setPlotData(1,1,sensorState[0]); // Plot pinA's state
        // Update the red line control plot
        setPlotData(0,1,sensorState[1]); // Plot pinA's state
        
        
        // If there are no events (button clicks/sensors)
        // to process skip the rest of the loop
        if (hasEvent() == 0) {
            continue;
        }

        // Get the list of events that we need to process
        uint8_t event_data[FW_GET_EVENT_DATA_MAX] = {0};
        auto last_event = getEventData(event_data);

        // If there is any event to edit numbers, go back to the main screen
        // as it is not supported now
        if(last_event == FWGUI_EVENT_GUI_NUMEDIT){
            showPanel(panelIndex);
            // TODO: Add recalculation of all 
            // numbers upon edit
        }

        // We only want to process button presses
        // Skip the rest if the last event received was not a button event
        if (std::find(std::begin(Buttons), std::end(Buttons), last_event) == std::end(Buttons)) {
            continue;
        }
        
        // Respond to buttons and events ~~~~~~~~~~~~~~~~~~~~~~~
        // The button responses are WEIRD, maybe even BROKEN
        // I found no way to stop the grey button from opening
        // some sort of "debug" window, or the yellow button 
        // from selecting the elements in the GUI
        // There seems to be a function in fwwasm.h that is trying
        // to address this (it seems?) but it FAILS to do so
        // and just opens up a blank window that I can't do anything
        // about.
        // aka this function: setCanDisplayReactToButtons

        // When the Gray button is pressed, do not show the debug window!
        if (last_event == FWGuiEventType::FWGUI_EVENT_GRAY_BUTTON) {
            // Override the debug window that it usually pop ups with
           showPanel(panelIndex);

        }

        // "Toggle" the simulation of the quadrature encoder when pressed
        if (last_event == FWGuiEventType::FWGUI_EVENT_BLUE_BUTTON) {
           stopSimulation = !stopSimulation;
        }
        // "Toggle" direction of the "quadrature"
        if (last_event == FWGuiEventType::FWGUI_EVENT_GREEN_BUTTON) {
            if(direction){
                direction = 0;
                // Update the direction on the screen
                setControlValue(panelIndex,directionNumberIndex,direction);
            }else{
                direction = 1;
                setControlValue(panelIndex,directionNumberIndex,direction);
            }
        }
        // If the button pressed was the RED button, then exit the application
        // This is the exit condition to go back to the main menu
        // To exit one MUST return to the main function and allow 
        // it to return.
        if (last_event == FWGuiEventType::FWGUI_EVENT_RED_BUTTON) {
           return;
        }

    }
}

auto main() -> int {

    // Setup the main panel 
    setup_panels();
    // Show a cool rainbow show of LED's :)
    show_rainbow_leds(2);

    // Process all the events indefinetly
    // functions just like loop() in Arduino
    process_events();

    // The app is not done executing until we return from main
    return 0;
}