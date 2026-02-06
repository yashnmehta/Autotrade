#ifndef WINDOW_CONSTANTS_H
#define WINDOW_CONSTANTS_H

/**
 * @brief Shared constants for window management system
 *
 * These constants are used across CustomMDIArea, CustomMDISubWindow,
 * and WindowCacheManager to ensure consistent behavior.
 */
namespace WindowConstants {

// Off-screen position for hidden cached windows
// Windows are moved here instead of being hidden for faster re-show
constexpr int OFF_SCREEN_X = -10000;
constexpr int OFF_SCREEN_Y = -10000;

// Threshold to determine if window is visible (not off-screen)
// Windows with x < VISIBLE_THRESHOLD_X are considered invisible
constexpr int VISIBLE_THRESHOLD_X = -1000;
constexpr int VISIBLE_THRESHOLD_Y = -1000;

// Cascade offset for window arrangement
constexpr int CASCADE_OFFSET = 30;

// Resize border width for edge detection
constexpr int RESIZE_BORDER_WIDTH = 8;

// Snap distance for edge snapping
constexpr int SNAP_DISTANCE = 15;

} // namespace WindowConstants

#endif // WINDOW_CONSTANTS_H
