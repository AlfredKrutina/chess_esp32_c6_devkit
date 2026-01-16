/**
 * @file game_colors.h
 * @brief ANSI Color Codes and Formatting Macros for Terminal Output
 * 
 * This header provides macros to simplify ANSI terminal formatting.
 * Eliminates repetitive escape sequences throughout the codebase.
 * 
 * @author Alfred Krutina
 * @version 1.0
 * @date 2025-12-22
 */

#ifndef GAME_COLORS_H
#define GAME_COLORS_H

#include <stdbool.h>
#include <stdarg.h>  // For va_list support in helpers

// ============================================================================
// BASIC ANSI ESCAPE SEQUENCES
// ============================================================================

// Color codes
#define ANSI_RED     "\033[91m"
#define ANSI_GREEN   "\033[92m"
#define ANSI_YELLOW  "\033[93m"
#define ANSI_BLUE    "\033[94m"
#define ANSI_MAGENTA "\033[95m"
#define ANSI_CYAN    "\033[96m"
#define ANSI_GRAY    "\033[90m"
#define ANSI_WHITE   "\033[97m"

// Text formatting
#define ANSI_BOLD    "\033[1m"
#define ANSI_DIM     "\033[2m"
#define ANSI_RESET   "\033[0m"

// ============================================================================
// COMPOSITE FORMATTING MACROS
// ============================================================================

/**
 * Format error messages (red + bold)
 * Usage: printf(FMT_ERROR("Invalid move!") "\n");
 */
#define FMT_ERROR(msg)   ANSI_RED ANSI_BOLD msg ANSI_RESET

/**
 * Format informational messages (yellow + bold)
 * Usage: printf(FMT_INFO("Move: e2e4") "\n");
 */
#define FMT_INFO(msg)    ANSI_YELLOW ANSI_BOLD msg ANSI_RESET

/**
 * Format hint/solution messages (gray)
 * Usage: printf(FMT_HINT("Try moving to a valid square") "\n");
 */
#define FMT_HINT(msg)    ANSI_GRAY msg ANSI_RESET

/**
 * Format data values (cyan + bold)
 * Usage: printf(FMT_DATA("Target: e4") "\n");
 */
#define FMT_DATA(msg)    ANSI_CYAN ANSI_BOLD msg ANSI_RESET

// ============================================================================
// COLOR CONTROL
// ============================================================================

/**
 * Global flag to enable/disable ANSI colors
 * Set to false for platforms without ANSI support
 */
extern bool g_colors_enabled;

/**
 * @brief Disable all ANSI color output
 * 
 * Call this function if the terminal doesn't support ANSI codes.
 * Macros will still work but produce plain text output.
 * 
 * Note: Current implementation doesn't support runtime disable.
 * This is a placeholder for future enhancement.
 */
static inline void disable_all_colors(void) {
    // Future: Could implement runtime color disable by
    // redefining macros to empty strings
    g_colors_enabled = false;
}

#endif // GAME_COLORS_H
