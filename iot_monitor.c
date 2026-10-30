/*
 * Dynamic IoT Event Monitoring and Callback Engine
 *
 * DEVICE TYPES, SIMULATED RANGES AND OPERATING LIMITS
 *   Type         Union member   Unit  Simulated range   Normal range
 *   Temperature  temperature    °C    10.0 to 50.0      15.0 to 40.0
 *   Pressure     pressure       kPa   80 to 130         95 to 115
 *   Battery      voltage        V     3.00 to 4.40      3.40 to 4.20
 *                                     (below 3.20 V is reported as critical)
 *   The simulated ranges are wider than the normal ranges on purpose, so both
 *   normal readings and alerts appear. All limits are #defines below.
 *
 * CALLBACK DISPATCH
 *   Each reading is passed to process_device() twice: first with the callback
 *   chosen from type_callbacks[device->type] (temperature_monitor,
 *   pressure_monitor or battery_monitor), then with the custom
 *   sensor_drift_detector. No callback reads a union member other than the one
 *   that matches device->type.
 *
 * CUSTOM CALLBACK: Sensor Drift Detector
 *   Purpose   - catches a sensor whose value jumps suddenly between two
 *               consecutive readings, which usually means a faulty sensor, a
 *               loose connection or a real sudden event, even when both values
 *               are inside the normal range.
 *   Algorithm - drift = |current reading - previous reading of the same device|
 *               if the device has no previous reading, the current one is
 *               stored as the baseline and nothing is flagged
 *               if drift > drift limit for the type, raise a drift alert
 *               the current reading then becomes the new previous reading
 *   Limits    - temperature 10.0 °C, pressure 15 kPa, battery 0.50 V
 *   Results   - number of comparisons, drift alerts per type and the largest
 *               jump seen, all shown in the monitoring summary.
 *
 * Build: gcc -Wall -Wextra -o iot_monitor iot_monitor.c
 * Run:   iot_monitor          (seeds rand() with the current time)
 *        iot_monitor 42       (fixed seed, so the run can be repeated)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* Simulated ranges and operating limits */
#define TEMP_SIM_MIN 10.0f
#define TEMP_SIM_MAX 50.0f
#define TEMP_MIN 15.0f
#define TEMP_MAX 40.0f
#define TEMP_DRIFT 10.0f

#define PRESSURE_SIM_MIN 80
#define PRESSURE_SIM_MAX 130
#define PRESSURE_MIN 95
#define PRESSURE_MAX 115
#define PRESSURE_DRIFT 15

#define VOLT_SIM_MIN 3.00f
#define VOLT_SIM_MAX 4.40f
#define VOLT_MIN 3.40f
#define VOLT_MAX 4.20f
#define VOLT_CRITICAL 3.20f
#define VOLT_DRIFT 0.50f

#define NAME_LEN 40
#define TEXT_LEN 96
#define INITIAL_CAPACITY 2

enum DeviceType { TEMPERATURE, PRESSURE, BATTERY, TYPE_COUNT };
enum DeviceStatus { STATUS_NO_READING, STATUS_NORMAL, STATUS_ALERT };

const char *type_names[TYPE_COUNT] = {"Temperature", "Pressure", "Battery"};
const char *type_units[TYPE_COUNT] = {"°C", "kPa", "V"};
const int type_decimals[TYPE_COUNT] = {1, 0, 2};
const char *status_names[] = {"NO READING", "NORMAL", "ALERT"};

/* Only the member that matches the device type is ever valid */
union DeviceReading
{
    float temperature;
    int pressure;
    float voltage;
};

struct Device
{
    int id;
    char name[NAME_LEN];
    int type;
    union DeviceReading reading;
    int status;

    union DeviceReading previous; /* last processed reading, used by the drift detector */
    int has_previous;
    int readings_processed;
    int alert_count;
};

/* A generated reading waiting to be processed. It stores the device ID, not a
   pointer, because the device array can move or shrink before processing */
struct PendingReading
{
    int device_id;
    int type;
    union DeviceReading value;
};

/* Alerts copy the device details so they stay valid after the device is removed */
struct Alert
{
    int device_id;
    char device_name[NAME_LEN];
    int type;
    union DeviceReading value;
    char expected[TEXT_LEN];
    char reason[TEXT_LEN];
    const char *callback;
};

typedef void (*device_callback)(struct Device *device);

struct Device *devices = NULL;
int device_count = 0, device_capacity = 0;

struct PendingReading *pending = NULL;
int pending_count = 0, pending_capacity = 0;

struct Alert *alerts = NULL;
int alert_count = 0, alert_capacity = 0;

/* Running totals for the monitoring summary */
struct
{
    int registered;
    int removed;
    int processed;
    int normal;
    int abnormal;
    int skipped;
    int range_alerts[TYPE_COUNT];
    int drift_alerts[TYPE_COUNT];
    int drift_checks;
    int baselines;
    double largest_drift;
    int largest_drift_type;
    int largest_drift_id;
    char largest_drift_name[NAME_LEN];
} stats;

int next_device = 0; /* rotates through the devices when readings are generated */

/* ---------------------------------------------------------------------------
 * Input helpers
 * ------------------------------------------------------------------------- */

/* Reads one line into buf, dropping the newline and anything that did not fit */
int read_line(char *buf, size_t size)
{
    size_t len;
    int ch;

    if (fgets(buf, (int)size, stdin) == NULL)
        return 0;

    len = strlen(buf);

    if (len > 0 && buf[len - 1] == '\n')
        buf[--len] = '\0';
    else
        while ((ch = getchar()) != '\n' && ch != EOF)
            ;

    while (len > 0 && isspace((unsigned char)buf[len - 1]))
        buf[--len] = '\0';

    return 1;
}

/* Prompts until a whole-line integer is entered. Returns 0 at end of input */
int get_int(const char *prompt, int *value)
{
    char line[64];
    char *end;
    long n;

    while (1)
    {
        printf("%s", prompt);

        if (!read_line(line, sizeof line))
            return 0;

        n = strtol(line, &end, 10);

        if (end != line && *end == '\0')
        {
            *value = (int)n;
            return 1;
        }

        printf("Invalid input. Enter a whole number.\n");
    }
}

/* ---------------------------------------------------------------------------
 * Union helpers: the device type decides which member is read or written
 * ------------------------------------------------------------------------- */

double reading_as_double(int type, const union DeviceReading *r)
{
    switch (type)
    {
    case TEMPERATURE:
        return r->temperature;
    case PRESSURE:
        return r->pressure;
    case BATTERY:
        return r->voltage;
    }

    return 0.0;
}

void format_reading(char *buf, size_t size, int type, const union DeviceReading *r)
{
    switch (type)
    {
    case TEMPERATURE:
        snprintf(buf, size, "%.1f %s", r->temperature, type_units[type]);
        break;
    case PRESSURE:
        snprintf(buf, size, "%d %s", r->pressure, type_units[type]);
        break;
    case BATTERY:
        snprintf(buf, size, "%.2f %s", r->voltage, type_units[type]);
        break;
    default:
        snprintf(buf, size, "?");
    }
}

float random_float(float min, float max)
{
    return min + (max - min) * ((float)rand() / RAND_MAX);
}

int random_int(int min, int max)
{
    return min + rand() % (max - min + 1);
}

/* Fills the union member for the given type with a value in its simulated range */
union DeviceReading simulate_reading(int type)
{
    union DeviceReading r;

    switch (type)
    {
    case TEMPERATURE:
        r.temperature = random_float(TEMP_SIM_MIN, TEMP_SIM_MAX);
        break;
    case PRESSURE:
        r.pressure = random_int(PRESSURE_SIM_MIN, PRESSURE_SIM_MAX);
        break;
    default:
        r.voltage = random_float(VOLT_SIM_MIN, VOLT_SIM_MAX);
        break;
    }

    return r;
}

/* ---------------------------------------------------------------------------
 * Dynamic arrays
 * ------------------------------------------------------------------------- */

/* Doubles an array's capacity. Returns the new block, or NULL with the old one untouched */
void *grow_array(void *array, int *capacity, size_t element_size, const char *label)
{
    int new_cap = *capacity > 0 ? *capacity * 2 : INITIAL_CAPACITY;
    void *grown = realloc(array, new_cap * element_size);

    if (grown == NULL)
    {
        printf("Memory allocation failed while growing the %s.\n", label);
        return NULL;
    }

    printf("  [memory] %s grown %d -> %d entries, now at %p\n", label, *capacity, new_cap, grown);
    *capacity = new_cap;
    return grown;
}

struct Device *find_device(int id)
{
    struct Device *d;

    for (d = devices; d < devices + device_count; d++)
        if (d->id == id)
            return d;

    return NULL;
}

/* Adds a device to the array. Returns 1 on success */
int add_device(int id, const char *name, int type)
{
    struct Device *grown;
    struct Device *d;

    if (find_device(id) != NULL)
    {
        printf("A device with ID %d is already registered.\n", id);
        return 0;
    }

    if (device_count == device_capacity)
    {
        grown = grow_array(devices, &device_capacity, sizeof *devices, "device array");

        if (grown == NULL)
            return 0;

        devices = grown;
    }

    d = devices + device_count;
    memset(d, 0, sizeof *d);
    d->id = id;
    strncpy(d->name, name, NAME_LEN - 1);
    d->name[NAME_LEN - 1] = '\0';
    d->type = type;
    d->status = STATUS_NO_READING;

    device_count++;
    stats.registered++;
    printf("Registered device %d \"%s\" (%s) at %p\n", d->id, d->name, type_names[type], (void *)d);
    return 1;
}

/* Halves the device array when it is at most a quarter full */
void shrink_devices(void)
{
    struct Device *shrunk;
    int new_cap = device_capacity / 2;

    if (device_capacity <= INITIAL_CAPACITY || device_count > device_capacity / 4)
        return;

    shrunk = realloc(devices, new_cap * sizeof *devices);

    /* A failed shrink is harmless: the larger block is still valid */
    if (shrunk == NULL)
        return;

    printf("  [memory] device array shrunk %d -> %d entries, now at %p\n",
           device_capacity, new_cap, (void *)shrunk);
    devices = shrunk;
    device_capacity = new_cap;
}

/* ---------------------------------------------------------------------------
 * Alerts and callbacks
 * ------------------------------------------------------------------------- */

/* Marks the device as abnormal, prints the alert and stores a copy in the log */
void raise_alert(struct Device *device, const char *callback, const char *expected,
                 const char *reason, int from_custom)
{
    struct Alert *grown;
    struct Alert *a;

    device->status = STATUS_ALERT;
    device->alert_count++;

    if (from_custom)
        stats.drift_alerts[device->type]++;
    else
        stats.range_alerts[device->type]++;

    printf("    -> %-22s ALERT: %s (expected %s)\n", callback, reason, expected);

    if (alert_count == alert_capacity)
    {
        grown = grow_array(alerts, &alert_capacity, sizeof *alerts, "alert log");

        if (grown == NULL)
        {
            printf("       The alert above could not be stored in the log.\n");
            return;
        }

        alerts = grown;
    }

    a = alerts + alert_count++;
    a->device_id = device->id;
    strcpy(a->device_name, device->name);
    a->type = device->type;
    a->value = device->reading;
    snprintf(a->expected, sizeof a->expected, "%s", expected);
    snprintf(a->reason, sizeof a->reason, "%s", reason);
    a->callback = callback;
}

void report_normal(struct Device *device, const char *callback, const char *expected)
{
    device->status = STATUS_NORMAL;
    printf("    -> %-22s normal (within %s)\n", callback, expected);
}

/* Each callback checks it was given the right device type before touching the union */
int wrong_type(const struct Device *device, int expected_type, const char *callback)
{
    if (device->type == expected_type)
        return 0;

    printf("    -> %s skipped: device %d is not a %s device\n", callback, device->id,
           type_names[expected_type]);
    return 1;
}

void temperature_monitor(struct Device *device)
{
    char expected[TEXT_LEN];
    char reason[TEXT_LEN];
    float t;

    if (wrong_type(device, TEMPERATURE, __func__))
        return;

    t = device->reading.temperature;
    snprintf(expected, sizeof expected, "%.1f to %.1f °C", TEMP_MIN, TEMP_MAX);

    if (t > TEMP_MAX)
        snprintf(reason, sizeof reason, "overheating, %.1f °C above the maximum", t - TEMP_MAX);
    else if (t < TEMP_MIN)
        snprintf(reason, sizeof reason, "too cold, %.1f °C below the minimum", TEMP_MIN - t);
    else
    {
        report_normal(device, __func__, expected);
        return;
    }

    raise_alert(device, __func__, expected, reason, 0);
}

void pressure_monitor(struct Device *device)
{
    char expected[TEXT_LEN];
    char reason[TEXT_LEN];
    int p;

    if (wrong_type(device, PRESSURE, __func__))
        return;

    p = device->reading.pressure;
    snprintf(expected, sizeof expected, "%d to %d kPa", PRESSURE_MIN, PRESSURE_MAX);

    if (p > PRESSURE_MAX)
        snprintf(reason, sizeof reason, "over-pressure, %d kPa above the maximum", p - PRESSURE_MAX);
    else if (p < PRESSURE_MIN)
        snprintf(reason, sizeof reason, "under-pressure, %d kPa below the minimum", PRESSURE_MIN - p);
    else
    {
        report_normal(device, __func__, expected);
        return;
    }

    raise_alert(device, __func__, expected, reason, 0);
}

void battery_monitor(struct Device *device)
{
    char expected[TEXT_LEN];
    char reason[TEXT_LEN];
    float v;

    if (wrong_type(device, BATTERY, __func__))
        return;

    v = device->reading.voltage;
    snprintf(expected, sizeof expected, "%.2f to %.2f V", VOLT_MIN, VOLT_MAX);

    if (v > VOLT_MAX)
        snprintf(reason, sizeof reason, "overcharged, %.2f V above the maximum", v - VOLT_MAX);
    else if (v < VOLT_CRITICAL)
        snprintf(reason, sizeof reason, "critically low, below %.2f V; replace the battery", VOLT_CRITICAL);
    else if (v < VOLT_MIN)
        snprintf(reason, sizeof reason, "low voltage, %.2f V below the minimum", VOLT_MIN - v);
    else
    {
        report_normal(device, __func__, expected);
        return;
    }

    raise_alert(device, __func__, expected, reason, 0);
}

/* Custom callback: flags a reading that jumps too far from the device's previous one */
void sensor_drift_detector(struct Device *device)
{
    static const double drift_limits[TYPE_COUNT] = {TEMP_DRIFT, PRESSURE_DRIFT, VOLT_DRIFT};
    char expected[TEXT_LEN];
    char reason[TEXT_LEN];
    char before[32], now[32];
    double limit = drift_limits[device->type];
    int decimals = type_decimals[device->type];
    double drift;

    format_reading(now, sizeof now, device->type, &device->reading);

    if (!device->has_previous)
    {
        printf("    -> %-22s baseline set at %s\n", __func__, now);
        stats.baselines++;
    }
    else
    {
        drift = reading_as_double(device->type, &device->reading) -
                reading_as_double(device->type, &device->previous);

        if (drift < 0)
            drift = -drift;

        stats.drift_checks++;

        if (drift > stats.largest_drift)
        {
            stats.largest_drift = drift;
            stats.largest_drift_type = device->type;
            stats.largest_drift_id = device->id;
            strcpy(stats.largest_drift_name, device->name);
        }

        format_reading(before, sizeof before, device->type, &device->previous);
        snprintf(expected, sizeof expected, "change of at most %.*f %s", decimals, limit,
                 type_units[device->type]);

        if (drift > limit)
        {
            snprintf(reason, sizeof reason, "sudden jump of %.*f %s (from %s to %s)",
                     decimals, drift, type_units[device->type], before, now);
            raise_alert(device, __func__, expected, reason, 1);
        }
        else
            printf("    -> %-22s stable, changed %.*f %s since %s\n", __func__, decimals, drift,
                   type_units[device->type], before);
    }

    /* Copying the whole union keeps whichever member is active */
    device->previous = device->reading;
    device->has_previous = 1;
}

/* Runs one callback on one device */
void process_device(struct Device *device, void (*callback)(struct Device *))
{
    if (device != NULL && callback != NULL)
        callback(device);
}

/* ---------------------------------------------------------------------------
 * Menu operations
 * ------------------------------------------------------------------------- */

void register_device(void)
{
    char name[NAME_LEN];
    int id, type;

    printf("\nREGISTER A NEW DEVICE\n");

    if (!get_int("Device ID (positive number): ", &id))
        return;

    if (id <= 0)
    {
        printf("The device ID must be a positive number.\n");
        return;
    }

    if (find_device(id) != NULL)
    {
        printf("A device with ID %d is already registered.\n", id);
        return;
    }

    printf("Device name: ");

    if (!read_line(name, sizeof name))
        return;

    if (name[0] == '\0')
    {
        printf("The device name cannot be empty.\n");
        return;
    }

    if (!get_int("Device type (1 = Temperature, 2 = Pressure, 3 = Battery): ", &type))
        return;

    if (type < 1 || type > TYPE_COUNT)
    {
        printf("The device type must be 1, 2 or 3.\n");
        return;
    }

    add_device(id, name, type - 1);
}

void display_devices(void)
{
    struct Device *d;
    char value[32];

    printf("\nREGISTERED DEVICES (%d of %d slots used, array at %p)\n",
           device_count, device_capacity, (void *)devices);

    if (device_count == 0)
    {
        printf("No devices are registered.\n");
        return;
    }

    printf("%-6s %-22s %-12s %-12s %-11s %8s %7s\n", "ID", "Name", "Type", "Last value", "Status",
           "Readings", "Alerts");
    printf("------------------------------------------------------------------------------------\n");

    for (d = devices; d < devices + device_count; d++)
    {
        if (d->status == STATUS_NO_READING)
            strcpy(value, "-");
        else
            format_reading(value, sizeof value, d->type, &d->reading);

        /* "°C" is two characters but three bytes, so pad its column by one more */
        printf("%-6d %-22s %-12s %-*s %-11s %8d %7d\n", d->id, d->name, type_names[d->type],
               d->type == TEMPERATURE && d->status != STATUS_NO_READING ? 13 : 12, value,
               status_names[d->status], d->readings_processed, d->alert_count);
    }
}

void remove_device(void)
{
    struct Device *d;
    int id;

    printf("\nREMOVE A DEVICE\n");

    if (!get_int("ID of the device to remove: ", &id))
        return;

    d = find_device(id);

    if (d == NULL)
    {
        printf("No device with ID %d is registered.\n", id);
        return;
    }

    printf("Removed device %d \"%s\".\n", d->id, d->name);

    /* Close the gap; entries after d move down one place */
    memmove(d, d + 1, (devices + device_count - (d + 1)) * sizeof *d);
    device_count--;
    stats.removed++;
    next_device = 0;

    shrink_devices();
}

void generate_readings(void)
{
    struct PendingReading *grown;
    struct PendingReading *r;
    struct Device *d;
    char value[32];
    int count, i;

    printf("\nGENERATE SIMULATED READINGS\n");

    if (device_count == 0)
    {
        printf("Register a device first.\n");
        return;
    }

    if (!get_int("How many readings? ", &count))
        return;

    if (count < 1 || count > 1000)
    {
        printf("Enter a number between 1 and 1000.\n");
        return;
    }

    /* Devices take turns, so every type reads and devices read more than once */
    for (i = 0; i < count; i++)
    {
        if (pending_count == pending_capacity)
        {
            grown = grow_array(pending, &pending_capacity, sizeof *pending, "reading queue");

            if (grown == NULL)
                return;

            pending = grown;
        }

        d = devices + (next_device % device_count);
        next_device = (next_device + 1) % device_count;

        r = pending + pending_count++;
        r->device_id = d->id;
        r->type = d->type;
        r->value = simulate_reading(d->type);

        format_reading(value, sizeof value, r->type, &r->value);
        printf("Queued reading %2d: device %d %-22s %-12s %s\n", pending_count, d->id, d->name,
               type_names[d->type], value);
    }

    printf("%d reading(s) waiting to be processed.\n", pending_count);
}

void process_readings(void)
{
    /* The callback for each device type, indexed by enum DeviceType */
    static const device_callback type_callbacks[TYPE_COUNT] = {
        temperature_monitor, pressure_monitor, battery_monitor};
    struct PendingReading *r;
    struct Device *d;
    char value[32];

    printf("\nPROCESS READINGS THROUGH CALLBACKS\n");

    if (pending_count == 0)
    {
        printf("No readings are waiting. Generate readings first.\n");
        return;
    }

    for (r = pending; r < pending + pending_count; r++)
    {
        d = find_device(r->device_id);

        if (d == NULL)
        {
            printf("\nDevice %d was removed; its queued reading is skipped.\n", r->device_id);
            stats.skipped++;
            continue;
        }

        d->reading = r->value;
        d->readings_processed++;
        stats.processed++;

        format_reading(value, sizeof value, d->type, &d->reading);
        printf("\nReading %d | ID %d | %s | %s | %s\n", stats.processed, d->id, d->name,
               type_names[d->type], value);

        process_device(d, type_callbacks[d->type]);
        process_device(d, sensor_drift_detector);

        if (d->status == STATUS_ALERT)
        {
            stats.abnormal++;
            printf("    Result: ABNORMAL\n");
        }
        else
        {
            stats.normal++;
            printf("    Result: NORMAL\n");
        }
    }

    pending_count = 0;
}

void display_alerts(void)
{
    struct Alert *a;
    char value[32];
    int n = 1;

    printf("\nDETECTED ALERTS (%d)\n", alert_count);

    if (alert_count == 0)
    {
        printf("No alerts have been raised.\n");
        return;
    }

    for (a = alerts; a < alerts + alert_count; a++, n++)
    {
        format_reading(value, sizeof value, a->type, &a->value);
        printf("\nAlert %d\n", n);
        printf("  Device:    %d \"%s\" (%s)\n", a->device_id, a->device_name, type_names[a->type]);
        printf("  Reading:   %s\n", value);
        printf("  Expected:  %s\n", a->expected);
        printf("  Reason:    %s\n", a->reason);
        printf("  Callback:  %s\n", a->callback);
    }
}

void monitoring_summary(void)
{
    int i;

    printf("\nMONITORING SUMMARY\n");
    printf("------------------------------------------\n");
    printf("Registered devices now:     %d (%d registered, %d removed)\n",
           device_count, stats.registered, stats.removed);
    printf("Readings processed:         %d\n", stats.processed);
    printf("Normal readings:            %d\n", stats.normal);
    printf("Abnormal readings:          %d\n", stats.abnormal);
    printf("Skipped (device removed):   %d\n", stats.skipped);
    printf("Readings still queued:      %d\n", pending_count);

    printf("\nAlerts per device type\n");
    printf("%-12s %12s %12s %8s\n", "Type", "Out of range", "Drift", "Total");

    for (i = 0; i < TYPE_COUNT; i++)
        printf("%-12s %12d %12d %8d\n", type_names[i], stats.range_alerts[i], stats.drift_alerts[i],
               stats.range_alerts[i] + stats.drift_alerts[i]);

    printf("\nCustom callback: sensor_drift_detector\n");
    printf("Baselines recorded:         %d\n", stats.baselines);
    printf("Consecutive comparisons:    %d\n", stats.drift_checks);
    printf("Drift alerts:               %d\n",
           stats.drift_alerts[TEMPERATURE] + stats.drift_alerts[PRESSURE] + stats.drift_alerts[BATTERY]);

    if (stats.drift_checks > 0)
        printf("Largest jump:               %.*f %s on device %d \"%s\"\n",
               type_decimals[stats.largest_drift_type], stats.largest_drift,
               type_units[stats.largest_drift_type], stats.largest_drift_id, stats.largest_drift_name);
}

/* ---------------------------------------------------------------------------
 * Main program
 * ------------------------------------------------------------------------- */

void free_all(void)
{
    free(devices);
    free(pending);
    free(alerts);
    devices = NULL;
    pending = NULL;
    alerts = NULL;
    device_count = pending_count = alert_count = 0;
}

int main(int argc, char *argv[])
{
    struct menu_item
    {
        const char *label;
        void (*run)(void);
    };
    static const struct menu_item menu[] = {
        {"Register a new device", register_device},
        {"Display all devices", display_devices},
        {"Remove a device", remove_device},
        {"Generate simulated readings", generate_readings},
        {"Process readings through callbacks", process_readings},
        {"Display detected alerts", display_alerts},
        {"Generate monitoring summary", monitoring_summary},
    };
    const int items = sizeof menu / sizeof menu[0];
    unsigned int seed;
    int choice, i;

#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    seed = argc > 1 ? (unsigned int)strtoul(argv[1], NULL, 10) : (unsigned int)time(NULL);
    srand(seed);

    printf("==========================================================\n");
    printf("Dynamic IoT Event Monitoring and Callback Engine\n");
    printf("Custom callback: Sensor Drift Detector\n");
    printf("Random seed: %u\n", seed);
    printf("==========================================================\n\n");

    /* Starting devices, two of each type, registered the same way as menu input */
    if (!add_device(101, "Boiler Room", TEMPERATURE) ||
        !add_device(102, "Server Rack", TEMPERATURE) ||
        !add_device(201, "Water Main", PRESSURE) ||
        !add_device(202, "HVAC Duct", PRESSURE) ||
        !add_device(301, "Gateway Battery", BATTERY) ||
        !add_device(302, "Field Node Battery", BATTERY))
    {
        free_all();
        return 1;
    }

    while (1)
    {
        printf("\n========== MENU ==========\n");

        for (i = 0; i < items; i++)
            printf("%d. %s\n", i + 1, menu[i].label);

        printf("%d. Exit\n", items + 1);

        if (!get_int("Choice: ", &choice) || choice == items + 1)
            break;

        if (choice < 1 || choice > items)
        {
            printf("Invalid choice. Enter a number between 1 and %d.\n", items + 1);
            continue;
        }

        menu[choice - 1].run();
    }

    free_all();
    printf("\nAll memory freed. Goodbye.\n");
    return 0;
}
