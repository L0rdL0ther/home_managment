#ifndef CONTROL_TYPES_H
#define CONTROL_TYPES_H

// Kontrol tiplerini tanımlayan enum
typedef enum {
    CONTROL_TYPE_SWITCH,         // On/Off control
    CONTROL_TYPE_SLIDER,         // Range control (e.g., dimmer, temperature)
    CONTROL_TYPE_RGB_PICKER,     // Color selection
    CONTROL_TYPE_BUTTON_GROUP,   // Multiple button options
    CONTROL_TYPE_NUMERIC_INPUT,  // Number input
    CONTROL_TYPE_TEXT_DISPLAY,   // Read-only text display
    CONTROL_TYPE_DROPDOWN,       // Selection from options
    CONTROL_TYPE_SCHEDULE,       // Time-based scheduling
    CONTROL_TYPE_UNKNOWN         // Unknown control type
} control_type_t;

#endif // CONTROL_TYPES_H